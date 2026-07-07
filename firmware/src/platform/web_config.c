#include "web_config.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "web_cfg";

static httpd_handle_t   s_server   = NULL;
static bool             s_done     = false;
static web_config_result_t s_result = {0};

/* ---- URL-decoder ---- */
static void url_decode(const char *src, char *dst, size_t dst_len) {
    size_t i = 0;
    while (*src && i < dst_len - 1) {
        if (*src == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' ';
            src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

/* ---- HTML-formulier ---- */
static const char HTML_FORM[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SenseCap Configuratie</title>"
    "<style>body{font-family:sans-serif;max-width:480px;margin:32px auto;padding:0 16px}"
    "h2{color:#4FC3F7}label{display:block;margin-top:16px;font-weight:bold}"
    "input,textarea{width:100%;box-sizing:border-box;padding:8px;font-size:14px;"
    "border:1px solid #ccc;border-radius:4px;margin-top:4px}"
    "textarea{height:120px;font-family:monospace}"
    "button{margin-top:20px;width:100%;padding:12px;background:#4FC3F7;"
    "border:none;border-radius:4px;font-size:16px;cursor:pointer}"
    "</style></head><body>"
    "<h2>Home Assistant Configuratie</h2>"
    "<form method='POST' action='/save'>"
    "<label>HA Adres</label>"
    "<input type='text' name='ha_url' placeholder='http://192.168.1.10:8123'>"
    "<label>Long-Lived Access Token</label>"
    "<textarea name='ha_token' placeholder='eyJhbGci...'></textarea>"
    "<button type='submit'>Opslaan &amp; Doorgaan</button>"
    "</form></body></html>";

static const char HTML_OK[] =
    "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;padding:48px'>"
    "<h2 style='color:#4FC3F7'>&#10003; Opgeslagen!</h2>"
    "<p>Het apparaat herstart nu en maakt verbinding met Home Assistant.</p>"
    "</body></html>";

/* ---- Handlers ---- */

static esp_err_t get_root(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, HTML_FORM, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t post_save(httpd_req_t *req) {
    char body[640] = {0};
    int  len = httpd_req_recv(req, body, sizeof(body) - 1);
    if (len <= 0) { httpd_resp_send_500(req); return ESP_FAIL; }
    body[len] = '\0';

    /* Parse url-encoded body: ha_url=...&ha_token=... */
    char raw_url[120] = {0}, raw_tok[600] = {0};
    char *p = body;
    while (p && *p) {
        char *eq  = strchr(p, '=');
        char *amp = strchr(p, '&');
        if (!eq) break;
        *eq = '\0';
        if (amp) *amp = '\0';
        if (strcmp(p, "ha_url") == 0)
            url_decode(eq + 1, raw_url, sizeof(raw_url));
        else if (strcmp(p, "ha_token") == 0)
            url_decode(eq + 1, raw_tok, sizeof(raw_tok));
        p = amp ? amp + 1 : NULL;
    }

    if (raw_url[0] && raw_tok[0]) {
        strncpy(s_result.ha_url,   raw_url, sizeof(s_result.ha_url)   - 1);
        strncpy(s_result.ha_token, raw_tok, sizeof(s_result.ha_token) - 1);
        s_done = true;
        ESP_LOGI(TAG, "Config ontvangen: %s", s_result.ha_url);
        httpd_resp_set_type(req, "text/html");
        httpd_resp_send(req, HTML_OK, HTTPD_RESP_USE_STRLEN);
    } else {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Beide velden zijn verplicht");
    }
    return ESP_OK;
}

/* ---- Publieke API ---- */

void web_config_start(const char *device_ip) {
    (void)device_ip;
    if (s_server) return;
    s_done = false;
    memset(&s_result, 0, sizeof(s_result));

    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.server_port = 80;

    if (httpd_start(&s_server, &cfg) != ESP_OK) {
        ESP_LOGE(TAG, "Kan HTTP-server niet starten");
        return;
    }
    httpd_uri_t get  = {.uri = "/",     .method = HTTP_GET,  .handler = get_root};
    httpd_uri_t save = {.uri = "/save", .method = HTTP_POST, .handler = post_save};
    httpd_register_uri_handler(s_server, &get);
    httpd_register_uri_handler(s_server, &save);
    ESP_LOGI(TAG, "Config-server gestart op poort 80");
}

void web_config_stop(void) {
    if (s_server) { httpd_stop(s_server); s_server = NULL; }
}

bool web_config_is_done(void)  { return s_done; }

void web_config_get_result(web_config_result_t *out) {
    *out = s_result;
}
