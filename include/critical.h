#ifndef CRITICAL_H
#define CRITICAL_H
#include <types.h>
typedef volatile u32int Spinlock;

void acquire_spinlock(Spinlock *s);
void release_spinlock(Spinlock *s);

void enter_critical(Spinlock *s);
void exit_critical(Spinlock *s);
void _enter_critical(Spinlock *s);
void _exit_critical(Spinlock *s);


#endif
