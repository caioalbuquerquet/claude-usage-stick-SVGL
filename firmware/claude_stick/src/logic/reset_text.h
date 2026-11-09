#pragma once
// Texto do reset das janelas nos cards da tela inicial (item 37). Função pura,
// sem Arduino nem LVGL: a hora sai de gmtime_r(epoch + tzOffsetSec), como em
// projection_text, para o teste não depender do fuso da máquina.
#include <stddef.h>
#include <stdint.h>

enum ResetFmt : uint8_t {
    RESET_IN_AT,       // "1h12m · 16:40" (5h, layouts 1 a 3)
    RESET_AT,          // "16:40" (5h, layout 4)
    RESET_WEEKDAY_AT,  // "qui 09:00" (semana)
};

// O reset já passou: relógio válido, resetEpoch conhecido e resetEpoch <= now.
// A leitura guardada é da janela anterior até a próxima consulta.
bool reset_passed(uint32_t resetEpoch, uint32_t now);

// lang: 0 = português, != 0 = inglês. Relógio sem NTP (now < 1e9) ou
// resetEpoch == 0 -> "--". Em RESET_IN_AT o tempo é "%dh%02dm" (como no
// mockup: "0h50m"), "%dd%02dh" a partir de 24 h, e um reset que já passou vira
// "passou · 16:40" / "passed · 16:40" (nunca "agora" parado na tela).
void reset_text(uint32_t resetEpoch, uint32_t now, int32_t tzOffsetSec, uint8_t lang,
                ResetFmt fmt, char* out, size_t outSize);
