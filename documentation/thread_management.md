Thread Management
=================
The thread management (tm/) system is primarily responsible for handling
threads and processes, scheduling them on each CPU and across CPUs, and handling
asynchronous events.

Kernel Objects
--------------
The thread management system defines a number of kernel objects:
* async_call - A call to a function with a given argument that can be triggered.
* workqueue - Keeps a number of async_calls in a heap. Any thread may choose to execute the next task in it.

