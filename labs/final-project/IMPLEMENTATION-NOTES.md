# Phase 1 Implementation Notes

## Files Created

1. **`net/pbuf.h`** - Packet buffer pool interface
   - Fixed-size buffers with headroom
   - `pbuf_push()` / `pbuf_pull()` for zero-copy header manipulation
   - Pool of 32 buffers

2. **`net/pbuf.c`** - Packet buffer pool implementation
   - Simple allocator from pre-allocated pool
   - Reference counting

3. **`net/nrf_frag.h`** - Fragmentation header and types
   - 4-byte fragment header structure
   - Reassembly slot structure
   - Fragmentation context

4. **`net/nrf_frag.c`** - Fragmentation/reassembly implementation
   - `nrf_frag_tx()` - Fragments pbuf into 32-byte NRF packets
   - `nrf_frag_rx()` - Receives fragments and reassembles into pbuf
   - Timeout handling for incomplete reassemblies

5. **`tests/10_frag_local.c`** - Test program
   - Tests fragmentation/reassembly with different message sizes
   - Verifies CRC32 after reassembly

## Key Implementation Details

### Fragment Header
- 4 bytes: `msg_id`, `frag_seq`, `total_frags`, `payload_len`
- Leaves 28 bytes for payload per 32-byte NRF packet
- Max message: 64 fragments × 28 bytes = 1792 bytes

### Reassembly
- 4 concurrent reassembly slots
- Bitmask tracking (8 bytes = 64 bits) for received fragments
- 2-second timeout for incomplete reassemblies
- Duplicate fragment detection

### Zero-Copy Design
- TX: Reads slices from pbuf directly (no full message copy)
- RX: Assembles fragments into pbuf (only copy is from NRF RX queue)
- Headers: Manipulated via pointer arithmetic (`pbuf_push`/`pbuf_pull`)

### Known Limitations
- Fragment TX must copy into 32-byte NRF buffer (hardware requirement)
- This is acceptable - we're not copying the full message multiple times
- True zero-copy verification will come in Phase 5

## Testing

Run the test:
```bash
cd labs/final-project
make run
```

The test will:
1. Send 100, 500, and 1500 byte messages
2. Fragment them into 32-byte NRF packets
3. Reassemble on receiving side
4. Verify CRC32 matches

## Dependencies

- Uses NRF driver from `labs/14-nrf-networking/code/`
- Requires staff NRF driver (or your implementation)
- Uses `timeout.h` from `libpi/include/`

## Next Steps (Phase 2)

- Add Ethernet framing (`net/eth.h`)
- Add ARP responder (`net/arp.h/c`)
- Create Linux TAP bridge (`relay/tap_nrf_bridge.c`)


# Why this is necessary
- Remember: Ethernet/IP packets are ~1500 bytes

NRF can only send 32 bytes at a time
- The pbuf system ensures: 
You store full packets in one place, 
You pass pointers, not copies, 
You have space to prepend headers, 
You can reuse memory safely, 
This becomes the backbone of zero-copy