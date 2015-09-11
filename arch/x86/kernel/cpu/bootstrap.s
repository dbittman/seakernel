[GLOBAL rm_gdt]
[GLOBAL rm_gdt_end]
[GLOBAL rm_gdt_pointer]
[EXTERN cpu_entry]
[GLOBAL trampoline_start]
[GLOBAL trampoline_end]
[BITS 64]
trampoline_start:
trampoline_end:

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
