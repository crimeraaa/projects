#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Test cmd.exe and notepad.exe
#define PROGRAM_NAME L"cmd.exe"

HANDLE console_handle;
STARTUPINFOW startup_info;
PROCESS_INFORMATION process_info;

int main(void) {
    console_handle = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE,   // DWORD    dwDesiredAccess
        0,                              // DWORD    dwShareMode
        NULL,   // const SECURITY_ATTRIBUES *       lpSecurityAttributes
        CONSOLE_TEXTMODE_BUFFER,        // DWORD    dwFlags
        NULL                            // LPVOID   lpScreenBufferData
    );
    ZeroMemory(&startup_info, sizeof(startup_info));
    startup_info.cb = sizeof(startup_info);
    ZeroMemory(&process_info, sizeof(process_info));
    // Automatically "concatenated" (not really, is a preprocessor thing!)
    LPCWSTR program_name = L"C:\\WINDOWS\\system32\\" PROGRAM_NAME;
    // Expands to CreateProcessW so view the docs for that.
    // For dwCreation Flags, try 0, CREATE_NEW_CONSOLE, or DETACHED_PROCESS
    //  See: stackoverflow.com/a/68914196
    CreateProcessW(         // (Type)                   (Parameter Name)
        program_name,       // LPCWSTR                  lpApplicationName
        NULL,               // LPWSTR                   lpCommandLine
        NULL,               // LPSECURITY_ATTRIBUES     lpProcessAttributes
        NULL,               // LPSECURITY_ATTRIBUES     lpThreadAttributes
        TRUE,               // WINBOOL                  bInheritHandles
        CREATE_NEW_CONSOLE, // DWORD                    dwCreationFlags
        NULL,               // LPVOID                   lpEnvironment
        NULL,               // LPCWSTR                  lpCurrentDirectory
        &startup_info,      // LPSTARTUPINFOW           lpStartupInfo 
        &process_info       // LPPROCESS_INFORMATION    lpProcessInformation
    );
    ExitProcess(0);
    return 0;
}
