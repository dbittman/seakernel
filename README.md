SeaOS - seakernel
=================

NOTE - A current large refactoring of this kernel is happening in a separate repo and will be merged into this one
as soon as it is stable!

A self-hosting *simple* UNIX compatible operating system written from scratch and easy to modify. It is a modular hybrid kernel designed to provide simplicity without giving up performance.

Current Stats
-------------
![Current Codebase Statistics](http://googoo-16.ssrc.ucsc.edu/stat.png)

Features
--------
* Multithreading
* SMP
* x86\_64 (Ports to ARM coming soon!)
* IPv4
* UDP
* AHCI drives (SATA)
* ATA drives
* Ext2 Filesystem
* Self hosting
* Many ported programs (bash, gcc, etc)

Building
--------
In order to build the system, you'll need the SeaOS toolchain builder and userspace builder, 
which can be found at ![Sea repo](http://github.com/dbittman/sea).

Special Thanks
--------------
* SSRC at UC Santa Cruz (http://www.ssrc.ucsc.edu/)
* \#osdev on Freenode
* forums.osdev.org, and wiki.osdev.org
