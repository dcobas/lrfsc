#================================================================
# Makefile to produce lrfsc library
#================================================================

CPU=L865

include /ps/dsc/src/co/Make.auto

COMPILE_TIME:=$(shell date +%s)

CFLAGS= -DCOMPILE_TIME=$(COMPILE_TIME) -g -Wall -I. -I../include -I../driver -fPIC

SRCS=liblrfsc.c liblrfsc.h

all: liblrfsc.$(CPU).so liblrfsc.$(CPU).a

liblrfsc.$(CPU).o: liblrfsc.c

liblrfsc.$(CPU).a: liblrfsc.$(CPU).o
	-$(RM) $@
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

liblrfsc.$(CPU).so: liblrfsc.$(CPU).o
	-$(RM) $@
	$(CC) $(CFLAGS) -o $@ -shared $^

clean:
	rm -f liblrfsc.$(CPU).o
	rm -f liblrfsc.$(CPU).a
