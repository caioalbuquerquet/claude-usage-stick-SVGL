#pragma once
// Rótulo de conta (model/accounts.cpp). Funções puras, sem Arduino. O
// tools/token_bridge.py manda o rótulo em UTF-8 cru e o firmware compara com
// strcmp (logic/bridge_route), então o rótulo salvo precisa guardar os bytes
// UTF-8 como vieram. O formato na NVS não muda (string; a v2.3 ainda lê).
#include <stddef.h>

// Copia `in` para out (outSize bytes, com o '\0') aceitando UTF-8 bem
// formado de U+0020 a U+10FFFF. Descarta caracteres de controle (C0, DEL e
// C1), '"', '\\', '<' e '>', e sequências malformadas (byte solto, overlong,
// surrogate, acima de U+10FFFF, sequência cortada). Corta em outSize - 1
// bytes sem partir um caractere no meio e tira os espaços do fim.
void label_sanitize(const char* in, char* out, size_t outSize);

// out = base + " N" (makeUnique), cortando a base numa fronteira de caractere
// UTF-8 para o sufixo caber em outSize - 1 bytes. base pode ser out.
void label_with_suffix(const char* base, int n, char* out, size_t outSize);

// Bytes do maior prefixo de s com no máximo maxBytes bytes que termina numa
// fronteira de caractere UTF-8.
size_t utf8_fit_bytes(const char* s, size_t maxBytes);
