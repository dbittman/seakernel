#ifndef ISR_H
#define ISR_H

#define IRQ0 32
#define IRQ1 33
#define IRQ2 34
#define IRQ3 35
#define IRQ4 36
#define IRQ5 37
#define IRQ6 38
#define IRQ7 39
#define IRQ8 40
#define IRQ9 41
#define IRQ10 42
#define IRQ11 43
#define IRQ12 44
#define IRQ13 45
#define IRQ14 46
#define IRQ15 47

#define IOINT_PIC  1
#define IOINT_APIC 2

typedef struct registers
{
  volatile   u32int ds;                  // Data segment selector
  volatile   u32int edi, esi, ebp, esp, ebx, edx, ecx, eax; // Pushed by pusha.
  volatile   u32int int_no, err_code;    // Interrupt number and error code (if applicable)
  volatile   u32int eip, cs, eflags, useresp, ss; // Pushed by the processor automatically.
} volatile registers_t;

typedef void (*isr_t)(registers_t);

typedef struct handlist_s
{
	isr_t handler;
	unsigned n;
	char block;
	struct handlist_s *next, *prev;
} handlist_t;

void register_interrupt_handler(u8int n, isr_t handler);
void unregister_interrupt_handler(u8int n, isr_t handler);
int irq_wait(int n);
void wait_isr(int no);
extern char interrupt_controller;
handlist_t *get_interrupt_handler(u8int n);
#endif
