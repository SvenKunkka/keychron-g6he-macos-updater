/*
 * g6he_sensor.h - PAW3950/PAW3955 transport interface and variant selection.
 *
 * BR-022. The sensor-side interface is FACT from the vendor datasheets:
 * 16-pin DIP, 4-wire SPI (CPOL=1/CPHA=1, fSCLK 0.5..16 MHz), MOTION interrupt,
 * NRESET, LED_P. VDD 1.8-2.1 V, VDDIO 1.8-3.6 V. PAW3950 and PAW3955 share the
 * same pin table; they differ in maximum cpi (30,000 vs 40,000).
 *
 * The MCU-side pins are UNKNOWN (no schematic/ball map), so this module owns no
 * GPIOs. It only forwards register transactions through a caller-supplied
 * transport and refuses to operate until a variant is selected and the
 * transport is complete.
 */
#ifndef G6HE_SENSOR_H
#define G6HE_SENSOR_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    G6HE_SENSOR_UNSET = 0,
    G6HE_SENSOR_PAW3950 = 1,
    G6HE_SENSOR_PAW3955 = 2
} g6he_sensor_variant_t;

/* Transport callbacks return 0 on success, non-zero on error. */
typedef struct {
    int (*read)(void *ctx, uint8_t reg, uint8_t *buf, size_t len);
    int (*write)(void *ctx, uint8_t reg, const uint8_t *buf, size_t len);
    int (*reset)(void *ctx);
    int (*motion_asserted)(void *ctx);
    void *ctx;
} g6he_sensor_transport_t;

typedef struct {
    g6he_sensor_variant_t variant;
    g6he_sensor_transport_t transport;
    bool bound;
} g6he_sensor_t;

/* Bind a variant and transport. Rejects an unset variant or missing callbacks. */
g6he_status_t g6he_sensor_bind(g6he_sensor_t *sensor,
                               g6he_sensor_variant_t variant,
                               const g6he_sensor_transport_t *transport);

/* Datasheet maximum cpi; 0 when unset. */
uint16_t g6he_sensor_max_cpi(const g6he_sensor_t *sensor);

g6he_status_t g6he_sensor_reset(g6he_sensor_t *sensor);
g6he_status_t g6he_sensor_motion_asserted(g6he_sensor_t *sensor,
                                          bool *asserted);
g6he_status_t g6he_sensor_read_reg(g6he_sensor_t *sensor, uint8_t reg,
                                   uint8_t *buf, size_t len);
g6he_status_t g6he_sensor_write_reg(g6he_sensor_t *sensor, uint8_t reg,
                                    const uint8_t *buf, size_t len);

/*
 * The sensor's register map is NOT recovered. This symbol exists so a target
 * build that tries to use a concrete register table fails closed instead of
 * inventing addresses.
 */
#define G6HE_SENSOR_REGISTER_MAP_EVIDENCED 0

#ifdef __cplusplus
}
#endif

#endif /* G6HE_SENSOR_H */
