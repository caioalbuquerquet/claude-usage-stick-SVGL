// Tela de abertura (ver screen_splash.h). Desenho aprovado: "Loading A" do
// canvas de mockups (pássaro, 18 px, logo, 18 px, pontos; fundo C_BG).
//
// Custo por quadro: com DISP_BANDS=1 o envio vai da linha nativa 0 até a
// coluna mais à esquerda tocada (479 - x). Por isso um único timer de
// SPLASH_FRAME_MS move a faixa e os pontos juntos (um envio por quadro), e só
// as colunas da faixa (antes e agora) são redesenhadas. O logo fica em
// x = 108..371, então o pior quadro manda ~372 linhas (~30 ms).
#include "ui/screen_splash.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <lvgl.h>
#include "hw/frame_stats.h"
#include "ui/assets_thep.h"
#include "ui/theme.h"

namespace {

constexpr int LOGO_W = 264, LOGO_H = 96;
constexpr int BIRD = 56, GAP = 18;
constexpr int DOT = 6, DOT_GAP = 10, DOTS_H = 8, NDOTS = 3;
constexpr uint32_t C_DOT = 0x2fb5dd;              // thep-cyan
constexpr uint32_t DOT_CYCLE_MS = 1800, DOT_STAGGER_MS = 300;
constexpr lv_opa_t DOT_OPA_MIN = 64;              // 25%

// Faixa de brilho (CSS: linear-gradient 105deg, pico #e2f0ff a 85%, some em
// ~10% da largura do gradiente de cada lado). As linhas da faixa inclinam
// 15° da vertical: x + tan(15°)·y = constante (o topo fica mais à direita).
constexpr uint32_t C_SHINE = 0xe2f0ff;
constexpr uint32_t SHINE_PASS_MS = 2000;          // uma passada da faixa
constexpr int SHINE_PEAK = 217;                   // 85% de 255
constexpr int SHINE_HW = 71;                      // meia largura, em px
constexpr int SHINE_K256 = 69;                    // tan(15°) * 256
constexpr int SHINE_SKEW = (SHINE_K256 * (LOGO_H - 1)) / 256;   // 25 px
// Uma passada, da esquerda para a direita como no mockup (background-position
// de 130% a -30%), com a faixa entrando e saindo inteira do logo.
constexpr int SHINE_C0 = -SHINE_HW;
constexpr int SHINE_C1 = LOGO_W - 1 + SHINE_SKEW + SHINE_HW;

lv_obj_t* s_root = nullptr;
lv_obj_t* s_logo = nullptr;
lv_obj_t* s_dots[NDOTS] = {};
lv_opa_t s_dotOpa[NDOTS] = {};
lv_timer_t* s_timer = nullptr;
uint8_t* s_buf = nullptr;          // RGB565A8: LOGO_W*LOGO_H*2 de cor + LOGO_W*LOGO_H de alfa
lv_image_dsc_t s_dsc;
uint16_t s_band[3];                // cor de cada faixa em RGB565
int s_prevX0 = 0, s_prevX1 = -1;   // colunas da faixa no quadro anterior
uint32_t s_t0 = 0;
uint32_t s_frames = 0;
AppState s_next = ST_PIN;

uint16_t rgb565(uint32_t c) {
    return (uint16_t)(((c >> 8) & 0xF800) | ((c >> 5) & 0x07E0) | ((c >> 3) & 0x001F));
}

// ease-in-out do CSS, cubic-bezier(.42, 0, .58, 1); t e o retorno em 0..1024
int32_t ease(int32_t t) {
    return lv_cubic_bezier(t, LV_BEZIER_VAL_FLOAT(0.42), 0, LV_BEZIER_VAL_FLOAT(0.58),
                           LV_BEZIER_VAL_MAX);
}

// Monta o buffer: alfa do A1 (bit 7 = pixel da esquerda) e a cor da faixa.
bool logo_alloc() {
    const size_t n = (size_t)LOGO_W * LOGO_H;
    s_buf = (uint8_t*)heap_caps_malloc(n * 3, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_buf) return false;
    const uint32_t bands[3] = THEP_LOGO_SPLASH_BANDS;
    for (int i = 0; i < 3; i++) s_band[i] = rgb565(bands[i]);
    uint16_t* px = (uint16_t*)s_buf;
    uint8_t* a = s_buf + n * 2;
    const uint8_t* src = img_thep_logo_splash.data;
    const uint32_t stride = img_thep_logo_splash.header.stride;
    for (int y = 0; y < LOGO_H; y++) {
        uint16_t c = s_band[y / THEP_LOGO_SPLASH_BAND_H];
        for (int x = 0; x < LOGO_W; x++) {
            bool on = src[y * stride + x / 8] & (0x80 >> (x % 8));
            px[y * LOGO_W + x] = on ? c : 0;
            a[y * LOGO_W + x] = on ? 0xFF : 0;
        }
    }
    memset(&s_dsc, 0, sizeof(s_dsc));
    s_dsc.header.magic = LV_IMAGE_HEADER_MAGIC;
    s_dsc.header.cf = LV_COLOR_FORMAT_RGB565A8;
    s_dsc.header.w = LOGO_W;
    s_dsc.header.h = LOGO_H;
    s_dsc.header.stride = LOGO_W * 2;
    s_dsc.data_size = n * 3;
    s_dsc.data = s_buf;
    return true;
}

// Colunas tocadas pela faixa com centro em c (x0 > x1: fora do logo).
void band_cols(int c, int* x0, int* x1) {
    *x0 = c - SHINE_HW - SHINE_SKEW;
    *x1 = c + SHINE_HW;
    if (*x0 < 0) *x0 = 0;
    if (*x1 > LOGO_W - 1) *x1 = LOGO_W - 1;
}

// Reescreve as colunas x0..x1: cor da faixa misturada com o brilho em c.
void shine_cols(int c, int x0, int x1) {
    const int sr = (C_SHINE >> 19) & 0x1F, sg = (C_SHINE >> 10) & 0x3F, sb = (C_SHINE >> 3) & 0x1F;
    const int hw = SHINE_HW * 256;
    uint16_t* px = (uint16_t*)s_buf;
    const uint8_t* a = s_buf + (size_t)LOGO_W * LOGO_H * 2;
    for (int y = 0; y < LOGO_H; y++) {
        const uint16_t base = s_band[y / THEP_LOGO_SPLASH_BAND_H];
        const int br = base >> 11, bg = (base >> 5) & 0x3F, bb = base & 0x1F;
        const int row = y * LOGO_W;
        for (int x = x0; x <= x1; x++) {
            if (!a[row + x]) continue;
            int d = x * 256 + SHINE_K256 * y - c * 256;
            if (d < 0) d = -d;
            if (d >= hw) { px[row + x] = base; continue; }
            int k = SHINE_PEAK * (hw - d) / hw;          // 0..217
            int r = br + (sr - br) * k / 255;
            int g = bg + (sg - bg) * k / 255;
            int b = bb + (sb - bb) * k / 255;
            px[row + x] = (uint16_t)(r << 11 | g << 5 | b);
        }
    }
}

// Pontos: opacidade 25% -> 100% (40% do ciclo) -> 25%, ease-in-out em cada
// trecho, ciclo de 1,8 s, 0,3 s de atraso entre um e outro.
lv_opa_t dot_opa(int32_t t) {
    if (t < 0) return DOT_OPA_MIN;
    int32_t ph = (int32_t)((uint32_t)t % DOT_CYCLE_MS) * LV_BEZIER_VAL_MAX / (int32_t)DOT_CYCLE_MS;
    const int32_t up = LV_BEZIER_VAL_FLOAT(0.4);
    int32_t e = ph < up ? ease(ph * LV_BEZIER_VAL_MAX / up)
                        : LV_BEZIER_VAL_MAX - ease((ph - up) * LV_BEZIER_VAL_MAX / (LV_BEZIER_VAL_MAX - up));
    return (lv_opa_t)(DOT_OPA_MIN + (LV_OPA_COVER - DOT_OPA_MIN) * e / LV_BEZIER_VAL_MAX);
}

void frame(uint32_t elapsed) {
    if (elapsed > SPLASH_MS) elapsed = SPLASH_MS;
    if (s_logo && s_buf) {
        // Duas passadas de SHINE_PASS_MS (2 s) nos 4 s, pedido do dono; no
        // fim a faixa fica fora do logo (posição final da 2ª passada).
        uint32_t inPass = elapsed >= SPLASH_MS ? SHINE_PASS_MS : elapsed % SHINE_PASS_MS;
        int32_t e = ease((int32_t)(inPass * LV_BEZIER_VAL_MAX / SHINE_PASS_MS));
        int c = SHINE_C0 + (SHINE_C1 - SHINE_C0) * e / LV_BEZIER_VAL_MAX;
        int x0, x1;
        band_cols(c, &x0, &x1);
        // redesenha a faixa de agora e devolve a cor às colunas da anterior
        int u0 = x0, u1 = x1;
        if (s_prevX1 >= s_prevX0) {
            if (u1 < u0) { u0 = s_prevX0; u1 = s_prevX1; }
            else { u0 = LV_MIN(u0, s_prevX0); u1 = LV_MAX(u1, s_prevX1); }
        }
        if (u1 >= u0) {
            shine_cols(c, u0, u1);
            lv_area_t a;
            lv_obj_get_coords(s_logo, &a);
            a.x2 = a.x1 + u1;
            a.x1 += u0;
            lv_obj_invalidate_area(s_logo, &a);
        }
        s_prevX0 = x0;
        s_prevX1 = x1;
    }
    for (int i = 0; i < NDOTS; i++) {
        if (!s_dots[i]) continue;
        lv_opa_t o = dot_opa((int32_t)elapsed - i * (int32_t)DOT_STAGGER_MS);
        if (o == s_dotOpa[i]) continue;
        s_dotOpa[i] = o;
        lv_obj_set_style_bg_opa(s_dots[i], o, 0);
    }
}

void timer_cb(lv_timer_t*) {
    uint32_t elapsed = millis() - s_t0;
    frame(elapsed);
    s_frames++;
    if (elapsed < SPLASH_MS) return;
    FrameWindow fw = frame_window_take();
    Serial.printf("[SPLASH] %lu ms, %lu passos, %lu quadros, frame_max %lu ms, flush_max %lu ms, "
                  "rows_max %lu\n",
                  (unsigned long)elapsed, (unsigned long)s_frames, (unsigned long)fw.frames,
                  (unsigned long)((fw.frameMaxUs + 999) / 1000),
                  (unsigned long)((fw.flushMaxUs + 999) / 1000), (unsigned long)fw.rowsMax);
    lv_timer_delete(s_timer);
    s_timer = nullptr;
    request_state(s_next);
}

lv_obj_t* box(lv_obj_t* parent) {
    lv_obj_t* o = lv_obj_create(parent);
    lv_obj_remove_style_all(o);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE);
    return o;
}

}  // namespace

void screen_splash_build() {
    screen_splash_reset();
    lv_obj_t* scr = lv_screen_active();
    s_root = box(scr);
    lv_obj_set_size(s_root, LV_PCT(100), LV_PCT(100));

    // pilha centrada: pássaro, GAP, logo, GAP, pontos
    const int stackH = BIRD + GAP + LOGO_H + GAP + DOTS_H;
    const int top = (lv_obj_get_height(scr) - stackH) / 2;

    lv_obj_t* bird = lv_image_create(s_root);
    lv_image_set_src(bird, &img_bird_braille_56);
    lv_obj_align(bird, LV_ALIGN_TOP_MID, 0, top);

    if (logo_alloc()) {
        s_logo = lv_image_create(s_root);
        lv_image_set_src(s_logo, &s_dsc);
        lv_obj_align(s_logo, LV_ALIGN_TOP_MID, 0, top + BIRD + GAP);
    } else {
        Serial.println("[SPLASH] sem PSRAM para o logo");
    }

    const int dotsW = NDOTS * DOT + (NDOTS - 1) * DOT_GAP;
    const int dotY = top + BIRD + GAP + LOGO_H + GAP + (DOTS_H - DOT) / 2;
    for (int i = 0; i < NDOTS; i++) {
        lv_obj_t* d = box(s_root);
        lv_obj_set_size(d, DOT, DOT);
        lv_obj_set_style_radius(d, LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_bg_color(d, lv_color_hex(C_DOT), 0);
        lv_obj_set_style_bg_opa(d, DOT_OPA_MIN, 0);
        lv_obj_align(d, LV_ALIGN_TOP_MID, -dotsW / 2 + DOT / 2 + i * (DOT + DOT_GAP), dotY);
        s_dots[i] = d;
        s_dotOpa[i] = DOT_OPA_MIN;
    }
}

void screen_splash_start(AppState next) {
    s_next = next;
    if (!s_root) { request_state(next); return; }
    s_t0 = millis();
    s_frames = 0;
    s_prevX0 = 0;
    s_prevX1 = -1;
    frame_window_take();      // a janela de quadros passa a contar só a animação
    if (s_timer) lv_timer_delete(s_timer);
    s_timer = lv_timer_create(timer_cb, SPLASH_FRAME_MS, nullptr);
}

void screen_splash_reset() {
    if (s_timer) lv_timer_delete(s_timer);
    s_timer = nullptr;
    // apaga os objetos antes do buffer que o logo desenha
    if (s_root) lv_obj_delete(s_root);
    s_root = s_logo = nullptr;
    for (int i = 0; i < NDOTS; i++) s_dots[i] = nullptr;
    if (s_buf) heap_caps_free(s_buf);
    s_buf = nullptr;
}
