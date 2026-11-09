#pragma once
#include <stdint.h>
#include <stddef.h>

// Token cifrado com AES-256-GCM; chave derivada do PIN (PBKDF2-ish via SHA-256).
// ciphertext[512] cabe folgado o token OAuth (sk-ant-oat01-..., ~110 chars).
struct EncryptedBlob {
    uint8_t  iv[12];
    uint8_t  tag[16];
    uint8_t  salt[6];
    uint16_t len;
    uint8_t  ciphertext[512];
};

void deriveKey(const char* pin, const uint8_t* salt, size_t saltLen, uint8_t* keyOut32);
bool encryptToken(const char* plaintext, const char* pin, EncryptedBlob& blob);
bool decryptToken(const EncryptedBlob& blob, const char* pin, char* plainOut, size_t plainMaxLen);

// ── Chave da sessão (v3) ─────────────────────────────────
// A v3 deriva a chave UMA vez no desbloqueio e a usa para todos os slots.
// O formato do blob não muda: salt = MAC (o mesmo que encryptToken grava),
// então blobs da v2.3 abrem aqui e blobs da v3 abrem na v2.3 (rollback).
// Quem chama zera a chave (memset) ao sair da sessão.

// deriveKey(pin, MAC, 6, keyOut). Mesmo custo de KDF_ROUNDS do encryptToken.
void deriveSessionKey(const char* pin, uint8_t keyOut[32]);
// Igual a encryptToken, sem derivar: blob.salt = MAC, IV novo aleatório.
// Falso se o texto não cabe em blob.ciphertext ou se a cifra falhar.
bool encryptTokenWithKey(const char* plaintext, const uint8_t key[32], EncryptedBlob& blob);
// Decifra com a chave pronta. Falso se blob.len >= plainMaxLen, se blob.len
// passa do ciphertext ou se a tag não confere; nesses casos plainOut é zerado.
bool decryptTokenWithKey(const EncryptedBlob& blob, const uint8_t key[32],
                         char* plainOut, size_t plainMaxLen);
