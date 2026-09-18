; ============================================================================
; CodeForge Assembly Core - secure zeroization (x86-64, MASM/ml64)
; ============================================================================
; void cf_asm_secure_zero(void* p, size_t n);
;
; Writes zeros over n bytes at p. Compiled as real machine code, the stores
; cannot be elided by the C++ optimizer (unlike a plain memset the compiler
; may remove as "dead stores"). This is the Windows x64 hot path used by the
; security layer (SecureBuffer) to wipe keys and staged plaintext.
;
; Windows x64 calling convention: rcx = p, rdx = n. Leaf function, volatile
; registers only, no stack use, no unwind info required.
; ============================================================================

option casemap:none

PUBLIC cf_asm_secure_zero

.code
cf_asm_secure_zero proc
    mov     rax, rcx                ; rax = p (walking pointer)
    mov     r8,  rdx                ; r8 = n (saved for tail)
    mov     rcx, rdx
    shr     rcx, 3                  ; rcx = qword count
    test    rcx, rcx
    jz      cf_sz_tail
cf_sz_qwords:
    mov     qword ptr [rax], 0
    add     rax, 8
    dec     rcx
    jnz     cf_sz_qwords
cf_sz_tail:
    mov     rcx, r8
    and     rcx, 7                  ; remaining bytes (n mod 8)
    test    rcx, rcx
    jz      cf_sz_done
cf_sz_bytes:
    mov     byte ptr [rax], 0
    inc     rax
    dec     rcx
    jnz     cf_sz_bytes
cf_sz_done:
    ret
cf_asm_secure_zero endp

end
