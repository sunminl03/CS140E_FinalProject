# PROGS += hdlc_example.c
# PROGS += tests/tests-hdlc/hdlc_loopback_test.c
# PROGS += tests/tests-hdlc/tests-ppp/ppp_test.c
# PROGS += tests/tests-ppp/ppp_loopback_test.c
# PROGS += tests/tests-lcp/lcp_combined_test.c
# PROGS += tests/tests-lcp/lcp_fullpath_test.c
# PROGS += tests/tests-lcp/pppd_lcp_test.c
# PROGS += tests/tests-lcp/lcp_send_config.c
# PROGS += tests/tests-lcp/pppd_lcp_clean_retry_test.c
# PROGS += tests/tests-lcp/lcp_nak_rej_handler_test.c
# PROGS += tests/tests-lcp/lcp_nak_rej_accept_test.c
# PROGS += tests/tests-ipcp/ipcp_handler_test.c
# PROGS += tests/tests-ipcp/pppd_ipcp_integration_test.c
# PROGS += tests/tests-ip/ip_test.c
# PROGS += tests/tests-ip/ip_icmp_handle_test.c
# PROGS += tests/tests-udp/udp_test.c
# PROGS += tests/tests-ip/pppd_icmp_echo_test.c
# PROGS += tests/tests-tcp/wrap32_test.c
# PROGS += tests/tests-tcp/tcp_test.c
# PROGS += tests/tests-tcp/byte_stream_test.c
# PROGS += tests/tests-tcp/reassembler_test.c
# # PROGS += tests/tests-tcp/tcp_receiver_test.c
PROGS += tests/tests-tcp/tcp_sender_test.c
PROGS += tests/tests-tcp/tcp_receiver_edge_test.c

# comment out: this is to temporarily change $CS140E_2026_PATH
-include Makefile.local

COMMON_SRC += hdlc_framing.c
COMMON_SRC += ppp.c
COMMON_SRC += lcp.c
COMMON_SRC += ipcp.c
COMMON_SRC += ip.c
COMMON_SRC += udp.c
COMMON_SRC += wrap32.c
COMMON_SRC += tcp.c
COMMON_SRC += byte_stream.c
COMMON_SRC += reassembler.c
COMMON_SRC += tcp_receiver.c
COMMON_SRC += tcp_sender.c

BOOTLOADER = my-install
TTYUSB =
RUN = 1
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

clean::
	make -C tests clean
