OFILES=shiv.o vtx.o exploit.o
ADDITIONAL_TARGETS=ex.h
NAME=shiv
OUTPUT=$(NAME).m
DEPTHDOTS=../..


include ../submake.inc

ex.h: ex.s
	echo "[AS] ex.s -> ex.h"
	nasm ex.s
	xxd -i ex > ex.h
