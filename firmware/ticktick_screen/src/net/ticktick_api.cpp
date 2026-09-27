#include "ticktick_api.h"

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

#include "../../config.h"
#include "certs.h"

static WiFiClientSecure s_client;      // api.ticktick.com, reusada no ciclo
static WiFiClientSecure s_oauthClient; // ticktick.com, so na renovacao
static HTTPClient s_http;
static bool s_configured = false;

static void configure(WiFiClientSecure &c, bool insecure) {
    if (insecure) c.setInsecure();
    else c.setCACert(CA_BUNDLE);
}

void api_set_insecure(bool insecure) {
    s_client.stop();
    s_oauthClient.stop();
    configure(s_client, insecure);
    configure(s_oauthClient, insecure);
    s_configured = true;
}

static void ensure_configured() {
    if (!s_configured) api_set_insecure(false);
}

// Le o corpo para o sink (ou so descarta) e fecha a requisicao; com reuso, a
// conexao TCP/TLS continua aberta para a proxima.
static int finish(HTTPClient &http, int code, PsramSink *sink) {
    if (code > 0 && sink) {
        sink->clear();
        const int w = http.writeToStream(sink);
        if (w < 0) code = (w == HTTPC_ERROR_STREAM_WRITE) ? NET_ERR_MEMORY : w;
    }
    http.end();
    return code;
}

static bool begin_request(const char *path, const char *token) {
    ensure_configured();
    const String url = String(API_BASE) + path;
    s_http.setReuse(true);
    if (!s_http.begin(s_client, url)) return false;
    s_http.setTimeout(HTTP_TIMEOUT_MS);
    s_http.setConnectTimeout(HTTP_TIMEOUT_MS);
    s_http.addHeader("Authorization", String("Bearer ") + token);
    return true;
}

int api_get(const char *path, const char *token, PsramSink &sink) {
    if (!begin_request(path, token)) return HTTPC_ERROR_CONNECTION_REFUSED;
    return finish(s_http, s_http.GET(), &sink);
}

int api_post_empty(const char *path, const char *token) {
    if (!begin_request(path, token)) return HTTPC_ERROR_CONNECTION_REFUSED;
    // O HTTPClient so manda Content-Length quando ha corpo; POST sem ele pode
    // levar 411 de alguns servidores.
    s_http.addHeader("Content-Length", "0");
    return finish(s_http, s_http.POST((uint8_t *)nullptr, 0), nullptr);
}

static String urlencode(const char *s) {
    static const char hex[] = "0123456789ABCDEF";
    String out;
    for (const char *p = s; *p; p++) {
        const unsigned char c = (unsigned char)*p;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += (char)c;
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

int api_refresh_token(const Creds &c, PsramSink &sink) {
    ensure_configured();
    HTTPClient http; // outro host: conexao propria, sem reuso
    if (!http.begin(s_oauthClient, OAUTH_TOKEN_URL)) return HTTPC_ERROR_CONNECTION_REFUSED;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setConnectTimeout(HTTP_TIMEOUT_MS);
    http.setAuthorization(c.cid, c.csec); // Basic auth
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    String body = "grant_type=refresh_token&refresh_token=";
    body += urlencode(c.rtok);
    const int code = finish(http, http.POST(body), &sink);
    s_oauthClient.stop();
    return code;
}

void api_close() {
    s_http.end();
    s_client.stop();
}
