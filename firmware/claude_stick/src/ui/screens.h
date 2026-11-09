#pragma once
// Conteúdos do shell em src/ui/screen_*.cpp (item 39). Cada um monta dentro
// de shell_content() (438 x 214) e é registrado no app_state pelo .ino.
// As outras telas ainda vivem no .ino e entram aqui na próxima onda.
#include <stddef.h>
#include <stdint.h>
#include <lvgl.h>

// /agora (item 36): um card por slot salvo, nos 4 layouts do item 37.
void screen_home_build(lv_obj_t* parent);
// Chegou resultado da net_task: reescreve só os valores. Se o conjunto de
// slots salvos mudou, pede a recriação do conteúdo (shell_show(HOME)).
void screen_home_refresh();
// Uma vez por segundo: contagens, idade dos cards em falha.
void screen_home_tick();

// /modelos (item 38): lista em texto, uma linha por modelo.
void screen_models_build(lv_obj_t* parent);
void screen_models_refresh();

// Esquecem os ponteiros de objetos (chamadas no app_on_ui_reset do .ino,
// antes de qualquer limpeza de conteúdo).
void screen_home_reset();
void screen_models_reset();

// Ainda no .ino até o overlay_incident.cpp (item 40).
const char* severity_label(uint8_t sev);
bool affected_models(char* out, size_t sz);
