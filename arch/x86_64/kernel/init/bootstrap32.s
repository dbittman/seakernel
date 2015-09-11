MBOOT_PAGE_ALIGN	equ 1<<0	; Load kernel and modules on a page boundary
MBOOT_MEM_INFO		equ 1<<1	; Provide kernel with memory info
MBOOT_O_INFO		equ 1<<2	
MBOOT_HEADER_MAGIC	equ 0x1BADB002	; Multiboot Magic value
MBOOT_HEADER_FLAGS	equ MBOOT_PAGE_ALIGN | MBOOT_MEM_INFO
MBOOT_CHECKSUM		equ -(MBOOT_HEADER_MAGIC + MBOOT_HEADER_FLAGS)

bits 32
global bootstrap
extern start64
section .boot

align 4
mboot:
	dd  MBOOT_HEADER_MAGIC      ; GRUB will search for this value on each
                                ; 4-byte boundary in your kernel file
	dd  MBOOT_HEADER_FLAGS      ; How GRUB should load your file / settings
	dd  MBOOT_CHECKSUM          ; To ensure that the above values are correct

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

align 0x1000
global boot_pdpt
boot_pdpt:
	dq boot_pd
	times 511 dq 0

align 0x1000
global boot_pd
boot_pd:
	dq 0x85
	dq 0x200085
	dq 0x400085
	times 509 dq 0

align 0x1000
global boot_pml4
boot_pml4:
	dq boot_pdpt
	times 255 dq 0
	dq boot_pdpt
	times 255 dq 0

bootstrap:
	mov eax, cr0
	and eax, 0x1FFFFFFF
	mov cr0, eax
	cli
	
	mov eax, boot_pml4
	or dword [eax], 5
	or dword [eax + 256*8], 5
	
	mov eax, boot_pdpt
	or dword [eax], 5
	
	; enable PAE
	mov eax, cr4
	bts eax, 5
	mov cr4, eax
	
	; enable long mode
	mov ecx, 0xC0000080
	rdmsr
	bts eax, 8
	wrmsr

	; enable paging
	mov edi, boot_pml4
	mov cr3, edi
	mov eax, cr0
	bts eax, 31
	mov cr0, eax
	; lead the GDT with 64-bit segments
	lgdt [gdtdesc]
	; long jump into 64-bit mode
	jmp 0x08:(startlong)

bits 64
startlong:
	mov rcx, start64
	jmp rcx

