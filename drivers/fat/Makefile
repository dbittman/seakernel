OFILES=fat.o rw.o
NAME=fat
OUTPUT=$(NAME).m
all: make.deps $(OUTPUT)

%.o: %.c
	echo -n -e "[CC] M\t$(NAME)/$@     \n"
	$(CC) $(MCFLAGS) $< -c -o $@


$(OUTPUT): $(OFILES)
	echo -n -e "[LD] M\t$(NAME)/$@         \n"
	ld $(OFILES) -o $(OUTPUT).stage1 -m elf_i386 -r ../../library/klib.a  -T ../link.ld
	cat ../head $(OUTPUT).stage1 > $(OUTPUT)

clean:
	@-rm *.o $(OUTPUT).stage1 $(OUTPUT) make.deps

deps:
	@$(CC) $(MCFLAGS) -MM -MP ${OFILES:.o=.c} | sed -e "s@^\(.*\)\.o:@\1.o:@" > make.deps

make.deps:
	@touch make.deps
	@${MAKE} deps

ifneq ($(MAKECMDGOALS),clean)
-include make.deps
endif
