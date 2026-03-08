# PROGS += hdlc_example.c
# PROGS += tests/tests-hdlc/hdlc_loopback_test.c
# PROGS += tests/tests-hdlc/tests-ppp/ppp_test.c
# PROGS += tests/tests-ppp/ppp_loopback_test.c
# PROGS += tests/tests-lcp/lcp_combined_test.c
# PROGS += tests/tests-lcp/lcp_fullpath_test.c
# PROGS += tests/tests-lcp/pppd_lcp_test.c
# PROGS += tests/tests-lcp/lcp_send_config.c
PROGS += tests/tests-lcp/pppd_lcp_clean_retry_test.c

COMMON_SRC += hdlc_framing.c
COMMON_SRC += ppp.c
COMMON_SRC += lcp.c

BOOTLOADER = my-install
TTYUSB = 
RUN = 1
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

clean::
	make -C tests clean