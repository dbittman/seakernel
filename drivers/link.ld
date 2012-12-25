SECTIONS
{
    .text :
    {
        *(.text)
    }

    .data :
    {
        data = .; _data = .; __data = .;
        *(.data)
        *(.rodata)
		*(.bss)
    }

}
