#================================================================
# Makefile to produce lrfsc driver test program
#================================================================

CPU=L865

include /acc/dsc/src/co/Make.auto

COMPILE_TIME:=$(shell date +%s)

CFLAGS = -g -Wall -I. -I../include -I../driver -DCOMPILE_TIME=$(COMPILE_TIME)

LDLIBS= -lm

ALL  = lrfsctest.$(CPU).o lrfsctest.$(CPU)

SRCS = lrfsctest.c LrfscCmds.c DoCmd.c GetAtoms.c Cmds.c PrintAtoms.c

HDRS = Cmds.h

all: $(ALL)

# Run on Workstation only

lrfsctest.$(CPU).o: $(SRCS) $(HDRS)

lrfsctest.$(CPU): lrfsctest.$(CPU).o
