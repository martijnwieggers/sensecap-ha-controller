#include "mock_storage.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ENTRIES 32
#define MAX_KEY_LEN 32
#define MAX_VAL_LEN 256

typedef struct { char key[MAX_KEY_LEN]; char val[MAX_VAL_LEN]; } kv_t;

static kv_t   s_store[MAX_ENTRIES];
static int    s_count   = 0;
static char   s_path[256] = "config.ini";

static kv_t *find(const char *key) {
    for (int i = 0; i < s_count; i++)
        if (strcmp(s_store[i].key, key) == 0) return &s_store[i];
    return NULL;
}

static void load_ini(void) {
    FILE *f = fopen(s_path, "r");
    if (!f) return;
    char line[300];
    while (fgets(line, sizeof(line), f)) {
        if (line[0] == '#' || line[0] == '[') continue;
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        char *key = line, *val = eq + 1;
        val[strcspn(val, "\r\n")] = '\0';
        storage_set_string(key, val);
    }
    fclose(f);
}

static void save_ini(void) {
    FILE *f = fopen(s_path, "w");
    if (!f) return;
    fprintf(f, "# SenseCap HA Simulator config\n");
    for (int i = 0; i < s_count; i++)
        fprintf(f, "%s=%s\n", s_store[i].key, s_store[i].val);
    fclose(f);
}

void mock_storage_init(const char *ini_path) {
    strncpy(s_path, ini_path, sizeof(s_path) - 1);
    load_ini();
    printf("[storage] Geladen uit %s (%d sleutels)\n", s_path, s_count);
}

void storage_init(void) { /* al gedaan in mock_storage_init */ }

void storage_get_string(const char *key, char *out, size_t len,
                        const char *default_val) {
    kv_t *e = find(key);
    strncpy(out, e ? e->val : (default_val ? default_val : ""), len - 1);
    out[len - 1] = '\0';
}

void storage_set_string(const char *key, const char *value) {
    kv_t *e = find(key);
    if (!e) {
        if (s_count >= MAX_ENTRIES) return;
        e = &s_store[s_count++];
        strncpy(e->key, key, MAX_KEY_LEN - 1);
    }
    strncpy(e->val, value, MAX_VAL_LEN - 1);
    save_ini();
}

uint8_t storage_get_u8(const char *key, uint8_t default_val) {
    kv_t *e = find(key);
    return e ? (uint8_t)atoi(e->val) : default_val;
}

void storage_set_u8(const char *key, uint8_t value) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", value);
    storage_set_string(key, buf);
}

void storage_clear_all(void) {
    s_count = 0;
    save_ini();
    printf("[storage] Gewist\n");
}
