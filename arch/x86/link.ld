ENTRY(start)
SECTIONS
{
	. = 0xC0100000;

    .text : AT(ADDR(.text) - 0xC0000000)
    {
		kernel_start = .;
    	*(.text)
		code = .; _code = .; __code = .;
		*(.trampoline)
	}

    .data ALIGN (0x1000) : AT(ADDR(.data) - 0xC0000000)
    {
        data = .; _data = .; __data = .;
        *(.data)
        *(.rodata)
    }

    .bss ALIGN(0x1000) : AT(ADDR(.bss) - 0xC0000000)
    {
        bss = .; _bss = .; __bss = .;
        *(.bss)
    }
    end = .; _end = .; __end = .;
}

