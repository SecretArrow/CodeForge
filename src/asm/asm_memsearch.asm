; ============================================================================
; CodeForge Assembly Core - substring search (x86-64, MASM/ml64)
; ============================================================================
; int64_t cf_asm_memsearch(const uint8_t* hay, size_t hayLen,
;                          const uint8_t* needle, size_t needleLen);
;
; Returns the byte offset of the first occurrence of needle within hay,
; 0 when needle is empty, or -1 when not found. First-byte scanning keeps
; the common miss path to one compare per position.
;
; Windows x64 ABI: rcx = hay, rdx = hayLen, r8 = needle, r9 = needleLen.
; Non-leaf (preserves rsi/rdi), therefore carries proper unwind metadata.
; ============================================================================

option casemap:none

PUBLIC cf_asm_memsearch

.code
cf_asm_memsearch proc frame
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    .endprolog

    test    r9, r9
    jz      cf_ms_empty             ; empty needle matches at 0
    cmp     r9, rdx
    ja      cf_ms_notfound          ; needle longer than haystack
    mov     r10, rdx
    sub     r10, r9                 ; r10 = last valid start offset
    xor     r11, r11                ; r11 = current start offset
    movzx   eax, byte ptr [r8]      ; al = needle[0]
cf_ms_scan:
    cmp     r11, r10
    ja      cf_ms_notfound
    cmp     byte ptr [rcx + r11], al
    je      cf_ms_try
    inc     r11
    jmp     cf_ms_scan
cf_ms_try:
    lea     rsi, [rcx + r11 + 1]    ; hay  + i + 1
    lea     rdi, [r8 + 1]           ; needle + 1
    mov     rax, r9
    dec     rax                     ; remaining bytes to verify
cf_ms_inner:
    test    rax, rax
    jz      cf_ms_match
    movzx   edx, byte ptr [rsi]
    cmp     byte ptr [rdi], dl
    jne     cf_ms_next
    inc     rsi
    inc     rdi
    dec     rax
    jmp     cf_ms_inner
cf_ms_match:
    mov     rax, r11
    pop     rdi
    pop     rsi
    ret
cf_ms_next:
    inc     r11
    movzx   eax, byte ptr [r8]
    jmp     cf_ms_scan
cf_ms_empty:
    xor     eax, eax
    pop     rdi
    pop     rsi
    ret
cf_ms_notfound:
    mov     rax, -1
    pop     rdi
    pop     rsi
    ret
cf_asm_memsearch endp

end
