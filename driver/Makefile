CPU=L865

# KVER=2.6.24.7-rt21
# KVER=2.6.24.7-rt27
# KVER=2.6.29.4-rt15
# KVER=2.6.31.6-rt19

KVER=2.6.24.7-rt27

KERNELSRC=/acc/sys/$(CPU)/usr/src/kernels/$(KVER)

modules: 
	make -C $(KERNELSRC) M=`pwd` KVER=$(KVER) modules
clean:
	rm -f *.o *.a *.pyc *.so
	rm -rf $(CPU)
	make -C $(KERNELSRC) M=`pwd` clean

