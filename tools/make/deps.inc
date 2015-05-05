.PHONY: do_deps
do_deps:
	@echo "# Automatically generated" > $(DEPSFILE)
	@for i in $(DOBJS) ; do \
		echo -n -e "\e[2K[DP]    calculating deps: `basename $$i`\r"; \
		echo -n $$i >> $(DEPSFILE) ; \
		if ! $(CC) $(DCFLAGS) -MM `echo $$i | sed -e "s@^${BUILDDIR}\/\(.*\)\.${OBJ_EXT}@\1.c@"` | sed -e "s@^\(.*\)\.o:@:@" >> $(DEPSFILE); then \
			rm $(DEPSFILE) ; \
			break ; \
		fi ; \
	done
	@echo -e "\e[2K[DP]    calculating deps: done"

$(DEPSFILE): $(BUILDDIR) $(ADHEADS)
	@if [ ! -f $(DEPSFILE) ]; then \
		touch $(DEPSFILE) ; \
		${MAKE} -s do_deps ; \
	fi

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include $(DEPSFILE)
endif
endif

