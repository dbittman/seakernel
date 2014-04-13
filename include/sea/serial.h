#ifndef __SEA_SERIAL_H
#define __SEA_SERIAL_H

void arch_serial_write(int p, char a);
char arch_serial_read(int p);
void arch_serial_init(int *serial_debug_port_minor, int *serial_enable);
char arch_serial_received(int minor);

#endif
