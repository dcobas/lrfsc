#================================================================
# Makefile to produce standard LRFSC test program
#================================================================

CPU=ppc4

include /ps/dsc/src/co/Make.auto

DDIR = lrfsc

COMPILE_TIME:=$(shell ./gettime)

CFLAGS= -g -Wall -I. -I../driver -DCOMPILE_TIME=$(COMPILE_TIME)

LDLIBS = -ltgm -lerr -lnc

ALL  = lrfsctest.$(CPU) lrfsctest.$(CPU).o

SRCS = lrfsctest.c LrfscCmds.c LrfscOpen.c DoCmd.c GetAtoms.c Cmds.c

HDRS = Cmds.h

TEST = lrfsctest.c LrfscCmds.c LrfscOpen.c DoCmd.c GetAtoms.c Cmds.c

all: $(ALL)

clean:
	$(RM) $(ALL) $(BAKS)

# Run on Workstation only

lrfsctest.$(CPU).o: $(TEST) $(HDRS)

install: lrfsctest.$(CPU)
	@for f in $(ACCS); do \
	    dsc_install lrfsctest.$(CPU) /acc/dsc/$$f/$(CPU)/$(BSP)/$(DDIR); \
	done;

