#include <sea/kernel.h>
#include <sea/asm/system.h>
#include <sea/cpu/cpu-io.h>
/* This does the actual reset */
void arch_cpu_reset()
{
	/* flush the keyboard controller */
	unsigned temp;
	do
	{
		temp = inb(0x64);
		if((temp & 0x01) != 0)
		{
			(void)inb(0x60);
			continue;
		}
	} while((temp & 0x02) != 0);
	/* Reset! */
	outw((short)0x64, (short)0xFE);
	
	/* Should the above code fail, we triple fault */
	asm("lgdt 0x0");
	asm("lidt 0x0");
	asm("sti");
	asm("int $0x03");
	
	for(;;) asm("hlt");
}
