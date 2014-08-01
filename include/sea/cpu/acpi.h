#ifndef ACPI_H
#define ACPI_H

#include <sea/arch-include/cpu-acpi.h>

void *acpi_get_table_data(const char *sig, int *length);
void acpi_init();
#endif
