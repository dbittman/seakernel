[GLOBAL rm_gdt]
[GLOBAL rm_gdt_end]
[GLOBAL pmode_enter]
[GLOBAL pmode_enter_end]
[GLOBAL rm_gdt_pointer]
[EXTERN cpu_entry]
[GLOBAL trampoline_start]
[GLOBAL trampoline_end]

[BITS 16]
[section .trampoline]
trampoline_start:
  cli
  xor ax, ax
  mov ds, ax
  mov si, 0x7100
  lgdt [ds:si]
  mov eax, cr0
  or al, 0x01
  mov cr0, eax
  ; Jump into protected-mode
  jmp 0x08:(0x4 + 0x18 + 0x7100)
trampoline_end:

[BITS 32]
[section .text]

pmode_enter:
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov ss, ax
  
  ; un-set the cache-disable bit
  mov eax, cr0
  and eax, 0x1FFFFFFF
  mov cr0, eax
  ; set the stack to be right below the GDT data
  ; this will get changed almost immediately
  mov esp, (0x7100-4)
  mov eax, DWORD cpu_entry
  jmp eax
pmode_enter_end:

rm_gdt:
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
rm_gdt_end:

rm_gdt_pointer:
  dw 0x18 - 1
  dd (0x7100 + 0x4)
