#pragma once
#include <stdint.h>

// Uso de rate-limit do Claude (unified), extraído dos headers da resposta.
// Headers validados contra a conta real (ver sondagem): todos os unified-*.
struct UsageData {
    float    h5;                 // utilização 5h em % (0–100)
    float    d7;                 // utilização 7d em % (0–100)
    uint32_t h5ResetEpoch;       // unix ts do reset da janela 5h
    uint32_t d7ResetEpoch;       // unix ts do reset da janela 7d
    uint32_t unifiedResetEpoch;  // unix ts do reset da janela representativa

    char     statusOverall[16];  // allowed | allowed_warning | rejected
    char     status5h[16];       // allowed | rejected | ...
    char     status7d[16];
    char     repClaim[20];       // five_hour | seven_day (quem é o gargalo)
    float    fallbackPct;        // 0–100 (fallback-percentage * 100)
    char     overageStatus[16];  // allowed | rejected
    char     overageReason[32];  // out_of_credits | org_level_disabled | ...

    bool     ok;                 // true se o fetch teve sucesso
    char     error[64];          // mensagem de erro se ok=false
};

// Configura CA e timeouts de uma requisição HTTPS. Chamar DEPOIS de
// https.begin(client, url). Usado por fetchUsage e fetchModelStatus.
//   - handshake TLS: NET_HANDSHAKE_S (segundos; padrão do core seria 120 s)
//   - conexão TCP:   NET_CONNECT_MS (o HTTPClient repassa a cada connect e
//                    sobrescreve o do client, por isso não se usa
//                    client.setConnectionTimeout)
//   - leitura:       API_TIMEOUT_MS (espera máxima por dados, não o total)
// WiFiClientSecure é typedef de NetworkClientSecure (core 3.x).
class NetworkClientSecure;
class HTTPClient;
void net_client_setup(NetworkClientSecure& client, HTTPClient& https);

// Maior token aceito (o Authorization vai num char[220] de stack).
#define API_TOKEN_MAX 199

// POST /v1/messages (max_tokens:1) com `modelId`. Lê os headers unified-*.
// Devolve true se a resposta trouxe headers unified-* (hasUnifiedHeaders);
// nesse caso `out` é preenchido, inclusive em 401/403/429. Não classifica:
// a classe sai de classify_usage(http, hasHeaders).
//   *httpOut: sempre definido. Código HTTP, ou negativo em erro de transporte
//             (HTTPC_ERROR_*; falha no begin ou token inválido = -1). Nunca 0.
//   *msOut:   latência da requisição em ms (teto 65000).
// out.ok = hasUnifiedHeaders; out.error recebe "http_<n>"/"no_usage_h_<n>" sem
// headers. Não loga (o log com [slot N] fica com quem chama).
bool fetchUsage(const char* token, const char* modelId, UsageData& out,
                int* httpOut, uint16_t* msOut);

// Resultado da sonda de UM modelo (o USAGE da conta em foco usa o modelo do
// rodízio). 200 = disponível · 429 = limitado · 5xx = falha na API.
struct ProbeResult {
    int      code;   // código HTTP (0 = nunca sondado, <0 = erro de rede)
    uint16_t ms;     // latência da requisição
};
