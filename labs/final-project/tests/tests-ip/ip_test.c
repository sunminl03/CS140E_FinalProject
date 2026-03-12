#include "rpi.h"
#include "ip.h"

/*
expected output:
    IP TEST START
    ip_checksum: 0x0000 (valid header → 0)
    ip_parse: 0
    IP: ver=4 ihl=5 len=40 proto=1 ttl=64 src=10.0.0.1 dst=10.0.0.2
    payload_len=20
    ip_build: 40
    rebuild checksum valid: 1
    round-trip src match: 1
    round-trip dst match: 1
    IP TEST DONE
*/

void notmain(void) {
    printk("IP TEST START\n");

    // ip packet with20-byte header + 20 bytes payload
    // src=10.0.0.1 dst=10.0.0.2 proto=ICMP ttl=64
    uint8_t raw_pkt[] = {
        0x45, 0x00, 0x00, 0x28, // ver=4, ihl=5, tos=0, total_len=40
        0x00, 0x01, 0x40, 0x00, // id=1, flags=DF, frag_offset=0
        0x40, 0x01, 0x00, 0x00, // ttl=64, proto=1(ICMP), checksum=0 (placeholder)
        0x0A, 0x00, 0x00, 0x01, // src=10.0.0.1
        0x0A, 0x00, 0x00, 0x02, // dst=10.0.0.2
        // 20 bytes of fake payload
        0x08, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x01,
        0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68,
        0x69, 0x6A, 0x6B, 0x6C,
    };

    uint16_t cksum = ip_checksum(raw_pkt, 20);
    raw_pkt[10] = (cksum >> 8) & 0xFF;
    raw_pkt[11] = cksum & 0xFF;

    // should return 0 for valid header
    uint16_t verify = ip_checksum(raw_pkt, 20);
    assert(verify == 0);
    printk("ip_checksum: 0x%x (valid header -> 0)\n", verify);

    // parse
    ip_hdr_t hdr;
    int r = ip_parse(raw_pkt, sizeof raw_pkt, &hdr);
    printk("ip_parse: %d\n", r);

    ip_print_hdr(&hdr);
    printk("payload_len=%d\n", hdr.payload_len);

    // rebuild
    uint8_t rebuilt[64];
    int n = ip_build(&hdr, hdr.payload, hdr.payload_len, rebuilt, sizeof rebuilt);
    printk("ip_build: %d\n", n);

    // verify rebuilt checksum
    uint16_t rebuilt_cksum = ip_checksum(rebuilt, 20);
    printk("rebuild checksum valid: %d\n", rebuilt_cksum == 0);

    // verify round-trip
    ip_hdr_t hdr2;
    ip_parse(rebuilt, n, &hdr2);
    printk("round-trip src match: %d\n", hdr2.src == hdr.src);
    printk("round-trip dst match: %d\n", hdr2.dst == hdr.dst);

    printk("IP TEST DONE\n");
    clean_reboot();
}
