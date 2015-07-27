KOBJS+= kernel/config.o \
		kernel/console.o \
		kernel/debugger.o \
		kernel/kernel.o \
		kernel/mutex.o \
		kernel/panic.o \
		kernel/rwlock.o \
		kernel/syscall.o \
		kernel/tqueue.o \
		kernel/trace.o \
		kernel/vsprintf.o

include kernel/cpu/make.inc
include kernel/dm/make.inc
include kernel/drivers/make.inc
include kernel/fs/make.inc
include kernel/init/make.inc
include kernel/loader/make.inc
include kernel/machine/make.inc
include kernel/mm/make.inc
include kernel/net/make.inc
include kernel/tm/make.inc
