# User‑mode hooking methods

This repository demonstrates four classic techniques for intercepting API calls
in user mode **without modifying any executable file on disk**. All examples
target `MessageBoxA` from `user32.dll`.

## 1. Import Address Table (IAT) modification

`iat_modifying.c`

The most widely used method. It walks the Import Address Table of the current
process and replaces the target function pointer with the address of a custom
handler.

**Pros:**
- Simple to implement.
- Well‑understood and stable.

**Cons:**
- Only hooks functions that are **statically linked** (present in the IAT).
- Does not affect calls obtained via `GetProcAddress` or delayed imports.
- Can be mitigated by hooking `LoadLibrary` or `GetProcAddress` to catch
  dynamically resolved functions.

---

## 2. Inline code modification (push‑ret / jmp / call / …)

`trampoline.c`

Overwrites the first few bytes of the target function with a trampoline that
redirects execution to the hook handler. The original bytes are saved and can
be restored when calling the real function.

**Pros:**
- Hooks **all** calls, regardless of how the function was resolved.
- Works for both statically and dynamically linked functions.

**Cons:**
- On **x64** there is no `push imm64` instruction, so the trampoline requires
  more bytes (e.g., `mov rax, addr; push rax; ret`).
- A very short function may not have enough space to hold the trampoline
  without overwriting adjacent code.
- Easier to detect by integrity checks (e.g., comparing code sections against
  a known‑good hash).

---

## 3. Hardware breakpoints (debug registers)

`hardware_breakpoint.c`

Uses the x64 debug registers (`Dr0`–`Dr3`, `Dr7`) to set a hardware breakpoint
on the target function. A vectored exception handler catches the resulting
`EXCEPTION_SINGLE_STEP` and redirects execution.

**Pros:**
- **Safest** in terms of code integrity – the original function bytes are never
  modified.
- No need to temporarily remove the hook to call the original function.

**Cons:**
- Limited to **4 breakpoints** per thread (one per debug register).
- Exception handling is relatively **slow**.
- Easy to detect and disable (e.g., by clearing `Dr7` or checking debug
  registers).

---

## 4. Software breakpoint (int 3) + VEH

`software_breakpoint.c`

Replaces the **first byte** of the target function with `0xCC` (`int 3`). A
vectored exception handler intercepts the `EXCEPTION_BREAKPOINT`, temporarily
restores the original byte, calls the real function, and then reinstalls the
`0xCC` breakpoint.

**Pros:**
- Only **one byte** is modified – works for very short functions.
- Can hook an arbitrary number of functions (no limit like hardware
  breakpoints).
- Harder to spot at a glance than a full inline trampoline.

**Cons:**
- Still modifies code (1 byte), so integrity checks may detect it.
- Exception handling overhead remains, though often acceptable.
