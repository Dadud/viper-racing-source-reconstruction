/*
 * Early engine process/CRT reconstruction from race.bin.
 * Original code is not recovered; this file records observed state transitions
 * and recognizable library behavior from the entry point at 0x4BD690.
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

/* Internal malloc/free wrappers use the engine's private heap. */
extern void *__cdecl vr_private_alloc(size_t bytes); /* 0x4BF8C0 */
extern void __cdecl vr_private_free(void *memory);   /* 0x4C33B0 */

/* VA 0x4C14F0 counts environment strings without '='; hidden entries such
 * as '=C:=...' are also skipped because the later copy loop checks first char. */
static size_t vr_count_environment_entries(const char *block)
{
    size_t count = 0;
    const char *at = block;
    if (!at)
        return 0;
    while (*at) {
        if (*at != '=')
            ++count;
        at += strlen(at) + 1;
    }
    return count;
}

/*
 * 0x4C14F0 allocates a NULL-terminated pointer array, duplicates whole
 * environment strings whose first character is not '=', then passes the
 * original block to a CRT helper. Allocation helpers are at 0x4BF8C0 and
 * 0x4C33B0. This routine intentionally exposes the behavior without claiming
 * the original ownership or environment-string parser implementation.
 */
static char **vr_environment_block_reconstruction(const char *block)
{
    size_t count = vr_count_environment_entries(block), index = 0;
    char **copy;
    const char *at = block;
    if (!block)
        return NULL;
    copy = (char **)vr_private_alloc((count + 1) * sizeof(*copy));
    if (!copy)
        return NULL;
    memset(copy, 0, (count + 1) * sizeof(*copy));
    /* The binary duplicates complete NAME=VALUE strings, then terminates the
       pointer array with NULL. */
    while (*at) {
        if (*at != '=') {
            size_t length = strlen(at) + 1;
            copy[index] = (char *)vr_private_alloc(length);
            if (!copy[index]) {
                    vr_private_free(copy);
                    return NULL;
            }
            memcpy(copy[index], at, length);
            ++index;
        }
        at += strlen(at) + 1;
    }
    return copy;
}

static void vr_free_environment_reconstruction(char **entries, size_t count)
{
    size_t i;
    if (!entries)
        return;
    for (i = 0; i < count; ++i)
        if (entries[i])
            vr_private_free(entries[i]);
    vr_private_free(entries);
}

/* 0x4C1850 includes a wide-to-multibyte environment conversion path. */
static BOOL vr_convert_environment_block(const WCHAR *wide_block,
                                         char *narrow_block,
                                         DWORD capacity,
                                         DWORD *characters_written)
{
    int converted;
    if (!wide_block || !narrow_block || !capacity)
        return FALSE;
    converted = WideCharToMultiByte(CP_ACP, 0, wide_block, -1,
                                    narrow_block, (int)capacity,
                                    NULL, NULL);
    if (characters_written)
        *characters_written = converted > 0 ? (DWORD)converted : 0;
    return converted > 0;
}

/* ASCII approximation of the entry's executable-token scan. The binary also
 * checks multibyte lead bytes with __ismbblead; that behavior is not modeled
 * by this helper. The observed application handoff is 0x411F10. */
static const char *vr_skip_executable_token(const char *line)
{
    const char *at = line;
    if (!at)
        return NULL;
    if (*at == '"') {
        ++at;
        while (*at && *at != '"')
            ++at;
        if (*at == '"')
            ++at;
    } else {
        while (*at > ' ')
            ++at;
    }
    while (*at && *at <= ' ')
        ++at;
    return at;
}
