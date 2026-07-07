#include "../../firmware/src/platform/web_config.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <SDL2/SDL.h>

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

#define SIM_PORT 8080

/* Server socket wordt aangemaakt in web_config_start() zodat
   de poort direct luistert, nog voor de thread begint. */
static SOCKET              s_srv     = INVALID_SOCKET;
static volatile bool       s_running = false;
static volatile bool       s_done    = false;
static web_config_result_t s_result  = {0};
static SDL_Thread         *s_thread  = NULL;

/* ---- URL-decoder ---- */
static void url_decode(const char *src, char *dst, size_t dst_len) {
    size_t i = 0;
    while (*src && i < dst_len - 1) {
        if (src[0] == '%' && src[1] && src[2]) {
            char hex[3] = {src[1], src[2], 0};
            dst[i++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[i++] = ' '; src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

/* ---- HTML ---- */
#define HTML_HEADER_OK  "HTTP/1.1 200 OK\r\nContent-Type: text/html; charset=utf-8\r\nConnection: close\r\n\r\n"
#define HTML_HEADER_400 "HTTP/1.1 400 Bad Request\r\nConnection: close\r\n\r\nOntbrekende velden"

static const char HTML_BODY[] =
    "<!DOCTYPE html><html><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>SenseCap Configuratie</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:520px;margin:40px auto;padding:0 20px;background:#1A1A2E;color:#E0E0E0}"
    "h2{color:#4FC3F7}label{display:block;margin-top:20px;font-weight:bold;color:#9E9E9E}"
    "input,textarea{width:100%;box-sizing:border-box;padding:10px;font-size:14px;"
    "background:#16213E;color:#E0E0E0;border:1px solid #2A2A4A;border-radius:6px;margin-top:6px}"
    "textarea{height:130px;font-family:monospace;resize:vertical}"
    "button{margin-top:24px;width:100%;padding:14px;background:#4FC3F7;"
    "border:none;border-radius:6px;font-size:16px;font-weight:bold;color:#1A1A2E;cursor:pointer}"
    "button:hover{background:#81D4FA}"
    ".hint{font-size:12px;color:#9E9E9E;margin-top:4px}"
    "</style></head><body>"
    "<h2>&#127968; Home Assistant Configuratie</h2>"
    "<p>Vul hieronder je Home Assistant gegevens in. Het token kun je kopiëren vanuit<br>"
    "<b>HA &rarr; Profiel &rarr; Long-Lived Access Tokens</b>.</p>"
    "<form method='POST' action='/save'>"
    "<label>HA Adres</label>"
    "<input type='text' name='ha_url' placeholder='http://192.168.1.10:8123' required>"
    "<p class='hint'>Zonder /api/websocket — dat wordt automatisch toegevoegd.</p>"
    "<label>Long-Lived Access Token</label>"
    "<textarea name='ha_token' placeholder='eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9...' required></textarea>"
    "<button type='submit'>&#10003; Opslaan &amp; Doorgaan</button>"
    "</form></body></html>";

static const char HTML_OK[] =
    "<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;"
    "padding:60px;background:#1A1A2E;color:#E0E0E0'>"
    "<h2 style='color:#4FC3F7'>&#10003; Opgeslagen!</h2>"
    "<p>Configuratie ontvangen. Het apparaat herstart.<br>Je kunt dit tabblad sluiten.</p>"
    "</body></html>";

/* ---- POST-body verwerken ---- */
static bool parse_post_body(const char *body) {
    char raw_url[120] = {0}, raw_tok[600] = {0};
    const char *p = body;
    while (p && *p) {
        const char *eq  = strchr(p, '=');
        if (!eq) break;
        const char *amp = strchr(eq, '&');
        size_t klen = (size_t)(eq - p);
        char key[32] = {0};
        if (klen < sizeof(key)) memcpy(key, p, klen);
        const char *vstart = eq + 1;
        size_t vlen = amp ? (size_t)(amp - vstart) : strlen(vstart);
        char *val = (char *)malloc(vlen + 1);
        if (val) {
            memcpy(val, vstart, vlen);
            val[vlen] = '\0';
            if (strcmp(key, "ha_url") == 0)
                url_decode(val, raw_url, sizeof(raw_url));
            else if (strcmp(key, "ha_token") == 0)
                url_decode(val, raw_tok, sizeof(raw_tok));
            free(val);
        }
        p = amp ? amp + 1 : NULL;
    }
    if (!raw_url[0] || !raw_tok[0]) return false;
    strncpy(s_result.ha_url,   raw_url, sizeof(s_result.ha_url)   - 1);
    strncpy(s_result.ha_token, raw_tok, sizeof(s_result.ha_token) - 1);
    return true;
}

/* ---- Helper: stuur string volledig via socket ---- */
static void send_all(SOCKET s, const char *data, int len) {
    while (len > 0) {
        int sent = send(s, data, len, 0);
        if (sent <= 0) break;
        data += sent;
        len  -= sent;
    }
}

/* ---- Verwerk één client-verbinding ---- */
static void handle_client(SOCKET cli) {
    /* Wacht tot data beschikbaar is (max 5 s) */
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(cli, &rfds);
    struct timeval tv = {5, 0};
    if (select((int)cli + 1, &rfds, NULL, NULL, &tv) <= 0) {
        printf("[web_config] Geen data van browser (timeout)\n");
        closesocket(cli);
        return;
    }

    char req[4096] = {0};
    int  n = recv(cli, req, sizeof(req) - 1, 0);
    if (n <= 0) { closesocket(cli); return; }

    bool is_get  = (strncmp(req, "GET ",  4) == 0);
    bool is_post = (strncmp(req, "POST ", 5) == 0);
    bool is_save = is_post && (strstr(req, " /save") != NULL);

    printf("[web_config] %s %s\n", is_get ? "GET" : is_post ? "POST" : "?",
           is_save ? "/save" : "/");

    if (is_save) {
        const char *body = strstr(req, "\r\n\r\n");
        bool ok = body && parse_post_body(body + 4);
        send_all(cli, HTML_HEADER_OK, (int)strlen(HTML_HEADER_OK));
        if (ok) {
            send_all(cli, HTML_OK, (int)strlen(HTML_OK));
            s_done = true;
            printf("[web_config] Config opgeslagen: %s\n", s_result.ha_url);
        } else {
            send_all(cli, HTML_HEADER_400, (int)strlen(HTML_HEADER_400));
        }
    } else {
        /* GET / — ook favicon-verzoeken vangen we hier op (200 + leeg) */
        send_all(cli, HTML_HEADER_OK, (int)strlen(HTML_HEADER_OK));
        if (is_get && strstr(req, "GET / ")) {
            send_all(cli, HTML_BODY, (int)strlen(HTML_BODY));
        }
    }

    shutdown(cli, SD_SEND);
    closesocket(cli);
}

/* ---- Server-thread ---- */
static int SDLCALL server_thread(void *data) {
    (void)data;
    printf("[web_config] Server-thread gestart\n");

    while (s_running && !s_done) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(s_srv, &rfds);
        struct timeval tv = {0, 200000}; /* 200 ms */
        int ready = select((int)s_srv + 1, &rfds, NULL, NULL, &tv);
        if (ready < 0) { SDL_Delay(50); continue; }
        if (ready == 0) continue;

        SOCKET cli = accept(s_srv, NULL, NULL);
        if (cli == INVALID_SOCKET) continue;
        handle_client(cli);
    }

    printf("[web_config] Server-thread gestopt\n");
    return 0;
}

/* ================================================================
   Publieke API
   ================================================================ */

void web_config_start(const char *device_ip) {
    (void)device_ip;
    if (s_running) return;

    /* Winsock initialiseren (veilig om meerdere keren aan te roepen) */
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[web_config] WSAStartup mislukt\n");
        return;
    }

    /* Socket aanmaken en binden VOOR de thread start,
       zodat de poort direct luistert. */
    s_srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s_srv == INVALID_SOCKET) {
        printf("[web_config] socket() mislukt: %d\n", WSAGetLastError());
        return;
    }

    DWORD opt = 1;
    setsockopt(s_srv, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(SIM_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(s_srv, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        printf("[web_config] bind() mislukt op poort %d: %d\n",
               SIM_PORT, WSAGetLastError());
        closesocket(s_srv);
        s_srv = INVALID_SOCKET;
        return;
    }
    if (listen(s_srv, 8) != 0) {
        printf("[web_config] listen() mislukt: %d\n", WSAGetLastError());
        closesocket(s_srv);
        s_srv = INVALID_SOCKET;
        return;
    }

    s_running = true;
    s_done    = false;
    memset(&s_result, 0, sizeof(s_result));

    printf("[web_config] HTTP-server luistert op http://localhost:%d\n", SIM_PORT);
    s_thread = SDL_CreateThread(server_thread, "web_config", NULL);
}

void web_config_stop(void) {
    s_running = false;
    if (s_srv != INVALID_SOCKET) {
        closesocket(s_srv);
        s_srv = INVALID_SOCKET;
    }
    if (s_thread) {
        SDL_WaitThread(s_thread, NULL);
        s_thread = NULL;
    }
    WSACleanup();
}

bool web_config_is_done(void) { return s_done; }

void web_config_get_result(web_config_result_t *out) { *out = s_result; }
