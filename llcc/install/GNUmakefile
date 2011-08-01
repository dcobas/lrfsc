#================================================================
# Makefile to produce llcc Installation program
#================================================================

include /ps/dsc/src/co/Make.auto

CFLAGS= -g -Wall -I/ps/local/$(CPU)/include/tgm -I../driver -I.
LDLIBS=
ACCS=tst mcr
SRCS=llccinstall.c drvr_parser.c

ALL=llccinstall.$(CPU)

all:$(ALL)

install:   $(ALL)
	for f in $(ACCS); do \
	    for c in $(BSPS); do \
		y=`echo $$c | awk -F/ '{print $$1}'`; x=/ps/dsc/$$f/$$c/llcc; \
		if [ -w $$x -a -f llccinstall.$$y ]; then \
			echo dsc_install $^ $$x ; \
			dsc_install $^ $$x ; \
		fi; \
	    done ;\
	done

depend: $(SRCS1)
	mkdepend -o .'$$(CPU)'.o -- $(STDFLAGS)  -- $(SRCS)



clean:
	$(RM) llccinstall.$(CPU) *.o *.old $(BAKS)


llccinstall.$(CPU): $(SRCS:.c=.$(CPU).o)


# DO NOT DELETE THIS LINE -- make depend depends on it.

