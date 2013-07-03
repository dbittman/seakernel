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
[GLOBAL _start]                 ; Kernel entry point.
[EXTERN kmain]                  ; This is the entry point of our C code
STACKSIZE equ 0x4000            ; that's 16k.
   
   
   
   

align 8

 ;/* this is the GDT descriptor */
gdtdesc:
	dw	0x37			;/* limit */
 	dd	gdtable			;/* addr */
	dd	0			;/* in case we go to 64-bit mode */
	
align 8
gdtable:
dw	0, 0				;/* 0x0000 */
db	0, 0, 0, 0

;/* code segment */			;/* 0x0008 */
dw	0xFFFF, 0
db	0, 0x9A, 0xCF, 0

;/* data segment */			;/* 0x0010 */
dw	0xFFFF, 0
db	0, 0x92, 0xCF, 0

;/* 16 bit real mode CS */		;/* 0x0018 */
dw	0xFFFF, 0
db	0, 0x9E, 0, 0

;/* 16 bit real mode DS/SS */		;/* 0x0020 */
dw	0xFFFF, 0
db	0, 0x92, 0, 0

;/* 64 bit long mode CS */		;/* 0x0028 */
dw	0xFFFF, 0
db	0, 0x9A, 0xAF, 0

;/* 64-bit long mode SS */		;/* 0x0030 */
dw	0xFFFF, 0
db	0, 0x92, 0xAF, 0
 


start:
   mov esp, stack+STACKSIZE-4     ; set up the stack
   cli
   

   
   
   
   
   
    mov edi, 0x70000    ; Set the destination index to 0x1000.
    mov cr3, edi       ; Set control register 3 to the destination index.
    xor eax, eax       ; Nullify the A-register.
    mov ecx, 4096      ; Set the C-register to 4096.
    rep stosd          ; Clear the memory.
    mov edi, cr3       ; Set the destination index to control register 3.
    
    mov DWORD [edi], (0x71003)      ; Set the double word at the destination index to 0x2003.
    add edi, 0x1000              ; Add 0x1000 to the destination index.
    mov DWORD [edi], (0x72003)       ; Set the double word at the destination index to 0x3003.
    add edi, 0x1000              ; Add 0x1000 to the destination index.
    mov DWORD [edi], (0x73003)       ; Set the double word at the destination index to 0x4003.
    add edi, 0x1000   
    
    
    mov ebx, 0x00000003          ; Set the B-register to 0x00000003.
    mov ecx, 512                 ; Set the C-register to 512.
.SetEntry:
    mov DWORD [edi], ebx         ; Set the double word at the destination index to the B-register.
    add ebx, 0x1000              ; Add 0x1000 to the B-register.
    add edi, 8                   ; Add eight to the destination index.
    loop .SetEntry 
    
   
    mov eax, cr4
	bts eax, 5
	mov cr4, eax
  
   
   
    mov ecx, 0xC0000080
	rdmsr
	bts eax, 8
	bts eax, 0
	wrmsr
   
   
   
    mov eax, cr0
	bts eax, 31
	mov cr0, eax
	
	
	
	
	
	lgdt [gdtdesc]

    jmp 0x28:(start64)
  
hang:
   hlt                          ; halt machine should kernel return
   jmp   hang                   ; course, this wont happen

 
   ; Use 64-bit.
[BITS 64]
 
start64:
    mov ax, 0x18            ; Set the A-register to the data descriptor.
    mov ds, ax                    ; Set the data segment to the A-register.
    mov es, ax                    ; Set the extra segment to the A-register.
    mov fs, ax                    ; Set the F-segment to the A-register.
    mov gs, ax                    ; Set the G-segment to the A-register.
    mov edi, 0xB8000              ; Set the destination index to 0xB8000.
    mov rax, 0x1F201F201F201F20   ; Set the A-register to 0x1F201F201F201F20.
    mov ecx, 500                  ; Set the C-register to 500.
    rep movsq                     ; Clear the screen.
    hlt                           ; Halt the processor.

section .bss
align 32
stack:
   resb STACKSIZE               ; reserve 16k stack on a quadword boundary
