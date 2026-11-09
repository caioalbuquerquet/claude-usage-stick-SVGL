#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "config.h"

#define MAX_SAVED_NETWORKS 3

class WiFiManager {
public:
    struct NetworkInfo {
        char ssid[33];
        int rssi;
        bool open;
    };

    // Rede salva (SSID + senha). Pública na v3 para a cópia do RECONNECT
    // (token_store), que a net_task lê sem tocar no NVS.
    struct SavedNet {
        char ssid[33];
        char pass[65];
    };

    bool begin() {
        _prefs.begin("wifi", false);
        _loadAll();
        WiFi.mode(WIFI_STA);
        WiFi.disconnect();
        return true;
    }

    // Tenta cada rede salva até uma conectar
    bool autoConnect(int timeout_ms = 10000) {
        for (int i = 0; i < _count; i++) {
            Serial.printf("WiFi: trying '%s' (%d/%d)...\n", _nets[i].ssid, i + 1, _count);
            WiFi.begin(_nets[i].ssid, _nets[i].pass);

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < (unsigned long)timeout_ms) {
                delay(100);
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("WiFi: connected to '%s'! IP=%s\n",
                    _nets[i].ssid, WiFi.localIP().toString().c_str());
                if (i > 0) _promote(i);
                return true;
            }
            WiFi.disconnect();
        }
        Serial.println("WiFi: no saved network available");
        return false;
    }

    bool connectTo(const char *ssid, const char *pass, int timeout_ms = 15000) {
        Serial.printf("WiFi: connecting to '%s'...\n", ssid);
        WiFi.begin(ssid, pass);

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < (unsigned long)timeout_ms) {
            delay(100);
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("WiFi: connected! IP=%s\n", WiFi.localIP().toString().c_str());
            _addNetwork(ssid, pass);
            return true;
        }
        Serial.println("WiFi: connection failed");
        WiFi.disconnect();
        return false;
    }

    // Scan assíncrono (tela de WiFi, no loop(), sem bloquear): scanStart()
    // dispara e scanPoll() devolve -1 enquanto roda, ou quantas redes copiou
    // (0 se o scan falhou ou passou do tempo do core).
    bool scanStart() {
        int r = WiFi.scanNetworks(true);
        return r == WIFI_SCAN_RUNNING || r >= 0;
    }
    int scanPoll(NetworkInfo *results, int max_results) {
        int n = WiFi.scanComplete();
        if (n == WIFI_SCAN_RUNNING) return -1;
        if (n < 0) { WiFi.scanDelete(); return 0; }
        int count = min(n, max_results);
        for (int i = 0; i < count; i++) {
            strncpy(results[i].ssid, WiFi.SSID(i).c_str(), 32);
            results[i].ssid[32] = '\0';
            results[i].rssi = WiFi.RSSI(i);
            results[i].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
        }
        WiFi.scanDelete();
        return count;
    }

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    String getIP() { return WiFi.localIP().toString(); }
    String getSSID() { return WiFi.SSID(); }

    String getSavedSSID() {
        if (_count > 0) return String(_nets[0].ssid);
        return "";
    }

    int getSavedCount() { return _count; }
    const char *getSavedSSID(int idx) {
        if (idx >= 0 && idx < _count) return _nets[idx].ssid;
        return "";
    }

    void disconnect() { WiFi.disconnect(); }

    bool connectSaved(int idx, int timeout_ms = 10000) {
        if (idx < 0 || idx >= _count) return false;
        return connectTo(_nets[idx].ssid, _nets[idx].pass, timeout_ms);
    }

    // ── v3: WiFi sem NVS para a net_task ─────────────────
    // Copia as redes salvas (ordem de preferência) e devolve quantas.
    // Chamada no loop(), dono do NVS, antes de enfileirar um RECONNECT.
    int copySaved(SavedNet out[MAX_SAVED_NETWORKS]) {
        for (int i = 0; i < _count; i++) out[i] = _nets[i];
        return _count;
    }

    // Tenta cada rede da lista, em ordem, como autoConnect(), mas SEM
    // _promote/_saveAll: não toca no NVS nem no estado do objeto. É a única
    // função de WiFi chamada pela net_task. *winnerIdx = índice que conectou
    // (-1 se nenhuma). A senha nunca é logada.
    static bool connectList(const SavedNet* nets, int n, int timeoutMs, int* winnerIdx) {
        if (winnerIdx) *winnerIdx = -1;
        if (!nets || n <= 0) return false;
        if (n > MAX_SAVED_NETWORKS) n = MAX_SAVED_NETWORKS;
        for (int i = 0; i < n; i++) {
            if (nets[i].ssid[0] == '\0') continue;
            Serial.printf("WiFi: trying '%s' (%d/%d)...\n", nets[i].ssid, i + 1, n);
            WiFi.begin(nets[i].ssid, nets[i].pass);

            unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED && millis() - start < (unsigned long)timeoutMs) {
                delay(100);
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("WiFi: connected to '%s'! IP=%s\n",
                    nets[i].ssid, WiFi.localIP().toString().c_str());
                if (winnerIdx) *winnerIdx = i;
                return true;
            }
            WiFi.disconnect();
        }
        Serial.println("WiFi: no saved network available");
        return false;
    }

    // Grava (ou atualiza e promove) a rede no topo da lista, no NVS. Chamada
    // pelo loop() quando o WIFI_JOIN da tela de WiFi conecta.
    void addNetwork(const char *ssid, const char *pass) { _addNetwork(ssid, pass); }

    // Promove a rede idx para o topo e grava no NVS. Chamada pelo loop()
    // quando o resultado do RECONNECT chega (netIdx). idx <= 0 não faz nada.
    void promote(int idx) { _promote(idx); }

    void forgetNetwork(int idx) {
        if (idx < 0 || idx >= _count) return;
        for (int i = idx; i < _count - 1; i++) _nets[i] = _nets[i + 1];
        _count--;
        _saveAll();
        Serial.printf("WiFi: forgot network at index %d, %d remaining\n", idx, _count);
    }

    void forgetAll() {
        _count = 0;
        _prefs.clear();
        Serial.println("WiFi: all networks forgotten");
    }

private:
    Preferences _prefs;

    SavedNet _nets[MAX_SAVED_NETWORKS];
    int _count = 0;

    void _loadAll() {
        _count = _prefs.getInt("count", 0);
        if (_count > MAX_SAVED_NETWORKS) _count = MAX_SAVED_NETWORKS;
        for (int i = 0; i < _count; i++) {
            char ks[8], kp[8];
            snprintf(ks, sizeof(ks), "s%d", i);
            snprintf(kp, sizeof(kp), "p%d", i);
            String s = _prefs.getString(ks, "");
            String p = _prefs.getString(kp, "");
            strncpy(_nets[i].ssid, s.c_str(), 32); _nets[i].ssid[32] = '\0';
            strncpy(_nets[i].pass, p.c_str(), 64); _nets[i].pass[64] = '\0';
        }
    }

    void _saveAll() {
        _prefs.putInt("count", _count);
        for (int i = 0; i < _count; i++) {
            char ks[8], kp[8];
            snprintf(ks, sizeof(ks), "s%d", i);
            snprintf(kp, sizeof(kp), "p%d", i);
            _prefs.putString(ks, _nets[i].ssid);
            _prefs.putString(kp, _nets[i].pass);
        }
    }

    void _addNetwork(const char *ssid, const char *pass) {
        for (int i = 0; i < _count; i++) {
            if (strcmp(_nets[i].ssid, ssid) == 0) {
                strncpy(_nets[i].pass, pass, 64);
                if (i > 0) _promote(i);
                _saveAll();
                return;
            }
        }
        int slots = min(_count + 1, MAX_SAVED_NETWORKS);
        for (int i = slots - 1; i > 0; i--) {
            _nets[i] = _nets[i - 1];
        }
        strncpy(_nets[0].ssid, ssid, 32); _nets[0].ssid[32] = '\0';
        strncpy(_nets[0].pass, pass, 64); _nets[0].pass[64] = '\0';
        _count = slots;
        _saveAll();
    }

    void _promote(int idx) {
        if (idx <= 0 || idx >= _count) return;
        SavedNet tmp = _nets[idx];
        for (int i = idx; i > 0; i--) _nets[i] = _nets[i - 1];
        _nets[0] = tmp;
        _saveAll();
    }
};

extern WiFiManager g_wifi;

#endif // WIFI_MANAGER_H
