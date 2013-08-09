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


cpu_start32:
	mov eax, cr4
	bts eax, 5
	mov cr4, eax
	; now, set up a basic PML4 paging setup, since 64-bit requires paging
	mov edi, 0x70000            ; Set the destination index to 0x7000.
	mov cr3, edi                ; Set control register 3 to the destination index.
	xor eax, eax                ; Nullify the A-register.
	mov ecx, 4096               ; Set the C-register to 4096.
	rep stosd                   ; Clear the memory.
	mov edi, cr3                ; Set the destination index to control register 3.

	mov DWORD [edi], (0x71003)  ; Set the qword at the destination index to 0x71003.
	mov DWORD [edi+4], (0) 
	
	add edi, 0x1000             ; Add 0x1000 to the destination index.
	mov DWORD [edi], (0x72003)  ; Set the qword at the destination index to 0x72003.
	mov DWORD [edi+4], (0)

    add edi, 0x1000
	mov ebx, (0x00000003 | (1 << 7))
	mov ecx, (512)
.set_entry:
	mov DWORD [edi], ebx        ; Set the qword at the destination index to the B-register.
	mov DWORD [edi+4], 0
	add ebx, 0x200000           ; Add 0x1000 to the B-register.
	add edi, 8                  ; Add eight to the destination index.
	loop .set_entry 
	
	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	bts eax, 8
	wrmsr

	; enable paging
	mov eax, cr0
	bts eax, 31
	mov cr0, eax
	mov edi, 0x70000            ; Set the destination index to 0x7000.
	mov cr3, edi                ; Set control register 3 to the destination index.
	; lead the GDT with 64-bit segments
	lgdt [gdtdesc]
	; long jump into 64-bit mode
	jmp 0x08:(cpu_start64)

[BITS 64]
cpu_start64:
	; set the segments...
	mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax

    call cpu_entry
