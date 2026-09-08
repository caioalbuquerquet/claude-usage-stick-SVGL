#pragma once
#include <stdint.h>

// Saúde dos modelos via status.claude.com. Incidentes não resolvidos citam a
// família do modelo no texto ("Elevated errors on Claude Opus 4.6"), então um
// scan de palavra-chave é todo o parse necessário.
// Gravidade reportada pelo Statuspage no campo "impact" de cada incidente.
// Ordem importa: incident_sync() compara para detectar escalada.
enum IncidentSeverity : uint8_t {
    SEV_NONE = 0, SEV_MINOR = 1, SEV_MAJOR = 2, SEV_CRITICAL = 3
};

struct ModelStatus {
    bool haikuUp;
    bool sonnetUp;
    bool opusUp;
    bool fableUp;
    bool ok;            // true depois que ao menos um fetch teve sucesso
    uint8_t severity;   // maior "impact" entre os incidentes nao resolvidos
    // versao citada logo apos o nome da familia ("opus 4.6" -> "4.6"), na
    // ordem haiku/sonnet/opus/fable. Vazio = incidente nao disse a versao.
    char version[4][8];
};

bool fetchModelStatus(ModelStatus& out);
