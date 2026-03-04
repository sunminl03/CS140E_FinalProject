# Zero-Copy IPv4 Stack over NRF24L01P - Phase 1

## Phase 1: pbuf + Fragmentation/Reassembly

This phase implements:
1. **Packet Buffer Pool** (`net/pbuf.h/c`) - Zero-copy buffer management
2. **Fragmentation Layer** (`net/nrf_frag.h/c`) - Fragment large messages into 32-byte NRF packets
3. **Test** (`tests/10_frag_local.c`) - Local fragmentation/reassembly test

## Building

```bash
cd labs/final-project
make
```

## Running Tests

```bash
make run
# or
make check
```

The test `10_frag_local.c` will:
- Send 100, 500, and 1500 byte messages
- Fragment them into 32-byte NRF packets
- Reassemble on the receiving side
- Verify CRC32 matches

## File Structure

```
final-project/
├── net/
│   ├── pbuf.h          # Packet buffer pool (header-only interface)
│   ├── pbuf.c          # Packet buffer pool implementation
│   ├── nrf_frag.h      # Fragmentation header and types
│   └── nrf_frag.c      # Fragmentation/reassembly implementation
├── tests/
│   └── 10_frag_local.c # Local fragmentation test
├── Makefile
└── README.md
```

## Key Design

### Packet Buffer Pool (pbuf)
- Fixed-size buffers (1600 bytes data + 64 bytes headroom)
- `pbuf_push()` - Prepend header by moving data pointer backward (zero-copy!)
- `pbuf_pull()` - Consume header by moving data pointer forward (zero-copy!)
- Pool of 32 buffers, no dynamic allocation per packet

### Fragmentation
- 4-byte header: `msg_id`, `frag_seq`, `total_frags`, `payload_len`
- 28 bytes payload per 32-byte NRF packet
- Max message: 64 fragments × 28 bytes = 1792 bytes
- Reassembly slots with timeout (2 seconds)

### Zero-Copy
- TX: Reads slices from pbuf without copying payload
- RX: Assembles fragments into pbuf (only copy here is from NRF RX queue)
- Headers prepended/consumed via pointer manipulation

## Next Steps

Phase 2 will add:
- Ethernet framing
- ARP responder
- Linux TAP bridge
