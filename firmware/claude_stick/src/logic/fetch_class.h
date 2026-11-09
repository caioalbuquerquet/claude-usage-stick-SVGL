#pragma once
// Classe do resultado de uma consulta, compartilhada entre a lógica pura, a
// rede e o modelo. Só depende de <stdint.h> para compilar também nos testes de
// host. account_state.h inclui este header, e não o contrário.
// FC_DISCARDED nunca entra no AccountState.
#include <stdint.h>

enum FetchClass : uint8_t {
    FC_NONE, FC_OK, FC_AUTH, FC_RATE, FC_ERR, FC_SRV, FC_NET, FC_NOWIFI, FC_NOCLOCK, FC_DISCARDED
};

// Nome sem o prefixo FC_ ("NONE", "OK", "AUTH"...). Usado no card, nos logs e,
// em minúsculas, no `reason` de /token/status.
inline const char* fetch_class_name(FetchClass c) {
    switch (c) {
        case FC_NONE:      return "NONE";
        case FC_OK:        return "OK";
        case FC_AUTH:      return "AUTH";
        case FC_RATE:      return "RATE";
        case FC_ERR:       return "ERR";
        case FC_SRV:       return "SRV";
        case FC_NET:       return "NET";
        case FC_NOWIFI:    return "NOWIFI";
        case FC_NOCLOCK:   return "NOCLOCK";
        case FC_DISCARDED: return "DISCARDED";
    }
    return "?";
}
