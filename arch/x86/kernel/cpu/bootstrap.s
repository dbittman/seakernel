[BITS 16]
[GLOBAL bootmainloop]
[GLOBAL bootmainloop_end]

bootmainloop:
  cli
  ;mov eax, 1
  ;mov DWORD [0x7200], eax
  xor ax, ax
  mov ds, ax
  mov si, 0x7100
  lgdt [ds:si]
  mov eax, cr0
  or al, 0x01
  mov cr0, eax
  ;jmp bootmainloop
  ; Jump into protected-mode
  jmp 0x08:0x7204

[BITS 32]
[GLOBAL RMGDT]
[GLOBAL RMGDT_END]
[GLOBAL pmode_enter]
[GLOBAL pmode_enter_end]
[EXTERN cpu_entry]
pmode_enter:
  ;jmp pmode_enter
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov gs, ax
  mov ss, ax
  mov esp, 0x6900
  mov eax, DWORD [0x72FC]
  jmp eax
pmode_enter_end:
  jmp pmode_enter_end



RMGDT:
  dd 0x00000000
  dd 0x00000000
  ; kernel-code
  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0x98
  db 0xCF
  db 0x00
  ; kernel-data
  dw 0xFFFF
  dw 0x0000
  db 0x00
  db 0x92
  db 0xCF
  db 0x00
RMGDT_END:
[GLOBAL GDTR]
GDTR:
  dw 0x18 - 1
  dd 0x7150

[bits 32]
[section .text]
[GLOBAL enable_A20]
enable_A20:
        cli
 
        call    a20wait
        mov     al,0xAD
        out     0x64,al
 
        call    a20wait
        mov     al,0xD0
        out     0x64,al
 
        call    a20wait2
        in      al,0x60
        push    eax
 
        call    a20wait
        mov     al,0xD1
        out     0x64,al
 
        call    a20wait
        pop     eax
        or      al,2
        out     0x60,al
 
        call    a20wait
        mov     al,0xAE
        out     0x64,al
 
        call    a20wait
        sti
        ret
 
a20wait:
        in      al,0x64
        test    al,2
        jnz     a20wait
        ret
 
 
a20wait2:
        in      al,0x64
        test    al,1
        jz      a20wait2
        ret

