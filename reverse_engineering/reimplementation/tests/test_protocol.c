/*
 * test_protocol.c - BR-008, BR-009, BR-010, BR-012.
 *
 * Frame vectors are the exact bytes captured in
 * tests/test_firmware_parser.py; the response vector is the captured
 * two-report 0x60 reply from a G6 HE running 1.0.0+82.
 */
#include "g6he_test.h"

#include "g6he_crc.h"
#include "g6he_protocol.h"

static const uint8_t captured_identity[G6HE_IDENTITY_DATA_SIZE] = {
    0x35, 0x34, 0x4c, 0x4d, 0x47, 0x36, 0x48, 0x45, 0x00, 0x00,
    0x05, 0x03,
    0x31, 0x2e, 0x30, 0x2e, 0x30, 0x2b, 0x38, 0x32, 0x00, 0x00,
    0x35, 0x34, 0x4c, 0x4d, 0x76, 0x31, 0x2e, 0x30, 0x00, 0x00,
    0x01, 0x00};

static const uint8_t captured_response[] = {
    0xb1, 0xaa, 0x55, 0x28, 0xd7, 0x01, 0xa3, 0x01, 0x60, 0x00, 0x35,
    0x34, 0x4c, 0x4d, 0x47, 0x36, 0x48, 0x45, 0x00, 0x00, 0x05, 0x03,
    0x31, 0x2e, 0x30, 0x2e, 0x30, 0x2b, 0x38, 0x32, 0x00, 0x00, 0x35,
    0xb1, 0x34, 0x4c, 0x4d, 0x76, 0x31, 0x2e, 0x30, 0x00, 0x00, 0x01,
    0x00, 0xa2, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

static g6he_status_t roundtrip(uint8_t sequence, const uint8_t *payload,
                               size_t payload_len, bool update_frame,
                               g6he_request_t *request)
{
    uint8_t frame[G6HE_REPORT_SIZE];
    if (g6he_frame_build(frame, sequence, payload, payload_len, update_frame) !=
        G6HE_REPORT_SIZE) {
        return G6HE_ERR_ARG;
    }
    return g6he_frame_parse(frame, request);
}

static void init_updater(g6he_updater_t *updater)
{
    g6he_identity_t identity;
    g6he_capabilities_t caps;

    CHECK_EQ_INT(g6he_identity_decode(captured_identity,
                                      sizeof(captured_identity), &identity),
                 G6HE_OK);
    caps.protocol_version = 1u;
    caps.dfu_version = 0u;
    caps.supported_update_modes = 0x01u;
    caps.bootloader_required = 0u;
    g6he_updater_init(updater, &identity, &caps);
}

void test_protocol(void)
{
    uint8_t frame[G6HE_REPORT_SIZE];
    g6he_request_t request;
    g6he_response_t response;
    g6he_updater_t updater;

    printf("[protocol]\n");

    /* Captured request frames. */
    CHECK_EQ_INT(g6he_frame_build(frame, 1u, (const uint8_t *)"\x60", 1u,
                                  false),
                 G6HE_REPORT_SIZE);
    CHECK_EQ_INT(frame[0], 0xB2);
    CHECK_EQ_INT(frame[1], 0xAA);
    CHECK_EQ_INT(frame[2], 0x55);
    CHECK_EQ_INT(frame[3], 0x03);
    CHECK_EQ_INT(frame[4], 0xFC);
    CHECK_EQ_INT(frame[5], 0x01);
    CHECK_EQ_INT(frame[6], 0x60);
    CHECK_EQ_INT(frame[7], 0x60);
    CHECK_EQ_INT(frame[8], 0x00);
    {
        unsigned i;
        for (i = 9; i < G6HE_REPORT_SIZE; ++i) {
            CHECK_EQ_INT(frame[i], 0);
        }
    }
    CHECK_EQ_INT(g6he_frame_build(frame, 4u, (const uint8_t *)"\x63", 1u,
                                  true),
                 G6HE_REPORT_SIZE);
    CHECK_EQ_INT(frame[2], 0x56);
    CHECK_EQ_INT(frame[3], 0x03);
    CHECK_EQ_INT(frame[4], 0xFC);
    CHECK_EQ_INT(frame[6], 0x63);
    CHECK_EQ_INT(frame[7], 0x63);

    CHECK_EQ_INT(roundtrip(7u, (const uint8_t *)"\x62\x00", 2u, false,
                           &request),
                 G6HE_OK);
    CHECK_EQ_INT(request.sequence, 7);
    CHECK_EQ_INT(request.payload_len, 2);
    CHECK(request.update_frame == false);

    /* Captured two-report response. */
    CHECK_EQ_INT(g6he_response_decode(captured_response,
                                      sizeof(captured_response), &response),
                 G6HE_OK);
    CHECK_EQ_INT(response.type, G6HE_RESP_NORMAL);
    CHECK_EQ_INT(response.sequence, 1);
    CHECK_EQ_INT(response.command, G6HE_CMD_IDENTITY);
    CHECK_EQ_INT(response.status, 0);
    CHECK_EQ_INT(response.data_len, G6HE_IDENTITY_DATA_SIZE);
    CHECK_MEM_EQ(response.data, captured_identity, G6HE_IDENTITY_DATA_SIZE);

    /* Device model: identity and capabilities. */
    init_updater(&updater);
    CHECK_EQ_INT(roundtrip(1u, (const uint8_t *)"\x60", 1u, false, &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.type, G6HE_RESP_NORMAL);
    CHECK_EQ_INT(response.command, G6HE_CMD_IDENTITY);
    CHECK_EQ_INT(response.status, 0);
    CHECK_EQ_INT(response.data_len, G6HE_IDENTITY_DATA_SIZE);
    CHECK_MEM_EQ(response.data, captured_identity, G6HE_IDENTITY_DATA_SIZE);

    /* Response encode/decode round trip across multiple reports. */
    {
        uint8_t reports[G6HE_MAX_RESPONSE_BYTES];
        size_t bytes = 0;
        g6he_response_t decoded;
        CHECK_EQ_INT(g6he_response_encode(&response, false, reports,
                                          sizeof(reports), &bytes),
                     G6HE_OK);
        CHECK_EQ_INT(bytes, 2u * G6HE_REPORT_SIZE);
        CHECK_EQ_INT(g6he_response_decode(reports, bytes, &decoded), G6HE_OK);
        CHECK_EQ_INT(decoded.data_len, G6HE_IDENTITY_DATA_SIZE);
        CHECK_MEM_EQ(decoded.data, captured_identity, G6HE_IDENTITY_DATA_SIZE);
    }

    CHECK_EQ_INT(roundtrip(2u, (const uint8_t *)"\x61", 1u, false, &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.command, G6HE_CMD_CAPABILITIES);
    CHECK_EQ_INT(response.data_len, G6HE_CAPABILITY_DATA_SIZE);
    CHECK_EQ_INT(response.data[0], 1);
    CHECK_EQ_INT(response.data[1], 0);
    CHECK_EQ_INT(response.data[2], 0x01);
    CHECK_EQ_INT(response.data[3], 0);

    /* Transfer before start is rejected by state. */
    {
        uint8_t payload[1 + G6HE_CHUNK_SIZE];
        payload[0] = G6HE_CMD_TRANSFER;
        memset(payload + 1, 0xAB, G6HE_CHUNK_SIZE);
        CHECK_EQ_INT(roundtrip(3u, payload, sizeof(payload), true, &request),
                     G6HE_OK);
        CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response),
                     G6HE_OK);
        CHECK_EQ_INT(response.status, G6HE_PROTO_STATUS_STATE);
    }

    /* Select mode then start. */
    CHECK_EQ_INT(roundtrip(4u, (const uint8_t *)"\x62\x00", 2u, false,
                           &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.status, 0);
    CHECK_EQ_INT(updater.state, G6HE_UPDATER_MODE_SELECTED);

    CHECK_EQ_INT(roundtrip(5u, (const uint8_t *)"\x63", 1u, true, &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.type, G6HE_RESP_UPDATE);
    CHECK_EQ_INT(response.status, 0);
    CHECK_EQ_INT(updater.state, G6HE_UPDATER_UPDATING);

    /* Transfer a 16-byte chunk, then a matching CRC check. */
    {
        uint8_t payload[1 + G6HE_CHUNK_SIZE];
        uint8_t crc_payload[9];
        uint32_t running;
        unsigned i;
        payload[0] = G6HE_CMD_TRANSFER;
        for (i = 0; i < G6HE_CHUNK_SIZE; ++i) {
            payload[1 + i] = (uint8_t)(i + 1u);
        }
        CHECK_EQ_INT(roundtrip(6u, payload, sizeof(payload), true, &request),
                     G6HE_OK);
        CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response),
                     G6HE_OK);
        CHECK_EQ_INT(response.status, 0);
        CHECK_EQ_INT(updater.bytes_received, G6HE_CHUNK_SIZE);

        running = updater.running_crc;
        crc_payload[0] = G6HE_CMD_CHECK_CRC;
        crc_payload[1] = (uint8_t)(running & 0xFFu);
        crc_payload[2] = (uint8_t)((running >> 8) & 0xFFu);
        crc_payload[3] = (uint8_t)((running >> 16) & 0xFFu);
        crc_payload[4] = (uint8_t)((running >> 24) & 0xFFu);
        memcpy(crc_payload + 5, crc_payload + 1, 4u);
        CHECK_EQ_INT(roundtrip(7u, crc_payload, sizeof(crc_payload), false,
                               &request),
                     G6HE_OK);
        CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response),
                     G6HE_OK);
        CHECK_EQ_INT(response.status, 0);

        crc_payload[5] ^= 0xFFu;
        CHECK_EQ_INT(roundtrip(8u, crc_payload, sizeof(crc_payload), false,
                               &request),
                     G6HE_OK);
        CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response),
                     G6HE_OK);
        CHECK_EQ_INT(response.status, G6HE_PROTO_STATUS_CRC);
    }

    /* Reset returns to idle. */
    CHECK_EQ_INT(roundtrip(9u, (const uint8_t *)"\x66", 1u, false, &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.status, 0);
    CHECK_EQ_INT(updater.state, G6HE_UPDATER_IDLE);
    CHECK(updater.reset_requested);

    /* Bootloader switch and unknown commands are rejected. */
    CHECK_EQ_INT(roundtrip(10u, (const uint8_t *)"\x67", 1u, false, &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.status, G6HE_PROTO_STATUS_BAD_COMMAND);

    CHECK_EQ_INT(roundtrip(11u, (const uint8_t *)"\x99", 1u, false, &request),
                 G6HE_OK);
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response), G6HE_OK);
    CHECK_EQ_INT(response.status, G6HE_PROTO_STATUS_BAD_COMMAND);

    /* Empty request payload is malformed. */
    memset(&request, 0, sizeof(request));
    request.sequence = 12u;
    CHECK_EQ_INT(g6he_updater_handle(&updater, &request, &response),
                 G6HE_ERR_COMMAND);

    /* Bad checksum and bad report ID fail closed. */
    {
        uint8_t bad[G6HE_REPORT_SIZE];
        g6he_frame_build(bad, 1u, (const uint8_t *)"\x60", 1u, false);
        bad[7] ^= 0xFFu;
        CHECK_EQ_INT(g6he_frame_parse(bad, &request), G6HE_ERR_CHECKSUM);
        g6he_frame_build(bad, 1u, (const uint8_t *)"\x60", 1u, false);
        bad[0] = 0x00;
        CHECK_EQ_INT(g6he_frame_parse(bad, &request), G6HE_ERR_MAGIC);
    }
}
