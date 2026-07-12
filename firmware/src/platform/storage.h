#pragma once
#include <stdint.h>
#include <stddef.h>

/* Abstractie over NVS (firmware) en bestandsopslag (simulator).
   Beide implementaties delen deze interface. */

#ifdef __cplusplus
extern "C" {
#endif

void    storage_init(void);
void    storage_get_string(const char *key, char *out, size_t len, const char *default_val);
void    storage_set_string(const char *key, const char *value);
uint8_t storage_get_u8(const char *key, uint8_t default_val);
void    storage_set_u8(const char *key, uint8_t value);
uint32_t storage_get_u32(const char *key, uint32_t default_val);
void     storage_set_u32(const char *key, uint32_t value);
void    storage_clear_all(void);

#ifdef __cplusplus
}
#endif
