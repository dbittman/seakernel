include $(DEPTHDOTS)/make.inc
include $(DEPTHDOTS)/arch/$(ARCH)/drivers/submake.inc

OUTDIR := $(DEPTHDOTS)/$(shell echo $(BUILDDIR) | cut --complement -c1-3)/$(NAME)

MCFLAGS+=-I $(DEPTHDOTS)/arch/$(ARCH)/include -I $(DEPTHDOTS)/include -I $(DEPTHDOTS)/arch/include

DEPSFILE = $($(OUTDIR)/make.deps)

OFILES := $(addprefix $(OUTDIR)/, $(OFILES))

all: $(OUTDIR) $(DEPSFILE) $(OUTDIR)/$(OUTPUT)

$(OUTDIR):
	@mkdir -p $(OUTDIR)

$(OUTDIR)/%.o: %.c
	@echo -n -e "[CC] M\t$(MODPATH)$(NAME)/$<     \n"
	@$(CC) $(MCFLAGS) $< -c -o $@

$(OUTDIR)/$(OUTPUT) $(OUTDIR)/../built/$(NAME): $(OFILES)
	@echo -n -e "[LD] M\t$(MODPATH)$(NAME).m         \n"
	@$(CC) $(MCFLAGS) $(LDFLAGS_ARCH) $(OFILES) -o $(OUTDIR)/$(OUTPUT) -Wl,-r,-T $(DEPTHDOTS)/drivers/link.ld -lgcc -static
	@cp $(OUTDIR)/$(OUTPUT) $(OUTDIR)/../built/$(NAME)

clean:
	@-rm -f *.o $(OUTPUT).stage1 $(OUTPUT) $(DEPSFILE)

deps:
	@echo "[DP]    $(MODPATH)$(NAME) make.deps"
	@$(CC) $(MCFLAGS) -MM -MP ${OFILES:.o=.c} | sed -e "s@^\(.*\)\.o:@\1.o:@" > $(DEPSFILE)

$(DEPSFILE):
	@touch $(DEPSFILE)
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include $(DEPSFILE)
endif

