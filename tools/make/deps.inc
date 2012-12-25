do_deps:
	@-rm make.deps 2> /dev/null
	for i in $(DOBJS) ; do \
		echo -n $$i >> make.deps ; \
		$(CC) $(DCFLAGS) -MM `echo $$i | sed -e "s@^\(.*\)\.${OBJ_EXT}@\1.c@"` | sed -e "s@^\(.*\)\.o:@:@" >> make.deps; \
	done
	

make.deps:
	@touch make.deps
	@${MAKE} -s do_deps

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(MAKECMDGOALS),distclean)
-include make.deps
endif
endif
