#include "rpi.h"
#include "udp.h"
#include "net_util.h"

/*
expected output:
    UDP TEST START
    udp_checksum verify: 0x0
    udp_parse: 0
    src_port=1234 dst_port=9000 len=13 payload_len=5
    payload matches: 1
    corrupted checksum nonzero: 1
    short packet parse: -4
    bad len parse: -4
    tiny len parse: -2
    UDP TEST DONE
*/

void notmain(void) {
    printk("UDP TEST START\n");

    uint32_t src_ip = 0x0A000001; // 10.0.0.1
    uint32_t dst_ip = 0x0A000002; // 10.0.0.2

    uint8_t raw_udp[] = {
        0x00, 0x00, 0x00, 0x00, // src/dst port
        0x00, 0x00, 0x00, 0x00, // len/checksum
        'h', 'e', 'l', 'l', 'o'
    };

    put_be16(&raw_udp[0], 1234);
    put_be16(&raw_udp[2], UDP_ECHO_PORT);
    put_be16(&raw_udp[4], sizeof raw_udp);

    uint16_t cksum = udp_checksum(src_ip, dst_ip, raw_udp, sizeof raw_udp);
    if (cksum == 0)
        cksum = 0xFFFF;
    put_be16(&raw_udp[6], cksum);

    uint16_t verify = udp_checksum(src_ip, dst_ip, raw_udp, sizeof raw_udp);
    printk("udp_checksum verify: 0x%x\n", verify);
    assert(verify == 0);

    udp_hdr_t hdr;
    int r = udp_parse(raw_udp, sizeof raw_udp, &hdr);
    printk("udp_parse: %d\n", r);
    assert(r == UDP_OK);

    printk("src_port=%d dst_port=%d len=%d payload_len=%d\n",
           hdr.src_port, hdr.dst_port, hdr.len, hdr.payload_len);
    assert(hdr.src_port == 1234);
    assert(hdr.dst_port == UDP_ECHO_PORT);
    assert(hdr.len == sizeof raw_udp);
    assert(hdr.payload_len == 5);

    int payload_ok = hdr.payload[0] == 'h' &&
                     hdr.payload[1] == 'e' &&
                     hdr.payload[2] == 'l' &&
                     hdr.payload[3] == 'l' &&
                     hdr.payload[4] == 'o';
    printk("payload matches: %d\n", payload_ok);
    assert(payload_ok);

    uint8_t corrupted[sizeof raw_udp];
    for (unsigned i = 0; i < sizeof raw_udp; i++)
        corrupted[i] = raw_udp[i];
    corrupted[UDP_HDR_LEN] ^= 0x01;

    uint16_t corrupted_verify = udp_checksum(src_ip, dst_ip, corrupted, sizeof corrupted);
    printk("corrupted checksum nonzero: %d\n", corrupted_verify != 0);
    assert(corrupted_verify != 0);

    udp_hdr_t hdr2;
    int short_r = udp_parse(raw_udp, UDP_HDR_LEN - 1, &hdr2);
    printk("short packet parse: %d\n", short_r);
    assert(short_r == UDP_WRONG_SIZE);

    uint8_t wrong_len[sizeof raw_udp];
    for (unsigned i = 0; i < sizeof raw_udp; i++)
        wrong_len[i] = raw_udp[i];
    put_be16(&wrong_len[4], sizeof wrong_len + 1);
    int wrong_len_r = udp_parse(wrong_len, sizeof wrong_len, &hdr2);
    printk("bad len parse: %d\n", wrong_len_r);
    assert(wrong_len_r == UDP_WRONG_SIZE);

    uint8_t tiny_len[sizeof raw_udp];
    for (unsigned i = 0; i < sizeof raw_udp; i++)
        tiny_len[i] = raw_udp[i];
    put_be16(&tiny_len[4], UDP_HDR_LEN - 1);
    int tiny_len_r = udp_parse(tiny_len, sizeof tiny_len, &hdr2);
    printk("tiny len parse: %d\n", tiny_len_r);
    assert(tiny_len_r == UDP_BAD_HDR);

    printk("UDP TEST DONE\n");
    clean_reboot();
}
