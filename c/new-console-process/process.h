
#pragma once

// Prefer to define these on the command line, but this is for intellisense!
// See: https://stackoverflow.com/a/11950350
// and: https://devblogs.microsoft.com/oldnewthing/20040212-00/?p=40643

#include <windows.h> 

/**
 * Formatted error print. Writes to `stderr`.
 * @param file Pass the __FILE__ macro.
 * @param line Pass the __LINE__ macro.
 */
static inline int eprintf(const char *file, int line, const char *fmt, ...);

/**
 * Initialize some basic console window data and creates the child process.
 * @note Can fail on the call to `CreateProcess`. Please check the return value!
 * Use `GetLastError()`, a `DWORD` or `unsigned long`.
 */
WINBOOL child_init(STARTUPINFO *si, PROCESS_INFORMATION *pi, const wchar_t *progname);

/**
 * Close our created process and thread handles.
 * @note please check if `<return value> == TRUE`, our cleanup can fail!
 * In that case you'll have to check `GetLastError()`.
 */
WINBOOL child_clean(PROCESS_INFORMATION *pi, const wchar_t *progname);
