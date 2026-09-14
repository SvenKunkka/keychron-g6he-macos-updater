/*
 * g6he_protocol.h - HID firmware-update frame codec and device model.
 *
 * BR-008, BR-009, BR-010, BR-012.
 *
 * Evidence:
 *   - E-004/FACT: 33-byte reports, output report ID 0xB2, input 0xB1, frame
 *     header B2 AA (55|56) length ~length sequence, payload checksum is the
 *     little-endian 16-bit sum of payload bytes; responses echo sequence and
 *     command.
 *   - E-005/FACT: commands 0x60 identity, 0x61 capabilities, 0x62 select mode,
 *     0x63 start, 0x64 16-byte chunks, 0x65 CRC, 0x66 reset, 0x67 bootloader
 *     switch (only for devices that require it). Normal responses use A3,
 *     update-frame responses use A1.
 *   - tests/test_firmware_parser.py captured the exact query/update frames and
 *     a two-report 0x60 response.
 *
 * The device-side action for each command is INFERENCE from the host contract
 * plus the captured responses. It is a behavioural model, not the vendor
 * implementation. Status codes other than 0 are INFERENCE; the decompiled
 * configuration protocol uses 8 (bad parameter) and 9 (write/CRC failure), and
 * those values are reused here as reconstructed labels.
 */
#ifndef G6HE_PROTOCOL_H
#define G6HE_PROTOCOL_H

#include "g6he_common.h"
#include "g6he_identity.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_REPORT_SIZE 33U
#define G6HE_REPORT_ID_OUT 0xB2U
#define G6HE_REPORT_ID_IN 0xB1U
#define G6HE_FRAME_MARKER 0xAAU
#define G6HE_FRAME_KIND_NORMAL 0x55U
#define G6HE_FRAME_KIND_UPDATE 0x56U
#define G6HE_RESP_NORMAL 0xA3U
#define G6HE_RESP_UPDATE 0xA1U

#define G6HE_MAX_REQUEST_PAYLOAD (G6HE_REPORT_SIZE - 8U) /* 25 */
#define G6HE_MAX_RESPONSE_DATA 256U
#define G6HE_MAX_RESPONSE_PAYLOAD (4U + G6HE_MAX_RESPONSE_DATA)
#define G6HE_CHUNK_SIZE 16U
#define G6HE_MAX_RESPONSE_REPORTS 16U
#define G6HE_MAX_RESPONSE_BYTES (G6HE_MAX_RESPONSE_REPORTS * G6HE_REPORT_SIZE)

#define G6HE_CMD_IDENTITY 0x60U
#define G6HE_CMD_CAPABILITIES 0x61U
#define G6HE_CMD_SELECT_MODE 0x62U
#define G6HE_CMD_START 0x63U
#define G6HE_CMD_TRANSFER 0x64U
#define G6HE_CMD_CHECK_CRC 0x65U
#define G6HE_CMD_RESET 0x66U
#define G6HE_CMD_BOOTLOADER_SWITCH 0x67U

#define G6HE_PROTO_STATUS_OK 0x00U
#define G6HE_PROTO_STATUS_BAD_COMMAND 0x08U /* INFERENCE */
#define G6HE_PROTO_STATUS_BAD_PARAM 0x08U   /* INFERENCE */
#define G6HE_PROTO_STATUS_CRC 0x09U         /* INFERENCE */
#define G6HE_PROTO_STATUS_STATE 0x0AU       /* INFERENCE */

typedef struct {
    uint8_t sequence;
    uint8_t payload[G6HE_MAX_REQUEST_PAYLOAD];
    size_t payload_len;
    bool update_frame;
} g6he_request_t;

typedef struct {
    uint8_t type;
    uint8_t sequence;
    uint8_t command;
    uint8_t status;
    uint8_t data[G6HE_MAX_RESPONSE_DATA];
    size_t data_len;
} g6he_response_t;

/* Build one request report. Returns the number of bytes written (33) or 0. */
size_t g6he_frame_build(uint8_t out[G6HE_REPORT_SIZE], uint8_t sequence,
                        const uint8_t *payload, size_t payload_len,
                        bool update_frame);

/* Parse a single request report. */
g6he_status_t g6he_frame_parse(const uint8_t report[G6HE_REPORT_SIZE],
                               g6he_request_t *out);

/* Encode a response as one or more reports. *out_bytes is set to 33*n. */
g6he_status_t g6he_response_encode(const g6he_response_t *response,
                                   bool update_frame, uint8_t *out,
                                   size_t out_cap, size_t *out_bytes);

/* Reassemble and validate a response from one or more reports. */
g6he_status_t g6he_response_decode(const uint8_t *reports, size_t reports_len,
                                   g6he_response_t *out);

typedef enum {
    G6HE_UPDATER_IDLE = 0,
    G6HE_UPDATER_MODE_SELECTED = 1,
    G6HE_UPDATER_UPDATING = 2
} g6he_updater_state_t;

typedef struct {
    g6he_updater_state_t state;
    g6he_identity_t identity;
    g6he_capabilities_t capabilities;
    uint32_t running_crc;
    uint32_t bytes_received;
    uint8_t selected_mode;
    bool reset_requested;
} g6he_updater_t;

void g6he_updater_init(g6he_updater_t *updater,
                       const g6he_identity_t *identity,
                       const g6he_capabilities_t *capabilities);

/*
 * Handle one parsed request and produce a response. Returns G6HE_OK when a
 * response was produced (including protocol-level rejections), or an error for
 * malformed input. Protocol-level rejections set response->status != 0.
 */
g6he_status_t g6he_updater_handle(g6he_updater_t *updater,
                                  const g6he_request_t *request,
                                  g6he_response_t *response);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_PROTOCOL_H */
