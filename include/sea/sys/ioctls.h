#ifndef __ASM_GENERIC_IOCTLS_H
#define __ASM_GENERIC_IOCTLS_H

#include <sea/sys/ioctl.h>

/*
 * These are the most common definitions for tty ioctl numbers.
 * Most of them do not use the recommended _IOC(), but there is
 * probably some source code out there hardcoding the number,
 * so we might as well use them for all new platforms.
 *
 * The architectures that use different values here typically
 * try to be compatible with some Unix variants for the same
 * architecture.
 */

/* 0x54 is just a magic number to make these relatively unique ('T') */

#define TCGETS		0x5401
#define TCSETS		0x5402
#define TCSETSW		0x5403
#define TCSETSF		0x5404
#define TCGETA		0x5405
#define TCSETA		0x5406
#define TCSETAW		0x5407
#define TCSETAF		0x5408
#define TCSBRK		0x5409
#define TCXONC		0x540A
#define TCFLSH		0x540B
#define TIOCEXCL	0x540C
#define TIOCNXCL	0x540D
#define TIOCSCTTY	0x540E
#define TIOCGPGRP	0x540F
#define TIOCSPGRP	0x5410
#define TIOCOUTQ	0x5411
#define TIOCSTI		0x5412
#define TIOCGWINSZ	0x5413
#define TIOCSWINSZ	0x5414
#define TIOCMGET	0x5415
#define TIOCMBIS	0x5416
#define TIOCMBIC	0x5417
#define TIOCMSET	0x5418
#define TIOCGSOFTCAR	0x5419
#define TIOCSSOFTCAR	0x541A
#define FIONREAD	0x541B
#define TIOCINQ		FIONREAD
#define TIOCLINUX	0x541C
#define TIOCCONS	0x541D
#define TIOCGSERIAL	0x541E
#define TIOCSSERIAL	0x541F
#define TIOCPKT		0x5420
#define FIONBIO		0x5421
#define TIOCNOTTY	0x5422
#define TIOCSETD	0x5423
#define TIOCGETD	0x5424
#define TCSBRKP		0x5425	/* Needed for POSIX tcsendbreak() */
#define TIOCSBRK	0x5427  /* BSD compatibility */
#define TIOCCBRK	0x5428  /* BSD compatibility */
#define TIOCGSID	0x5429  /* Return the session ID of FD */
#define TCGETS2		_IOR('T', 0x2A, struct termios2)
#define TCSETS2		_IOW('T', 0x2B, struct termios2)
#define TCSETSW2	_IOW('T', 0x2C, struct termios2)
#define TCSETSF2	_IOW('T', 0x2D, struct termios2)
#define TIOCGRS485	0x542E
#define TIOCSRS485	0x542F
#define TIOCGPTN	_IOR('T', 0x30, unsigned int) /* Get Pty Number (of pty-mux device) */
#define TIOCSPTLCK	_IOW('T', 0x31, int)  /* Lock/unlock Pty */
#define TIOCGDEV	_IOR('T', 0x32, unsigned int) /* Get real dev no below /dev/console */
#define TCGETX		0x5432 /* SYS5 TCGETX compatibility */
#define TCSETX		0x5433
#define TCSETXF		0x5434
#define TCSETXW		0x5435

#define FIONCLEX	0x5450
#define FIOCLEX		0x5451
#define FIOASYNC	0x5452
#define TIOCSERCONFIG	0x5453
#define TIOCSERGWILD	0x5454
#define TIOCSERSWILD	0x5455
#define TIOCGLCKTRMIOS	0x5456
#define TIOCSLCKTRMIOS	0x5457
#define TIOCSERGSTRUCT	0x5458 /* For debugging only */
#define TIOCSERGETLSR   0x5459 /* Get line status register */
#define TIOCSERGETMULTI 0x545A /* Get multiport config  */
#define TIOCSERSETMULTI 0x545B /* Set multiport config */

#define TIOCMIWAIT	0x545C	/* wait for a change on serial input line(s) */
#define TIOCGICOUNT	0x545D	/* read serial port __inline__ interrupt counts */

/*
 * some architectures define FIOQSIZE as 0x545E, which is used for
 * TIOCGHAYESESP on others
 */
#ifndef FIOQSIZE
# define TIOCGHAYESESP	0x545E  /* Get Hayes ESP configuration */
# define TIOCSHAYESESP	0x545F  /* Set Hayes ESP configuration */
# define FIOQSIZE	0x5460
#endif

/* Used for packet mode */
#define TIOCPKT_DATA		 0
#define TIOCPKT_FLUSHREAD	 1
#define TIOCPKT_FLUSHWRITE	 2
#define TIOCPKT_STOP		 4
#define TIOCPKT_START		 8
#define TIOCPKT_NOSTOP		16
#define TIOCPKT_DOSTOP		32

#define TIOCSER_TEMT	0x01	/* Transmitter physically empty */


/* Routing table calls.  */
#define SIOCADDRT       0x890B          /* add routing table entry      */
#define SIOCDELRT       0x890C          /* delete routing table entry   */
#define SIOCRTMSG       0x890D          /* call to routing system       */

/* Socket configuration controls. */
#define SIOCGIFNAME     0x8910          /* get iface name               */
#define SIOCSIFLINK     0x8911          /* set iface channel            */
#define SIOCGIFCONF     0x8912          /* get iface list               */
#define SIOCGIFFLAGS    0x8913          /* get flags                    */
#define SIOCSIFFLAGS    0x8914          /* set flags                    */
#define SIOCGIFADDR     0x8915          /* get PA address               */
#define SIOCSIFADDR     0x8916          /* set PA address               */
#define SIOCGIFDSTADDR  0x8917          /* get remote PA address        */
#define SIOCSIFDSTADDR  0x8918          /* set remote PA address        */
#define SIOCGIFBRDADDR  0x8919          /* get broadcast PA address     */
#define SIOCSIFBRDADDR  0x891a          /* set broadcast PA address     */
#define SIOCGIFNETMASK  0x891b          /* get network PA mask          */
#define SIOCSIFNETMASK  0x891c          /* set network PA mask          */
#define SIOCGIFMETRIC   0x891d          /* get metric                   */
#define SIOCSIFMETRIC   0x891e          /* set metric                   */
#define SIOCGIFMEM      0x891f          /* get memory address (BSD)     */
#define SIOCSIFMEM      0x8920          /* set memory address (BSD)     */
#define SIOCGIFMTU      0x8921          /* get MTU size                 */
#define SIOCSIFMTU      0x8922          /* set MTU size                 */
#define SIOCSIFNAME     0x8923          /* set interface name           */
#define SIOCSIFHWADDR   0x8924          /* set hardware address         */
#define SIOCGIFENCAP    0x8925          /* get/set encapsulations       */
#define SIOCSIFENCAP    0x8926
#define SIOCGIFHWADDR   0x8927          /* Get hardware address         */
#define SIOCGIFSLAVE    0x8929          /* Driver slaving support       */
#define SIOCSIFSLAVE    0x8930
#define SIOCADDMULTI    0x8931          /* Multicast address lists      */
#define SIOCDELMULTI    0x8932
#define SIOCGIFINDEX    0x8933          /* name -> if_index mapping     */
#define SIOGIFINDEX     SIOCGIFINDEX    /* misprint compatibility :-)   */
#define SIOCSIFPFLAGS   0x8934          /* set/get extended flags set   */
#define SIOCGIFPFLAGS   0x8935
#define SIOCDIFADDR     0x8936          /* delete PA address            */
#define SIOCSIFHWBROADCAST      0x8937  /* set hardware broadcast addr  */
#define SIOCGIFCOUNT    0x8938          /* get number of devices */

#define SIOCGIFBR       0x8940          /* Bridging support             */
#define SIOCSIFBR       0x8941          /* Set bridging options         */

#define SIOCGIFTXQLEN   0x8942          /* Get the tx queue length      */
#define SIOCSIFTXQLEN   0x8943          /* Set the tx queue length      */

#define SIOCGIFDATA     0x8945

/* ARP cache control calls. */
                    /*  0x8950 - 0x8952  * obsolete calls, don't re-use */
#define SIOCDARP        0x8953          /* delete ARP table entry       */
#define SIOCGARP        0x8954          /* get ARP table entry          */
#define SIOCSARP        0x8955          /* set ARP table entry          */


#endif /* __ASM_GENERIC_IOCTLS_H */
