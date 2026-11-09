#pragma once
// Roteamento do contrato do token bridge pelo rótulo (Technical Plan §8,
// item 6, fase 4). Funções puras, sem Arduino: o web_api passa os campos de
// g_accts (used/label/active) e recebe o slot.
#include <stddef.h>
#include <stdint.h>

// Slot em uso cujo rótulo é exatamente lbl (strcmp, diferenciando maiúsculas,
// ao contrário do makeUnique de accounts.cpp). -1 se não existe ou lbl é nulo.
// labels aponta para n rótulos de lblSize bytes cada (char[n][lblSize]).
int bridge_find_label(const bool* used, const char* labels, size_t lblSize, int n,
                      const char* lbl);

// Slot de destino de GET /window e POST /tokens:
//   lbl nulo ou vazio -> a conta em foco (active, se estiver em uso; senão -1);
//   lbl conhecido     -> o slot com esse rótulo;
//   lbl desconhecido  -> -1 (o handler responde 404).
int bridge_target_slot(const bool* used, const char* labels, size_t lblSize, int n,
                       int active, const char* lbl);

// Copia s para out como conteúdo de string JSON (sem as aspas): escapa aspas e barra
// invertida e troca bytes de controle por \u00XX. Bytes >= 0x80 (UTF-8) passam como
// estão. Trunca sem cortar um escape no meio; out sempre termina em 0.
// Devolve o tamanho escrito.
size_t json_escape(const char* s, char* out, size_t outSize);
