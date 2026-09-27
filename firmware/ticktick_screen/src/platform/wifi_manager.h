// Ate 3 redes salvas na NVS "wifi" (spec 6.3). Adaptado de
// claude-usage-stick-SVGL/firmware/claude_stick/wifi_manager.h, que funciona
// nesta mesma placa; ficou so o que este projeto usa, e o scan passou a
// descartar redes ocultas e SSIDs repetidos (varios APs da mesma rede).
#ifndef PLATFORM_WIFI_MANAGER_H
#define PLATFORM_WIFI_MANAGER_H

#include <Arduino.h>
#include <Preferences.h>
#include <WiFi.h>

#include "../../config.h"

#define MAX_SAVED_NETWORKS 3

class WiFiManager {
public:
    struct NetworkInfo {
        char ssid[33];
        int rssi;
        bool open;
    };

    bool begin() {
        _prefs.begin(NVS_WIFI, false);
        _loadAll();
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true); // queda no meio do dia: o driver religa sozinho
        WiFi.disconnect();
        return true;
    }

    // Tenta cada rede salva ate uma conectar. Bloqueia ate timeout_ms POR REDE;
    // `tick` e chamado a cada ~100 ms para quem chama bombear o LVGL.
    bool autoConnect(int timeout_ms, void (*tick)(const char *ssid, int idx, int total)) {
        for (int i = 0; i < _count; i++) {
            Serial.printf("[wifi] tentando '%s' (%d/%d)\n", _nets[i].ssid, i + 1, _count);
            WiFi.begin(_nets[i].ssid, _nets[i].pass);
            const unsigned long start = millis();
            while (WiFi.status() != WL_CONNECTED &&
                   millis() - start < (unsigned long)timeout_ms) {
                if (tick) tick(_nets[i].ssid, i + 1, _count);
                delay(100);
            }
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("[wifi] conectado a '%s', IP %s\n", _nets[i].ssid,
                              WiFi.localIP().toString().c_str());
                if (i > 0) _promote(i);
                return true;
            }
            WiFi.disconnect();
        }
        Serial.println("[wifi] nenhuma rede salva respondeu");
        return false;
    }

    // Conecta numa rede nova e, dando certo, salva como a primeira da lista.
    bool connectTo(const char *ssid, const char *pass, int timeout_ms,
                   void (*tick)() = nullptr) {
        Serial.printf("[wifi] conectando a '%s'\n", ssid);
        WiFi.begin(ssid, pass);
        const unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED &&
               millis() - start < (unsigned long)timeout_ms) {
            if (tick) tick();
            delay(100);
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.printf("[wifi] conectado, IP %s\n", WiFi.localIP().toString().c_str());
            _addNetwork(ssid, pass);
            return true;
        }
        Serial.println("[wifi] falhou");
        WiFi.disconnect();
        return false;
    }

    // Redes visiveis, mais forte primeiro (ordem do driver), sem ocultas e sem
    // SSID repetido.
    int scanNetworks(NetworkInfo *results, int max_results) {
        const int n = WiFi.scanNetworks();
        int count = 0;
        for (int i = 0; i < n && count < max_results; i++) {
            const String ssid = WiFi.SSID(i);
            if (ssid.length() == 0) continue;
            bool dup = false;
            for (int k = 0; k < count; k++) {
                if (ssid == results[k].ssid) { dup = true; break; }
            }
            if (dup) continue;
            strlcpy(results[count].ssid, ssid.c_str(), sizeof(results[count].ssid));
            results[count].rssi = WiFi.RSSI(i);
            results[count].open = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN);
            count++;
        }
        WiFi.scanDelete();
        return count;
    }

    bool isConnected() { return WiFi.status() == WL_CONNECTED; }
    String getIP() { return WiFi.localIP().toString(); }
    String getSSID() { return WiFi.SSID(); }
    int getRSSI() { return WiFi.RSSI(); }
    int getSavedCount() { return _count; }

    void forgetAll() {
        _count = 0;
        _prefs.clear();
        Serial.println("[wifi] redes salvas apagadas");
    }

private:
    struct SavedNet {
        char ssid[33];
        char pass[65];
    };

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
            strlcpy(_nets[i].ssid, _prefs.getString(ks, "").c_str(), sizeof(_nets[i].ssid));
            strlcpy(_nets[i].pass, _prefs.getString(kp, "").c_str(), sizeof(_nets[i].pass));
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
                strlcpy(_nets[i].pass, pass, sizeof(_nets[i].pass));
                if (i > 0) _promote(i);
                _saveAll();
                return;
            }
        }
        const int slots = min(_count + 1, MAX_SAVED_NETWORKS);
        for (int i = slots - 1; i > 0; i--) _nets[i] = _nets[i - 1];
        strlcpy(_nets[0].ssid, ssid, sizeof(_nets[0].ssid));
        strlcpy(_nets[0].pass, pass, sizeof(_nets[0].pass));
        _count = slots;
        _saveAll();
    }

    void _promote(int idx) {
        if (idx <= 0 || idx >= _count) return;
        const SavedNet tmp = _nets[idx];
        for (int i = idx; i > 0; i--) _nets[i] = _nets[i - 1];
        _nets[0] = tmp;
        _saveAll();
    }
};

// Instancia unica, criada no primeiro uso.
inline WiFiManager &wifi() {
    static WiFiManager w;
    return w;
}

#endif // PLATFORM_WIFI_MANAGER_H
