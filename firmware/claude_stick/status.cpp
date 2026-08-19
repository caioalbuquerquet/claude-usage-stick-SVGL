#include "status.h"
#include "config.h"
#include "certs.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

// Procura "impact":"<nivel>" no corpo ja em minusculas. O Statuspage manda
// JSON compacto, mas aceitar o espaco depois dos dois-pontos sai de graca.
static bool hasImpact(const String& body, const char* level) {
    String n = String("\"impact\":\"") + level + "\"";
    if (body.indexOf(n) >= 0) return true;
    n = String("\"impact\": \"") + level + "\"";
    return body.indexOf(n) >= 0;
}

bool fetchModelStatus(ModelStatus& out) {
    WiFiClientSecure client;
    client.setCACert(CA_BUNDLE);

    HTTPClient https;
    if (!https.begin(client, STATUS_ENDPOINT)) {
        Serial.println("[STATUS] https_init failed");
        return false;
    }

    https.addHeader("User-Agent", "claude-usage-stick/1.0");
    https.setTimeout(API_TIMEOUT_MS);
    https.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.printf("[STATUS] GET %s\n", STATUS_ENDPOINT);
    int code = https.GET();
    Serial.printf("[STATUS] HTTP %d\n", code);

    if (code != 200) {
        https.end();
        return false;   // mantém o último estado conhecido
    }

    int len = https.getSize();
    if (len > 131072) {
        Serial.printf("[STATUS] body too large (%d)\n", len);
        https.end();
        return false;
    }

    String body = https.getString();
    https.end();

    body.toLowerCase();
    out.haikuUp  = body.indexOf("haiku")  < 0;
    out.sonnetUp = body.indexOf("sonnet") < 0;
    out.opusUp   = body.indexOf("opus")   < 0;
    out.fableUp  = body.indexOf("fable")  < 0;

    // Versao logo depois do nome da familia. Percorre as ocorrencias ate achar
    // uma seguida de numero: "claude opus" solto num link nao serve, mas
    // "claude opus 4.6" no titulo serve.
    static const char* FAM[4] = {"haiku", "sonnet", "opus", "fable"};
    for (int i = 0; i < 4; i++) {
        out.version[i][0] = 0;
        int flen = strlen(FAM[i]), at = 0;
        while ((at = body.indexOf(FAM[i], at)) >= 0) {
            int p = at + flen;
            while (p < (int)body.length() && body[p] == ' ') p++;
            int n = 0;
            while (p < (int)body.length() && n < 7 &&
                   (isdigit((unsigned char)body[p]) || body[p] == '.'))
                out.version[i][n++] = body[p++];
            while (n && out.version[i][n - 1] == '.') n--;   // "4." nao vale
            out.version[i][n] = 0;
            if (n) break;
            at += flen;
        }
    }

    // maior gravidade vence: um incidente critical manda na cor do dialog
    out.severity = hasImpact(body, "critical") ? SEV_CRITICAL
                 : hasImpact(body, "major")    ? SEV_MAJOR
                 : hasImpact(body, "minor")    ? SEV_MINOR
                                               : SEV_NONE;
    out.ok = true;

    Serial.printf("[STATUS] haiku:%d sonnet:%d opus:%d fable:%d sev:%d ver:%s/%s/%s/%s\n",
                  out.haikuUp, out.sonnetUp, out.opusUp, out.fableUp, out.severity,
                  out.version[0], out.version[1], out.version[2], out.version[3]);
    return true;
}
