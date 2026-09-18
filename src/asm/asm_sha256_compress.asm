; ============================================================================
; CodeForge Assembly Core - SHA-256 block compression (x86-64, MASM/ml64)
; ============================================================================
; void cf_asm_sha256_compress(uint32_t state[8], const uint8_t block[64]);
;
; Full FIPS 180-4 SHA-256 compression function for one 64-byte block,
; hand-written in x86-64 assembly: 64 rounds of message-schedule expansion
; and state rotation. The caller owns chaining: initialize state with the
; SHA-256 IV, call once per padded block, read the final state as the digest.
; Differential-tested against the NIST "abc" vector and Qt's
; QCryptographicHash on random inputs (tests/test_asmcore.cpp).
;
; Windows x64 ABI: rcx = state, rdx = block.
; Register map:
;   rbp = state pointer   rbx = round index   W[0..63] = [rsp .. rsp+255]
;   a..h = r8d..r15d      temps = eax, ecx, edx, esi, edi
; Non-leaf-shaped prolog (stack alloc) -> proper unwind metadata emitted.
; ============================================================================

option casemap:none

PUBLIC cf_asm_sha256_compress

.const
align 16
K256 dword 0428a2f98h, 071374491h, 0b5c0fbcfh, 0e9b5dba5h
      dword 03956c25bh, 059f111f1h, 0923f82a4h, 0ab1c5ed5h
      dword 0d807aa98h, 012835b01h, 0243185beh, 0550c7dc3h
      dword 072be5d74h, 080deb1feh, 09bdc06a7h, 0c19bf174h
      dword 0e49b69c1h, 0efbe4786h, 00fc19dc6h, 0240ca1cch
      dword 02de92c6fh, 04a7484aah, 05cb0a9dch, 076f988dah
      dword 0983e5152h, 0a831c66dh, 0b00327c8h, 0bf597fc7h
      dword 0c6e00bf3h, 0d5a79147h, 006ca6351h, 014292967h
      dword 027b70a85h, 02e1b2138h, 04d2c6dfch, 053380d13h
      dword 0650a7354h, 0766a0abbh, 081c2c92eh, 092722c85h
      dword 0a2bfe8a1h, 0a81a664bh, 0c24b8b70h, 0c76c51a3h
      dword 0d192e819h, 0d6990624h, 0f40e3585h, 0106aa070h
      dword 019a4c116h, 01e376c08h, 02748774ch, 034b0bcb5h
      dword 0391c0cb3h, 04ed8aa4ah, 05b9cca4fh, 0682e6ff3h
      dword 0748f82eeh, 078a5636fh, 084c87814h, 08cc70208h
      dword 090befffah, 0a4506cebh, 0bef9a3f7h, 0c67178f2h

.code
cf_asm_sha256_compress proc frame
    push    rbp
    .pushreg rbp
    push    rbx
    .pushreg rbx
    push    rsi
    .pushreg rsi
    push    rdi
    .pushreg rdi
    sub     rsp, 280                ; W[64] = 256 bytes, 16-aligned stack
    .allocstack 280
    .endprolog

    mov     rbp, rcx                ; rbp = state[8]
    ; load working variables a..h from the chaining state (FIPS 180-4)
    mov     r8d,  dword ptr [rbp + 0]
    mov     r9d,  dword ptr [rbp + 4]
    mov     r10d, dword ptr [rbp + 8]
    mov     r11d, dword ptr [rbp + 12]
    mov     r12d, dword ptr [rbp + 16]
    mov     r13d, dword ptr [rbp + 20]
    mov     r14d, dword ptr [rbp + 24]
    mov     r15d, dword ptr [rbp + 28]
    xor     rbx, rbx                ; i = 0

cf_sha_round:
    cmp     rbx, 16
    jae     cf_sha_sched
    ; W[i] = big-endian load of block[i]
    mov     eax, dword ptr [rdx + rbx*4]
    bswap   eax
    mov     dword ptr [rsp + rbx*4], eax
    jmp     cf_sha_have_w

cf_sha_sched:
    ; s0 = rotr(W[i-15],7) ^ rotr(W[i-15],18) ^ (W[i-15] >> 3)   -> ecx
    mov     eax, dword ptr [rsp + rbx*4 - 60]
    mov     ecx, eax
    ror     ecx, 7
    mov     edx, eax
    ror     edx, 18
    xor     ecx, edx
    shr     eax, 3
    xor     ecx, eax
    ; s1 = rotr(W[i-2],17) ^ rotr(W[i-2],19) ^ (W[i-2] >> 10)    -> esi
    mov     eax, dword ptr [rsp + rbx*4 - 8]
    mov     esi, eax
    ror     esi, 17
    mov     edi, eax
    ror     edi, 19
    xor     esi, edi
    shr     eax, 10
    xor     esi, eax
    ; W[i] = W[i-16] + s0 + W[i-7] + s1
    mov     eax, dword ptr [rsp + rbx*4 - 64]
    add     eax, ecx
    add     eax, dword ptr [rsp + rbx*4 - 28]
    add     eax, esi
    mov     dword ptr [rsp + rbx*4], eax

cf_sha_have_w:
    ; esi = Sigma1(e) = rotr(e,6) ^ rotr(e,11) ^ rotr(e,25)
    mov     esi, r12d
    ror     esi, 6
    mov     edi, r12d
    ror     edi, 11
    xor     esi, edi
    mov     edi, r12d
    ror     edi, 25
    xor     esi, edi
    ; edi = Ch(e,f,g) = (e & f) ^ (~e & g)
    mov     eax, r12d
    not     eax
    and     eax, r14d
    mov     edi, r12d
    and     edi, r13d
    or      edi, eax
    ; eax = T1 = h + Sigma1(e) + Ch + K[i] + W[i]
    ; NOTE 1: [K256 + rbx*4] cannot be encoded RIP-relative (an index
    ; register is present), and an absolute ADDR32 relocation is rejected
    ; by the x64 linker (LNK2017) - so materialize the table address with
    ; a RIP-relative LEA.
    ; NOTE 2: the LEA must target rcx, NOT rdx: rdx holds the block pointer
    ; for the W[0..15] big-endian loads of the next rounds (rcx is dead
    ; here - the state pointer lives in rbp).
    mov     eax, r15d
    add     eax, esi
    add     eax, edi
    lea     rcx, [K256]
    add     eax, dword ptr [rcx + rbx*4]
    add     eax, dword ptr [rsp + rbx*4]
    ; esi = Sigma0(a) = rotr(a,2) ^ rotr(a,13) ^ rotr(a,22)
    mov     esi, r8d
    ror     esi, 2
    mov     edi, r8d
    ror     edi, 13
    xor     esi, edi
    mov     edi, r8d
    ror     edi, 22
    xor     esi, edi
    ; edi = Maj(a,b,c) = (a&b) ^ (a&c) ^ (b&c)
    mov     edi, r8d
    and     edi, r9d
    mov     ecx, r8d
    and     ecx, r10d
    xor     edi, ecx
    mov     ecx, r9d
    and     ecx, r10d
    xor     edi, ecx
    ; T2 = Sigma0(a) + Maj(a,b,c)
    add     esi, edi
    ; e = d + T1 ; a = T1 + T2
    add     r11d, eax
    add     eax, esi
    ; rotate working variables a..h
    mov     r15d, r14d              ; h = g
    mov     r14d, r13d              ; g = f
    mov     r13d, r12d              ; f = e(old)
    mov     r12d, r11d              ; e = d + T1
    mov     r11d, r10d              ; d = c
    mov     r10d, r9d               ; c = b
    mov     r9d,  r8d               ; b = a(old)
    mov     r8d,  eax               ; a = T1 + T2

    inc     rbx
    cmp     rbx, 64
    jb      cf_sha_round

    ; fold the working variables back into the chaining state (H[i] += a..h)
    add     r8d,  dword ptr [rbp + 0]
    mov     dword ptr [rbp + 0],  r8d
    add     r9d,  dword ptr [rbp + 4]
    mov     dword ptr [rbp + 4],  r9d
    add     r10d, dword ptr [rbp + 8]
    mov     dword ptr [rbp + 8],  r10d
    add     r11d, dword ptr [rbp + 12]
    mov     dword ptr [rbp + 12], r11d
    add     r12d, dword ptr [rbp + 16]
    mov     dword ptr [rbp + 16], r12d
    add     r13d, dword ptr [rbp + 20]
    mov     dword ptr [rbp + 20], r13d
    add     r14d, dword ptr [rbp + 24]
    mov     dword ptr [rbp + 24], r14d
    add     r15d, dword ptr [rbp + 28]
    mov     dword ptr [rbp + 28], r15d

    add     rsp, 280
    pop     rdi
    pop     rsi
    pop     rbx
    pop     rbp
    ret
cf_asm_sha256_compress endp

end
