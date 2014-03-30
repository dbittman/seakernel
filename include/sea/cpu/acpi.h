#ifndef ACPI_H
#define ACPI_H
#include <sea/config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <acpi-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <acpi-x86_common.h>
#endif

void *acpi_get_table_data(const char *sig, int *length);
void acpi_init();
#endif
