ENTRY(bootstrap)
OUTPUT_FORMAT("elf64-x86-64")
SECTIONS
{
	. = 0x100000;
	kernel_start = .;

	.bootstrap :
	{
		*(.boot)
		*(.trampoline)
	}

	. += 0xffff800000000000;

    .text ALIGN (0x1000) : AT(ADDR(.text) - 0xffff800000000000)
    {
    	*(.text)
	}

    .data ALIGN (0x1000) : AT(ADDR(.data) - 0xffff800000000000)
    {
        *(.data)
        *(.rodata)
    }
	
	.eh_frame ALIGN(0x1000) : AT(ADDR(.eh_frame) - 0xffff800000000000)
   	{
   	    *(.eh_frame)
   	}

    .bss ALIGN(0x1000) : AT(ADDR(.bss) - 0xffff800000000000)
    {
        *(.bss)
        *(COMMON)
    }
	
    kernel_end = .;
    /DISCARD/ :
    {
       	*(.comment)
    }
}

