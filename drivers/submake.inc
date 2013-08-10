include ../../make.inc
include ../../arch/$(ARCH)/drivers/submake.inc

all: make.deps $(OUTPUT)

%.o: %.c
	echo -n -e "[CC] M\t$(NAME)/$@     \n"
	$(CC) $(MCFLAGS) $< -c -o $@

$(OUTPUT): $(OFILES)
	echo -n -e "[LD] M\t$(NAME)/$@         \n"
	$(CC) $(MCFLAGS) $(LDFLAGS_ARCH) $(OFILES) -o $(OUTPUT).stage1 -Wl,-r,-T../link.ld ../../library/klib.a -lgcc -static
	cat ../head $(OUTPUT).stage1 > $(OUTPUT)

clean:
	@-rm -f *.o $(OUTPUT).stage1 $(OUTPUT) make.deps

deps:
	@$(CC) $(MCFLAGS) -MM -MP ${OFILES:.o=.c} | sed -e "s@^\(.*\)\.o:@\1.o:@" > make.deps

make.deps:
	@touch make.deps
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif
