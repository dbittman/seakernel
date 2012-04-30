OFILES=ata.o atapi.o dma.o init.o pio.o util.o
NAME=ata
OUTPUT=$(NAME).m
CC=i586-pc-seaos-gcc
LD=i586-pc-seaos-ld
all: make.deps $(OUTPUT)

CFLAGS=-std=c99 -Wall -Wextra \
          -Wformat-security -Wformat-nonliteral -fno-rename-registers -fno-regmove\
	  -Wshadow -Wpointer-arith -Wcast-align -Wno-unused \
	  -Wnested-externs -Winline -Wno-long-long -mno-red-zone -c -O3 -m32 -I../include -I./include -I../../include -I../../../include -ffreestanding -fno-common -nostdlib

%.o: %.c
	echo -n -e "[CC] M\t$(NAME)/$@     \n"
	$(CC) $(CFLAGS) $< -o $@


$(OUTPUT): $(OFILES)
	echo -n -e "[LD] M\t$(NAME)/$@         \n"
	$(LD) $(OFILES) -o $(OUTPUT).stage1 -m seaos_i386 -r ../../library/klib.a  -T ../link.ld
	cat ../head ../head2 $(OUTPUT).stage1 > $(OUTPUT)

clean:
	@-rm *.o $(OUTPUT).stage1 $(OUTPUT) make.deps

deps:
	@$(CC) $(CFLAGS) -MM -MP ${OFILES:.o=.c} | sed -e "s@^\(.*\)\.o:@\1.o:@" > make.deps

make.deps:
	@touch make.deps
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif
