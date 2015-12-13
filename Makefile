# seakernel makefile
.DEFAULT_GOAL = all
.DEFAULT = all
SHELL=/bin/bash
BUILDCFG ?= default
BUILDCONTAINER = build
export BUILDDIR = $(BUILDCONTAINER)/$(BUILDCFG)
ARCH=__none__
ifneq ($(MAKECMDGOALS),config)
ifneq ($(MAKECMDGOALS),defconfig)
ifneq ($(MAKECMDGOALS),all)
include $(BUILDDIR)/sea_defines.inc
endif
endif
endif

# process the arch config, and set the arch string
ifeq ($(CONFIG_ARCH), 2)
	ARCH=x86_64
	ARCH_TC=x86_64
endif

# TODO: That no-isolate-erroneous-paths-dereference should not be here
# no-tree-loop-distribute-patterns is required because of stupid gcc.
CFLAGS_NOARCH = -std=gnu11 \
                -ffreestanding \
                -mno-red-zone \
                -nostdlib \
                -mpush-args -mno-accumulate-outgoing-args \
                -Iarch/${ARCH}/include \
                -Iinclude \
                -D__KERNEL__ \
                -Wall -Wextra -Wformat-security -Wformat-nonliteral \
	            -Wno-strict-aliasing -Wshadow -Wpointer-arith -Wcast-align \
	            -Wno-unused -Wnested-externs -Waddress -Winline \
	            -Wno-long-long -Wno-unused-parameter -Wno-unused-but-set-parameter\
	            -mno-mmx -mno-sse -mno-sse2 -mno-sse3 -mno-3dnow \
	            -fno-omit-frame-pointer \
	            -fno-tree-loop-distribute-patterns \
			    -fno-isolate-erroneous-paths-dereference \
			    -Werror
	        
	        
CFLAGS_NOARCH += -O$(CONFIG_OPTIMIZATION_LEVEL)

ifeq ($(CONFIG_DEBUG),y)
	CFLAGS_NOARCH += -g -D__DEBUG__
endif

ifeq ($(CONFIG_WERROR),y)
	CFLAGS_NOARCH += -Werror
endif

include make.inc
include version
	        
ifneq ($(ARCH),__none__)
include arch/${ARCH}/make.inc
endif

CFLAGS_NOARCH += -I$(BUILDDIR)/arch/include -I$(BUILDDIR) -include $(BUILDDIR)/sea_defines.h

export ARCH_TC
export ARCH
export CC
export LD
export AR

export CFLAGS  = ${CFLAGS_NOARCH} ${CFLAGS_ARCH}
export LDFLAGS = ${LDFLAGS_ARCH}
export ASFLAGS = ${ASFLAGS_ARCH}
export GASFLAGS= ${GASFLAGS_ARCH}

VERSION_H = include/sea/version.h

include arch/make.inc
include kernel/make.inc
include drivers/make.inc
include library/make.inc

ADHEADS := $(addprefix $(BUILDDIR)/, $(ADHEADS))
DEPSFILE=$(BUILDDIR)/make.deps
KERNEL = $(BUILDDIR)/skernel

all:
	@if [ "$(BUILDCFGS)" == "" ]; then \
		echo "===== Building Configuration" $(BUILDCFG) "->" $(BUILDDIR) "=====" ;\
		$(MAKE) -s do_all ;\
	else \
		dirs=($(shell ls -d $(addprefix $(BUILDCONTAINER)/, $(BUILDCFGS)))) ;\
		cd $(BUILDCONTAINER) ;\
		cfgs=($$(ls -d $(BUILDCFGS))) ;\
		cd .. ;\
		for ((i=0;i<$${#dirs[@]};++i)); do \
			echo "===== Building Configuration" $${cfgs[$$i]} "->" $${dirs[$$i]} "=====" ;\
			BUILDCFG=$${cfgs[$$i]} $(MAKE) -s do_all ;\
		done ;\
	fi

do_all: $(BUILDDIR) $(DEPSFILE) $(VERSION_H) $(ADHEADS) $(KERNEL) modules

$(BUILDDIR):
	@mkdir -p $(BUILDDIR)

ifneq ($(MAKECMDGOALS),config)
  ifneq ($(MAKECMDGOALS),defconfig)
    ifneq ($(MAKECMDGOALS),clean)
    ifneq ($(MAKECMDGOALS),all)
      DOBJS=$(KOBJS)
      DCFLAGS=$(CFLAGS)
      export OBJ_EXT=o
      include tools/make/deps.inc
    endif
  endif
  endif
endif

KOBJS := $(addprefix $(BUILDDIR)/, $(KOBJS))
AOBJS := $(addprefix $(BUILDDIR)/, $(AOBJS))

.PHONY: clean,kernel,modules,all,distclean,help,love,gcc-print-optimizers,deps,help
#.NOTPARALLEL: $(VERSION_H)
#.NOTPARALLEL: $(ADHEADS)

$(VERSION_H): version
	@echo "[GH]    $(VERSION_H)"
	@echo "/* auto generated during build */" > $(VERSION_H)
	@echo "#ifndef __SEA_VERSION_H" >> $(VERSION_H)
	@echo "#define __SEA_VERSION_H" >> $(VERSION_H)
	@echo "#define CONFIG_VERSION_STRING \"$(VERSION_STRING)\"" >> $(VERSION_H)
	@echo "#define CONFIG_VERSION \"$(VERSION)\"" >> $(VERSION_H)
	@echo "#define CONFIG_VERSION_NUMBER $(VERSION_NUMBER)" >> $(VERSION_H)
	@echo "#endif" >> $(VERSION_H)

$(ADHEADS):
	@mkdir -p $(BUILDDIR)/arch/include/sea/arch-include
	@echo "[GH]    $@"
	@tools/arch-dep-header-gen.sh $@ > $@

$(KERNEL): $(VERSION_H) $(ADHEADS) $(AOBJS) $(KOBJS)
	@echo "[LD]	${KERNEL}"
	@$(CC) $(CFLAGS) $(LDFLAGS) -o $(KERNEL) $(AOBJS) $(KOBJS) -nostdlib -lgcc -static-libgcc -static

modules: $(VERSION_H) $(ADHEADS)
	@echo Building modules, pass 1...
	@$(MAKE) -s -C drivers BUILDDIR=../$(BUILDDIR)/drivers

deps: do_deps
	@make -s -C drivers do_deps BUILDDIR=../$(BUILDDIR)/drivers

install: $(KERNEL) modules
	@echo "installing kernel..."
	@cp -f $(KERNEL) /sys/kernel
	@make -C drivers install VERSION=${VERSION} BUILDDIR=../$(BUILDDIR)/drivers

clean:
	@find $(BUILDDIR)/* ! -name sea_defines.h ! -name sea_defines.inc ! -name .config.cfg -delete

config:
	@tools/conf.rb config.cfg
	@echo post-processing configuration...
	@tools/config.rb .config.cfg
	@mv .config.cfg sea_defines.{h,inc} $(BUILDDIR)/
	@echo "run make clean to remove old objects if architecture has changed"
	
defconfig:
	@tools/conf.rb -d config.cfg
	@echo post-processing configuration...
	@tools/config.rb .config.cfg

doc:
	make -s -C documentation pdf
	make -s -C documentation aux_clean

love:
	@echo Not war

gcc_print_optimizers:
	@$(CC) $(CFLAGS) -Q --help=optimizers

help:
	@echo "make [target]"
	@echo "Useful targets:"
	@echo -e " config:\truns the configuration utility"
	@echo -e " defconfig:\tcreates a default configuration"
	@echo -e " clean:\t\tremoves compiled objects from the source tree"
	@echo -e " deps:\t\trecalculates dependencies for the source tree"
	@echo -e " install:\tcopies the compiled binaries to their proper locations in the file system"
	@echo -e " all,os:\tcompiles the kernel"
	@echo

$(BUILDDIR)/%.o: %.s
	@echo -n -e "[AS]\t$@            \n"
	@mkdir -p $$(dirname $@)
	@nasm $(ASFLAGS) $< -o $@
	
$(BUILDDIR)/%.o: %.c $(ADHEAD) $(VERSION_H)
	@echo -n -e "[CC]\t$@            \n"
	@mkdir -p $$(dirname $@)
	@$(CC) -c $(CFLAGS) -o $@ $<

