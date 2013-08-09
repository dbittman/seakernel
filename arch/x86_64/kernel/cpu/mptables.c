#include <config.h>
#if CONFIG_SMP
#include <kernel.h>
#include <task.h>
#include <mutex.h>
#include <cpu.h>
#include <memory.h>
#include <atomic.h>
#include <imps-x86_64.h>
volatile int imps_release_cpus = 0;
addr_t imps_lapic_addr = 0;

#endif
