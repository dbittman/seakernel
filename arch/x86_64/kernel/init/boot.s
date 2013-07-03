section .boot
MBOOT_PAGE_ALIGN	equ 1<<0	; Load kernel and modules on a page boundary
MBOOT_MEM_INFO		equ 1<<1	; Provide kernel with memory info
MBOOT_O_INFO		equ 1<<2	
MBOOT_HEADER_MAGIC	equ 0x1BADB002	; Multiboot Magic value
MBOOT_HEADER_FLAGS	equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO | MBOOT_O_INFO
MBOOT_CHECKSUM		equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

[BITS 32]                       ; All instructions should be 32-bit.

[GLOBAL mboot]                  ; Make 'mboot' accessible from C.
[EXTERN code]                   ; Start of the '.text' section.
[EXTERN bss]                    ; Start of the .bss section.
[EXTERN end]                    ; End of the last loadable section.
align 4
mboot:
	dd  MBOOT_HEADER_MAGIC      ; GRUB will search for this value on each
                                ; 4-byte boundary in your kernel file
	dd  MBOOT_HEADER_FLAGS      ; How GRUB should load your file / settings
	dd  MBOOT_CHECKSUM          ; To ensure that the above values are correct

	dd  mboot                   ; Location of this descriptor
	dd  code                    ; Start of kernel '.text' (code) section.
	dd  bss                     ; End of kernel '.data' section.
	dd  end                     ; End of kernel.
	dd  start                   ; Kernel entry point (initial EIP).

section .text
[GLOBAL start]                  ; Kernel entry point.
[EXTERN kmain]                  ; This is the entry point of our C code
STACKSIZE equ 0x4000            ; that's 16k.

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

; GRUB starts us off in 32-bit protected mode. We need to get to
; 64-bit long mode, since that's what the C code kernel is compiled
; for. This isn't too tricky, just need to set up paging and enable
; some things.
; Here is where the 32-bit entry code is.
start:
	mov esp, stack+STACKSIZE-4  ; set up the stack
	cli
	mov [ebx_backup], ebx       ; the kernel expects this as an argument later, so save it

	; now, set up a basic PML4 paging setup, since 64-bit requires paging
	mov edi, 0x70000            ; Set the destination index to 0x1000.
	mov cr3, edi                ; Set control register 3 to the destination index.
	xor eax, eax                ; Nullify the A-register.
	mov ecx, 4096               ; Set the C-register to 4096.
	rep stosd                   ; Clear the memory.
	mov edi, cr3                ; Set the destination index to control register 3.

	mov DWORD [edi], (0x71003)  ; Set the double word at the destination index to 0x2003.
	add edi, 0x1000             ; Add 0x1000 to the destination index.
	mov DWORD [edi], (0x72003)  ; Set the double word at the destination index to 0x3003.
	add edi, 0x1000             ; Add 0x1000 to the destination index.
	mov DWORD [edi], (0x73003)  ; Set the double word at the destination index to 0x4003.
	add edi, 0x1000   


	mov ebx, 0x00000003         ; Set the B-register to 0x00000003.
	mov ecx, 512                ; Set the C-register to 512.
.SetEntry:
	mov DWORD [edi], ebx        ; Set the double word at the destination index to the B-register.
	add ebx, 0x1000             ; Add 0x1000 to the B-register.
	add edi, 8                  ; Add eight to the destination index.
	loop .SetEntry 
	
	; enable PAE
	mov eax, cr4
	bts eax, 5
	mov cr4, eax
	
	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	bts eax, 8
	bts eax, 0
	wrmsr

	; enable paging
	mov eax, cr0
	bts eax, 31
	mov cr0, eax

	; lead the GDT with 64-bit segments
	lgdt [gdtdesc]
	; long jump into 64-bit mode
	jmp 0x08:(start64)

[BITS 64]
start64:
	; set the segments...
	mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    ; reset the stack
	mov rsp, stack+STACKSIZE-8
	cli
	; restore ebx
	mov ebx, [ebx_backup]
	; function call!
	push rsp
	push rbx
	call  kmain                  ; call kernel proper
	
    cli
    lgdt [0]
    lidt [0]
    sti
    int 3

section .bss
align 32
stack:
   resb STACKSIZE                ; reserve 16k stack on a quadword boundary

ebx_backup:
	resd 1
