Coding Style Guidelines
=======================
For Fun and Profit!

The actual C code formatting I'm not super picky about. The Linux Kernel
Coding Guidelines document is pretty reasonable in many ways. If you prefer
to put opening braces on the next line of an if statement, go for it, I don't
really care.

Some points:
  * I do prefer /\* \*/ style comments to // style comments.
  * Indent with tabs, align with spaces.
  * Camel case is evil.

A subsystem is a group of code and files that does one specific thing. For
example, the memory manager is a subsystem along with the virtual file system,
the process system, the device manager, etc. Typically these files are grouped
together in a subdirectory named for the subsystem. If code is obviously part
of a subsystem, it should go in an existing file for that subsystem or in a
file in the appropriate directory.

Directory Layout
----------------
The top level directories that contain code are:
  * arch
  * include
  * kernel
  * library

arch contains architecture dependent code *only*, and kernel contains
contains architecture independent code *only*. This makes code *much*
more readable and easier to port. More on this later.

include contains include files. Under include is a directory 'sea'.
This directory contains include files that may be used in kernel code. These
files may not define achitecture dependent things directly, but must instead
include a file that exists under arch/{arch}/include if it needs to use or
define such a thing.

kernel contains kernel source code. Inside this directory are more directories
named for subsystems (mm is memory management, etc).

library contains code that is more library code than kernel code (though it
gets linked into the kernel in the end). Implementations of libc functions go
in here.

Architecture Dependent Code
---------------------------
There is a *firm* seperation between architecture dependent code and
architecture independent code. Code must:
  * Never define architecture dependent code outside of arch/{arch}.
  * Never define architecture independent code insode of arch/{arch}.
  * Never call architecture dependent code from another subsystem. Instead,
    call an architecture independent wrapper function. For example, if you
	need to map a page inside the virtual file system code, do NOT call
	arch_mm_vm_map, instead call mm_vm_map. Basically, architecture dependent
	code calls must stay within the same subsystem. The process manager must
	talk to the architecture independent side of the memory manager. This helps
	solidify the kernel APIs and allows easier porting.
  * Minimize archtecture dependent code. If it can be made portable, make it
    portable.

Under arch/{arch}, there is a near-mirror layout to the top level directory.
Each arch contains at least the directories kernel, include, and library.
Inside kernel and include are mirrors of the top-level versions. The same
structure rules apply in here.

Function Names
--------------
  * Functions that are designed to be called by outside of the same file
    should be named as {short-subsystem-string}_{function-name}. For example,
	the function to resolve a path is named fs_path_resolve: fs is the
	subsystem's short name, and path_resolve is a descriptive name.
  * Functions should descripbe what they're doing well. They should be named
    for the thing that they are acting on and what they're doing. I prefer to
	name them as {noun}_{verb}, so a function to create a process will be named
	process_create. This way all functions that are acting on a certain thing
	start the same way, and makes it easier to group them together and organize
	whats happening in my mind. If I need a more complicated grammar to parse
	your function name, it's probably a bad name.
  * The exception to the above is for functions that implement libc functions,
    functions that are very close to them, and system calls. System calls
	should be named as sys_{name}. memset is a libc function, and so is named
	as such. kprintf obviously wants to be printf, so it isn't restricted.
  * If your function doesn't need to be called from elsewhere, declare it
    static.
  * Architecture dependent functions should start with arch\_. Thus, keeping the
    earlier naming convention, an architecture dependent function would be
	something like arch_cpu_set_interrupts.
  * Entry points into the kernel (such as for system calls, interrupts, etc)
    should end with \_entry.

Drivers
-------
The organization of drivers/ is not yet finalized.

Global Variables
----------------
Again, must be descriptive. Try not to use them.

Restrictions on Classic Coding Techniques
-----------------------------------------
Recursion is EXTREMELY frowned upon. The kernel has a limited size stack, and
we don't want to overrun it because, well, we're the *kernel*.

make.inc
--------
When adding files to the kernel, and adding entries to make.inc files, keep
them in alphabetical order.

