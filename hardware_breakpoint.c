/**
 * @file hardware_breakpoint.c
 * @brief Hardware breakpoint (debug register) hooking example for MessageBoxA.
 *
 * Demonstrates how to intercept API calls by setting a hardware breakpoint on
 * the target function using the x64 debug registers (Dr0–Dr7). When the
 * breakpoint is hit, a single‑step exception is raised and caught by a vectored
 * exception handler, which then redirects execution to a custom hook handler.
 */

#include <Windows.h>

/**
 * @brief Sets a local hardware breakpoint on the specified address.
 *
 * Configures debug register Dr0 to hold the target address and enables the
 * breakpoint by setting the appropriate bits in Dr7. The breakpoint is local
 * to the current thread.
 *
 * @param addr Address of the function on which to set the breakpoint.
 */
void SetBreakpoint(FARPROC addr)
{
    CONTEXT ctx = {0};
    ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;

    // Retrieve the current thread context
    GetThreadContext(GetCurrentThread(), &ctx);

    // Place the target address into debug register 0
    ctx.Dr0 = (DWORD64)addr;

    // Enable local breakpoint for Dr0 (L0 bit = 1)
    ctx.Dr7 = 1;

    // Apply the modified context
    SetThreadContext(GetCurrentThread(), &ctx);
}

/**
 * @brief Hook handler for MessageBoxA.
 *
 * This function is invoked after the hardware breakpoint is hit. The breakpoint
 * has already been disabled by the exception handler (Dr7 = 0), so this call
 * reaches the original MessageBoxA. After the real function returns, the
 * breakpoint is reinstalled for future calls.
 *
 * @param hWnd      Handle to the owner window.
 * @param lpText    Text to be displayed (ignored in this handler).
 * @param lpCaption Caption of the message box.
 * @param uType     Type of message box (ignored in this handler).
 * @return          Always 0.
 */
int WINAPI FakeMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    // The breakpoint is already disabled, so this calls the real MessageBoxA
    MessageBoxA(hWnd, "MessageBoxA was hooked", lpCaption, MB_ICONSTOP);

    // Reinstall the hardware breakpoint for subsequent calls
    SetBreakpoint((FARPROC)MessageBoxA);

    return 0;
}

/**
 * @brief Vectored exception handler that intercepts single‑step exceptions.
 *
 * When a hardware breakpoint is hit, the CPU raises a `EXCEPTION_SINGLE_STEP`
 * exception. This handler clears the breakpoint enable bits (Dr7 = 0) and
 * redirects execution to `FakeMessageBoxA` by modifying the instruction pointer
 * (Rip) in the exception context.
 *
 * @param exc Pointer to the exception information.
 * @return    `EXCEPTION_CONTINUE_EXECUTION` to resume execution at the new
 *            instruction pointer.
 */
LONG WINAPI Handler(_EXCEPTION_POINTERS *exc)
{
    if (exc->ExceptionRecord->ExceptionCode == EXCEPTION_SINGLE_STEP)
    {
        // Disable the hardware breakpoint
        exc->ContextRecord->Dr7 = 0;

        // Redirect execution to our hook handler
        exc->ContextRecord->Rip = (DWORD64)FakeMessageBoxA;
    }

    return EXCEPTION_CONTINUE_EXECUTION;
}

int main()
{
    // Register a vectored exception handler to catch single‑step exceptions
    AddVectoredExceptionHandler(1, Handler);

    // Original, unhooked call
    MessageBoxA(nullptr, "hello world", "hello", MB_ICONINFORMATION);

    // Set a hardware breakpoint on MessageBoxA
    SetBreakpoint((FARPROC)MessageBoxA);

    // These calls trigger an EXCEPTION_SINGLE_STEP and are redirected to FakeMessageBoxA
    MessageBoxA(nullptr, "hello world", "hello", MB_ICONINFORMATION);
    MessageBoxA(nullptr, "hello world", "hello", MB_ICONINFORMATION);

    return 0;
}