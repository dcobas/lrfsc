#!	/bin/sh

# what ppc4 installation used to be:
# ../install/lrfscinstall.ppc4 -object lrfscdrvr.ces.ppc4.o -M0x100000 -V150 -L2

insmod lrfscdrvr.ko base_address=0x100000 vector=150 level=2

