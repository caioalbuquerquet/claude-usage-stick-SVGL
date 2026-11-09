#include "net/api.h"
#include "config.h"
#include "net/certs.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <mbedtls/platform_util.h>

#define H5U "anthropic-ratelimit-unified-5h-utilization"
#define H5R "anthropic-ratelimit-unified-5h-reset"
#define H5S "anthropic-ratelimit-unified-5h-status"
#define D7U "anthropic-ratelimit-unified-7d-utilization"
#define D7R "anthropic-ratelimit-unified-7d-reset"
#define D7S "anthropic-ratelimit-unified-7d-status"
#define UST "anthropic-ratelimit-unified-status"
#define URS "anthropic-ratelimit-unified-reset"
#define URC "anthropic-ratelimit-unified-representative-claim"
#define UFB "anthropic-ratelimit-unified-fallback-percentage"
#define UOS "anthropic-ratelimit-unified-overage-status"
#define UOR "anthropic-ratelimit-unified-overage-disabled-reason"

static const char* RL_HEADERS[] = {
    H5U, H5R, H5S, D7U, D7R, D7S, UST, URS, URC, UFB, UOS, UOR
};
static const int RL_HEADER_COUNT = 12;

void net_client_setup(NetworkClientSecure& client, HTTPClient& https) {
    client.setCACert(CA_BUNDLE);
    client.setHandshakeTimeout(NET_HANDSHAKE_S);   // segundos (NetworkClientSecure.cpp:450-451)
    https.setConnectTimeout(NET_CONNECT_MS);       // repassado a cada connect (HTTPClient.cpp:1109)
    https.setTimeout(API_TIMEOUT_MS);              // espera máxima por dados (HTTPClient.cpp:1287)
}

static uint16_t clamp_ms(uint32_t dt) { return dt > 65000 ? 65000 : (uint16_t)dt; }

bool fetchUsage(const char* token, const char* modelId, UsageData& out,
                int* httpOut, uint16_t* msOut) {
    *httpOut = -1;
    *msOut = 0;
    out.ok = false;
    if (!token || !modelId || strlen(token) > API_TOKEN_MAX || strlen(modelId) > 64) {
        strlcpy(out.error, "bad_args", sizeof(out.error));
        return false;
    }

    WiFiClientSecure client;
    HTTPClient https;
    if (!https.begin(client, MESSAGES_ENDPOINT)) {
        strlcpy(out.error, "https_init", sizeof(out.error));
        return false;
    }
    net_client_setup(client, https);

    // Authorization num buffer de stack, zerado no fim. O addHeader copia o
    // valor para um String no heap que o HTTPClient libera sem zerar (risco
    // residual declarado no design, Technical Plan §1).
    char auth[220];
    snprintf(auth, sizeof(auth), "Bearer %s", token);
    https.addHeader("Authorization", auth);
    https.addHeader("anthropic-version", ANTHROPIC_VERSION);
    https.addHeader("anthropic-beta", "oauth-2025-04-20");
    https.addHeader("content-type", "application/json");
    https.addHeader("User-Agent", "claude-code/2.1.5");
    https.collectHeaders(RL_HEADERS, RL_HEADER_COUNT);

    char body[160];
    snprintf(body, sizeof(body),
             "{\"model\":\"%s\",\"max_tokens\":1,"
             "\"messages\":[{\"role\":\"user\",\"content\":\".\"}]}", modelId);

    uint32_t t0 = millis();
    int code = https.POST((uint8_t*)body, strlen(body));
    *msOut = clamp_ms(millis() - t0);
    *httpOut = (code == 0) ? -1 : code;   // 0 quer dizer "nunca sondado" em /modelos

    bool has = false;
    if (code > 0) {
        String h5u = https.header(H5U);
        String d7u = https.header(D7U);
        has = h5u.length() > 0 || d7u.length() > 0;
        if (has) {
            out.h5 = h5u.toFloat() * 100.0f;
            out.d7 = d7u.toFloat() * 100.0f;
            out.h5ResetEpoch = (uint32_t)https.header(H5R).toInt();
            out.d7ResetEpoch = (uint32_t)https.header(D7R).toInt();
            out.unifiedResetEpoch = (uint32_t)https.header(URS).toInt();
            out.fallbackPct = https.header(UFB).toFloat() * 100.0f;

            strlcpy(out.statusOverall, https.header(UST).c_str(), sizeof(out.statusOverall));
            strlcpy(out.status5h,      https.header(H5S).c_str(), sizeof(out.status5h));
            strlcpy(out.status7d,      https.header(D7S).c_str(), sizeof(out.status7d));
            strlcpy(out.repClaim,      https.header(URC).c_str(), sizeof(out.repClaim));
            strlcpy(out.overageStatus, https.header(UOS).c_str(), sizeof(out.overageStatus));
            strlcpy(out.overageReason, https.header(UOR).c_str(), sizeof(out.overageReason));
            out.error[0] = 0;
        } else {
            snprintf(out.error, sizeof(out.error), "no_usage_h_%d", code);
        }
    } else {
        snprintf(out.error, sizeof(out.error), "http_%d", *httpOut);
    }

    https.end();
    mbedtls_platform_zeroize(auth, sizeof(auth));   // memset seria otimizado fora
    out.ok = has;
    return has;
}
