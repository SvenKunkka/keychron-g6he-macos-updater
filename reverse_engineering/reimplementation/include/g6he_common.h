/*
 * g6he_common.h - shared types and status codes.
 *
 * Clean-room reimplementation. This header was written from the evidence
 * documents in ../ (evidence.md, firmware_static_report.json, bug_risk_audit.md,
 * ghidra_hall_math.txt, ghidra_callers.txt) and from public MCUboot/HID
 * specifications. It contains no vendor source code and no decompiler output.
 *
 * Evidence classes used across the tree: FACT, INFERENCE, HYPOTHESIS, UNKNOWN.
 * See docs/traceability.md and docs/unknowns.md.
 */
#ifndef G6HE_COMMON_H
#define G6HE_COMMON_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Stable status codes. Numeric values are part of the test contract. */
typedef enum {
    G6HE_OK = 0,
    G6HE_ERR_ARG = -1,
    G6HE_ERR_RANGE = -2,
    G6HE_ERR_MAGIC = -3,
    G6HE_ERR_TRUNCATED = -4,
    G6HE_ERR_DIGEST = -5,
    G6HE_ERR_TLV = -6,
    G6HE_ERR_UNSUPPORTED = -7,
    G6HE_ERR_CHECKSUM = -8,
    G6HE_ERR_SEQUENCE = -9,
    G6HE_ERR_COMMAND = -10,
    G6HE_ERR_STATE = -11,
    G6HE_ERR_MODEL = -12,
    G6HE_ERR_SIGNATURE = -13,
    G6HE_ERR_NOSPACE = -14
} g6he_status_t;

#ifdef __cplusplus
}
#endif

#endif /* G6HE_COMMON_H */
