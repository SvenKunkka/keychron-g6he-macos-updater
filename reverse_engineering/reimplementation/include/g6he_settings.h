/*
 * g6he_settings.h - settings schema and explicit key migration.
 *
 * BR-016 / BR-024. The +87 artifact renamed the per-axis Hall settings keys:
 *
 *   +84 settings name table: ... 'hall/mem0', 'hall/mem1', 'hall',
 *                                 'hall/ax0', 'hall/ax1', ...
 *   +87 settings name table: ... 'ax0', 'ax1', 'hall/mem0', 'hall/mem1',
 *                                 'hall', ...
 *
 * Migration is explicit and idempotent: v1 keys are renamed to their v2 names
 * when the stored schema version is below 2. Unknown keys are preserved (they
 * are not silently dropped), and the migration is a pure rename — no value is
 * reinterpreted without evidence.
 */
#ifndef G6HE_SETTINGS_H
#define G6HE_SETTINGS_H

#include "g6he_common.h"

#ifdef __cplusplus
extern "C" {
#endif

#define G6HE_SETTINGS_SCHEMA_VERSION 2U
#define G6HE_SETTINGS_KEY_MAX 32U
#define G6HE_SETTINGS_VALUE_MAX 32U
#define G6HE_SETTINGS_MAX_ENTRIES 48U

typedef struct {
    char key[G6HE_SETTINGS_KEY_MAX];
    char value[G6HE_SETTINGS_VALUE_MAX];
} g6he_setting_entry_t;

typedef struct {
    uint32_t version;
    g6he_setting_entry_t entries[G6HE_SETTINGS_MAX_ENTRIES];
    size_t count;
} g6he_settings_t;

typedef struct {
    const char *from;
    const char *to;
} g6he_settings_rename_t;

/* The evidenced v1 -> v2 renames. */
size_t g6he_settings_rename_count(void);
const g6he_settings_rename_t *g6he_settings_renames(void);

void g6he_settings_init(g6he_settings_t *settings, uint32_t stored_version);

/* Apply every rename for version < 2 and set the current version. Idempotent. */
g6he_status_t g6he_settings_migrate(g6he_settings_t *settings);

g6he_status_t g6he_settings_set(g6he_settings_t *settings, const char *key,
                                const char *value);
g6he_status_t g6he_settings_get(const g6he_settings_t *settings,
                                const char *key, char *out, size_t out_size);
bool g6he_settings_has(const g6he_settings_t *settings, const char *key);

#ifdef __cplusplus
}
#endif

#endif /* G6HE_SETTINGS_H */
