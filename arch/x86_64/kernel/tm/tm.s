[GLOBAL arch_tm_read_ip]
arch_tm_read_ip:
  pop rax
  jmp rax

[GLOBAL arch_tm_do_switch]
arch_tm_do_switch:
  pushfq
  push rbp
  push rbx
  push r12
  push r13
  push r14
  push r15
  mov [rdi], rsp
  mov rsp, [rsi]
  test rdx, rdx
  je .normal
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

