#include <stdio.h>

/**
 * COMPILER OPTIONS:
 *      clang-cl /EHsc -v hello.c
 * 
 * (below: not needed, I mistakenly used x86 Dev PS, use x64 Dev CMD)
 * LINKER OPTIONS (append /link to COMPILER OPTIONS!)
 *      /entry:main /subsystem:console /out:hello.exe
 * 
 * LINKER LIBRARIES
 *      kernel32.lib legacy_stdio_definitions.lib msvcrt.lib
 * 
 *      If we don't specify /entry:main, clang-cl won't find this!
 * 
 * @note Please use x64 Developer Command Prompt! Developer PowerShell is x86...
 */
int main(void) {
    printf("Hi mom!\n");
    return 0;
}
