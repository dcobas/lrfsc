# =====================================================
# Makefile to produce LLCC device driver
# =====================================================

include /ps/src/dsc/co/Make.auto


ifeq ($(CPU),68k)
CFLAGS = -I. \
	 -I/ps/sys/68k/usr/include/headers_68k \
	 -I/ps/local/68k/include -Wall 
	
else
CFLAGS = -I. $(KERN_INCLUDES) -I/ps/local/$(CPU)/include

endif

DRVR=llccdrvr
DDIR=llcc
ACCS=tst mcr

#========================================================================
# compiler to be used to compile drivers
#========================================================================
ifeq ($(CPU),68k)
    ifeq ($(BSP),030_M147)
	DRCC=l-cc -D__Lynx__ -D__68k__ -D__m68030__ -DMC68030 -D__cpu_m68030__ -DM147
    else
	DRCC=l-cc -D__Lynx__ -D__68k__ -D__m68040__ -DMC68040 -D__cpu_m68040__ -DMVME
    endif
    POST_DRVR=cp -p $(DRVR).o $@
else
	DRCC=$(KERN_CC)
	POST_DRVR=$(KERN_LD) -bM:SRE -bimport:$(DRVR).$(BSP).imp -o $@ $(DRVR).$(CPU).o
	DRCC+=-Wall
endif


all: $(DRVR).$(BSP).$(CPU).o


$(DRVR).$(BSP).$(CPU).o: $(DRVR).c $(DRVR).h $(DRVR)P.h
	@$(RM) $(DRVR).$(CPU).o
	$(DRCC) $(CFLAGS) $(KERN_CFLAGS) -c -o $(DRVR).$(CPU).o $(DRVR).c
	$(TOOLS1)nm -u $(DRVR).$(CPU).o  | sed '/^[.]/s///' > $(DRVR).$(BSP).imp
	$(POST_DRVR)
	@$(RM) $(DRVR).$(CPU).o

install:
	@echo Please use gmake install.xxx where xxx=147,167 or ces

install.all: install.147 install.167 install.ppc

install.147:
	$(MAKE) CPU=68k BSP=030_M147 install_xxx

install.167:
	$(MAKE) CPU=68k BSP=040_MVME install_xxx

install.ces:
	$(MAKE) CPU=ppc BSP=ces install_xxx

install.ces4:
	$(MAKE) CPU=ppc4 install_xxx

install_xxx: $(DRVR).$(BSP).$(CPU).o
	for ACC in $(ACCS); do \
	if [ -w /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR) ]; then \
	    echo Installing $(DRVR).$(BSP).o in /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR) ;\
	    if [ -f /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o ]; then \
		if [ -f /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o.old ]; then \
		    rm -f /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o.old ;\
		fi; \
		mv /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o.old; \
	    fi;\
	    cp -p $(DRVR).$(BSP).$(CPU).o /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o;\
	    chmod -w /ps/dsc/$${ACC}/$(CPU)/$(BSP)/$(DDIR)/$(DRVR).o;\
	fi; \
	done

clean:
	-$(RM) $(DRVR).*.o $(DRVR).*.imp $(BAKS)


