/**
 * @file iat_modifying.c
 * @brief IAT (Import Address Table) hooking example for MessageBoxA.
 *
 * Demonstrates how to intercept API calls by overwriting the function pointer
 * in the Import Address Table of the current executable. The original function
 * address is saved and can be used to call the real implementation.
 */

#include <Windows.h>

/** Function pointer type matching MessageBoxA. */
typedef int (WINAPI* MessageBoxA_ptr)(HWND, LPCSTR, LPCSTR, UINT);

/** Stores the address of the original MessageBoxA function. */
MessageBoxA_ptr OrigMessageBoxA;

/**
 * @brief Hook handler for MessageBoxA.
 *
 * This function replaces the original MessageBoxA in the IAT. It calls the
 * original function with a modified message to indicate that the call was
 * intercepted.
 *
 * @param hWnd      Handle to the owner window.
 * @param lpText    Text to be displayed (ignored in this handler).
 * @param lpCaption Caption of the message box.
 * @param uType     Type of message box (ignored in this handler).
 * @return          Always 0.
 */
int WINAPI FakeMessageBoxA(HWND hWnd, LPCSTR lpText, LPCSTR lpCaption, UINT uType)
{
    // Call the original function with a replacement message
    OrigMessageBoxA(hWnd, "MessageBoxA was hooked", lpCaption, MB_ICONSTOP);

    return 0;
}

/**
 * @brief Overwrites an entry in the Import Address Table (IAT) with a hook handler.
 *
 * Walks the IAT of the current executable, locates the entry for the specified
 * function from the given library, and replaces it with the address of `handler`.
 * The original address is returned so that the real function can still be called.
 *
 * @param libName    Name of the DLL (e.g., "user32.dll").
 * @param methodName Name of the function to hook (e.g., "MessageBoxA").
 * @param handler    Pointer to the hook handler function.
 * @return           The original address of the hooked function, or `nullptr` if
 *                   the function was not found in the IAT.
 */
FARPROC SetIATHook(const char *libName, const char *methodName, FARPROC handler)
{
    FARPROC res = nullptr;

    // Get the base address of the current executable
    HMODULE hModule = GetModuleHandle(nullptr);

    // Locate the NT headers via the DOS header
    auto *dosHeader = (IMAGE_DOS_HEADER*)hModule;
    auto *ntHeaders = (IMAGE_NT_HEADERS64*)((BYTE*)hModule + dosHeader->e_lfanew);

    // Get the import directory
    auto importTableEntry = (IMAGE_IMPORT_DESCRIPTOR*)((BYTE*)hModule +
        ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    // Walk through all imported DLLs
    for (; importTableEntry->Name; importTableEntry++)
    {
        char *dll_name = (char*)hModule + importTableEntry->Name;

        // Case‑insensitive comparison of DLL names
        if (stricmp(dll_name, libName) != 0)
            continue;

        // Pointers to the Import Address Table and the name table
        auto *iat = (DWORD64*)((BYTE*)hModule + importTableEntry->FirstThunk);
        auto *name_table = (DWORD*)((BYTE*)hModule + importTableEntry->OriginalFirstThunk);

        // Walk through all imported functions from this DLL
        for (int i = 0; name_table[i]; i++)
        {
            auto *imgByName = (IMAGE_IMPORT_BY_NAME*)((BYTE*)hModule + name_table[i]);

            if (strcmp(methodName, imgByName->Name) != 0)
                continue;

            // Make the IAT entry writable
            DWORD old_protect;
            VirtualProtect(&iat[i], sizeof(DWORD64), PAGE_EXECUTE_READWRITE, &old_protect);

            // Save original address and overwrite with our handler
            res = (FARPROC)iat[i];
            iat[i] = (DWORD64)handler;

            // Restore original protection
            VirtualProtect(&iat[i], sizeof(DWORD64), old_protect, nullptr);
        }
    }

    return res;
}

int main()
{
    // Original, unhooked call
    MessageBoxA(nullptr, "hello world", "hello", MB_ICONINFORMATION);

    // Hook MessageBoxA by modifying its IAT entry
    OrigMessageBoxA = (MessageBoxA_ptr)SetIATHook("user32.dll", "MessageBoxA", (FARPROC)FakeMessageBoxA);

    // These calls are intercepted by FakeMessageBoxA
    MessageBoxA(nullptr, "hello world", "hello", MB_ICONINFORMATION);
    MessageBoxA(nullptr, "hello world", "hello", MB_ICONINFORMATION);

    return 0;
}