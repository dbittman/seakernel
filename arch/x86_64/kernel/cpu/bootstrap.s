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
  xchg bx, bx
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
[section .trampoline]

pmode_enter:
  mov ax, 0x10
  mov ds, ax
  mov es, ax
  mov fs, ax
  mov ss, ax
  mov eax, cr0
  and eax, 0x1FFFFFFF
  or eax, (1 << 16)
  mov cr0, eax
  ; set the stack to be right below the GDT data
  ; this will get changed almost immediately
  mov esp, (0x7100-4)
  mov eax, DWORD cpu_start32
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

align 8
; descriptor for the temporary 64-bit GDT
gdtdesc:
	dw	0x17
 	dd	gdtable
	dd	0
align 8
gdtable:
	dw	0, 0                    ; null segment
	db	0, 0, 0, 0

	dw	0xFFFF, 0               ; 64-bit kernel code segment
	db	0, 0x9A, 0xAF, 0

	dw	0xFFFF, 0               ; 64-bit kernel data segment
	db	0, 0x92, 0xAF, 0

extern boot_pdpt
align 0x1000
boot_ap_pml4:
	dq boot_pdpt
	times 255 dq 0
	dq boot_pdpt
	times 255 dq 0

cpu_start32:
	mov eax, cr4
	bts eax, 5
	mov cr4, eax
	
	mov ebx, boot_ap_pml4
	or dword [ebx], 7
	or dword [ebx + 256*8], 7
	
	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	bts eax, 8
	wrmsr

	; enable paging
	mov cr3, ebx
	mov eax, cr0
	bts eax, 31
	mov cr0, eax
	; load the GDT with 64-bit segments
	lgdt [gdtdesc]
	; long jump into 64-bit mode
	jmp 0x08:(cpu_start64_low)

[BITS 64]
cpu_start64_low:
	mov rcx, cpu_start64
	jmp rcx

section .text
extern boot_pml4
cpu_start64:
	; set the segments...
	mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
	mov rax, boot_pml4
	mov cr3, rax
	mov rsp, cpu_tmp_stack + 0x1000
	mov rcx, cpu_entry

	; write-protect for COW
	mov rax, cr0
	;or eax, (1 << 16)
	mov cr0, rax
    call rcx

section .bss
align 8
cpu_tmp_stack:
	resb 0x1000
