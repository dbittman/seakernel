#include <config.h>
#include <types.h>
#ifdef CONFIG_MODULE_CRC32
uint32_t calculate_crc32(uint32_t crc, const unsigned char *buf, size_t len);
#endif
