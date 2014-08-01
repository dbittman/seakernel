
.s.o:
#	echo "[AS]	$@"
	echo -n -e "[AS]\t$@            \n"
	nasm $(ASFLAGS) $<
	
.c.o: $(ADHEAD)
#	echo "[CC]	$@"
	echo -n -e "[CC]\t$@            \n"
	$(CC) -c $(CFLAGS) -o $@ $<
