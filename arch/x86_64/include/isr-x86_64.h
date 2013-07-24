#ifndef ISR_x86_64_H
#define ISR_x86_64_H

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

/* lower 4 bits of IRQ is priority sub class
 * and we want these to actually be in the correct order.
 * So, skip the lower 4 bits.
 * THE ASM COUNTER PARTS OF THESE ARE DEFINED IN INT.S
 * !! If you edit these, make sure you also update the ones in int.s
 * !! or some interesting bugs may appear...
 */
#define IPI_SCHED    0x90
#define IPI_SHUTDOWN 0xA0
#define IPI_TLB_ACK  0xB0
#define IPI_TLB      0xC0
#define IPI_DEBUG    0xD0
#define IPI_PANIC    0xE0

#define EFLAGS_INT (1 << 9)

#define MAX_HANDLERS 256
#define MAX_INTERRUPTS 256

typedef struct registers
{
	volatile   u64int ds;
	volatile   u64int r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi;
	volatile   u64int int_no, err_code;
	volatile   u64int eip, cs, eflags, useresp, ss;
} volatile registers_t;

typedef void (*isr_t)(registers_t *);

int register_interrupt_handler(u8int n, isr_t stage1_handler, isr_t stage2_handler);
void unregister_interrupt_handler(u8int n, int id);

int irq_wait(int n);
void wait_isr(int no);
extern char interrupt_controller;
void lapic_eoi();
void reset_timer_state();
void handle_ipi_cpu_halt(volatile registers_t regs);
void handle_ipi_reschedule(volatile registers_t regs);
void handle_ipi_tlb(volatile registers_t regs);
void handle_ipi_tlb_ack(volatile registers_t regs);

#endif
