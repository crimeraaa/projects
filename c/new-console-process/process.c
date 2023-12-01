#include <stdarg.h>
#include <stdio.h>
#include <wchar.h> // Let's not use TCHAR, just assume UNICODE
#include <string.h>

#define UNICODE // Used by Windows Headers, is compiler-agnostic
#define _UNICODE // Compiler vendors, C-runtime/MFC Headers
#define WIN32_LEAN_AND_MEAN
#include <windows.h> // Is affected by UNICODE and _UNICODE
#include "process.h"

// Name of executable we want to create a child process out of.
#define EXE "cmd.exe"

// Adapted from:
// https://learn.microsoft.com/en-us/windows/win32/procthread/creating-processes
int main(void) {
    // typedef'd to `STARTUPINFOW` when `UNICODE` defined.
    STARTUPINFO si;

    // Holds our process and thread IDs + handles. Very important to keep around!
    PROCESS_INFORMATION pi;

    /**
     * Would use `LPCTSTR` but jesus christ just no...
     * "Long pointer to constant null-terminated `TCHAR` string"
     * 
     * @note Automatically "concatenated" (not really, is a preprocessor thing!)
     */
    const wchar_t *progname = L"C:\\WINDOWS\\system32\\" EXE;
    if (!child_init(&si, &pi, progname)) {
        eprintf("Could not create process.");
    }
    // Implicit else: we were successful! Can safely dereference `pi` (probably).
    printf(
        "Created %ls with process ID %ld and thread ID %ld.\n", 
        progname,
        pi.dwProcessId,
        pi.dwThreadId
    );

    const wchar_t *message = L"Hi mom!";
    DWORD length = wcslen(message);
    DWORD written = 0;
    WriteConsoleOutputCharacter(
        pi.hProcess, 
        message, 
        length,
        (COORD){0,9}, // Yes we need cast in C
        &written
    );

    // Wait until child process exits
    printf("Waiting for process to exit...\n");
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!child_clean(&pi, progname)) {
        eprintf("Could not close handle/s.");
        return 1;
    }
    return 0;
}

WINBOOL child_init(STARTUPINFO *si, PROCESS_INFORMATION *pi, const wchar_t *progname) {
    ZeroMemory(si, sizeof(*si)); // Do as the romans do ig
    ZeroMemory(pi, sizeof(*pi));
    si->cb = sizeof(*si);
    si->lpTitle = L"Hi mom!";
    // When UNICODE defined, expands to CreateProcessW (Wide/Unicode chars).
    // Otherwise, expands to else CreateProcessA (ANSI).
    // See: stackoverflow.com/a/68914196
    return CreateProcess(
        progname, // module name, if NULL use command line
        NULL, // command line arguments, if any
        NULL, // process handle is inheritable? NULL = no
        NULL, // thread handle is inhertiable? NULL = no
        TRUE, // set handle inheritance
        CREATE_NEW_CONSOLE, // creation flags: 0, CREATE_NEW_CONSOLE, DETACHED_PROCESS
        NULL, // NULL = use parent's environment block
        NULL, // NULL = use parent's starting directory
        si, // pass address to populate our startup info struct
        pi // pass address to populate our process info struct
    );
}

WINBOOL child_clean(PROCESS_INFORMATION *pi, const wchar_t *progname) {
    printf(
        "Closing %ls process %ld and thread %ld...\n",
        progname,
        pi->dwProcessId, 
        pi->dwThreadId
    );
    return CloseHandle(pi->hProcess) && CloseHandle(pi->hThread);
}
