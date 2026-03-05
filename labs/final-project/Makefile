###############################################################
# Final project: simple Pi <-> laptop serial demo
# Builds programs for serial communication and PPP frame parsing

# Programs with `notmain` entry points.
PROGS += serial_comm.c
PROGS += ppp_test.c

# Common source files (shared across programs)
COMMON_SRC += ppp_frame.c

# Use the standard bootloader script; you can change this
# to my-install if you have your own.
BOOTLOADER = pi-install

# Set to 0 if you don't want `make run` to auto-load the binary.
RUN = 1

# Pull in the shared cs140e build rules.
include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v2

