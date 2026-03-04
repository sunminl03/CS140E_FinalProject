# Makefile for final project: Zero-Copy IPv4 Stack over NRF24L01P
# Phase 1: pbuf + fragmentation/reassembly

# NRF can be finicky about back to back runs
BOOTLOADER = sleep 1; my-install

# Test programs
PROGS += tests/10_frag_local.c

# Common source files
COMMON_SRC := \
    net/pbuf.c \
    net/nrf_frag.c

# NRF driver from lab 14
COMMON_SRC += \
    $(CS140E_2026_PATH)/labs/14-nrf-networking/code/nrf-hw-support.c \
    $(CS140E_2026_PATH)/labs/14-nrf-networking/code/nrf-public.c \
    $(CS140E_2026_PATH)/labs/14-nrf-networking/code/nrf-driver.c

# Staff objects
STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-hw-spi.o
STAFF_OBJS += $(CS140E_2026_PATH)/libpi/staff-objs/staff-kmalloc.o

# Use staff NRF driver for now (you can replace with your own)
STAFF_OBJS += $(CS140E_2026_PATH)/labs/14-nrf-networking/code/staff-nrf-driver.o

# Include paths
CFLAGS += -I$(CS140E_2026_PATH)/labs/14-nrf-networking/code
CFLAGS += -I$(CS140E_2026_PATH)/labs/final-project
CFLAGS += -I$(CS140E_2026_PATH)/labs/final-project/net

# Auto-run tests
RUN=1

EXCLUDE ?= grep -v simple_boot
GREP_STR := 'HASH:\|ERROR:\|PANIC:\|SUCCESS:\|NRF:\|TRACE:'

include $(CS140E_2026_PATH)/libpi/mk/Makefile.robust-v3

checkoff:
	make -f ./Makefile PROGS="$(wildcard tests/*.c)" check
