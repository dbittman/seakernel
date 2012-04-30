AR	=ar
AS	=as --32
CC      =i586-pc-seaos-gcc
LD	=i586-pc-seaos-ld -m elf_i386
LDFLAGS	=-s -x
CFLAGS	=-m32 -Wall -fstrength-reduce -fomit-frame-pointer \
	-finline-functions -fno-rename-registers -fno-regmove -fno-stack-protector -nostdinc -I../include -fno-builtin
CPP	=i586-pc-seaos-gcc -E -m32 -nostdinc -I../include

.c.o:
	echo -n -e "[CC]\t$@        \n"
	@$(CC) $(CFLAGS) -c -o $@ $<

STRINGOBJS  = string/memchr.o  string/memcpy.o  string/memmove.o  string/memset.o  string/strcat.o  string/strchr.o  string/strcmp.o string/strcpy.o string/strlen.o string/strncat.o string/strncmp.o string/strncpy.o string/strnlen.o string/strrchr.o
KLIBO = klib/bitops.o klib/bptree.o klib/crc32.o klib/qdivrem.o klib/udivdi3.o klib/umoddi3.o
all: make.deps klib.a

klib.a: $(KLIBO) $(STRINGOBJS)
	$(MAKE) -s string
	@$(CC) -O4 $(CFLAGS) -c -o klib/bptree.o klib/bptree.c
	$(AR) rcs klib.a $(KLIBO) $(STRINGOBJS)

string: $(STRINGOBJS)
	$(AR) rcs string/slib.a $(STRINGOBJS)
	
clean:
	@rm -f *.a $(STRINGOBJS) $(KLIBO) make.deps

deps:
	@gcc $(CFLAGS) -M ${STRINGOBJS:.o=.c} | sed -e "s@^\(.*\)\.o:@string/\1.o:@" > make.deps
	@gcc $(CFLAGS) -M ${KLIBO:.o=.c} | sed -e "s@^\(.*\)\.o:@klib/\1.o:@" >> make.deps

make.deps:
	@touch make.deps
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif


