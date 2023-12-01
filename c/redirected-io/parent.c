#define UNICODE // Used by Windows headers, is compiler-agnostic
#define _UNICODE // Used by compiler vendors, C runtime, MFC headers
#define WIN32_LEAN_AND_MEAN // Dunno if we need this but supposedly lightens windows.h
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <windows.h>
#include <shellapi.h>
#include <strsafe.h>

#define BUFFERSIZE 4096

// `rd` means "read".
HANDLE child_stdin_rd = NULL, child_stdout_rd = NULL;
// `wr` means "write".
HANDLE child_stdin_wr = NULL, child_stdout_wr = NULL;

HANDLE input_file = NULL;

// Poke at previously created pipes for STDIN and STDOUT.
void create_child_process(HANDLE child_stdin_read, HANDLE child_stdout_write);

// Read from a file and write contents to the pipe for child STDIN.
// Stop when there is no more data.
void write_to_pipe(HANDLE child_stdin_write);

// Read output from child process pipe for STDOUT
// then write to parent process pipe for STDOUT.
// Stop when no more data.
void read_from_pipe(HANDLE child_stdout_write);

// Format a readable error message and display a message box, then exit application.
void error_exit(wchar_t *logmessage);

// Adapted for my installation of mingw-w64,
// as it doesn't support the -municode linker flag.
int main() {
    // Hack for unicode `main` function without using `wmain`
    int argc; wchar_t **argv;

    // Helps control how the child process inherits from parent process.
    SECURITY_ATTRIBUTES se;
    
    // Hack: https://stackoverflow.com/a/55544461
    argv = CommandLineToArgvW(GetCommandLine(), &argc);

    printf("\n->Start of parent execution.\n");

    // Get handle from input file for the parent. This example assumes a plaintext
    // file and uses string output to verify dataflow.
    if (argc != 2) {
        error_exit(L"Please specify 1 input file.\n");
    }

    // set bInheritHandle flag so pipe handles are inherited.
    se.nLength = sizeof(SECURITY_ATTRIBUTES);
    se.bInheritHandle = TRUE;
    se.lpSecurityDescriptor = NULL;

    // Create pipe for child process's STDOUT
    if (!CreatePipe(&child_stdout_rd, &child_stdout_wr, &se, 0)) {
        error_exit(L"StdoutRd CreatePipe");
    }
    // Ensure the read handle to the pipe for STDOUT is not inherited.
    if (!SetHandleInformation(child_stdout_rd, HANDLE_FLAG_INHERIT, 0)) {
        error_exit(L"Stdout SetHandleInformation");
    }
    // Create a pipe for the child process's STDIN.
    if (!CreatePipe(&child_stdin_rd, &child_stdin_wr, &se, 0)) {
        error_exit(L"Stdin CreatePipe");
    }
    // Ensure the write handle to the pipe for STDIN is not inherited.
    if (!SetHandleInformation(child_stdin_wr, HANDLE_FLAG_INHERIT, 0)) {
        error_exit(L"Stdin SetHandleInformation");
    }
    // Poke at our globals to create the child process.
    create_child_process(child_stdin_rd, child_stdout_wr);

    input_file = CreateFile(
        argv[1],
        GENERIC_READ,
        0,
        NULL,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_READONLY,
        NULL
    );
    if (input_file == INVALID_HANDLE_VALUE) {
        error_exit(L"CreateFile");
    }
    // Write to the pipe that is the standard input for a child process.
    // Data is written to pipe's buffers, not necessary to wait
    // until the child process is running before writing data.
    write_to_pipe(child_stdin_wr);
    printf("\n->Contents of %ls written to child stdin pipe.\n", argv[1]);

    // Read from pipe that is the stdout for child process.
    printf("\n->Contents of child process STDOUT:\n\n");
    read_from_pipe(child_stdout_rd);
    printf("\n->End of parent execution.\n");
    // The remaining open handles are cleaned up when this process terminates.
    // To avoid resource leaks in larger application, close handles explicitly!
    CloseHandle(child_stdin_wr);
    CloseHandle(child_stdout_rd);
    LocalFree(argv); // Was allocated by GetCommandLineToArgvW
    return 0;
}

void create_child_process(HANDLE child_stdin_read, HANDLE child_stdout_write) {
    wchar_t cmdline[] = L"child"; // executable name is literally child.exe
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    BOOL success = FALSE;
    // Setup members of the PROCESS_INFORMATION struct.
    ZeroMemory(&pi, sizeof(PROCESS_INFORMATION));
    // Setup members of the STARTUPINFO struct.
    // It contains the STDIN and STDOUT handles for redirection.
    ZeroMemory(&si, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.hStdError = child_stdout_write;
    si.hStdOutput = child_stdout_write;
    si.hStdInput = child_stdin_read;
    si.dwFlags |= STARTF_USESTDHANDLES;

    // Create the child process.
    success = CreateProcess(
        NULL, // no module name, use cmdline
        cmdline, // command line arguments
        NULL, // process security attribs
        NULL, // primary thread security attributes
        TRUE, // handles are inherited
        0, // creation flags
        NULL, // use parent env
        NULL, // use parent CWD
        &si, // STARTUPINFO ptr
        &pi // receives PROCESS_INFORMATION
    );
    // If an error occurs, exit the application.
    if (!success) {
        error_exit(L"CreateProcess");
    }
    // Implied else: close handles to child process and its primary thread.
    // Some applications might keep these handles to monitor the status
    // of the child processs for example.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    // Close handles to the stdin and stdout pipes no longer needed by child.
    // If not explicitly closed, no way to recognize child process has ended!
    CloseHandle(child_stdin_read);
    CloseHandle(child_stdout_write);
}

void write_to_pipe(HANDLE child_stdin_write) {
    DWORD read, written;
    wchar_t buffer[BUFFERSIZE];
    BOOL success = FALSE;
    // Invokes an infinite loop
    for (;;) {
        success = ReadFile(input_file, buffer, BUFFERSIZE, &read, NULL);
        if (!success || read == 0) {
            break;
        }
        success = WriteFile(child_stdin_write, buffer, read, &written, NULL);
        if (!success) {
            break;
        }
    }
    // Close pipe handle so child process stops reading.
    if (!CloseHandle(child_stdin_write)) {
        error_exit(L"StdinWr CloseHandle");
    }
}

void read_from_pipe(HANDLE child_stdout_read) {
    DWORD read, written;
    wchar_t buffer[BUFFERSIZE];
    BOOL success = FALSE;
    HANDLE parent_stdout = GetStdHandle(STD_OUTPUT_HANDLE);
    // Invokes an infinite loop.
    for (;;) {
        success = ReadFile(child_stdout_read, buffer, BUFFERSIZE, &read, NULL);
        if (!success || read == 0) {
            break;
        }
        success = WriteFile(parent_stdout, buffer, read, &written, NULL);
        if (!success) {
            break;
        }
    }
}

void error_exit(wchar_t *logmessage) {
    wchar_t *message_buffer = NULL;
    wchar_t *display_buffer = NULL;
    DWORD lasterror = GetLastError();
    // Goodness those attributes
    FormatMessage(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        lasterror,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        message_buffer,
        0,
        NULL
    );
    // bruh
    display_buffer = LocalAlloc(
        LMEM_ZEROINIT,
        (lstrlen(message_buffer) + lstrlen(logmessage) + 40) * sizeof(wchar_t)
    );
    // I hate it here
    StringCchPrintf(
        display_buffer,
        LocalSize(display_buffer) / sizeof(wchar_t),
        L"%s failed with error %d: %s",
        logmessage,
        lasterror,
        message_buffer
    );
    
    // MessageBox(NULL, display_buffer, L"Error", MB_OK);
    fwprintf(stderr, L"Error: %ls", display_buffer);
    LocalFree(message_buffer);
    LocalFree(display_buffer);
    ExitProcess(1);
}
