#pragma once
#include <stdbool.h>

typedef struct {
    char ha_url[120];
    char ha_token[512];
} web_config_result_t;

/* Start de HTTP-configuratieserver op het opgegeven IP-adres.
   Firmware luistert op poort 80; simulator op poort 8080. */
void web_config_start(const char *device_ip);
void web_config_stop(void);

bool web_config_is_done(void);
void web_config_get_result(web_config_result_t *out);
