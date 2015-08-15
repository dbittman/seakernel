Thread Management
=================
The thread management (tm/) system is primarily responsible for handling
threads and processes, scheduling them on each CPU and across CPUs, and handling
asynchronous events.

Kernel Objects
--------------
The thread management system defines a number of kernel objects:
* async_call - A call to a function with a given argument that can be triggered.
* workqueue - Keeps a number of async_calls in a heap, organized by priority. Any thread may choose to execute the next task in it.
* ticker - Keeps a number of async_calls in a heap, organized by timeout. During timer interrupts, the next task will be run if the correct amount of time has passed.
* thread - Contains context for a thread of execution.
* process - A container of at least one thread, has a MM context, and has an optional executable image.

struct async_call
-----------------
An async_call contains a function pointer to a function that takes an unsigned long
argument. The async_call also contains the argument, the priority, and a flags field.
Upon calling async_call_execute() (see async_call.h), the function gets called with
the argument stored in the async_call structure.

The async_call kernel object has an addition restriction, in that they cannot be
allocated using kmalloc. Any async_call structure must either be static, or allocated
as part of another data structure. This is because the may be executed in an
interrupt context (see below).

Workqueue
---------
A workqueue is an abstraction to a priority heap of async_calls. Calling the insert
function adds an async_call struct to the heap, using the priority as the key. By
calling workqueue_dowork on a workqueue, a thread will pop the top of the heap off
and execute it if it exists. Any thread may do this.

Each CPU has its own workqueue on which interrupts enqueue their second-stage work
if they have any. Note that this is not workqueue from the CPU which fields the
interrupt, but rather the workqueue from the CPU that the fielding thread is attached
to. This is an attempt to spread out second-stage interrupt handling across all CPUs.
Upon scheduling to any thread, the workqueue_doqueue function is called once for the
CPU workqueue. The idle threads (see below) also execute workqueue_dowork as their
main function.

Additionally, each thread has its own workqueue. When the thread is scheduled to, every
task in the workqueue is executed before the schedule finishes.

Ticker
------
Each CPU has its own ticker structure, which has a monotomically increasing value
that gets incremented on the CPU's timing interrupt firing. The ticker has a heap
of async_calls organized by a timeout. When adding an async_call, the insert function
is also passed a timeout value (see timing, below). When this amount of time has passed,
the call is executed.

Thread
------
A thread contains context of execution. Each has its own kernel stack and userspace
stack, and each has its own architecture-dependent state structure that is used by the
architecture context-switching code. Threads are created inside tm_clone.

Threads are refcounted, and when the reference reaches 0, the kernel stack is freed
along with the data structure. Because the thread cannot free its own kernel stack (as
it would be using it at the time!), another thread must do it. Because of this, when
a thread exits, it enqueues a task in the CPU's workqueue that does the freeing.

An existing thread can be gotten with tm_thread_get(tid), which finds a thread by thread
identification number (tid), increases the refcount if it exists, and returns it.
When dropping a reference to the thread, tm_thread_put(thread) will decrement the
refcount and free it if it reaches 0.

A thread must have a process associated with it.

Process
-------
A process is a container for multiple threads. It stores a linked list of threads.
A process is optionally created by tm_clone, depending on the flags passed to tm_clone.

It also contains a virtual memory management context (vmm_context), which defines a
pages tables configuration. Each child thread ALWAYS executes within their processes
page tables.

When exiting, the thread data structure is forgotten completely once it exits (and the
cleanup function is scheduled). A process is kept around for a time. The last exiting
thread sets the exit status information for the process inside the process structure.
When all of a process's threads have existed, the process is kept around but never
scheduled (since threads are scheduled, not processes). The process is cleaned up when
another program calls waitpid on the process.

Scheduling
----------
Each CPU has its own queue of threads that are "probably" runnable. That is, they
are in the active queue. When scheduling, the scheduler looks at a thread, and sees
if the state is THREADSTATE_RUNNABLE. If it is, the thread is run. If not, and if
state is THREADSTATE_INTERRUPTIBLE, the thread might actually still be runnable. It
checks the signals, and if it has any, the thread is resumed. Threads are kept in
the runqueue but set to non-runnable if the thread is likely to be resumed soon, 
and/or if it doesn't make sense to place the thread in a blocklist. This removes
the overhead of the blocklist (see Blocking, below).

The scheduler runs on a per-cpu basis.

Timing
------
Timing is done by storing an integer in a time_t. Timing is resolved in microseconds,
though in practice, only goes as specific as the CPU timing interrupts (typically one
millisecond). But it can totally pretend that it knows about microseconds!

Two macros are defined in sea/tm/timing.h, ONE_SECOND and ONC_MILLISECOND. These
are useful when talking about timing, so tm_thread_delay(ONE_SECOND * 2) will sleep
for 2 seconds.

Critical Sections
-----------------
Sometimes it matters that a thread not be scheduled away for a section of code. For
this, two functions have been defined: cpu_disable_preemption, and cpu_enable_preemption.
Disabling preemption makes it so that the scheduler will not change processes for
this CPU. Other CPUs and their threads are not affected.

Blocking
--------
Threads don't want to run all the time. Typically, they stall on IO, or call waitpid,
or delay, or whatever.

Blocklists are used in situations where a thread might block for quite some time. They
are used in situations like IO (waiting for a block to be read, waiting for a packet
to send), or waitpid (waiting for a process to exit).

Blocklists are actually linked lists that store threads. The function
tm_thread_add_to_blocklist can be used to move a thread from an active queue in a CPU
to a given blocklist. However, this does not actually stop a thread from running, but
the next time it schedules away, it won't schedule back until the thread has been
unblocked. tm_thread_remove_from_blocklist does the opposite: removes a thread from
a blocklist and adds it to a runqueue.

A more helpful function is tm_thread_block, which adds a thread to a blocklist, sets
the thread's state to something other than RUNNING, and schedules away. It does this
safely, by disabling preemption before doing the work. If the state gets set to
something other than THREADSTATE_UNINTERRUPTIBLE, the function returns 0 if the thread
is unblocked normally, -ERESTART if the thread was signaled and the signal specifies
that the syscall be restarted (see Signals, below), or -EINTR if it was signaled and
the signal does not specify that the syscall be restarted.

The function tm_thread_unblock unblocks a given thread (and moves it from the blocklist
to the active queue). tm_blocklist_wakeall does this for every thread in a blocklist.
A common usage patterns for these functions is a thread calls tm_thread_block to add
itself to a blocklist waiting for a resource. Then a worker thread, interrupt handler,
or whatever wakes up the thread when the resource is ready.

Sometimes, the thread wants to wait with a timeout. tm_thread_block_timeout does the
same as tm_thread_block, but will return -ETIME if the timeout expired before it
was woken up for any reason (including recieving a signal).

Other times, a thread might want to schedule some work as it is in the process of
blocking. tm_thread_block_schedule_work is used in this case. It takes an async_call
which gets enqueued in a workqueue as the function is setting up the blocking.
Otherwise, this function works the same as tm_thread_block.

Finally, a thread might want to sleep for an amount of time. tm_thread_delay sleeps
a specified number of microseconds. It returns 0 if it timeouts, or -ERESTART or -EINTR
if it was signaled. Additionally, tm_thread_delay_sleep sleeps for a specified number
of microseconds, ignores signals, and tt DOES NOT block. This is good for a thread
that might need to sleep for a really small amount of time, and doesn't want to incur
the overhead of a context switch along with blocklists and such.

Signals
-------

Signals may be delivered to a thread or a process. If delivered to a thread, that
thread specifically will field it if its signal mask doesn't mask it. If delivered to
a process, the signal will be handled by the first thread in the thread list that
doesn't mask the signal. It will be handled by only one signal.

Signals sent to a thread are enqueued in a bitset within the thread structure. Upon
scheduling, when the scheduler considers a thread, it searches the bitset for a signal.
If it finds one, it resets that bit, and sets the threads signal field to the number
of that signal. Note that because the signal queue is a bitset, multiple signals of the
same number may not be handled multiple times. While the threads signal field is set,
the above checking for signals in the bitset is skipped during scheduling (in order to
prevent the signal field from being overwritten).

When a thread gets considered by the scheduler, if its signal field is set, it calls
the tm_thread_handle_signal function. This function checks to see if the signal is being
handled by userspace or ignored. If ignored, it resets the signal field to 0, allowing
the next signal in the bitset to be considered. If it's handled by userspace, the
function sets a flag that tells the thread that when it returns from the interrupt
it's currently in (see Interrupts, below), to handle setting up the userspace signal
handler. If neither of these conditions are true, the thread does the default action
for a signal. Most of the time, this involves calling tm_thread_exit to die. Some
signals, like SIGCHLD, do not kill the process by default. If the signal is handled
as the default action, the signal flag in the process is reset to zero.

Both threads and processes have signal masks, however each works differently. A process
signal mask prevents the delivery of a signal but keeps it in the signal queue bitset.
That is, a thread is still given the signal, but won't handle it until the process's
signal mask allows it. It will stay in the bitset until then. Thread signal masks,
on the other hand, completely prevent a signal from being delivered (and enqueued) to
a thread.

A function exists to test if a thread has been delivered a signal: tm_thread_got_signal.
This function checks if the thread has gotten a signal and if it will be handled. It
returns -ERESTART if the signal has been specified to restart a syscall, 0 if there is
not waiting signal, or the signal number otherwise. When a syscall blocks, it is good
practice to check for signals when convenient, and return appropriately.

Interrupts
----------
There are three ways for a thread to enter the kernel: Voluntarily via a syscall, involuntarily via a hardware interrupt, and involuntarily via a software interrupt.
Additionally, the kernel itself may be interrupted by a hardware or software
interrupt.

Syscalls set a threads system field to the number of a syscall. This field being
set imposes some restrictions on kernel code. For example, a signal will not be handled
while this field is non-zero, thus preventing signals from being processed while inside
a system call. This is because a signal may cause the thread to exit or stop, and it
is dangerous to do this while executing a syscall except in very specific locations.
Syscalls that need to block, or otherwise take a lot of time to complete an operation
should check to see if the thread has been delivered a signal at times when it is safe
to do so, and potentially cleanup and return -EINTR or -ERESTART to indicate that the
syscall was interrupted (see Signals, above).

A syscall that returns -ERESTART will be restarted with the same arguments before
leaving the kernel. However, signals will be given the upportunity to be handled at
this point, and may end up killing the thread.

Hardware interrupts need to execute FAST. They must never block or do any operation
that may take some time. They execute with interrupts DISABLED. Blocking, scheduling,
allocating memory, and acquiring locks that are not mutexes with the MT_NOSCHED option
enabled are all disallowed in hardware interrupt context (the kernel will panic if
you try to do these things). If more work needs to be done, then a second stage interrupt
handler must be used. This can be done by setting up an async_call and then calling
cpu_interrupt_schedule_stage2. Note that the second stage interrupt handler may not
run the exact number of times that a hardware interrupt fires, though it will run
at least once. Note that once the hardware interrupt is acknowledged, then it is
totally safe to do normal kernel operations again.

Software interrupts operate in the same context of a syscall.

Both software interrupts and hardware interrupts may interrupt another interrupt or
syscall. In doing so, they check to see if they're interrupting kernel code or
userspace code. If they're interrupting userspace code, any interrupt or syscall
will set current_thread->regs to point to the stored thread context from before
the interrupt. This is used by signals and by exec. If they're interrupting kernel
code, then this assignment is skipped. Thus, the regs pointer always points to
context from the thread's actual program code.

At the end of an interrupt or syscall, several flags are checked. THREAD_SIGNALED
indicates that a thread was signaled and the signal needs to be handled by userspace.
If this is set, and the interrupt code is returning to userspace, then the stored
context in the regs field is modified to return to the signal handler.

The flag THREAD_EXIT indicates that the thread needs to exit before the end of this
interrupt of syscall. The kernel doesn't immediately exit a thread when that thread
calls tm_thread_exit, because it might be from handling an error or from a signal.
Instead, tm_thread_exit sets this flag so that the actual exit function may be called
from a known, safe location. The end of an interrupt handler or syscall is such a
location. If the flag is set, the thread is exited.

THREAD_SCHEDULE indicates that the thread needs to be rescheduled. This is used because
a lot of the time a thread might do something and needs to be rescheduled, but can't
yet. For example, during a critical section a thread may recieve a signal, but cannot
reschedule to dequeue it from the bitset. Or a thread may set up a block during a
critical section, and then return from an interrupt. Or any number of reasons. The main
one is when handling a hardware timer interrupt, the kernel may choose to reschedule a
thread due to its timeslice running out. Instead of rescheduling during the handler (
this is not safe, and is not allowed), it sets the flag and continues on its way. At
the end of the interrupt handler, the flag is checked, and if set, the tm_schedule
function is called to reschedule.

