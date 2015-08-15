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

Blocking
--------

Signals
-------




