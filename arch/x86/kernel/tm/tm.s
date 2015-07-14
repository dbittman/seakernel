[GLOBAL arch_tm_read_ip]
arch_tm_read_ip:
  pop eax ; this is so cheating.
  jmp eax ; this really shouldn't work. But damn, it's cool.

[GLOBAL arch_tm_do_switch]
arch_tm_do_switch:
  mov eax, [esp + 4]
  mov ecx, [esp + 8]
  mov edx, [esp + 12]
  pushf
  push ebp
  push ebx
  push esi
  push edi
  mov [eax], esp
  mov esp, [ecx]
  test edx, edx
  je .normal
  jmp edx
  .normal:
  pop edi
  pop esi
  pop ebx
  pop ebp
  popf
  ret

[GLOBAL arch_tm_do_fork_setup]
arch_tm_do_fork_setup:
  mov eax, esp
  add eax, [esp + 12] ; offset
  sub eax, 20

  pushf
  pop edx
  mov [eax + 16], edx

  mov edx, ebp
  add edx, [esp + 12]
  mov [eax + 12], edx
  mov [eax + 8], ebx
  mov [eax + 4], edi
  mov [eax], esi
  
  ; store stack pointer
  mov edx, [esp + 4]
  mov [edx], eax
  mov eax, .forked_entry
  mov edx, [esp + 8]
  mov [edx], eax ; store jump point
  ret
  .forked_entry:
  pop esi
  pop edi
  pop ebx
  pop ebp
  popf
  ret

