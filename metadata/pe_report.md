# PE binary report

## game: `GameRoot/Viper Racing.exe`

- Size: 388,608 bytes
- SHA-256: `d75b38228d4b68b2c5ef39252b95e43a9de16976005c26fab4491d05ce3d32d4`
- Machine: `0x014C`
- Image base: `0x400000`
- Entry RVA: `0x24C0`

| Section | RVA | Virtual size | File offset | Raw size |
|---|---:|---:|---:|---:|
| `.text` | `0x1000` | `0x3F88` | `0x400` | `0x4000` |
| `.rdata` | `0x5000` | `0x89` | `0x4400` | `0x200` |
| `.data` | `0x6000` | `0x2730` | `0x4600` | `0x1600` |
| `.idata` | `0x9000` | `0x6CE` | `0x5C00` | `0x800` |
| `.rsrc` | `0xA000` | `0x5887C` | `0x6400` | `0x58A00` |

### Imports

- `KERNEL32.dll`: `GetExitCodeProcess`, `CreateProcessA`, `GetLastError`, `CreateSemaphoreA`, `GetVersion`, `WaitForSingleObject`, `Sleep`, `GetDriveTypeA`, `GetFileAttributesA`, `GetModuleFileNameA`, `GetSystemDirectoryA`, `WinExec`, `GetDiskFreeSpaceA`, `GlobalMemoryStatus`, `ExitProcess`, `GetCPInfo`, `HeapCreate`, `GetEnvironmentStringsW`, `FreeEnvironmentStringsW`, `WideCharToMultiByte`, `FreeEnvironmentStringsA`, `GetEnvironmentStrings`, `RtlUnwind`, `SetStdHandle`, `LoadLibraryA`, `GetProcAddress`, `HeapAlloc`, `HeapFree`, `SetFilePointer`, `FlushFileBuffers`, `CloseHandle`, `WriteFile`, `GetCommandLineA`, `UnhandledExceptionFilter`, `GetStdHandle`, `GetFileType`, `SetHandleCount`, `GetOEMCP`, `GetACP`, `GetModuleHandleA`, `GetStartupInfoA`
- `USER32.dll`: `GetDlgItem`, `InvalidateRect`, `MessageBoxA`, `UpdateWindow`, `GetSystemMetrics`, `SetWindowLongA`, `EnableWindow`, `FindWindowA`, `ShowWindow`, `GetWindowLongA`
- `ADVAPI32.dll`: `RegCloseKey`, `RegQueryValueExA`, `RegOpenKeyExA`
- `WSOCK32.dll`: `send`, `recv`, `WSAStartup`, `socket`, `WSAGetLastError`, `closesocket`, `WSACleanup`, `inet_addr`, `gethostbyname`, `htons`, `connect`, `bind`
- `VERSION.dll`: `GetFileVersionInfoA`, `VerQueryValueA`
## engine: `GameBin/race.bin`

- Size: 1,298,432 bytes
- SHA-256: `f9d6bd3ee1b9e8a7e101bf4cc3c4712969176255111bcf76a85d82568096da56`
- Machine: `0x014C`
- Image base: `0x400000`
- Entry RVA: `0xBD690`

| Section | RVA | Virtual size | File offset | Raw size |
|---|---:|---:|---:|---:|
| `.text` | `0x1000` | `0xC5CCD` | `0x400` | `0xC5E00` |
| `.rdata` | `0xC7000` | `0x5244` | `0xC6200` | `0x5400` |
| `.data` | `0xCD000` | `0x9F6FC` | `0xCB600` | `0x1E200` |
| `.idata` | `0x16D000` | `0x13DE` | `0xE9800` | `0x1400` |
| `.rsrc` | `0x16F000` | `0x5237C` | `0xEAC00` | `0x52400` |

### Imports

- `KERNEL32.dll`: `CreateSemaphoreA`, `ExitProcess`, `GetModuleFileNameA`, `UnmapViewOfFile`, `GetExitCodeProcess`, `CreateFileMappingA`, `GetCurrentThreadId`, `CreateProcessA`, `QueryPerformanceCounter`, `QueryPerformanceFrequency`, `GetSystemInfo`, `WaitForSingleObject`, `HeapAlloc`, `HeapValidate`, `HeapCreate`, `HeapDestroy`, `CreateThread`, `SetThreadPriority`, `GetCurrentThread`, `ResumeThread`, `Sleep`, `SuspendThread`, `ExitThread`, `GetExitCodeThread`, `InitializeCriticalSection`, `HeapFree`, `LeaveCriticalSection`, `DeleteCriticalSection`, `CreateEventA`, `PulseEvent`, `SetUnhandledExceptionFilter`, `CreateDirectoryA`, `MapViewOfFile`, `GetLocaleInfoA`, `GetCurrencyFormatA`, `GetDateFormatA`, `ClearCommError`, `SetCommTimeouts`, `GetCommModemStatus`, `GetCommProperties`, `SetCommState`, `GetCommState`, `SetConsoleTitleA`, `SetConsoleMode`, `GetStdHandle`, `AllocConsole`, `WriteConsoleA`, `FreeConsole`, `ReadConsoleA`, `MultiByteToWideChar`, `FindNextFileA`, `FindFirstFileA`, `DeleteFileA`, `SetFileAttributesA`, `GetCurrentProcess`, `DuplicateHandle`, `WriteFile`, `ReadFile`, `GetLastError`, `GetFileSize`, `CloseHandle`, `SetFilePointer`, `GetTempFileNameA`, `CreateFileA`, `CopyFileA`, `LoadLibraryA`, `CreateMailslotA`, `FindClose`, `DeviceIoControl`, `EnterCriticalSection`, `HeapReAlloc`, `LCMapStringW`, `GetStringTypeA`, `GetProcAddress`, `GetFullPathNameA`, `SetCurrentDirectoryA`, `GetCurrentDirectoryA`, `SetEnvironmentVariableA`, `GetCommandLineA`, `GetStartupInfoA`, `GetModuleHandleA`, `GetVersion`, `HeapSize`, `LCMapStringA`, `UnhandledExceptionFilter`, `GlobalAlloc`, `FreeEnvironmentStringsA`, `GetEnvironmentStrings`, `FreeEnvironmentStringsW`, `GetEnvironmentStringsW`, `WideCharToMultiByte`, `GetCPInfo`, `GetACP`, `GetOEMCP`, `SetHandleCount`, `FlushFileBuffers`, `GlobalLock`, `GlobalUnlock`, `GetFileType`, `GetDriveTypeA`, `RtlUnwind`, `RaiseException`, `GetStringTypeW`, `SetStdHandle`, `GetLocalTime`
- `USER32.dll`: `GetSystemMetrics`, `GetForegroundWindow`, `LoadBitmapA`, `BeginPaint`, `EndPaint`, `CreateWindowExA`, `LoadCursorA`, `RegisterClassA`, `SetCursor`, `UpdateWindow`, `FindWindowA`, `DestroyWindow`, `ShowWindow`, `IsWindow`, `DefWindowProcA`, `MessageBoxA`, `SetFocus`, `SetWindowsHookExA`, `PostQuitMessage`, `ShowCursor`, `SetCursorPos`, `MapVirtualKeyA`, `GetMessageA`, `TranslateMessage`, `DispatchMessageA`, `PeekMessageA`, `UnhookWindowsHookEx`, `SetForegroundWindow`, `CallNextHookEx`, `ClipCursor`, `LoadIconA`, `EmptyClipboard`, `SetClipboardData`, `OpenClipboard`, `GetClipboardData`, `CloseClipboard`
- `GDI32.dll`: `DeleteDC`, `CreateCompatibleDC`, `DeleteObject`, `SelectObject`, `BitBlt`
- `ADVAPI32.dll`: `RegCloseKey`, `RegQueryValueExA`, `RegOpenKeyExA`, `RegEnumKeyExA`, `RegFlushKey`, `RegSetValueExA`
- `WINMM.dll`: `timeGetTime`, `timeKillEvent`
- `DDRAW.dll`: `DirectDrawEnumerateA`, `DirectDrawCreate`
- `DSOUND.dll`: `DirectSoundCreate`
- `DINPUT.dll`: `DirectInputCreateA`
- `WSOCK32.dll`: `WSACleanup`, `closesocket`, `WSAStartup`, `sendto`, `getsockopt`, `ntohs`, `bind`, `recvfrom`, `ioctlsocket`, `gethostname`, `gethostbyname`, `WSAGetLastError`, `inet_addr`, `WSAAsyncGetHostByName`, `WSACancelAsyncRequest`, `socket`, `htons`, `setsockopt`
- `TAPI32.dll`: `lineShutdown`, `lineInitialize`, `lineGetDevCaps`, `lineAnswer`, `lineMakeCall`, `lineNegotiateAPIVersion`, `lineSetDevConfig`, `lineGetDevConfig`, `lineGetCallInfo`, `lineDrop`, `lineDeallocateCall`, `lineOpen`, `lineGetID`, `lineClose`
- `ole32.dll`: `CoUninitialize`, `CoInitialize`, `CoCreateInstance`
