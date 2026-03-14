#include "rpi.h"
#include "tcp.h"
#include "net_util.h"

/*
expected output:
    TCP TEST START
    tcp_checksum verify: 0x0
    tcp_parse: 0
    src_port=1234 dst_port=80 flags=0x12 payload_len=5
    payload matches: 1
    tcp_build: 25
    rebuild checksum valid: 1
    short parse: -4
    bad offset parse: -2
    TCP TEST DONE
*/

void notmain(void) {
    printk("TCP TEST START\n");

    uint32_t src_ip = 0x0A000001; // 10.0.0.1
    uint32_t dst_ip = 0x0A000002; // 10.0.0.2

    tcp_hdr_t hdr = {
        .src_port = 1234,
        .dst_port = 80,
        .seqno = 0x11223344,
        .ackno = 0x55667788,
        .data_offset = 5,
        .flags = TCP_SYN | TCP_ACK,
        .window = 4096,
        .checksum = 0,
        .urgent_ptr = 0,
        .options = 0,
        .options_len = 0,
        .payload = 0,
        .payload_len = 0,
    };

    uint8_t payload[] = { 'h', 'e', 'l', 'l', 'o' };
    uint8_t seg[64];
    int n = tcp_build(&hdr, payload, sizeof payload, seg, sizeof seg);
    assert(n == 25);

    uint16_t cksum = tcp_checksum(src_ip, dst_ip, seg, n);
    put_be16(&seg[16], cksum);

    uint16_t verify = tcp_checksum(src_ip, dst_ip, seg, n);
    printk("tcp_checksum verify: 0x%x\n", verify);
    assert(verify == 0);

    tcp_hdr_t parsed;
    int r = tcp_parse(seg, n, &parsed);
    printk("tcp_parse: %d\n", r);
    assert(r == TCP_OK);

    printk("src_port=%d dst_port=%d flags=0x%x payload_len=%d\n",
           parsed.src_port, parsed.dst_port, parsed.flags, parsed.payload_len);
    assert(parsed.src_port == 1234);
    assert(parsed.dst_port == 80);
    assert(parsed.flags == (TCP_SYN | TCP_ACK));
    assert(parsed.payload_len == sizeof payload);

    int payload_ok = parsed.payload[0] == 'h' &&
                     parsed.payload[1] == 'e' &&
                     parsed.payload[2] == 'l' &&
                     parsed.payload[3] == 'l' &&
                     parsed.payload[4] == 'o';
    printk("payload matches: %d\n", payload_ok);
    assert(payload_ok);

    uint8_t rebuilt[64];
    parsed.checksum = 0;
    int rebuilt_n = tcp_build(&parsed, parsed.payload, parsed.payload_len, rebuilt, sizeof rebuilt);
    printk("tcp_build: %d\n", rebuilt_n);
    assert(rebuilt_n == n);

    uint16_t rebuilt_cksum = tcp_checksum(src_ip, dst_ip, rebuilt, rebuilt_n);
    put_be16(&rebuilt[16], rebuilt_cksum);
    printk("rebuild checksum valid: %d\n",
           tcp_checksum(src_ip, dst_ip, rebuilt, rebuilt_n) == 0);

    tcp_hdr_t bad;
    int short_r = tcp_parse(seg, TCP_HDR_LEN - 1, &bad);
    printk("short parse: %d\n", short_r);
    assert(short_r == TCP_TOO_SHORT);

    uint8_t bad_offset[64];
    for (unsigned i = 0; i < (unsigned)n; i++)
        bad_offset[i] = seg[i];
    bad_offset[12] = 0x40; // data_offset = 4
    int bad_offset_r = tcp_parse(bad_offset, n, &bad);
    printk("bad offset parse: %d\n", bad_offset_r);
    assert(bad_offset_r == TCP_BAD_HDR);

    printk("TCP TEST DONE\n");
    clean_reboot();
}
