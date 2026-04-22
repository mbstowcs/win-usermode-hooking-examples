/**
 * @file trampoline.c
 * @brief Inline hooking (trampoline) example for MessageBoxA.
 *
 * Demonstrates a simple 64‑bit hook by overwriting the first 12 bytes of a
 * target function with a trampoline that redirects execution to a user‑supplied
 * handler. The original bytes are preserved and can be restored.
 */

#include <Windows.h>

/** Buffer that holds the original first 12 bytes of the hooked function. */
BYTE old[12] = {0};

/**
 * @brief Installs an inline hook (trampoline) at the beginning of the target function.
 *
 * The first 12 bytes of `proc` are saved to the global `old` buffer and then
 * overwritten with a short assembly stub that jumps to `handler`. The stub uses
 * the `rax` register, which is safe under the stdcall/x64 calling convention
 * because it holds the return value and is otherwise unused early in the function.
 *
 * @param proc    Pointer to the function to be hooked.
 * @param handler Pointer to the hook handler (replacement function).
 */
void SetJMP(FARPROC proc, FARPROC handler)
{
    BYTE trampoline[] = {
        0x48, 0xb8,                         // mov rax, <address>
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, // placeholder for handler address
        0x50,                               // push rax
        0xc3                                // ret
    };

    // Write the handler address into the trampoline
    memcpy(trampoline + 2, &handler, 8);

    DWORD old_protect;
    VirtualProtect((void*)proc, 12, PAGE_EXECUTE_READWRITE, &old_protect);

    memcpy(old, (void*)proc, 12);          // save original bytes
    memcpy((void*)proc, trampoline, 12);   // overwrite with trampoline

    VirtualProtect((void*)proc, 12, old_protect, NULL);
}

/**
 * @brief Removes the previously installed hook and restores the original function bytes.
 *
 * The first 12 bytes of `proc` are restored from the global `old` buffer.
 *
 * @param proc Pointer to the hooked function.
 */
void RemoveJMP(FARPROC proc)
{
    DWORD old_protect;
    VirtualProtect((void*)proc, 12, PAGE_EXECUTE_READWRITE, &old_protect);

    memcpy((void*)proc, old, 12);          // restore original bytes

    VirtualProtect((void*)proc, 12, old_protect, NULL);
}

/**
 * @brief Hook handler for MessageBoxA.
 *
 * This function replaces the original MessageBoxA. It temporarily removes the
 * hook, displays a message box indicating that the call was intercepted, and
 * then reinstalls the hook for subsequent calls.
 *
 * @param hWnd      Handle to the owner window.
 * @param lpText    Text to be displayed (ignored in this handler).
 * @param lpCaption Caption of the message box.
 * @param uType     Type of message box (ignored in this handler).
 * @return          Always 0.
 */
int WINAPI FakeMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    RemoveJMP((FARPROC)MessageBoxA);

    MessageBoxA(hWnd, "MessageBoxA was hooked", lpCaption, MB_ICONSTOP);

    SetJMP((FARPROC)MessageBoxA, (FARPROC)FakeMessageBoxA);

    return 0;
}

int main()
{
    // Original, unhooked call
    MessageBoxA(GetConsoleWindow(), "hello world", "hello", MB_ICONINFORMATION);

    // Install the hook
    SetJMP((FARPROC)MessageBoxA, (FARPROC)FakeMessageBoxA);

    // These calls are intercepted by FakeMessageBoxA
    MessageBoxA(GetConsoleWindow(), "hello world", "hello", MB_ICONINFORMATION);
    MessageBoxA(GetConsoleWindow(), "hello world", "hello", MB_ICONINFORMATION);

    return 0;
}