// ping pong 4-byte packets between two pis.
//
// each person sets <i_am_server> and swaps the addresses.
//   server: i_am_server=1, my_addr=0xd1d1d1, partner_addr=0xe1e1e1
//   client: i_am_server=0, my_addr=0xe1e1e1, partner_addr=0xd1d1d1
#include "nrf-test.h"

// useful to mess around with these.
enum { ntrial = 1000, timeout_usec = 1000, nbytes = 4 };

// >>> set these for your side <<<
enum {
    i_am_server = 0,
    my_addr = 0xe1e1e1,
    partner_addr = 0xd1d1d1
};

// example possible wrapper to recv a 32-bit value.
static int net_get32(nrf_t *nic, uint32_t *out) {
    int ret = nrf_read_exact_timeout(nic, out, 4, timeout_usec);
    if(ret != 4) {
        debug("receive failed: ret=%d\n", ret);
        return 0;
    }
    return 1;
}
// example possible wrapper to send a 32-bit value.
static void net_put32(nrf_t *nic, uint32_t txaddr, uint32_t x) {
    int ret = nrf_send_noack(nic, txaddr, &x, 4);
    if(ret != 4)
        panic("ret=%d, expected 4\n");
}

void notmain(void) {
    kmalloc_init_mb(1);

    trace("partner ping-pong: %s rx=[%x] sending to [%x]\n",
                i_am_server ? "SERVER" : "CLIENT",
                my_addr, partner_addr);

    nrf_t *nic = server_mk_noack(my_addr, nbytes);
    nrf_stat_start(nic);

    unsigned npackets = 0, ntimeout = 0;
    uint32_t exp = 0, got = 0;

    for(unsigned i = 0; i < ntrial; i++) {
        if(i && i % 100 == 0)
            trace("sent %d no-ack'd packets [timeouts=%d]\n",
                    npackets, ntimeout);

        if(i_am_server) {
            // server sends first, then waits for reply.
            net_put32(nic, partner_addr, ++exp);
            if(!net_get32(nic, &got))
                ntimeout++;
            else if(got != exp)
                nrf_output("got %d (expected=%d)\n", got, exp);
            else
                npackets++;
        } else {
            // client waits for a message, then echoes it back.
            if(!net_get32(nic, &got)) {
                ntimeout++;
                continue;
            }
            exp = got;
            npackets++;
            net_put32(nic, partner_addr, exp);
        }
    }

    trace("total successfully sent %d packets [lost=%d]\n",
        npackets, ntimeout);
    nrf_stat_print(nic, "done with partner ping-pong test");
}
