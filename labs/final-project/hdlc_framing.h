#ifndef __HDLC_FRAMING_H__
#define __HDLC_FRAMING_H__

#include <stdint.h>

// HDLC-like framing constants per RFC 1662
#define HDLC_FLAG    0x7E  // Flag sequence
#define HDLC_ESCAPE  0x7D  // Escape sequence
#define HDLC_XOR     0x20  // XOR value for escape octet

// Maximum frame size (including FCS)
#define HDLC_MAX_FRAME_SIZE  2048
#define HDLC_MAX_PAYLOAD_SIZE (HDLC_MAX_FRAME_SIZE - 6)  // Account for flags, FCS, escaping

// Return codes
#define HDLC_OK           0
#define HDLC_ERROR       -1
#define HDLC_FCS_ERROR   -2
#define HDLC_TOO_LARGE   -3
#define HDLC_INCOMPLETE  -4

/**
 * Calculate CRC-16-CCITT (X.25) for FCS
 * This is the standard FCS used in RFC 1662
 * 
 * @param data: pointer to data buffer
 * @param len: length of data
 * @return: 16-bit CRC value
 */
uint16_t hdlc_crc16(const uint8_t *data, unsigned len);

/**
 * Encode a frame with HDLC-like framing (RFC 1662)
 * 
 * Format: [FLAG] [escaped payload] [escaped FCS] [FLAG]
 * 
 * @param payload: input data to frame
 * @param payload_len: length of payload
 * @param frame: output buffer for framed data
 * @param frame_size: size of frame buffer
 * @return: length of encoded frame on success, negative on error
 */
int hdlc_encode(const uint8_t *payload, unsigned payload_len,
                uint8_t *frame, unsigned frame_size);

/**
 * Decode an HDLC-like frame (RFC 1662)
 * 
 * @param frame: input framed data
 * @param frame_len: length of frame data
 * @param payload: output buffer for decoded payload
 * @param payload_size: size of payload buffer
 * @return: length of decoded payload on success, negative on error
 */
int hdlc_decode(const uint8_t *frame, unsigned frame_len,
                uint8_t *payload, unsigned payload_size);

/**
 * Send a framed packet over UART
 * 
 * @param payload: data to send
 * @param payload_len: length of data
 * @return: 0 on success, negative on error
 */
int hdlc_send(const uint8_t *payload, unsigned payload_len);

/**
 * Receive a framed packet from UART
 * 
 * @param payload: buffer to store received data
 * @param payload_size: size of payload buffer
 * @return: length of received payload on success, negative on error
 */
int hdlc_recv(uint8_t *payload, unsigned payload_size);

#endif // __HDLC_FRAMING_H__
