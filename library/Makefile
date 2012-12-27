.c.o:
	echo -n -e "[CC]\t$@        \n"
	@$(CC) $(CFLAGS) -c -o $@ $<

-include ../make.inc
-include ../sea_defines.inc

include ../arch/$(ARCH)/library/make.inc

KLIBO = klib/ll.o \
		klib/strtoint.o

-include arch/$(ARCH)/library/make.inc

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


