#=======================================================
# Makefile to produce LLCC test program and rt task
#=======================================================

CFLAGS = -g -I. -I../driver -Wall

DSC=psdsc13
ACC=tst


include /ps/dsc/src/co/Make.auto

ALL = llcctest.$(CPU)

LDLIBS =-ltgm -ldscrt -ldrvrutil -lerr -lerr_dummy -lbsd $(XTRALIBS)
LDFLAGS = -g

all: $(ALL)

llcctest.$(CPU): llcctest.$(CPU).o

llcctest.$(CPU).o: llcctest.c

clean:
	$(RM) $(ALL) *.o $(BAKS)

install: $(ALL)
	dsc_install llcctest.$(CPU) /ps/dsc/tst/$(CPU)/$(BSP)/llcc
