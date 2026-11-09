#pragma once
// Alerta de incidente do status.claude.com (item 40): overlay no
// lv_layer_top, sobre qualquer conteúdo do shell, com o pássaro em braille
// recolorido em cinza e o chip de gravidade. Mesmas regras da v2.3: some no
// toque e só volta quando o incidente é resolvido e outro surge, ou quando a
// gravidade escala.
#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>

// Abre/fecha conforme a última leitura do status (hook do poller e /agora).
void incident_sync();
// Palavra do "impact" do Statuspage ("CRÍTICO", "GRAVE", "LEVE"…).
const char* severity_label(uint8_t sev);
// "Opus 4.6 • Sonnet 4.5" (ou "Todos"); false = nenhum modelo citado.
bool affected_models(char* out, size_t sz);
// Raiz do overlay (nullptr = fechado), para o overlay_restack().
lv_obj_t* incident_obj();
// Esquece o overlay (o app_state limpa o lv_layer_top numa troca de estado).
void incident_forget();
