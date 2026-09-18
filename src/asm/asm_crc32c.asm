; ============================================================================
; CodeForge Assembly Core - hardware CRC-32C (x86-64, MASM/ml64)
; ============================================================================
; uint32_t cf_asm_crc32c(uint32_t seed, const void* p, size_t n);
;
; CRC-32C (Castagnoli, poly 0x1EDC6F41 / reflected 0x82F63B78) computed with
; the SSE4.2 CRC32 instruction - one hardware cycle-stepped accumulation per
; chunk instead of the 8-iteration software loop. Reference check value:
;   crc32c(0, "123456789", 9) == 0xE3069283
;
; The function applies the standard external inversion: crc = ~seed on entry,
; ~crc on exit, so crc32c(0, ...) yields the published check value and
; chaining works: crc32c(crc32c(0,a,la), b, lb) == crc32c(0, ab, la+lb).
;
; Requires SSE4.2 (every x86-64 CPU targeted by Windows 10/11 has it).
; Windows x64 ABI: ecx = seed, rdx = p, r8 = n. Leaf, volatile regs only.
; ============================================================================

option casemap:none

PUBLIC cf_asm_crc32c

.code
cf_asm_crc32c proc
    mov     eax, ecx
    not     eax                     ; crc = ~seed (standard init)
cf_crc_align8:
    cmp     r8, 8
    jb      cf_crc_tail4
    crc32   rax, qword ptr [rdx]
    add     rdx, 8
    sub     r8, 8
    jmp     cf_crc_align8
cf_crc_tail4:
    cmp     r8, 4
    jb      cf_crc_tail2
    crc32   eax, dword ptr [rdx]
    add     rdx, 4
    sub     r8, 4
cf_crc_tail2:
    cmp     r8, 2
    jb      cf_crc_tail1
    crc32   eax, word ptr [rdx]
    add     rdx, 2
    sub     r8, 2
cf_crc_tail1:
    test    r8, r8
    jz      cf_crc_done
    crc32   eax, byte ptr [rdx]
cf_crc_done:
    not     eax                     ; crc = ~crc (standard final xor)
    ret
cf_asm_crc32c endp

end
