/*
 * g6he_sensor.c - PAW3950/PAW3955 transport and variant handling.
 */
#include "g6he_sensor.h"

#include <string.h>

static bool transport_complete(const g6he_sensor_transport_t *t)
{
    return t != NULL && t->read != NULL && t->write != NULL &&
           t->reset != NULL && t->motion_asserted != NULL;
}

g6he_status_t g6he_sensor_bind(g6he_sensor_t *sensor,
                               g6he_sensor_variant_t variant,
                               const g6he_sensor_transport_t *transport)
{
    if (sensor == NULL) {
        return G6HE_ERR_ARG;
    }
    if (variant != G6HE_SENSOR_PAW3950 && variant != G6HE_SENSOR_PAW3955) {
        return G6HE_ERR_UNSUPPORTED; /* unset or unknown variant */
    }
    if (!transport_complete(transport)) {
        return G6HE_ERR_ARG;
    }
    memset(sensor, 0, sizeof(*sensor));
    sensor->variant = variant;
    sensor->transport = *transport;
    sensor->bound = true;
    return G6HE_OK;
}

uint16_t g6he_sensor_max_cpi(const g6he_sensor_t *sensor)
{
    if (sensor == NULL || !sensor->bound) {
        return 0u;
    }
    return (sensor->variant == G6HE_SENSOR_PAW3955) ? 40000u : 30000u;
}

g6he_status_t g6he_sensor_reset(g6he_sensor_t *sensor)
{
    if (sensor == NULL || !sensor->bound) {
        return G6HE_ERR_UNSUPPORTED;
    }
    return (sensor->transport.reset(sensor->transport.ctx) == 0)
               ? G6HE_OK
               : G6HE_ERR_STATE;
}

g6he_status_t g6he_sensor_motion_asserted(g6he_sensor_t *sensor,
                                          bool *asserted)
{
    int rc;

    if (sensor == NULL || asserted == NULL || !sensor->bound) {
        return G6HE_ERR_UNSUPPORTED;
    }
    rc = sensor->transport.motion_asserted(sensor->transport.ctx);
    if (rc < 0) {
        return G6HE_ERR_STATE;
    }
    *asserted = (rc != 0);
    return G6HE_OK;
}

g6he_status_t g6he_sensor_read_reg(g6he_sensor_t *sensor, uint8_t reg,
                                   uint8_t *buf, size_t len)
{
    if (sensor == NULL || buf == NULL || !sensor->bound) {
        return G6HE_ERR_UNSUPPORTED;
    }
    return (sensor->transport.read(sensor->transport.ctx, reg, buf, len) == 0)
               ? G6HE_OK
               : G6HE_ERR_STATE;
}

g6he_status_t g6he_sensor_write_reg(g6he_sensor_t *sensor, uint8_t reg,
                                    const uint8_t *buf, size_t len)
{
    if (sensor == NULL || buf == NULL || !sensor->bound) {
        return G6HE_ERR_UNSUPPORTED;
    }
    return (sensor->transport.write(sensor->transport.ctx, reg, buf, len) == 0)
               ? G6HE_OK
               : G6HE_ERR_STATE;
}
