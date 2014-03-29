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
    }
    
    .bss :
    {
	    *(.bss)
    }
    
	/DISCARD/ : { *(.comment) *(.eh_frame) *(.debug_abbrev) *(.debug_info) *(.debug_line) *(.debug_loc) *(.debug_pubnames) *(.debug_aranges) *(.debug_str) *(.debug_macinfo) *(.debug_ranges) *(.stab) *(.stabstr) }
}
