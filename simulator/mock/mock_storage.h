#pragma once
#include <stdint.h>
#include <stddef.h>

/* Initialiseert de mock-opslag met een .ini bestand op schijf.
   Vervangt de NVS Preferences API van de firmware. */
void    mock_storage_init(const char *ini_path);

/* Zelfde interface als firmware/src/platform/storage.h */
void    storage_init(void);
void    storage_get_string(const char *key, char *out, size_t len, const char *default_val);
void    storage_set_string(const char *key, const char *value);
uint8_t storage_get_u8(const char *key, uint8_t default_val);
void    storage_set_u8(const char *key, uint8_t value);
void    storage_clear_all(void);
