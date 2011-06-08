#================================================================
# Makefile to produce lrfscing library
#================================================================

include /ps/dsc/src/co/Make.auto

CFLAGS= -g -Wall -I. -I/ps/local/$(CPU)/include -I/dsrc/drivers/lrfsc/driver

SRCS=liblrfsc.c liblrfsc.h

INSTFILES=liblrfsc.$(CPU).a liblrfsc.h ../driver/lrfscdrvr.h ../driver/lrfscdrvrP.h

ACCS=

all:$(INSTFILES)

liblrfsc.$(CPU).o: liblrfsc.c

liblrfsc.$(CPU).a: liblrfsc.$(CPU).o
	-$(RM) $@
	$(AR) $(ARFLAGS) $@ $^
	$(RANLIB) $@

clean:
	rm -f liblrfsc.$(CPU).o
	rm -f liblrfsc.$(CPU).a

install: $(INSTFILES)
	cp liblrfsc.$(CPU).a /ps/local/$(CPU)/lrfsc/liblrfsc.a
	rm -f /ps/local/$(CPU)/lrfsc/lrfscdrvr.h
	rm -f /ps/local/$(CPU)/lrfsc/lrfscdrvrP.h
	rm -f /ps/local/$(CPU)/lrfsc/liblrfsc.h
	cp ../driver/lrfscdrvr.h /ps/local/$(CPU)/lrfsc
	cp ../driver/lrfscdrvrP.h /ps/local/$(CPU)/lrfsc
	cp liblrfsc.h /ps/local/$(CPU)/lrfsc
