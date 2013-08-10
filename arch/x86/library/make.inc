STRINGOBJS= ../arch/x86/library/string/memchr.o \
			string/memcmp.o \
			../arch/x86/library/string/memcpy.o \
			../arch/x86/library/string/memmove.o \
			string/memset.o \
			../arch/x86/library/string/strcat.o \
			../arch/x86/library/string/strchr.o \
			../arch/x86/library/string/strcmp.o \
			../arch/x86/library/string/strcpy.o \
			../arch/x86/library/string/strlen.o \
			../arch/x86/library/string/strncat.o \
			../arch/x86/library/string/strncmp.o \
			../arch/x86/library/string/strncpy.o \
			../arch/x86/library/string/strnlen.o \
			../arch/x86/library/string/strrchr.o \
			string/strtok.o

CFLAGS += -I ../arch/x86_common/include
