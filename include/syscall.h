#ifndef SYSCALL_H
#define SYSCALL_H
#include <kernel.h>
#include <isr.h>
#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <syscall-x86.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <syscall-x86_64.h>
#endif

int syscall_handler(registers_t *regs);
extern void *syscall_table[];

int sys_null();
int sys_setup();

#define SYS_SETUP 		  0
#define SYS_EXIT  		  1
#define SYS_FORK  		  2
#define SYS_WAIT 		  3
#define SYS_READ  		  4
#define SYS_WRITE 		  5 
#define SYS_OPEN  		  6
#define SYS_CLOSE 		  7
#define SYS_FSTAT 		  8
#define SYS_STAT  		  9
#define SYS_ISATTY		 10
#define SYS_SEEK  		 11
#define SYS_SIGNAL		 12
#define SYS_SBRK  		 13
#define SYS_TIMES 		 14
#define SYS_DUP   		 15
#define SYS_DUP2  		 16
#define SYS_IOCTL 		 17

#define SYS_UNUSED0		 18
#define SYS_UNUSED23	 19
#define SYS_UNUSED24 	 20

#define SYS_UNUSED2 	 25

#define SYS_EXECVE		 26
#define SYS_LMOD  		 27
#define SYS_ULMOD 		 28
#define SYS_CANUNLOAD	 29
#define SYS_ULALLMODS	 30
#define SYS_GETPID 		 31
#define SYS_GETPPID		 32

#define SYS_LINK  		 33
#define SYS_UNLINK		 34
#define SYS_GETREFCOUNT	 35
#define SYS_GETPWD		 36
#define SYS_GETPATH		 37

#define SYS_UNUSED22	 38

#define SYS_CHROOT		 39
#define SYS_CHDIR		 40
#define SYS_MOUNT		 41
#define SYS_UMOUNT		 42
#define SYS_READDIR		 43
#define SYS_UNUSED25		 44
#define SYS_CREATE_CONSOLE	 45
#define SYS_SWITCH_CONSOLE	 46

#define SYS_UNUSED19    	 47
#define SYS_UNUSED20	 48
#define SYS_UNUSED10		 49

#define SYS_MMAP		 50
#define SYS_MUNMAP		 51
#define SYS_SYNC		 52
#define SYS_RMDIR		 53
#define SYS_FSYNC		 54
#define SYS_ALARM		 55
#define SYS_SELECT		 56
#define SYS_GETDENTS		 57

#define SYS_UNUSED3		 58

#define SYS_SYSCONF		 59
#define SYS_SETSID		 60
#define SYS_SETPGID		 61
#define SYS_SWAPON  		 62
#define SYS_SWAPOFF 		 63
#define SYS_NICE    		 64

#define SYS_UNUSED4		 65
#define SYS_UNUSED5		 66
#define SYS_UNUSED6		 67

#define SYS_TSTAT   		 68

#define SYS_UNUSED7 		 69
#define SYS_UNUSED8 		 70

#define SYS_DELAY   		 71
#define SYS_KRESET  		 72
#define SYS_KPOWOFF 		 73
#define SYS_GETUID  		 74
#define SYS_GETGID  		 75
#define SYS_SETUID  		 76
#define SYS_SETGID  		 77
#define SYS_MEMSTAT 		 78
#define SYS_TPSTAT  		 79
#define SYS_MOUNT2  		 80

#define SYS_UNUSED9		 81
#define SYS_UNUSED21		 82

#define SYS_PIPE  		 83
#define SYS_SETSIG		 84

#define SYS_UNUSED11 		 85
#define SYS_UNUSED12		 86
#define SYS_UNUSED13  		 87
#define SYS_UNUSED14  		 88

#define SYS_GETTIME 		 89
#define SYS_TIMERTH 		 90
#define SYS_ISSTATE 		 91
#define SYS_WAIT3 		 92

#define SYS_UNUSED15 		 93
#define SYS_UNUSED16 		 94

#define SYS_GETCWDLEN    		 95
#define SYS_SWAPTASK 		 96

#define SYS_DIRSTAT	    	 97
#define SYS_SIGACT 		 98
#define SYS_ACCESS 		 99
#define SYS_CHMOD 		100
#define SYS_FCNTL 		101
#define SYS_DIRSTATFD 		102
#define SYS_GETDEPTH 		103
#define SYS_WAITPID  		104
#define SYS_MKNOD    		105
#define SYS_SYMLINK  		106
#define SYS_READLINK 		107
#define SYS_UMASK    		108
#define SYS_SIGPROCMASK 	109
#define SYS_FTRUNCATE		110
#define SYS_GETNODESTR		111
#define SYS_CHOWN		112
#define SYS_UTIME		113
#define SYS_GETHOSTNAME		114
#define SYS_GSPRI		115
#define SYS_UNAME		116
#define SYS_GETHOST     117
#define SYS_GETSERV     118
#define SYS_SETSERV		119
#define SYS_SYSLOG		120
#define SYS_POSFSSTAT   121

#define SYS_WAITAGAIN   	127

/* These are special */
#define SYS_RET_FROM_SIG 128

#endif
