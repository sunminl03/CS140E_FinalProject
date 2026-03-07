# PROGS += hdlc_example.c
# PROGS += tests/tests-hdlc/0-basic-encode-decode.c
# PROGS += tests/tests-hdlc/1-crc16-test.c
# PROGS += tests/tests-hdlc/2-escape-test.c
# PROGS += tests/tests-hdlc/3-fcs-verification.c
# PROGS += tests/tests-hdlc/4-edge-cases.c
# PROGS += tests/tests-hdlc/5-error-handling.c
# PROGS += tests/tests-hdlc/hdlc_loopback_test.c
# PROGS += tests/tests-hdlc/tests-ppp/ppp_test.c
PROGS += tests/tests-ppp/ppp_loopback_test.c

COMMON_SRC += hdlc_framing.c
COMMON_SRC += ppp.c

BOOTLOADER = my-install
TTYUSB = 
RUN = 1
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

clean::
	make -C tests clean