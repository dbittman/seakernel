.c.o:
	echo -n -e "[CC]\t$@        \n"
	@$(CC) $(CFLAGS) -c -o $@ $<

STRINGOBJS= string/memchr.o \
			string/memcpy.o \
			string/memmove.o \
			string/memset.o \
			string/strcat.o \
			string/strchr.o \
			string/strcmp.o \
			string/strcpy.o \
			string/strlen.o \
			string/strncat.o \
			string/strncmp.o \
			string/strncpy.o \
			string/strnlen.o \
			string/strrchr.o \
			string/strtok.o
KLIBO = klib/crc32.o \
		klib/ll.o 

all: make.deps klib.a

klib.a: $(KLIBO) $(STRINGOBJS)
	$(MAKE) -s string
	$(AR) rcs klib.a $(KLIBO) $(STRINGOBJS)

string: $(STRINGOBJS)
	$(AR) rcs string/slib.a $(STRINGOBJS)

clean:
	@rm -f *.a $(STRINGOBJS) $(KLIBO) make.deps

deps:
	@$(CC) $(CFLAGS) -M ${STRINGOBJS:.o=.c} | sed -e "s@^\(.*\)\.o:@string/\1.o:@" > make.deps
	@$(CC) $(CFLAGS) -M ${KLIBO:.o=.c} | sed -e "s@^\(.*\)\.o:@klib/\1.o:@" >> make.deps

make.deps:
	@touch make.deps
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif


