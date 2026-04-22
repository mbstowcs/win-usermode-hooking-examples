/**
 * @file software_breakpoint.c
 * @brief Software breakpoint (0xCC) hooking example for MessageBoxA.
 *
 * Demonstrates how to intercept API calls by overwriting the first byte of a
 * function with an `int 3` (0xCC) instruction. A vectored exception handler
 * catches the resulting breakpoint exception and redirects execution to a
 * custom hook handler.
 */

#include <Windows.h>

/** Stores the original first byte of the hooked function. */
BYTE old;

/**
 * @brief Modifies the first byte of a function's code.
 *
 * Changes the first byte at the given address to `new_byte` and returns the
 * original byte so it can be restored later. Memory protection is temporarily
 * adjusted to allow writing to executable code.
 *
 * @param addr     Address of the function to modify.
 * @param new_byte Byte value to write as the first instruction.
 * @return         The original byte value that was replaced.
 */
BYTE SetCC(FARPROC addr, BYTE new_byte)
{
    // Save the original first byte
    BYTE old_byte = *((BYTE*)addr);

    DWORD old_protect;
    VirtualProtect((void*)addr, 1, PAGE_EXECUTE_READWRITE, &old_protect);

    // Overwrite the first byte with the new value
    *((BYTE*)addr) = new_byte;

    VirtualProtect((void*)addr, 1, old_protect, nullptr);

    return old_byte;
}

/**
 * @brief Hook handler for MessageBoxA.
 *
 * This function is invoked when the software breakpoint on MessageBoxA is hit.
 * It temporarily restores the original first byte, calls the real MessageBoxA
 * with a replacement message, and then reinstalls the 0xCC breakpoint for
 * subsequent calls.
 *
 * @param hWnd      Handle to the owner window.
 * @param lpText    Text to be displayed (ignored in this handler).
 * @param lpCaption Caption of the message box.
 * @param uType     Type of message box (ignored in this handler).
 * @return          Always 0.
 */
int WINAPI FakeMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    // Restore the original first byte so the real function can be called
    SetCC((FARPROC)MessageBoxA, old);

    // Call the original function with a replacement message
    MessageBoxA(hWnd, "MessageBoxA was hooked", lpCaption, MB_ICONSTOP);

    // Reinstall the software breakpoint for future calls
    SetCC((FARPROC)MessageBoxA, 0xCC);

    return 0;
}

/**
 * @brief Vectored exception handler that intercepts software breakpoints.
 *
 * When an `EXCEPTION_BREAKPOINT` occurs, this handler assumes it was triggered
 * by the hook on MessageBoxA and redirects execution to `FakeMessageBoxA` by
 * modifying the instruction pointer (Rip) in the exception context.
 *
 * @param exc Pointer to the exception information.
 * @return    `EXCEPTION_CONTINUE_EXECUTION` to resume execution at the new
 *            instruction pointer.
 */
LONG WINAPI Handler(_EXCEPTION_POINTERS *exc)
{
    if (exc->ExceptionRecord->ExceptionCode == EXCEPTION_BREAKPOINT)
    {
        // Redirect execution to our hook handler
        exc->ContextRecord->Rip = (DWORD64)FakeMessageBoxA;
    }

    return EXCEPTION_CONTINUE_EXECUTION;
}

int main()
{
    // Register a vectored exception handler to catch breakpoint exceptions
    AddVectoredExceptionHandler(1, Handler);

    // Original, unhooked call
    MessageBoxA(GetConsoleWindow(), "hello world", "hello", MB_ICONINFORMATION);

    // Install software breakpoint on MessageBoxA (first byte -> 0xCC)
    old = SetCC((FARPROC)MessageBoxA, 0xCC);

    // These calls trigger an EXCEPTION_BREAKPOINT and are redirected to FakeMessageBoxA
    MessageBoxA(GetConsoleWindow(), "hello world", "hello", MB_ICONINFORMATION);
    MessageBoxA(GetConsoleWindow(), "hello world", "hello", MB_ICONINFORMATION);

    return 0;
}