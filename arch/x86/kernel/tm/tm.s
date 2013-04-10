[GLOBAL read_eip]
read_eip:
  pop eax
  jmp eax 

[GLOBAL do_switch_to_user_mode]
do_switch_to_user_mode:
	mov   ax, 0x23        ; Load the new data segment descriptor with an RPL of 3.
	mov   ds, ax          ; Propagate the change to all segment registers.
	mov   es, ax
	mov   fs, ax
	mov   eax, esp        ; Save the stack pointer before pushing anything
	push  0x23            ; Push the new stack segment with an RPL of 3.
	push  eax             ; Push what the was ESP before pushing anything.
	pushfd                ; Push EFLAGS
	pop eax
	or eax, 0x200         ; Enable interrupts
	push eax
	push 0x1B             ; Push the new code segment with an RPL of 3.
	push .usermode_return ; Push the EIP to IRET to.
	iret

.usermode_return:
	ret
