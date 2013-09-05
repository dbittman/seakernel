#ifndef ACPI_H
#define ACPI_H
#include <config.h>
#if CONFIG_ARCH == TYPE_ARCH_X86
#include <acpi-x86_common.h>
#elif CONFIG_ARCH == TYPE_ARCH_X86_64
#include <acpi-x86_common.h>
#endif

addr_t find_RSDT_entry(struct acpi_dt_header *rsdt, int pointer_size, const char *sig);
void *acpi_get_table_data(const char *sig, int *length);
void init_acpi();

#endif
