#ifndef MUTEX_H
#define MUTEX_H

typedef volatile struct mutex_s {
	volatile unsigned count;
	volatile unsigned magic;
	volatile int pid;
	volatile struct mutex_s *next, *prev;
	volatile unsigned line;
	volatile unsigned owner;
	volatile unsigned char flags;
	volatile char file[64];
} mutex_t;

#define MF_ALLOC 1
#define MF_REAL 2

#define MUTEX_MAGIC 0x12345678
mutex_t *create_mutex(mutex_t *existing);
void __mutex_on(mutex_t *m, char *, int);
void __mutex_off(mutex_t *m, char *, int);
void __destroy_mutex(mutex_t *m, char *, int);
void unlock_all_mutexes();
void reset_mutex(mutex_t *m);
#define mutex_off(m) (current_task ? __mutex_off(m, __FILE__, __LINE__) : task_uncritical())

#define mutex_on(m) (current_task ? __mutex_on(m, __FILE__, __LINE__) : task_critical())

#define destroy_mutex(m) (__destroy_mutex(m, __FILE__, __LINE__))
#define MUTEX_COUNT (~(unsigned)0)

#endif
