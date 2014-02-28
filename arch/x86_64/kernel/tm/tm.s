[GLOBAL read_eip]
read_eip:
  pop rax ; this is so cheating.
  jmp rax ; this really shouldn't work. But damn, it's cool.

[GLOBAL arch_do_switch_to_user_mode]
arch_do_switch_to_user_mode:
	mov   ax, 0x23        ; Load the new data segment descriptor with an RPL of 3.
	mov   ds, ax          ; Propagate the change to all segment registers.
	mov   es, ax
	mov   fs, ax
	mov   rax, rsp        ; Save the stack pointer before pushing anything
	push  0x23            ; Push the new stack segment with an RPL of 3.
	push  rax             ; Push what the was ESP before pushing anything.
	pushfq                ; Push EFLAGS
	pop rax
	or rax, 0x200         ; Enable interrupts
	push rax
	push 0x1B             ; Push the new code segment with an RPL of 3.
	push .usermode_return ; Push the EIP to IRET to.
	iretq

.usermode_return:
	ret
