/*
 * Behavioral reconstruction of GameRoot/Viper Racing.exe.
 *
 * This is analyst-written C, not recovered original source. Address comments
 * refer to the original PE at preferred base 0x400000. Functions below are
 * kept as small evidence-backed fragments; unknown helper behavior is named
 * conservatively rather than guessed.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define VR_REG_KEY       "Software\\Sierra On-Line\\Viper Racing"
#define VR_VALUE_DATA    "DataDirectory"

/* VA 0x4015B0 delegates command token matching to 0x4023D0 and 0x402280. */
/* 0x401640 compares GetVersion() against 0x80000000; on NT it reports that
 * this product cannot run. */
static BOOL version_is_not_nt_family(DWORD version_value)
{
    return (version_value & 0x80000000u) != 0;
}

/* 0x401670's call at IAT 0x409264 passes parameter 0x13. Preserve as an
 * observed numeric predicate until the import name is identified. */
static BOOL platform_query_succeeded(LONG status)
{
    return status != 0;
}

/*
 * The startup checker at 0x401200 builds this ordered check list:
 * 0x401640 OS version, 0x401670 a 0x13 platform/system query,
 * 0x401690 GlobalMemoryStatus, 0x4019C0 DirectXSetup version,
 * 0x4017C0 CD volume validation, 0x4017B0 (empty), and 0x401730 disk space.
 * The caller short-circuits the list when byte [0x406030] becomes nonzero.
 */
static const DWORD g_startup_check_addresses[] = {
    0x401640u, 0x401670u, 0x401690u, 0x4019C0u,
    0x4017C0u, 0x4017B0u, 0x401730u
};

/* Directory resolution at VA 0x401880 (registry) and 0x40192C (fallback).
 * The registry lookup uses HKLM and the exact key/value strings above. */
static BOOL query_registry_data_directory(char *out, DWORD capacity)
{
    HKEY key = NULL;
    DWORD type = 0, bytes = capacity;
    LONG status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, VR_REG_KEY, 0,
                                KEY_QUERY_VALUE, &key);
    if (status != ERROR_SUCCESS)
        return FALSE;
    status = RegQueryValueExA(key, VR_VALUE_DATA, NULL, &type,
                              (BYTE *)out, &bytes);
    RegCloseKey(key);
    if (status != ERROR_SUCCESS || type != REG_SZ || bytes == 0 || bytes > capacity)
        return FALSE;
    out[capacity - 1] = '\0';
    return TRUE;
}

static BOOL module_directory_data_fallback(char *out, DWORD capacity)
{
    DWORD n = GetModuleFileNameA(NULL, out, capacity);
    char *slash;
    if (!n || n >= capacity)
        return FALSE;
    slash = strrchr(out, '\\');
    if (!slash)
        return FALSE;
    slash[1] = '\0';
    return strcat_s(out, capacity, "Data") == 0;
}

/*
 * Helper 0x401840 calls GetDriveTypeA (IAT 0x4091CC); the original receives a
 * formatted candidate path and tests whether its drive type is known. This
 * resolver uses that observed predicate for the registry branch and retains
 * the executable-directory fallback when registry resolution fails.
 */
static BOOL candidate_is_usable_drive(const char *path)
{
    UINT type = GetDriveTypeA(path);
    return type != DRIVE_UNKNOWN && type != DRIVE_NO_ROOT_DIR;
}

/* VA 0x401880 checks the formatted registry path, then calls the fallback. */
static BOOL resolve_data_directory(char *out, DWORD capacity)
{
    if (query_registry_data_directory(out, capacity) &&
        candidate_is_usable_drive(out))
        return TRUE;
    return module_directory_data_fallback(out, capacity);
}

/*
 * VA 0x4017C0 asks a translation/resource helper for IDs 0x41 through 0x5A,
 * formats "%sdata\\%s" (0x40656C), and passes each candidate to helper
 * 0x401840. If no candidate succeeds, it formats the message at 0x406578.
 * Resource-ID meanings and helper 0x401840's exact predicate are unresolved.
 */
static BOOL disc_volume_check_reconstruction(const char *candidate_path)
{
    UINT type = GetDriveTypeA(candidate_path);
    return type != DRIVE_UNKNOWN && type != DRIVE_NO_ROOT_DIR;
}

/* VA 0x401730 calls GetDiskFreeSpaceA only when global byte 0x406038 is set.
 * It compares the computed free-byte count against 20 MiB. */
static BOOL disk_space_threshold_met(DWORD sectors_per_cluster,
                                    DWORD bytes_per_sector,
                                    DWORD free_clusters)
{
    ULONGLONG free_bytes = (ULONGLONG)sectors_per_cluster *
                           bytes_per_sector * free_clusters;
    return free_bytes >= 0x01400000ull;
}

/* VA 0x4019C0 reads a version record and compares DWORD at offset 0x10
 * against 0x00040005. Failure displays the string at VA 0x4065D8. */
static BOOL directx_setup_version_supported(DWORD packed_version)
{
    return packed_version >= 0x00040005u;
}

/* VA 0x401690 emits distinct low-memory messages at 23, 31 and 39 MiB. */
typedef struct VrMemoryThresholds {
    DWORD physical_warning_1; /* 0x01700000 */
    DWORD physical_warning_2; /* 0x01F00000 */
    DWORD pagefile_warning;   /* 0x02700000 */
} VrMemoryThresholds;

static const VrMemoryThresholds g_memory_thresholds = {
    0x01700000u, 0x01F00000u, 0x02700000u
};

static void classify_memory_status(const MEMORYSTATUS *status,
                                   BOOL *below_required_physical,
                                   BOOL *below_recommended_physical,
                                   BOOL *below_recommended_pagefile)
{
    if (!status)
        return;
    if (below_required_physical)
        *below_required_physical = status->dwTotalPhys < g_memory_thresholds.physical_warning_1;
    if (below_recommended_physical)
        *below_recommended_physical = status->dwTotalPhys < g_memory_thresholds.physical_warning_2;
    if (below_recommended_pagefile)
        *below_recommended_pagefile = status->dwTotalPageFile < g_memory_thresholds.pagefile_warning;
}

/*
 * The launch block at 0x401456 invokes CreateProcessA via IAT 0x4091B0.
 * It passes the selected executable path separately from its command string,
 * STARTUPINFOA, and PROCESS_INFORMATION. Exact path/argument formatting is
 * delegated to helpers 0x401000 and 0x402210 and is not yet reconstructed.
 */
static BOOL launch_module_reconstruction(const char *module_path,
                                         const char *command_line)
{
    STARTUPINFOA startup;
    PROCESS_INFORMATION process;
    memset(&startup, 0, sizeof(startup));
    memset(&process, 0, sizeof(process));
    startup.cb = sizeof(startup);
    if (!CreateProcessA(module_path, (LPSTR)command_line, NULL, NULL, FALSE,
                        0, NULL, NULL, &startup, &process))
        return FALSE;
    WaitForSingleObject(process.hProcess, 2500);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return TRUE;
}

/*
 * Startup error selection at 0x4014BB uses a small jump table. Confirmed
 * messages include DX requirement (0x4062B4), 3D texture support (0x406308),
 * 3D device support (0x4062D8), and video initialization (0x40633C).
 * The resource/DirectX error codes mapped to those branches remain unknown.
 */
