#include "sec/crypto.h"
#include "config.h"
#include "mbedtls/gcm.h"
#include "mbedtls/sha256.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_random.h"
#include <string.h>

void deriveKey(const char* pin, const uint8_t* salt, size_t saltLen, uint8_t* keyOut32) {
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);

    uint8_t hash[32];
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)pin, strlen(pin));
    mbedtls_sha256_update(&ctx, salt, saltLen);
    mbedtls_sha256_finish(&ctx, hash);

    for (int i = 1; i < KDF_ROUNDS; i++) {
        mbedtls_sha256_starts(&ctx, 0);
        mbedtls_sha256_update(&ctx, hash, 32);
        mbedtls_sha256_finish(&ctx, hash);
    }

    mbedtls_sha256_free(&ctx);
    memcpy(keyOut32, hash, 32);
    memset(hash, 0, sizeof(hash));
}

void deriveSessionKey(const char* pin, uint8_t keyOut[32]) {
    uint8_t mac[6];
    esp_efuse_mac_get_default(mac);
    deriveKey(pin, mac, sizeof(mac), keyOut);
}

bool encryptTokenWithKey(const char* plaintext, const uint8_t key[32], EncryptedBlob& blob) {
    size_t ptLen = strlen(plaintext);
    if (ptLen > sizeof(blob.ciphertext)) return false;

    esp_efuse_mac_get_default(blob.salt);
    esp_fill_random(blob.iv, sizeof(blob.iv));

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret == 0) {
        ret = mbedtls_gcm_crypt_and_tag(
            &gcm, MBEDTLS_GCM_ENCRYPT, ptLen,
            blob.iv, sizeof(blob.iv),
            NULL, 0,
            (const uint8_t*)plaintext,
            blob.ciphertext,
            sizeof(blob.tag), blob.tag
        );
    }
    mbedtls_gcm_free(&gcm);

    blob.len = (uint16_t)ptLen;
    return (ret == 0);
}

bool encryptToken(const char* plaintext, const char* pin, EncryptedBlob& blob) {
    // blob.salt = MAC, como antes: deriveSessionKey usa o mesmo salt.
    uint8_t key[32];
    deriveSessionKey(pin, key);
    bool ok = encryptTokenWithKey(plaintext, key, blob);
    memset(key, 0, sizeof(key));
    return ok;
}

bool decryptTokenWithKey(const EncryptedBlob& blob, const uint8_t key[32],
                         char* plainOut, size_t plainMaxLen) {
    if (plainMaxLen == 0) return false;
    if (blob.len > sizeof(blob.ciphertext) || blob.len > plainMaxLen - 1) {
        memset(plainOut, 0, plainMaxLen);
        return false;
    }

    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    int ret = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, key, 256);
    if (ret == 0) {
        ret = mbedtls_gcm_auth_decrypt(
            &gcm, blob.len,
            blob.iv, sizeof(blob.iv),
            NULL, 0,
            blob.tag, sizeof(blob.tag),
            blob.ciphertext,
            (uint8_t*)plainOut
        );
    }
    mbedtls_gcm_free(&gcm);

    if (ret != 0) {
        memset(plainOut, 0, plainMaxLen);
        return false;
    }
    plainOut[blob.len] = '\0';
    return true;
}

bool decryptToken(const EncryptedBlob& blob, const char* pin, char* plainOut, size_t plainMaxLen) {
    // Usa o salt gravado no blob (não o MAC), como na v2.3.
    if (plainMaxLen == 0 || blob.len > plainMaxLen - 1) return false;

    uint8_t key[32];
    deriveKey(pin, blob.salt, sizeof(blob.salt), key);
    bool ok = decryptTokenWithKey(blob, key, plainOut, plainMaxLen);
    memset(key, 0, sizeof(key));
    return ok;
}
