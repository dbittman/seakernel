[GLOBAL arch_tm_read_ip]
arch_tm_read_ip:
  pop eax
  jmp eax


; ALRIGHT. Let me break this shit DOWN.
; A thread switch happens. We need to store the current CPU state
; somewhere and load a previous state in. We use, of course, the
; stack for that. Push the callee-save registers (as the caller save
; registers don't matter since this is, you know, a function call).
; Also store EFLAGS. At that point, if this is a normal switch, 
; we switch stacks and load the state that was stored there when
; we scheduled away from that thread. Easy: pop everything and return.
;
; If this is NOT a normal switch (if we're switching to a thread that
; hasn't run yet (aka is has just been forked)), then there isn't
; a saved thread_switch state on the stack. Instead, we load the
; point to jump to (somewhere in arch_tm_do_fork_setup below),
; and switch stacks and then go there. That function will then load
; the correct state that was stored during the fork.

[GLOBAL arch_tm_do_switch]
arch_tm_do_switch:
  mov eax, [esp + 4]  ; first arg: old stack pointer's address
  mov ecx, [esp + 8]  ; second arg: new stack pointer's address
  mov edx, [esp + 12] ; third ard: jump location (set when thread hasn't run yet)

  ; store the old registers
  pushf
  push ebp
  push ebx
  push esi
  push edi
  mov [eax], esp ; save stack pointer
  mov esp, [ecx] ; load new stack pointer
  test edx, edx  ; if jump location is zero, jump to .normal
  je .normal
  jmp edx ; jump to the jump location
  .normal:
  pop edi ; restore the registers
  pop esi
  pop ebx
  pop ebp
  popf
  ret

; here we do a similar thing to a thread switch, except
; we don't actually switch. We also store the registers
; on the stack on the new thread.
; [esp + 12]: third arg, offset between stacks
; [esp + 8] : second arg, jump point pointer
; [esp + 4] : first arg, new stack pointer's address
[GLOBAL arch_tm_do_fork_setup]
arch_tm_do_fork_setup:
  mov eax, esp
  add eax, [esp + 12] ; determine what the stack pointer is for the new thread
  sub eax, 20 ; make room for our saved state

  ; 'push' EFLAGS
  pushf
  pop edx
  mov [eax + 16], edx

  ; calculate the offset for ebp too, and save it
  mov edx, ebp
  add edx, [esp + 12]
  mov [eax + 12], edx

  ; save the other registers
  mov [eax + 8], ebx
  mov [eax + 4], edi
  mov [eax], esi
  
  ; store new stack pointer
  mov edx, [esp + 4]
  mov [edx], eax
  
  ; load the jump point (.forked_entry)
  mov eax, .forked_entry
  mov edx, [esp + 8]
  mov [edx], eax ; store jump point
  ret
  ; this is where a new thread will begin executing. Restore
  ; the state from the stack and return.
  .forked_entry:
  pop esi
  pop edi
  pop ebx
  pop ebp
  popf
  ret

