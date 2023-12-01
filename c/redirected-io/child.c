#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>

#define BUFSIZE 4096 
 
// https://learn.microsoft.com/en-us/windows/win32/procthread/creating-a-child-process-with-redirected-input-and-output
int main(void) 
{ 
    CHAR buffer[BUFSIZE]; 
    DWORD read, written; 
    HANDLE h_stdin, h_stdout; 
    BOOL h_success; 

    h_stdout = GetStdHandle(STD_OUTPUT_HANDLE); 
    h_stdin = GetStdHandle(STD_INPUT_HANDLE); 
    if ((h_stdout == INVALID_HANDLE_VALUE) || (h_stdin == INVALID_HANDLE_VALUE)) {
        ExitProcess(1);
    }
 
    // Send something to this process's stdout using printf.
    printf("\n ** This is a message from the child process. ** \n");

    // This simple algorithm uses the existence of the pipes to control execution.
    // It relies on the pipe buffers to ensure that no data is lost.
    // Larger applications would use more advanced process control.
    for (;;) { 
        // Read from standard input and stop on error or no data.
        h_success = ReadFile(h_stdin, buffer, BUFSIZE, &read, NULL); 
      
        if (! h_success || read == 0) {
            break;
        }
 
        // Write to standard output and stop on error.
        h_success = WriteFile(h_stdout, buffer, read, &written, NULL); 
      
        if (!h_success) {
            break; 
        }
    } 
    return 0;
}
