; ============================================================================
; CodeForge Assembly Core - strict UTF-8 validation (x86-64, MASM/ml64)
; ============================================================================
; int cf_asm_utf8_validate(const char* s, size_t n);
;
; Returns 1 when the n bytes at s form a strictly valid UTF-8 sequence,
; 0 otherwise. Rejects: lone continuation bytes, truncated sequences,
; overlong encodings (C0/C1 leads, E0 80..9F, F0 80..8F), UTF-16 surrogate
; halves (ED A0..BF), and code points above U+10FFFF (F4 90.., F5..FF).
; An empty range (n == 0) is vacuously valid.
;
; Windows x64 ABI: rcx = s, rdx = n. Leaf, volatile regs only.
; Register map: rax = index, r8 = pending continuation bytes,
;               r9 = lower bound for next continuation byte,
;               r11 = upper bound, r10d = loaded byte.
; ============================================================================

option casemap:none

PUBLIC cf_asm_utf8_validate

.code
cf_asm_utf8_validate proc
    xor     rax, rax                ; i = 0
    xor     r8,  r8                 ; need = 0
    mov     r9d, 080h               ; default continuation range [80,BF]
    mov     r11d, 0BFh
cf_u8_loop:
    cmp     rax, rdx
    jae     cf_u8_end
    movzx   r10d, byte ptr [rcx + rax]
    inc     rax
    test    r8, r8
    jz      cf_u8_lead
    ; --- expecting a continuation byte within [r9b, r11b] ---
    cmp     r10b, r9b
    jb      cf_u8_bad
    cmp     r10b, r11b
    ja      cf_u8_bad
    dec     r8
    jz      cf_u8_loop              ; sequence complete
    mov     r9d, 080h               ; non-final continuations: default range
    mov     r11d, 0BFh
    jmp     cf_u8_loop
cf_u8_lead:
    ; --- expecting a lead byte (or ASCII) ---
    cmp     r10b, 080h
    jb      cf_u8_loop              ; 00..7F: plain ASCII
    mov     r11d, 0BFh              ; default upper bound
    cmp     r10b, 0C2h
    jb      cf_u8_bad               ; 80..C1: lone continuation / overlong
    cmp     r10b, 0DFh
    jbe     cf_u8_2byte             ; C2..DF
    cmp     r10b, 0E0h
    je      cf_u8_e0
    cmp     r10b, 0EDh
    je      cf_u8_ed
    cmp     r10b, 0F0h
    je      cf_u8_f0
    cmp     r10b, 0F4h
    je      cf_u8_f4
    cmp     r10b, 0EFh
    jbe     cf_u8_3byte             ; E1..EC, EE..EF
    cmp     r10b, 0F3h
    jbe     cf_u8_4byte             ; F1..F3
    jmp     cf_u8_bad               ; F5..FF: beyond U+10FFFF
cf_u8_2byte:
    mov     r8d, 1
    mov     r9d, 080h
    jmp     cf_u8_loop
cf_u8_3byte:
    mov     r8d, 2
    mov     r9d, 080h
    jmp     cf_u8_loop
cf_u8_e0:
    mov     r8d, 2
    mov     r9d, 0A0h               ; overlong guard: next in A0..BF
    jmp     cf_u8_loop
cf_u8_ed:
    mov     r8d, 2
    mov     r9d, 080h
    mov     r11d, 09Fh              ; surrogate guard: next in 80..9F
    jmp     cf_u8_loop
cf_u8_f0:
    mov     r8d, 3
    mov     r9d, 090h               ; overlong guard: next in 90..BF
    jmp     cf_u8_loop
cf_u8_f4:
    mov     r8d, 3
    mov     r9d, 080h
    mov     r11d, 08Fh              ; >U+10FFFF guard: next in 80..8F
    jmp     cf_u8_loop
cf_u8_4byte:
    mov     r8d, 3
    mov     r9d, 080h
    jmp     cf_u8_loop
cf_u8_end:
    xor     eax, eax
    test    r8, r8
    jnz     cf_u8_ret               ; truncated sequence at end of input
    mov     eax, 1
cf_u8_ret:
    ret
cf_u8_bad:
    xor     eax, eax
    ret
cf_asm_utf8_validate endp

end
