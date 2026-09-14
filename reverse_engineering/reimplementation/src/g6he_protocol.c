/*
 * g6he_protocol.c - HID update frame codec and device responder model.
 *
 * BR-008, BR-009, BR-010, BR-012.
 */
#include "g6he_protocol.h"

#include "g6he_crc.h"

#include <string.h>

#define G6HE_FIRST_REPORT_STREAM 27U /* bytes 6..32 */
#define G6HE_NEXT_REPORT_STREAM 32U  /* bytes 1..32 */

static uint32_t rd_le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

static uint8_t frame_kind(bool update_frame)
{
    return update_frame ? G6HE_FRAME_KIND_UPDATE : G6HE_FRAME_KIND_NORMAL;
}

size_t g6he_frame_build(uint8_t out[G6HE_REPORT_SIZE], uint8_t sequence,
                        const uint8_t *payload, size_t payload_len,
                        bool update_frame)
{
    uint16_t checksum;

    if (out == NULL || sequence == 0u || payload == NULL) {
        return 0;
    }
    if (payload_len > G6HE_MAX_REQUEST_PAYLOAD) {
        return 0;
    }
    memset(out, 0, G6HE_REPORT_SIZE);
    out[0] = G6HE_REPORT_ID_OUT;
    out[1] = G6HE_FRAME_MARKER;
    out[2] = frame_kind(update_frame);
    out[3] = (uint8_t)(payload_len + 2u);
    out[4] = (uint8_t)(~out[3]);
    out[5] = sequence;
    memcpy(out + 6, payload, payload_len);
    checksum = g6he_payload_checksum(payload, payload_len);
    out[6 + payload_len] = (uint8_t)(checksum & 0xFFu);
    out[7 + payload_len] = (uint8_t)(checksum >> 8);
    return G6HE_REPORT_SIZE;
}

g6he_status_t g6he_frame_parse(const uint8_t report[G6HE_REPORT_SIZE],
                               g6he_request_t *out)
{
    uint8_t payload_len;
    uint16_t checksum;

    if (report == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    if (report[0] != G6HE_REPORT_ID_OUT) {
        return G6HE_ERR_MAGIC;
    }
    if (report[1] != G6HE_FRAME_MARKER) {
        return G6HE_ERR_MAGIC;
    }
    if (report[2] != G6HE_FRAME_KIND_NORMAL &&
        report[2] != G6HE_FRAME_KIND_UPDATE) {
        return G6HE_ERR_ARG;
    }
    if (((uint8_t)(report[3] + report[4])) != 0xFFu) {
        return G6HE_ERR_CHECKSUM;
    }
    if (report[3] < 2u) {
        return G6HE_ERR_TRUNCATED;
    }
    payload_len = (uint8_t)(report[3] - 2u);
    if (payload_len > G6HE_MAX_REQUEST_PAYLOAD) {
        return G6HE_ERR_RANGE;
    }
    memset(out, 0, sizeof(*out));
    out->sequence = report[5];
    out->update_frame = (report[2] == G6HE_FRAME_KIND_UPDATE);
    out->payload_len = payload_len;
    if (payload_len != 0u) {
        memcpy(out->payload, report + 6, payload_len);
    }
    checksum = (uint16_t)(report[6 + payload_len] |
                          ((uint16_t)report[7 + payload_len] << 8));
    if (checksum != g6he_payload_checksum(out->payload, payload_len)) {
        return G6HE_ERR_CHECKSUM;
    }
    return G6HE_OK;
}

g6he_status_t g6he_response_encode(const g6he_response_t *response,
                                   bool update_frame, uint8_t *out,
                                   size_t out_cap, size_t *out_bytes)
{
    uint8_t logical[G6HE_MAX_RESPONSE_PAYLOAD];
    uint8_t stream[G6HE_MAX_RESPONSE_PAYLOAD + 2];
    size_t logical_len;
    size_t stream_len;
    size_t reports;
    size_t i;
    size_t stream_cursor;
    uint16_t checksum;

    if (response == NULL || out == NULL || out_bytes == NULL) {
        return G6HE_ERR_ARG;
    }
    if (response->data_len > G6HE_MAX_RESPONSE_DATA) {
        return G6HE_ERR_RANGE;
    }
    logical_len = 4u + response->data_len;
    stream_len = logical_len + 2u;
    if (stream_len > 255u) {
        return G6HE_ERR_RANGE;
    }
    reports = 1u;
    if (stream_len > G6HE_FIRST_REPORT_STREAM) {
        reports += (stream_len - G6HE_FIRST_REPORT_STREAM +
                    G6HE_NEXT_REPORT_STREAM - 1u) /
                   G6HE_NEXT_REPORT_STREAM;
    }
    if (reports * G6HE_REPORT_SIZE > out_cap) {
        return G6HE_ERR_NOSPACE;
    }

    logical[0] = response->type;
    logical[1] = response->sequence;
    logical[2] = response->command;
    logical[3] = response->status;
    if (response->data_len != 0u) {
        memcpy(logical + 4, response->data, response->data_len);
    }
    memcpy(stream, logical, logical_len);
    checksum = g6he_payload_checksum(logical, logical_len);
    stream[logical_len] = (uint8_t)(checksum & 0xFFu);
    stream[logical_len + 1] = (uint8_t)(checksum >> 8);

    memset(out, 0, reports * G6HE_REPORT_SIZE);
    out[0] = G6HE_REPORT_ID_IN;
    out[1] = G6HE_FRAME_MARKER;
    out[2] = frame_kind(update_frame);
    out[3] = (uint8_t)stream_len;
    out[4] = (uint8_t)(~out[3]);
    out[5] = response->sequence;

    memcpy(out + 6, stream, G6HE_FIRST_REPORT_STREAM);
    stream_cursor = G6HE_FIRST_REPORT_STREAM;
    for (i = 1; i < reports; ++i) {
        uint8_t *report = out + (i * G6HE_REPORT_SIZE);
        size_t take = stream_len - stream_cursor;
        if (take > G6HE_NEXT_REPORT_STREAM) {
            take = G6HE_NEXT_REPORT_STREAM;
        }
        report[0] = G6HE_REPORT_ID_IN;
        memcpy(report + 1, stream + stream_cursor, take);
        stream_cursor += take;
    }
    *out_bytes = reports * G6HE_REPORT_SIZE;
    return G6HE_OK;
}

g6he_status_t g6he_response_decode(const uint8_t *reports, size_t reports_len,
                                   g6he_response_t *out)
{
    uint8_t collected[G6HE_MAX_RESPONSE_PAYLOAD + 16];
    uint8_t payload_len;
    size_t required;
    size_t collected_len;
    size_t report_count;
    size_t report_index;
    uint16_t checksum;

    if (reports == NULL || out == NULL) {
        return G6HE_ERR_ARG;
    }
    if (reports_len < G6HE_REPORT_SIZE ||
        (reports_len % G6HE_REPORT_SIZE) != 0u) {
        return G6HE_ERR_TRUNCATED;
    }
    if (reports[0] != G6HE_REPORT_ID_IN || reports[1] != G6HE_FRAME_MARKER) {
        return G6HE_ERR_MAGIC;
    }
    if (((uint8_t)(reports[3] + reports[4])) != 0xFFu) {
        return G6HE_ERR_CHECKSUM;
    }
    if (reports[3] < 2u) {
        return G6HE_ERR_TRUNCATED;
    }
    payload_len = (uint8_t)(reports[3] - 2u);
    /* payload_len is uint8, so it can never exceed G6HE_MAX_RESPONSE_PAYLOAD
     * (260); the collected buffer below is sized to hold any uint8 payload. */
    if (payload_len < 4u) {
        return G6HE_ERR_TRUNCATED;
    }
    required = (size_t)payload_len + 2u;
    if (required > sizeof(collected)) {
        return G6HE_ERR_RANGE;
    }

    report_count = reports_len / G6HE_REPORT_SIZE;
    memcpy(collected, reports + 6, G6HE_FIRST_REPORT_STREAM);
    collected_len = G6HE_FIRST_REPORT_STREAM;
    report_index = 1u;
    while (collected_len < required) {
        size_t take;
        if (report_index >= report_count) {
            return G6HE_ERR_TRUNCATED;
        }
        if (reports[report_index * G6HE_REPORT_SIZE] != G6HE_REPORT_ID_IN) {
            return G6HE_ERR_MAGIC;
        }
        take = required - collected_len;
        if (take > G6HE_NEXT_REPORT_STREAM) {
            take = G6HE_NEXT_REPORT_STREAM;
        }
        memcpy(collected + collected_len,
               reports + (report_index * G6HE_REPORT_SIZE) + 1, take);
        collected_len += take;
        ++report_index;
    }

    checksum = (uint16_t)(collected[payload_len] |
                          ((uint16_t)collected[payload_len + 1] << 8));
    if (checksum != g6he_payload_checksum(collected, payload_len)) {
        return G6HE_ERR_CHECKSUM;
    }

    memset(out, 0, sizeof(*out));
    out->type = collected[0];
    out->sequence = collected[1];
    out->command = collected[2];
    out->status = collected[3];
    out->data_len = payload_len - 4u;
    if (out->data_len > G6HE_MAX_RESPONSE_DATA) {
        return G6HE_ERR_RANGE;
    }
    memcpy(out->data, collected + 4, out->data_len);
    return G6HE_OK;
}

void g6he_updater_init(g6he_updater_t *updater,
                       const g6he_identity_t *identity,
                       const g6he_capabilities_t *capabilities)
{
    if (updater == NULL) {
        return;
    }
    memset(updater, 0, sizeof(*updater));
    updater->state = G6HE_UPDATER_IDLE;
    updater->running_crc = 0xFFFFFFFFu;
    if (identity != NULL) {
        updater->identity = *identity;
    }
    if (capabilities != NULL) {
        updater->capabilities = *capabilities;
    }
}

g6he_status_t g6he_updater_handle(g6he_updater_t *updater,
                                  const g6he_request_t *request,
                                  g6he_response_t *response)
{
    uint8_t command;

    if (updater == NULL || request == NULL || response == NULL) {
        return G6HE_ERR_ARG;
    }
    memset(response, 0, sizeof(*response));
    response->type =
        request->update_frame ? G6HE_RESP_UPDATE : G6HE_RESP_NORMAL;
    response->sequence = request->sequence;
    response->status = G6HE_PROTO_STATUS_OK;

    if (request->payload_len == 0u) {
        response->status = G6HE_PROTO_STATUS_BAD_COMMAND;
        return G6HE_ERR_COMMAND;
    }
    command = request->payload[0];
    response->command = command;

    switch (command) {
    case G6HE_CMD_IDENTITY:
        g6he_identity_encode(&updater->identity, response->data);
        response->data_len = G6HE_IDENTITY_DATA_SIZE;
        break;
    case G6HE_CMD_CAPABILITIES:
        g6he_capabilities_encode(&updater->capabilities, response->data);
        response->data_len = G6HE_CAPABILITY_DATA_SIZE;
        break;
    case G6HE_CMD_SELECT_MODE:
        if (request->payload_len < 2u || request->payload[1] != 0u) {
            response->status = G6HE_PROTO_STATUS_BAD_PARAM;
        } else {
            updater->selected_mode = request->payload[1];
            updater->state = G6HE_UPDATER_MODE_SELECTED;
        }
        break;
    case G6HE_CMD_START:
        updater->state = G6HE_UPDATER_UPDATING;
        updater->running_crc = 0xFFFFFFFFu;
        updater->bytes_received = 0u;
        break;
    case G6HE_CMD_TRANSFER:
        if (request->payload_len < 2u) {
            response->status = G6HE_PROTO_STATUS_BAD_PARAM;
        } else if (updater->state != G6HE_UPDATER_UPDATING) {
            response->status = G6HE_PROTO_STATUS_STATE;
        } else {
            const uint8_t *chunk = request->payload + 1;
            const size_t chunk_len = request->payload_len - 1u;
            updater->running_crc =
                g6he_crc32_update(updater->running_crc, chunk, chunk_len);
            updater->bytes_received += (uint32_t)chunk_len;
        }
        break;
    case G6HE_CMD_CHECK_CRC:
        if (request->payload_len < 9u) {
            response->status = G6HE_PROTO_STATUS_BAD_PARAM;
        } else {
            const uint32_t expected = rd_le32(request->payload + 1);
            const uint32_t actual = rd_le32(request->payload + 5);
            if (expected != actual || actual != updater->running_crc) {
                response->status = G6HE_PROTO_STATUS_CRC;
            }
        }
        break;
    case G6HE_CMD_RESET:
        updater->state = G6HE_UPDATER_IDLE;
        updater->reset_requested = true;
        break;
    case G6HE_CMD_BOOTLOADER_SWITCH:
        /* E-006/FACT: the connected G6 HE declares no bootloader switch. */
        response->status = G6HE_PROTO_STATUS_BAD_COMMAND;
        break;
    default:
        response->status = G6HE_PROTO_STATUS_BAD_COMMAND;
        break;
    }
    return G6HE_OK;
}
