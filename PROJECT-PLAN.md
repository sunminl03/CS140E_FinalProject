# Zero-Copy IPv4 Stack over NRF24L01P - Implementation Plan

## System Architecture

**Two-Pi Setup:**
- **Bare-Metal Pi Zero W**: Runs IPv4 stack, responds to ARP/ping/UDP
- **Linux Relay Pi**: Hosts TAP interface, bridges Ethernet frames to/from NRF

```
Relay Pi (Linux)
  tap0 (192.168.7.1/24)
        |
  tap-nrf-bridge (userspace)
        |
     SPI + NRF #2
        ~~~ RF ~~~
     NRF #1 + SPI
        |
Bare-metal Pi Zero W
  Ethernet/ARP/IPv4/ICMP/UDP stack
  (Pi IP: 192.168.7.2/24)
```

**Key Configuration:**
- Relay Pi (tap0): `192.168.7.1/24`
- Bare-metal Pi: `192.168.7.2/24`
- Pi MAC: `02:00:00:00:00:02` (locally administered)
- MTU: 1500 bytes
- NRF payload: 32 bytes fixed (28 bytes after 4-byte fragment header)

## Using Existing Files

### Core Infrastructure (Already Available)

#### 1. **NRF24L01 Driver** (`labs/14-nrf-networking/code/`)
- **`nrf.h`** - Main NRF interface:
  - `nrf_t` structure (NIC abstraction)
  - `nrf_init()` - Initialize NRF hardware
  - `nrf_tx_send_ack()` / `nrf_tx_send_noack()` - Send 32-byte packets
  - `nrf_get_pkts()` - Pull packets from hardware RX FIFO
  - `nrf_t` has `recvq` (circular queue) for received data
  - Statistics tracking (sent/recv bytes, retransmissions, loss)

- **`nrf-driver.c`** - Your implementation (or staff version)
  - Handles NRF state machine (RX/TX transitions)
  - SPI communication with NRF chip
  - ACK/no-ACK packet handling

- **`nrf-hw-support.h/c`** - Low-level NRF register access
  - `nrf_get8()`, `nrf_put8()`, `nrf_getn()`, `nrf_putn()` - SPI read/write
  - `nrf_set_addr()` - Set multi-byte addresses
  - `nrf_dump()` - Debug: print NRF configuration

- **`nrf-public.c`** - Higher-level convenience functions
  - `nrf_read_exact()` - Blocking read
  - `nrf_read_exact_timeout()` - Timeout-based read
  - `nrf_nbytes_avail()` - Check available bytes

#### 2. **Memory Management** (`libpi/include/rpi.h`)
- **`kmalloc()`** - Simple bump-pointer allocator (no free)
- **`kmalloc_aligned()`** - Aligned allocation (for DMA)
- **`kmalloc_init()`** - Initialize heap
- **Note**: For zero-copy, you'll create a custom buffer pool

#### 3. **Circular Queue** (`libpi/libc/circular.h`)
- **`cq_t`** - Lock-free circular queue structure
- **`cq_init()`** - Initialize queue
- **`cq_push_n()`** - Push N bytes
- **`cq_pop_n()`** - Pop N bytes
- **`cq_nelem()`** - Number of elements
- **Used by**: NRF driver's `recvq` for received packets

#### 4. **Timing** (`libpi/include/timeout.h`, `rpi.h`)
- **`timer_get_usec()`** - Get microseconds since boot
- **`timeout_t`** - Timeout tracking structure
- **`timeout_start()`** - Start timeout
- **`timeout_usec()`** - Check if timeout expired
- **Used for**: Retransmission timers, reassembly timeouts

#### 5. **Threading** (`labs/5-threads/code-threads/`)
- **`rpi_fork()`** - Create thread
- **`rpi_yield()`** - Yield to other threads
- **`rpi_wait()`** - Wait/yield (used in blocking operations)
- **Used for**: Concurrent packet processing (optional but useful)

#### 6. **Utilities** (`libpi/include/rpi.h`)
- **`memcpy()`** - Memory copy (avoid for zero-copy!)
- **`memset()`** - Memory set
- **`PUT32()` / `GET32()`** - Direct memory access
- **`panic()`** - Fatal error
- **`output()`** - Print debugging

---

## File Organization

**Bare-Metal Code** (`labs/14-nrf-networking/code/`):
```
net/
    pbuf.h              -- packet buffer pool + push/pull ops (header-only)
    nrf_frag.h          -- fragment header + constants
    nrf_frag.c          -- fragmentation TX + reassembly RX (into pbuf)
    eth.h               -- Ethernet parse/build helpers (inline)
    arp.h / arp.c       -- ARP responder (in-place replies)
    ipv4.h / ipv4.c     -- IPv4 parse + header checksum + dispatch
    icmp.h / icmp.c     -- ICMP echo (ping) reply (in-place)
    udp.h / udp.c       -- UDP echo server (port 9000)
    net.h               -- master include + config (PI_MAC, PI_IP, MTU)
    net_stats.h         -- counters + zero-copy instrumentation

tests/
    10_frag_local.c     -- local frag/reasm test (no network stack)
    11_arp_ping.c       -- ARP + ICMP on bare-metal
    12_udp_echo.c       -- UDP echo on bare-metal
```

**Linux Relay Code** (separate folder/repo):
```
relay/
    tap_nrf_bridge.c    -- TAP <-> NRF bridge (Linux)
    Makefile
    scripts/setup_tap.sh
```

---

## Implementation Phases

### **Phase 1: pbuf + Fragmentation/Reassembly**

**Goal**: Build packet buffer pool and fragmentation layer

**What You're Building**:

1. **Packet Buffer Pool** (`net/pbuf.h`):
   ```c
   enum {
       PBUF_HEADROOM = 64,      // ETH(14)+IP(20)+UDP(8)+slack
       PBUF_DATA_SZ  = 1600,    // >= MTU + ETH header
       PBUF_POOL_N   = 32,
   };
   
   typedef struct pbuf {
       uint8_t  raw[PBUF_HEADROOM + PBUF_DATA_SZ];
       uint8_t *data;     // start of valid data (points into raw)
       unsigned len;      // length of valid data
       unsigned refcnt;   // 0=free, 1=in use
   } pbuf_t;
   
   // Operations:
   pbuf_t* pbuf_alloc(void);
   void pbuf_free(pbuf_t *p);
   void pbuf_push(pbuf_t *p, unsigned hdr_sz);  // prepend header
   void pbuf_pull(pbuf_t *p, unsigned hdr_sz);   // consume header
   ```

2. **Fragment Header** (`net/nrf_frag.h`):
   ```c
   typedef struct __attribute__((packed)) {
       uint8_t msg_id;       // rolling message ID (0-255)
       uint8_t frag_seq;     // fragment index within message
       uint8_t total_frags;  // total fragments (<= 64)
       uint8_t payload_len;  // payload bytes in THIS fragment (1..28)
   } nrf_frag_hdr_t;
   ```
   - 4-byte header → 28 bytes payload per NRF packet
   - Max message: 64 fragments × 28 bytes = 1792 bytes (sufficient for 1500-byte MTU)

3. **Reassembly State** (`net/nrf_frag.c`):
   ```c
   typedef struct {
       uint8_t  msg_id, total_frags;
       uint8_t  received_mask[8];  // bitmask up to 64 fragments
       unsigned frags_received;
       uint32_t start_usec;        // timeout
       pbuf_t  *pbuf;
   } reasm_slot_t;
   ```
   - Maintain 4 concurrent reassembly slots
   - Timeout incomplete frames (REASM_TIMEOUT_USEC)

4. **Fragmentation Functions** (`net/nrf_frag.c`):
   - `nrf_frag_tx(pbuf_t *p, uint32_t txaddr)`: Fragment pbuf into 32-byte NRF packets
   - `nrf_frag_rx(nrf_t *nrf)`: Receive fragments, reassemble into pbuf
   - Zero-copy: TX reads slices from pbuf without copying payload

**Files to Create**:
- `net/pbuf.h` - Packet buffer pool (header-only)
- `net/nrf_frag.h` - Fragment header definition
- `net/nrf_frag.c` - Fragmentation/reassembly implementation

**Using Existing Code**:
- `nrf_tx_send_ack()` - Send each 32-byte fragment
- `nrf_get_pkts()` - Receive fragments from NRF
- `timeout_t` / `timer_get_usec()` - Track reassembly timeouts
- `kmalloc()` - Only for initial pbuf pool allocation

**Testing** (`tests/10_frag_local.c`):
- Send 200-byte message → fragments into ~8 chunks
- Reassemble and verify CRC32 matches
- Test with packet loss scenarios
- Verify timeout handling for incomplete reassembly

---

### **Phase 2: Ethernet + ARP (Bare-Metal) + TAP Bridge Skeleton (Relay)**

**Goal**: Implement Ethernet framing, ARP responder, and Linux TAP bridge

**What You're Building**:

1. **Ethernet Helpers** (`net/eth.h` - inline functions):
   ```c
   // Parse Ethernet header (14 bytes)
   typedef struct __attribute__((packed)) {
       uint8_t  dst_mac[6];
       uint8_t  src_mac[6];
       uint16_t ethertype;  // 0x0800 = IPv4, 0x0806 = ARP
   } eth_hdr_t;
   
   // Build Ethernet header in-place on pbuf
   void eth_build_hdr(pbuf_t *p, const uint8_t *dst_mac, 
                      const uint8_t *src_mac, uint16_t ethertype);
   ```

2. **ARP Responder** (`net/arp.h/c`):
   - Parse ARP packets (28 bytes)
   - If ARP request for our IP (192.168.7.2):
     - Swap src/dst MAC and IP
     - Change opcode from request (1) to reply (2)
     - Modify pbuf **in-place** (zero-copy!)
     - Send back via `eth_output()`
   - No ARP table needed (we only respond, don't initiate)

3. **Ethernet Input/Output**:
   - `eth_input(pbuf_t *p)`: Parse header, dispatch to ARP or IPv4
   - `eth_output(pbuf_t *p)`: Update L2 header, send via `nrf_frag_tx()`

4. **Linux TAP Bridge** (`relay/tap_nrf_bridge.c`):
   - Open `/dev/net/tun` with `IFF_TAP | IFF_NO_PI`
   - Configure tap0: `ip addr add 192.168.7.1/24 dev tap0`
   - Main loop:
     - Read Ethernet frame from TAP (blocking)
     - Fragment and send over NRF (using Linux NRF driver)
     - Receive fragments from NRF, reassemble
     - Write Ethernet frame back to TAP
   - Uses same fragmentation protocol as bare-metal side

**Files to Create**:
- `net/eth.h` - Ethernet helpers (inline)
- `net/arp.h/c` - ARP responder
- `relay/tap_nrf_bridge.c` - Linux TAP bridge
- `relay/scripts/setup_tap.sh` - TAP setup script

**Using Existing Code**:
- `nrf_frag_tx()` / `nrf_frag_rx()` - From Phase 1
- `pbuf_t` - All network operations use pbuf
- Linux: `socket()`, `ioctl()`, TUN/TAP device

**Testing**:
- Run `tap_nrf_bridge` on relay Pi
- From relay Pi: `arping 192.168.7.2`
- Observe ARP request on bare-metal Pi
- Verify ARP reply received on relay Pi
- `arp -a` shows Pi's MAC address

---

### **Phase 3: IPv4 + ICMP (Ping)**

**Goal**: Implement IP routing and ICMP echo (ping)

**What You're Building**:

1. **IPv4 Parser** (`net/ipv4.h/c`):
   ```c
   typedef struct __attribute__((packed)) {
       uint8_t  ver_ihl;      // version (4) + IHL (5)
       uint8_t  tos;
       uint16_t total_len;
       uint16_t id;
       uint16_t flags_frag;
       uint8_t  ttl;
       uint8_t  protocol;     // 1=ICMP, 17=UDP
       uint16_t checksum;
       uint32_t src_ip;
       uint32_t dst_ip;
   } ipv4_hdr_t;
   
   // Parse and verify IP header
   int ipv4_input(pbuf_t *p);
   // Calculate IP header checksum
   uint16_t ipv4_checksum(ipv4_hdr_t *hdr);
   ```

2. **ICMP Echo Handler** (`net/icmp.h/c`):
   - Parse ICMP header (8 bytes):
     - Type (8 = echo request, 0 = echo reply)
     - Code, checksum, identifier, sequence
   - If echo request:
     - Change type to 0 (echo reply)
     - Swap src/dst IP in IP header
     - Recalculate IP and ICMP checksums
     - Modify pbuf **in-place** (zero-copy!)
     - Send back via `eth_output()`

3. **IP Dispatch**:
   - `ipv4_input()` checks destination IP (must be 192.168.7.2)
   - Routes to `icmp_input()` if protocol = 1
   - Routes to `udp_input()` if protocol = 17

**Files to Create**:
- `net/ipv4.h/c` - IPv4 protocol
- `net/icmp.h/c` - ICMP echo handler
- `net/net.h` - Master include + config

**Using Existing Code**:
- `eth_input()` / `eth_output()` - From Phase 2
- `pbuf_t` - All operations on pbuf

**Testing** (`tests/11_arp_ping.c`):
- From relay Pi: `ping 192.168.7.2`
- Verify ICMP echo request received
- Verify ICMP echo reply sent
- Ping succeeds with stable RTT

---

### **Phase 4: UDP Echo**

**Goal**: Implement UDP echo server on port 9000

**What You're Building**:

1. **UDP Parser** (`net/udp.h/c`):
   ```c
   typedef struct __attribute__((packed)) {
       uint16_t src_port;
       uint16_t dst_port;
       uint16_t len;
       uint16_t checksum;
   } udp_hdr_t;
   
   // Parse UDP header, verify checksum
   int udp_input(pbuf_t *p);
   ```

2. **UDP Echo Handler**:
   - If `dst_port == 9000`:
     - Swap src/dst ports
     - Swap src/dst IPs in IP header
     - Recalculate IP and UDP checksums
     - Modify pbuf **in-place** (zero-copy!)
     - Send back via `eth_output()`

3. **UDP Checksum**:
   - Calculate pseudo-header (src_ip, dst_ip, protocol, UDP len)
   - Checksum UDP header + payload
   - Set to 0 if checksum disabled (optional for IPv4)

**Files to Create**:
- `net/udp.h/c` - UDP protocol and echo handler

**Using Existing Code**:
- `ipv4_input()` - Routes UDP packets (protocol = 17) to `udp_input()`
- `pbuf_t` - All operations in-place

**Testing** (`tests/12_udp_echo.c`):
- From relay Pi: `echo hello | nc -u 192.168.7.2 9000`
- Verify UDP packet received
- Verify echo reply sent with correct payload
- Test with various payload sizes

---

### **Phase 5: Zero-Copy Instrumentation + Performance**

**Goal**: Verify zero-copy architecture and measure performance

**What You're Building**:

1. **Zero-Copy Counters** (`net/net_stats.h`):
   ```c
   typedef struct {
       uint32_t pkts_rx, pkts_tx;
       uint32_t fragments_rx, fragments_tx;
       uint32_t reasm_timeouts;
       uint32_t copy_bytes;        // Should be 0 above link layer!
       uint32_t pbuf_alloc, pbuf_free;
   } net_stats_t;
   ```

2. **Instrumentation**:
   - Count `memcpy()` calls (wrap `memcpy()` to track)
   - Count `pbuf_alloc()` / `pbuf_free()` calls
   - Track fragment counts per frame
   - Measure ping RTT
   - Measure throughput with different payload sizes

3. **Verification**:
   - `copy_bytes` should be 0 for payloads (only headers copied)
   - `pbuf_alloc` should match `pbuf_free` (no leaks)
   - Fragmentation overhead: ~54 fragments for full MTU frame

**Files to Create**:
- `net/net_stats.h` - Statistics structure
- Add instrumentation to all network functions

**Using Existing Code**:
- All previous phases - add counters throughout

**Testing**:
- Run ping, verify `copy_bytes == 0` for payload
- Run UDP echo, verify zero-copy
- Measure performance:
  - Small packets (ping): few fragments, low RTT
  - Full MTU frames: ~54 fragments, higher latency
  - Throughput: limited by 2Mbps NRF rate + ACK overhead

---

## Stack Flow (Bare-Metal Pi)

**RX Path:**
```
NRF RX → nrf_frag_rx() → pbuf (reassembled Ethernet frame)
  → eth_input(pbuf)
    → if ARP: arp_input(pbuf) [reply in-place]
    → if IPv4: ipv4_input(pbuf)
      → if ICMP: icmp_input(pbuf) [reply in-place]
      → if UDP port 9000: udp_input(pbuf) [echo in-place]
  → eth_output(pbuf) → nrf_frag_tx(pbuf)
```

**TX Path:**
```
nrf_frag_tx(pbuf) → fragments pbuf->data[0..len) into NRF fragments
  → reads slices without copying payload
  → sends 32-byte fragments via nrf_tx_send_ack()
```

**Zero-Copy Invariant:**
- Payload bytes are **never copied** between layers
- Responses reuse the same `pbuf` **in-place**
- Headers prepended via `pbuf_push()` (pointer manipulation)
- Headers consumed via `pbuf_pull()` (pointer manipulation)

---

## Key Design Decisions

1. **Packet Buffer Pool**:
   - Fixed-size buffers with headroom (64 bytes)
   - `pbuf_push()` / `pbuf_pull()` for header manipulation
   - No dynamic allocation per packet
   - Reference counting for safety

2. **Fragmentation**:
   - 4-byte header: msg_id, frag_seq, total_frags, payload_len
   - 28 bytes payload per 32-byte NRF packet
   - Reassembly slots with timeout
   - Zero-copy: TX reads slices, RX assembles into pbuf

3. **In-Place Modifications**:
   - ARP reply: swap MACs/IPs, change opcode
   - ICMP echo: swap IPs, change type, recalc checksums
   - UDP echo: swap ports/IPs, recalc checksums
   - All modify `pbuf` in-place (zero-copy!)

4. **Linux Relay Bridge**:
   - Separate userspace program
   - Uses TAP interface for OS integration
   - Same fragmentation protocol as bare-metal side
   - Enables standard tools (ping, arping, nc)

## Verification Checklist

- [ ] `10_frag_local.c`: CRC32 match after reassembly
- [ ] `arping 192.168.7.2`: ARP reply received
- [ ] `ping 192.168.7.2`: stable ICMP echo replies
- [ ] `nc -u 192.168.7.2 9000`: echo back correct payload
- [ ] Stats show expected fragment counts
- [ ] Zero-copy counters show `copy_bytes == 0` for payloads

## Expected Performance

- **Small packets (ping)**: Few fragments → low tens of ms RTT
- **Full MTU frames**: ~54 fragments → higher latency
- **Throughput**: Limited by 2Mbps NRF rate + ACK overhead
- **Goal**: Stable transfer + correctness (not maximum speed)

## Development Order

1. **Phase 1**: pbuf pool + fragmentation/reassembly
2. **Phase 2**: Ethernet + ARP + TAP bridge skeleton
3. **Phase 3**: IPv4 + ICMP (ping)
4. **Phase 4**: UDP echo
5. **Phase 5**: Zero-copy instrumentation

Each phase builds incrementally and can be tested independently.

---

## Relay Pi: Linux vs Bare-Metal

### **Why Linux is Required**

The relay Pi **must run Linux** because:

1. **TAP Interface Requirement**:
   - TAP interfaces are a **Linux kernel feature**
   - Created via `/dev/net/tun` device
   - Requires kernel networking stack support
   - Bare-metal has no kernel, so no TAP support

2. **Standard Tool Integration**:
   - Goal: Use `ping`, `arping`, `nc` from relay Pi
   - These tools require full OS networking stack
   - Linux provides ARP table, routing, socket APIs
   - Bare-metal can't run these standard tools

3. **Network Stack Integration**:
   - TAP makes bare-metal Pi appear as real IP node
   - Linux handles ARP, routing, IP configuration
   - Bare-metal can't provide this integration

### **Could Relay Pi Be Bare-Metal?**

**Short answer: No, not for this architecture.**

**Alternative architectures (if you wanted bare-metal relay):**

1. **UART Bridge** (more complex):
   ```
   Host Machine (Linux/Mac)
     |
   UART/USB Serial
     |
   Bare-Metal Relay Pi
     |
   NRF #2 ←→ NRF #1 ←→ Bare-Metal Pi Zero W
   ```
   - Relay Pi forwards frames via UART
   - Host machine needs custom bridge program
   - Can't use standard tools directly
   - More complex, less useful

2. **Dual NRF Relay** (even more complex):
   - Relay Pi has two NRFs
   - Forwards between them
   - Still need host machine for TAP
   - Doesn't solve the problem

### **Recommendation: Use Linux on Relay Pi**

**Benefits:**
- ✅ Standard tools work (`ping`, `arping`, `nc`)
- ✅ TAP interface integration
- ✅ Can SSH from MacBook to relay Pi
- ✅ Easier debugging with Wireshark
- ✅ Standard Linux networking stack

**What You Need:**
- Raspberry Pi running Linux (Raspberry Pi OS)
- NRF driver for Linux (port your code or use library)
- Root/sudo access for TAP interface

**Development Workflow:**
1. Develop bare-metal stack on Pi Zero W
2. Test with Linux relay Pi
3. SSH to relay Pi from your MacBook
4. Run `ping`, `arping`, `nc` from relay Pi

This is the cleanest and most useful architecture for your project.

## Plan Validation

### ✅ **Valid Aspects**

1. **Architecture**: Two-Pi setup is clean and testable
   - Bare-metal Pi runs full stack
   - Linux relay Pi bridges to standard tools
   - No macOS TAP complexity

2. **Fragment Header Design**: 4 bytes is optimal
   - 28 bytes payload per 32-byte NRF packet
   - Max message: 64 × 28 = 1792 bytes (sufficient for 1514-byte max frame)
   - Simple and efficient

3. **pbuf Design**: Zero-copy architecture is sound
   - Fixed buffers with headroom (64 bytes)
   - `push`/`pull` operations for header manipulation
   - In-place modifications for replies

4. **In-Place Modifications**: Correct approach
   - ARP/ICMP/UDP replies modify pbuf in-place
   - No payload copying
   - Only header fields swapped and checksums recalculated

5. **Phases**: Well-structured and incremental
   - Each phase builds on previous
   - Can test independently
   - Clear success criteria

### ⚠️ **Potential Issues & Solutions**

1. **Linux NRF Driver for Relay Pi**
   - **Issue**: Relay Pi needs NRF driver (SPI + NRF protocol)
   - **Solution**: 
     - Port bare-metal NRF code to Linux (use Linux SPI)
     - Or use existing Linux NRF24L01 library (e.g., `nrf24l01-lib`)
     - Or reuse NRF driver code with Linux SPI abstraction

2. **TAP Interface Permissions**
   - **Issue**: Creating TAP interface requires root/sudo
   - **Solution**: 
     - Use `sudo` for `tap_nrf_bridge`
     - Or set capabilities: `sudo setcap cap_net_admin+ep tap_nrf_bridge`
     - Document in setup script

3. **Fragment Reassembly Timeout**
   - **Issue**: Need to choose appropriate timeout value
   - **Solution**: 
     - Start with 1-2 seconds
     - Adjust based on NRF retransmission delays
     - Make configurable

4. **Concurrent Reassembly Slots**
   - **Issue**: 4 slots might not be enough under load
   - **Solution**: 
     - Start with 4, increase if needed
     - Timeout old slots aggressively
     - Monitor stats

5. **Checksum Calculation**
   - **Issue**: IP/UDP checksums need careful implementation
   - **Solution**: 
     - Use standard one's complement checksum
     - Test with known-good packets
     - Verify with Wireshark

6. **Zero-Copy Verification**
   - **Issue**: Need to track all `memcpy()` calls
   - **Solution**: 
     - Wrap `memcpy()` with counter
     - Only headers should be copied
     - Payloads should never be copied

### ✅ **Overall Assessment**

**The plan is VALID and well-designed.** The architecture is sound, the phases are logical, and the zero-copy design is correct. The main challenges are:

1. **Implementation complexity**: Networking stacks are complex, but the incremental phases help
2. **Linux NRF driver**: Need to port or find existing library
3. **Testing**: Need both Pis set up and working

**Recommendations:**
- Start with Phase 1 and get fragmentation working end-to-end
- Test thoroughly at each phase before moving on
- Use Wireshark on relay Pi to debug protocol issues
- Add extensive logging/debugging early
- Consider adding a simple test mode that doesn't require both Pis (local loopback)

**Estimated Effort**: 4-5 labs worth of work (as originally estimated)
