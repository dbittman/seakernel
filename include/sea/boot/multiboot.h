#ifndef __SEA_BOOT_MULTIBOOT_H
#define __SEA_BOOT_MULTIBOOT_H


#include <sea/types.h>
#define MULTIBOOT_FLAG_MEM     0x001
#define MULTIBOOT_FLAG_DEVICE  0x002
#define MULTIBOOT_FLAG_CMDLINE 0x004
#define MULTIBOOT_FLAG_MODS    0x008
#define MULTIBOOT_FLAG_AOUT    0x010
#define MULTIBOOT_FLAG_ELF     0x020
#define MULTIBOOT_FLAG_MMAP    0x040
#define MULTIBOOT_FLAG_CONFIG  0x080
#define MULTIBOOT_FLAG_LOADER  0x100
#define MULTIBOOT_FLAG_APM     0x200
#define MULTIBOOT_FLAG_VBE     0x400

struct multiboot
{
	uint32_t flags;
	uint32_t mem_lower;
	uint32_t mem_upper;
	uint32_t boot_device;
	uint32_t cmdline;
	uint32_t mods_count;
	uint32_t mods_addr;
	uint32_t num;
	uint32_t size;
	uint32_t addr;
	uint32_t shndx;
	uint32_t mmap_length;
	uint32_t mmap_addr;
	uint32_t drives_length;
	uint32_t drives_addr;
	uint32_t config_table;
	uint32_t boot_loader_name;
	uint32_t apm_table;
	uint32_t vbe_control_info;
	uint32_t vbe_mode_info;
	uint32_t vbe_mode;
	uint32_t vbe_interface_seg;
	uint32_t vbe_interface_off;
	uint32_t vbe_interface_len;
} __attribute__((packed));

typedef struct
{
  uint32_t size;
  uint32_t base_addr_low;
  uint32_t base_addr_high;
  uint32_t length_low;
  uint32_t length_high;
  uint32_t type;
} __attribute__((packed)) mmap_entry_t;


typedef struct multiboot_header multiboot_header_t;

struct vbecontrollerinfo {
   char signature[4];             // == "VESA"
   short version;                 // == 0x0300 for VBE 3.0
   short oemString[2];            // isa vbeFarPtr
   unsigned char capabilities[4];
   short videomodes[2];           // isa vbeFarPtr
   short totalMemory;             // as # of 64KB blocks
} __attribute__((packed));

struct vbemodeinfo {
	/* for all VBE revisions */
	uint16_t mode_attr;
	uint8_t  winA_attr;
	uint8_t  winB_attr;
	uint16_t win_granularity;
	uint16_t win_size;
	uint16_t winA_seg;
	uint16_t winB_seg;
	uint32_t win_func_ptr;
	uint16_t bytes_per_scan_line;

	/* for VBE 1.2+ */
	uint16_t x_res;
	uint16_t y_res;
	uint8_t  x_char_size;
	uint8_t  y_char_size;
	uint8_t  planes;
	uint8_t  bits_per_pixel;
	uint8_t  banks;
	uint8_t  memory_model;
	uint8_t  bank_size;
	uint8_t  image_pages;
	uint8_t  reserved1;

	/* Direct color fields for direct/6 and YUV/7 memory models. */
	/* Offsets are bit positions of lsb in the mask. */
	uint8_t  red_len;
	uint8_t  red_off;
	uint8_t  green_len;
	uint8_t  green_off;
	uint8_t  blue_len;
	uint8_t  blue_off;
	uint8_t  rsvd_len;
	uint8_t  rsvd_off;
	uint8_t  direct_color_info;	/* direct color mode attributes */

	/* for VBE 2.0+ */
	unsigned int phys_base_ptr;
	uint8_t  reserved2[6];

	/* for VBE 3.0+ */
	uint16_t lin_bytes_per_scan_line;
	uint8_t  bnk_image_pages;
	uint8_t  lin_image_pages;
	uint8_t  lin_red_len;
	uint8_t  lin_red_off;
	uint8_t  lin_green_len;
	uint8_t  lin_green_off;
	uint8_t  lin_blue_len;
	uint8_t  lin_blue_off;
	uint8_t  lin_rsvd_len;
	uint8_t  lin_rsvd_off;
	uint32_t max_pixel_clock;
	uint16_t mode_id;
	uint8_t  depth;
} __attribute__ ((packed));

#endif
