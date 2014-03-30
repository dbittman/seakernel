#ifndef __UNAME_H
#define __UNAME_H

#define __UTSNAMELEN 65
struct utsname {
    char sysname[__UTSNAMELEN];
    char nodename[__UTSNAMELEN];
    char release[__UTSNAMELEN];
    char version[__UTSNAMELEN];
    char machine[__UTSNAMELEN];
    char domainname[__UTSNAMELEN];
};
int sys_uname(struct utsname *name);
#endif
