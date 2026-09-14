/*
 * g6he_settings.c - settings store and explicit key migration.
 */
#include "g6he_settings.h"

#include <string.h>

static const g6he_settings_rename_t k_renames[] = {
    {"hall/ax0", "ax0"},
    {"hall/ax1", "ax1"},
};

size_t g6he_settings_rename_count(void)
{
    return sizeof(k_renames) / sizeof(k_renames[0]);
}

const g6he_settings_rename_t *g6he_settings_renames(void)
{
    return k_renames;
}

void g6he_settings_init(g6he_settings_t *settings, uint32_t stored_version)
{
    if (settings == NULL) {
        return;
    }
    memset(settings, 0, sizeof(*settings));
    settings->version = stored_version;
}

static g6he_setting_entry_t *find_entry(g6he_settings_t *settings,
                                        const char *key)
{
    size_t i;

    for (i = 0; i < settings->count; ++i) {
        if (strncmp(settings->entries[i].key, key,
                    G6HE_SETTINGS_KEY_MAX) == 0) {
            return &settings->entries[i];
        }
    }
    return NULL;
}

static const g6he_setting_entry_t *find_entry_const(
    const g6he_settings_t *settings, const char *key)
{
    size_t i;

    for (i = 0; i < settings->count; ++i) {
        if (strncmp(settings->entries[i].key, key,
                    G6HE_SETTINGS_KEY_MAX) == 0) {
            return &settings->entries[i];
        }
    }
    return NULL;
}

g6he_status_t g6he_settings_set(g6he_settings_t *settings, const char *key,
                                const char *value)
{
    g6he_setting_entry_t *entry;

    if (settings == NULL || key == NULL || value == NULL) {
        return G6HE_ERR_ARG;
    }
    if (strlen(key) >= G6HE_SETTINGS_KEY_MAX ||
        strlen(value) >= G6HE_SETTINGS_VALUE_MAX) {
        return G6HE_ERR_RANGE;
    }
    entry = find_entry(settings, key);
    if (entry == NULL) {
        if (settings->count >= G6HE_SETTINGS_MAX_ENTRIES) {
            return G6HE_ERR_NOSPACE;
        }
        entry = &settings->entries[settings->count++];
        memset(entry, 0, sizeof(*entry));
        memcpy(entry->key, key, strlen(key));
    }
    memset(entry->value, 0, sizeof(entry->value));
    memcpy(entry->value, value, strlen(value));
    return G6HE_OK;
}

g6he_status_t g6he_settings_migrate(g6he_settings_t *settings)
{
    size_t i;

    if (settings == NULL) {
        return G6HE_ERR_ARG;
    }
    if (settings->version >= G6HE_SETTINGS_SCHEMA_VERSION) {
        return G6HE_OK; /* already current; idempotent */
    }
    for (i = 0; i < g6he_settings_rename_count(); ++i) {
        const g6he_settings_rename_t *r = &k_renames[i];
        g6he_setting_entry_t *old = find_entry(settings, r->from);
        if (old == NULL) {
            continue;
        }
        /* Only rename when the destination does not already exist; otherwise
         * keep the newer key and drop the stale duplicate. */
        if (find_entry(settings, r->to) == NULL) {
            memset(old->key, 0, sizeof(old->key));
            memcpy(old->key, r->to, strlen(r->to));
        } else {
            /* remove the old entry by moving the last entry into its slot */
            size_t index = (size_t)(old - settings->entries);
            settings->entries[index] = settings->entries[settings->count - 1u];
            settings->count--;
            memset(&settings->entries[settings->count], 0,
                   sizeof(settings->entries[0]));
        }
    }
    settings->version = G6HE_SETTINGS_SCHEMA_VERSION;
    return G6HE_OK;
}

g6he_status_t g6he_settings_get(const g6he_settings_t *settings,
                                const char *key, char *out, size_t out_size)
{
    const g6he_setting_entry_t *entry;

    if (settings == NULL || key == NULL || out == NULL || out_size == 0u) {
        return G6HE_ERR_ARG;
    }
    entry = find_entry_const(settings, key);
    if (entry == NULL) {
        return G6HE_ERR_ARG; /* not found */
    }
    if (strlen(entry->value) >= out_size) {
        return G6HE_ERR_RANGE;
    }
    memcpy(out, entry->value, strlen(entry->value) + 1u);
    return G6HE_OK;
}

bool g6he_settings_has(const g6he_settings_t *settings, const char *key)
{
    return settings != NULL && key != NULL &&
           find_entry_const(settings, key) != NULL;
}
