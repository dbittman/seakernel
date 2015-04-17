include $(DEPTHDOTS)/make.inc
include $(DEPTHDOTS)/arch/$(ARCH)/drivers/submake.inc

MCFLAGS+=-I $(DEPTHDOTS)/arch/$(ARCH)/include -I $(DEPTHDOTS)/include -I $(DEPTHDOTS)/arch/include

all: make.deps $(ADDITIONAL_TARGETS) $(OUTPUT)

%.o: %.c
	echo -n -e "[CC] M\t$(MODPATH)$(NAME)/$@     \n"
	$(CC) $(MCFLAGS) $< -c -o $@

$(OUTPUT): $(OFILES)
	echo -n -e "[LD] M\t$(MODPATH)$(NAME)/$@         \n"
	$(CC) $(MCFLAGS) $(LDFLAGS_ARCH) $(OFILES) -o $(OUTPUT) -Wl,-r,-T $(DEPTHDOTS)/drivers/link.ld $(DEPTHDOTS)/library/klib.a -lgcc -static

clean:
	@-rm -f *.o $(OUTPUT).stage1 $(OUTPUT) make.deps

deps:
	echo "[DP]    $(MODPATH)$(NAME) make.deps"
	@$(CC) $(MCFLAGS) -MM -MP ${OFILES:.o=.c} | sed -e "s@^\(.*\)\.o:@\1.o:@" > make.deps

make.deps:
	@touch make.deps
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif
