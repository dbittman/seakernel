[GLOBAL arch_tm_read_ip]
arch_tm_read_ip:
  pop rax
  jmp rax

; NOTE: For a description of how this code works, look at
; the 32-bit version first. They are essentially the same
; in design, except here we have some more registers to play
; with and so it makes the code slightly easier to write.
; The main difference is that here we store more registers,
; and don't need to store rsi and rdi.
[GLOBAL arch_tm_do_switch]
arch_tm_do_switch:
  pushfq
  push rbp
  push rbx
  push r12
  push r13
  push r14
  push r15
  
  test rcx, rcx
  je .savestack
  mov cr3, rcx

  .savestack:
  mov [rdi], rsp
  mov rsp, [rsi]
  test rdx, rdx
  je .normal
  sti
  jmp rdx
  .normal:
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  popf
  ret

[GLOBAL arch_tm_do_fork_setup]
arch_tm_do_fork_setup:
  mov rax, rsp
  add rax, rdx
  sub rax, 56
  mov r8, rbp
  add r8, rdx

  pushfq
  pop r9
  mov [rax + 0x30], r9
  mov [rax + 0x28], r8
  mov [rax + 0x20], rbx
  mov [rax + 0x18], r12
  mov [rax + 0x10], r13
  mov [rax + 0x8], r14
  mov [rax], r15
  
  mov [rdi], rax
  mov rax, .forked_entry
  mov [rsi], rax
  ret
  .forked_entry:
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  popfq
  ret

