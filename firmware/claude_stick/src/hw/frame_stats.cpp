// Tempo de quadro do display (item 34). Ver frame_stats.h.
// Tudo roda no loop() (dono do LVGL); só o log do agendador lê de fora, e
// também do loop().
#include "hw/frame_stats.h"
#include <Arduino.h>

namespace {

const char* s_mode = "full";
uint32_t s_refrStartUs = 0;
bool s_inRefr = false;
uint32_t s_flushUs = 0;      // soma do flush no quadro em andamento
bool s_flushed = false;      // o quadro em andamento chamou o flush

FrameWindow s_win = {0, 0, 0};
uint32_t s_frameMaxUs = 0;
uint32_t s_flushMaxUs = 0;

uint32_t us_to_ms_ceil(uint32_t us) { return (us + 999) / 1000; }

}  // namespace

void frame_set_mode(const char* mode) { s_mode = mode ? mode : "?"; }
const char* frame_mode() { return s_mode; }

void frame_refr_start() {
    s_refrStartUs = micros();
    s_inRefr = true;
    s_flushUs = 0;
    s_flushed = false;
}

void frame_refr_end() {
    if (!s_inRefr) return;
    s_inRefr = false;
    if (!s_flushed) return;   // nada foi desenhado nesta passada
    uint32_t d = micros() - s_refrStartUs;
    s_win.frames++;
    if (d > s_win.frameMaxUs) s_win.frameMaxUs = d;
    if (s_flushUs > s_win.flushMaxUs) s_win.flushMaxUs = s_flushUs;
    if (d > s_frameMaxUs) s_frameMaxUs = d;
    if (s_flushUs > s_flushMaxUs) s_flushMaxUs = s_flushUs;
}

void frame_flush_add(uint32_t us) {
    s_flushUs += us;
    s_flushed = true;
}

FrameWindow frame_window_take() {
    FrameWindow w = s_win;
    s_win = FrameWindow{0, 0, 0};
    return w;
}

uint32_t frame_ms_max() { return us_to_ms_ceil(s_frameMaxUs); }
uint32_t frame_flush_ms_max() { return us_to_ms_ceil(s_flushMaxUs); }
