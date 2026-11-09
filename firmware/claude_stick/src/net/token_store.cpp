#include "net/token_store.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

namespace {

StaticSemaphore_t s_mxBuf;
SemaphoreHandle_t s_mx = nullptr;

char     s_tokens[ACCT_MAX][TOKEN_BUF_SIZE];
uint16_t s_gen[ACCT_MAX];

char s_pending[TOKEN_BUF_SIZE];
bool s_hasPending = false;

WiFiManager::SavedNet s_nets[MAX_SAVED_NETWORKS];
int s_netsN = 0;

void ensure_mutex() {
    if (!s_mx) s_mx = xSemaphoreCreateMutexStatic(&s_mxBuf);
}

// Trava o mutex no escopo (sem timeout: as seções críticas são só memcpy).
struct Lock {
    Lock()  { ensure_mutex(); xSemaphoreTake(s_mx, portMAX_DELAY); }
    ~Lock() { xSemaphoreGive(s_mx); }
    Lock(const Lock&) = delete;
    Lock& operator=(const Lock&) = delete;
};

bool slot_ok(int slot) { return slot >= 0 && slot < ACCT_MAX; }

// strlen limitado; TOKEN_BUF_SIZE quando passa do máximo.
size_t tok_len(const char* tok) { return tok ? strnlen(tok, TOKEN_BUF_SIZE) : 0; }

void zero(void* p, size_t n) {
    // volatile: o compilador não pode descartar a zeragem de um buffer
    // que não é lido depois.
    volatile uint8_t* v = (volatile uint8_t*)p;
    while (n--) *v++ = 0;
}

// Copia src (NUL-terminado, len < TOKEN_BUF_SIZE) para buf; zera buf se não cabe.
bool copy_out(const char* src, char* buf, size_t bufLen) {
    if (!buf || bufLen == 0) return false;
    size_t len = strnlen(src, TOKEN_BUF_SIZE);
    if (len == 0 || len + 1 > bufLen) { zero(buf, bufLen); return false; }
    memcpy(buf, src, len + 1);
    return true;
}

}  // namespace

void token_store_init() { ensure_mutex(); }

bool token_store_set(int slot, uint16_t gen, const char* tok) {
    if (!slot_ok(slot)) return false;
    size_t len = tok_len(tok);
    Lock l;
    zero(s_tokens[slot], TOKEN_BUF_SIZE);
    s_gen[slot] = gen;
    if (len == 0 || len > TOKEN_MAX_LEN) return false;
    memcpy(s_tokens[slot], tok, len);
    return true;
}

void token_store_clear(int slot, uint16_t newGen) {
    if (!slot_ok(slot)) return;
    Lock l;
    zero(s_tokens[slot], TOKEN_BUF_SIZE);
    s_gen[slot] = newGen;
}

void token_store_clear_all() {
    Lock l;
    zero(s_tokens, sizeof(s_tokens));
    zero(s_nets, sizeof(s_nets));
    s_netsN = 0;
}

bool token_store_copy(int slot, uint16_t gen, char* buf, size_t bufLen) {
    if (!slot_ok(slot)) {
        if (buf && bufLen) zero(buf, bufLen);
        return false;
    }
    Lock l;
    if (s_gen[slot] != gen) {
        if (buf && bufLen) zero(buf, bufLen);
        return false;
    }
    return copy_out(s_tokens[slot], buf, bufLen);
}

bool token_store_set_pending(const char* tok) {
    size_t len = tok_len(tok);
    if (len == 0 || len > TOKEN_MAX_LEN) return false;
    Lock l;
    if (s_hasPending) return false;
    zero(s_pending, sizeof(s_pending));
    memcpy(s_pending, tok, len);
    s_hasPending = true;
    return true;
}

bool token_store_copy_pending(char* buf, size_t bufLen) {
    Lock l;
    if (!s_hasPending) {
        if (buf && bufLen) zero(buf, bufLen);
        return false;
    }
    return copy_out(s_pending, buf, bufLen);
}

bool token_store_take_pending(char* buf, size_t bufLen) {
    Lock l;
    if (!s_hasPending) {
        if (buf && bufLen) zero(buf, bufLen);
        return false;
    }
    bool ok = copy_out(s_pending, buf, bufLen);
    if (ok) {                       // buf pequeno demais: o pendente fica
        zero(s_pending, sizeof(s_pending));
        s_hasPending = false;
    }
    return ok;
}

void token_store_clear_pending() {
    Lock l;
    zero(s_pending, sizeof(s_pending));
    s_hasPending = false;
}

void token_store_set_reconnect(const WiFiManager::SavedNet* nets, int n) {
    if (!nets || n < 0) n = 0;
    if (n > MAX_SAVED_NETWORKS) n = MAX_SAVED_NETWORKS;
    Lock l;
    zero(s_nets, sizeof(s_nets));
    for (int i = 0; i < n; i++) {
        s_nets[i] = nets[i];
        s_nets[i].ssid[sizeof(s_nets[i].ssid) - 1] = '\0';
        s_nets[i].pass[sizeof(s_nets[i].pass) - 1] = '\0';
    }
    s_netsN = n;
}

int token_store_copy_reconnect(WiFiManager::SavedNet* out) {
    if (!out) return 0;
    Lock l;
    for (int i = 0; i < s_netsN; i++) out[i] = s_nets[i];
    return s_netsN;
}
