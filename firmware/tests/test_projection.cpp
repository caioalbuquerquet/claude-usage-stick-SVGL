// Testes de src/logic/projection (item 9).
#include "check.h"
#include "logic/projection.h"

#include <cstring>

namespace {

// 2023-11-14 00:00:00 UTC; 15:20 UTC desse dia = DAY + 55200.
const uint32_t DAY = 1699920000UL;
const uint32_t NOW = DAY + 12 * 3600;      // 12:00 UTC
const uint32_t RESET = NOW + 3600;         // janela: [NOW - 4h, NOW + 1h]

// Ring buffer igual ao hist_push da v2.3: grava em head, avança, n até HIST_MAX.
struct Ring {
    Sample buf[HIST_MAX];
    int n = 0, head = 0;
    Ring() { std::memset(buf, 0, sizeof(buf)); }
    void push(uint32_t t, uint8_t h5) {
        buf[head] = Sample{t, h5, 0};
        head = (head + 1) % HIST_MAX;
        if (n < HIST_MAX) n++;
    }
    ProjResult proj(float pct, uint32_t reset = RESET, uint32_t now = NOW) const {
        return projection(buf, n, head, pct, reset, now);
    }
};

}  // namespace

TEST(projection_poucas_amostras) {
    Ring r;
    CHECK_EQ(r.proj(10).st, ProjResult::SEM_DADOS);         // 0 amostras: k = 1
    r.push(NOW - 600, 5);
    CHECK_EQ(r.proj(10).st, ProjResult::SEM_DADOS);         // 1 amostra: k = 2
    // amostras fora da janela não contam: t == 0, antes de ws e no futuro
    r.push(0, 5);
    r.push(RESET - 5 * 3600 - 1, 5);
    r.push(NOW + 1, 5);
    CHECK_EQ(r.proj(10).st, ProjResult::SEM_DADOS);
    r.push(NOW - 60, 6);                                    // 2 amostras: k = 3
    CHECK(r.proj(10).st != ProjResult::SEM_DADOS);
    // amostra exatamente em ws e exatamente em now contam
    Ring e;
    e.push(RESET - 5 * 3600, 1);
    e.push(NOW, 2);
    CHECK(e.proj(10).st != ProjResult::SEM_DADOS);
}

TEST(projection_esgotada_antes_de_k) {
    Ring r;
    r.push(NOW - 600, 90);
    ProjResult p = r.proj(99.6f);                            // regra 2 antes da 3
    CHECK_EQ(p.st, ProjResult::ESGOTADA);
    CHECK_EQ(p.etaEpoch, RESET);
    CHECK_EQ(p.minutes, 60);
    CHECK_EQ(p.rate, 0.0f);

    Ring z;                                                  // nem precisa de amostra
    CHECK_EQ(z.proj(100).st, ProjResult::ESGOTADA);
}

TEST(projection_limite_99_5) {
    Ring r;
    r.push(NOW - 1200, 99);
    r.push(NOW - 600, 99);
    CHECK_EQ(r.proj(99.5f).st, ProjResult::ESGOTADA);
    CHECK(r.proj(99.4f).st != ProjResult::ESGOTADA);
}

TEST(projection_sem_relogio_ou_reset) {
    Ring r;
    r.push(NOW - 1200, 10);
    r.push(NOW - 600, 20);
    CHECK_EQ(r.proj(30, 0).st, ProjResult::SEM_DADOS);                  // resetEpoch == 0
    CHECK_EQ(r.proj(99.9f, 0).st, ProjResult::SEM_DADOS);               // regra 1 antes da 2
    CHECK_EQ(r.proj(30, RESET, 999999999UL).st, ProjResult::SEM_DADOS); // now < 1e9
    ProjResult p = r.proj(30, 0);
    CHECK_EQ(p.etaEpoch, 0u);
    CHECK_EQ(p.minutes, 0);
    CHECK_EQ(p.rate, 0.0f);
}

TEST(projection_ring_buffer_dando_a_volta) {
    // head começa no fim do buffer; 10 amostras ocupam 155..159 e 0..4.
    Ring r;
    r.head = 155;
    for (int i = 0; i < HIST_MAX; i++) r.buf[i] = Sample{NOW - 100, 0, 0};   // lixo fora de n
    r.push(NOW - 14000, 0);     // 155: na janela, mas fora dos 45 min
    r.push(NOW - 3000, 0);      // 156: idem
    r.push(NOW - 1800, 40);     // 157: primeira dos últimos 45 min -> taxa
    for (int i = 0; i < 7; i++) r.push(NOW - 1500 + i * 60, 50);   // 158..159, 0..4
    CHECK_EQ(r.head, 5);
    CHECK_EQ(r.n, 10);
    ProjResult p = r.proj(70);  // (70 - 40) / 30 min = 1 %/min
    CHECK_EQ(p.rate, 1.0f);
    CHECK_EQ(p.st, ProjResult::ESGOTA);
    CHECK_EQ(p.etaEpoch, NOW + 1800);
    CHECK_EQ(p.minutes, 30);

    // mesmas amostras em ordem linear dão o mesmo resultado
    Ring lin;
    lin.push(NOW - 14000, 0);
    lin.push(NOW - 3000, 0);
    lin.push(NOW - 1800, 40);
    for (int i = 0; i < 7; i++) lin.push(NOW - 1500 + i * 60, 50);
    ProjResult q = lin.proj(70);
    CHECK_EQ(q.st, p.st);
    CHECK_EQ(q.etaEpoch, p.etaEpoch);
    CHECK_EQ(q.rate, p.rate);

    // buffer cheio (n == HIST_MAX): a mais antiga é a que está em head
    Ring full;
    for (int i = 0; i < HIST_MAX + 20; i++) full.push(NOW - 2400 + i * 10, 10);
    CHECK_EQ(full.n, HIST_MAX);
    // a mais antiga mantida é a de índice 20: t = NOW - 2200, h5 = 10
    ProjResult f = full.proj(32);   // (32 - 10) / (2200 / 60) = 0.6 %/min
    CHECK(f.rate > 0.5999f && f.rate < 0.6001f);
}

TEST(projection_primeira_amostra_recente_taxa_zero) {
    Ring r;
    r.push(NOW - 200, 10);      // primeira dos 45 min com menos de 300 s
    r.push(NOW - 100, 20);
    ProjResult p = r.proj(30);
    CHECK_EQ(p.rate, 0.0f);
    CHECK_EQ(p.st, ProjResult::OK_ATE_RESET);
    CHECK_EQ(p.etaEpoch, RESET);
    CHECK_EQ(p.minutes, 60);
    CHECK_EQ(p.pctAtReset, 30);

    Ring b;                     // exatamente 300 s: ainda taxa 0 (now > t + 300)
    b.push(NOW - 300, 10);
    b.push(NOW - 100, 20);
    CHECK_EQ(b.proj(30).rate, 0.0f);
}

TEST(projection_taxa_no_limite_0_02) {
    // (97.5 - 97) / 25 min = 0.02 %/min: não entra na regra 5
    const uint32_t reset = NOW + 130 * 60;
    Ring r;
    r.push(NOW - 1500, 97);
    r.push(NOW - 600, 97);
    ProjResult p = r.proj(97.5f, reset);
    CHECK_EQ(p.rate, 0.02f);
    CHECK_EQ(p.st, ProjResult::OK_ATE_RESET);
    CHECK_EQ(p.minutes, 130);
    CHECK_EQ(p.pctAtReset, 100);                // 97.5 + 0.02 * 130 = 100.1, limitado

    // logo acima: 0.6 / 25 = 0.024 %/min, esgota em 2.4 / 0.024 = 100 min
    ProjResult q = r.proj(97.6f, reset);
    CHECK(q.rate > 0.02f);
    CHECK_EQ(q.st, ProjResult::ESGOTA);
    CHECK(q.minutes >= 99 && q.minutes <= 100);
    CHECK(q.etaEpoch >= NOW + 99 * 60 && q.etaEpoch <= NOW + 100 * 60);
}

TEST(projection_ok_ate_reset_com_taxa) {
    // 1 %/min mas o reset vem antes: 70 + 0.5 * 50 = 95
    const uint32_t reset = NOW + 50 * 60;
    Ring r;
    r.push(NOW - 1800, 55);
    r.push(NOW - 600, 65);
    ProjResult p = r.proj(70, reset);           // (70 - 55) / 30 = 0.5 %/min
    CHECK_EQ(p.rate, 0.5f);
    CHECK_EQ(p.st, ProjResult::OK_ATE_RESET);   // esgotaria em 60 min > 50
    CHECK_EQ(p.etaEpoch, reset);
    CHECK_EQ(p.minutes, 50);
    CHECK_EQ(p.pctAtReset, 95);

    // eta exatamente no reset ainda é ESGOTA (eta <= resetEpoch)
    ProjResult q = r.proj(70, NOW + 60 * 60);
    CHECK_EQ(q.st, ProjResult::ESGOTA);
    CHECK_EQ(q.minutes, 60);

    // taxa negativa (uso caiu): pctAtReset = pctNow
    Ring d;
    d.push(NOW - 1800, 80);
    d.push(NOW - 600, 60);
    ProjResult n = d.proj(50.4f);
    CHECK(n.rate < 0);
    CHECK_EQ(n.st, ProjResult::OK_ATE_RESET);
    CHECK_EQ(n.pctAtReset, 50);
}

TEST(projection_text_exato) {
    char b[64];
    ProjResult ok = {ProjResult::OK_ATE_RESET, RESET, 60, 78, 0.1f};
    projection_text(ok, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x93 n\xC3\xA3o esgota \xC2\xB7 ~78%");
    projection_text(ok, 1, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x93 ok to reset \xC2\xB7 ~78%");

    const uint32_t at1520 = DAY + 55200;
    ProjResult e38 = {ProjResult::ESGOTA, at1520, 38, 0, 1.0f};
    projection_text(e38, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 esgota 15:20 \xC2\xB7 38m");
    projection_text(e38, 1, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 runs out 15:20 \xC2\xB7 38m");

    ProjResult e65 = {ProjResult::ESGOTA, at1520, 65, 0, 1.0f};
    projection_text(e65, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 esgota 15:20 \xC2\xB7 1h05m");
    projection_text(e65, 1, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 runs out 15:20 \xC2\xB7 1h05m");

    // 60 minutos já usa o formato com hora
    ProjResult e60 = {ProjResult::ESGOTA, at1520, 60, 0, 1.0f};
    projection_text(e60, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 esgota 15:20 \xC2\xB7 1h00m");

    // corte por maxChars = 20: o " · <tempo>" sai
    projection_text(e65, 0, 0, 20, b, sizeof(b));            // 22 code points inteiro
    CHECK_STR(b, "\xE2\x9C\x97 esgota 15:20");
    projection_text(e38, 1, 0, 20, b, sizeof(b));            // 22 code points inteiro
    CHECK_STR(b, "\xE2\x9C\x97 runs out 15:20");
    projection_text(e38, 0, 0, 20, b, sizeof(b));            // 20 code points: cabe
    CHECK_STR(b, "\xE2\x9C\x97 esgota 15:20 \xC2\xB7 38m");
    projection_text(ok, 0, 0, 20, b, sizeof(b));             // 19 code points: cabe
    CHECK_STR(b, "\xE2\x9C\x93 n\xC3\xA3o esgota \xC2\xB7 ~78%");

    // fuso: a hora sai de gmtime(eta + tzOffsetSec)
    projection_text(e38, 0, -3 * 3600, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 esgota 12:20 \xC2\xB7 38m");

    ProjResult gone = {ProjResult::ESGOTADA, RESET, 60, 100, 0};
    projection_text(gone, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 esgotada");
    projection_text(gone, 1, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 exhausted");

    ProjResult none = {ProjResult::SEM_DADOS, 0, 0, 0, 0};
    projection_text(none, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x80\x94 sem hist\xC3\xB3rico");
    projection_text(none, 1, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x80\x94 no history");
}

TEST(projection_ponta_a_ponta) {
    // 1 %/min a partir de 40% às 11:30 -> 70% às 12:00, esgota às 12:30 UTC
    Ring r;
    r.push(NOW - 1800, 40);
    r.push(NOW - 600, 60);
    ProjResult p = r.proj(70);
    char b[64];
    projection_text(p, 0, 0, 60, b, sizeof(b));
    CHECK_STR(b, "\xE2\x9C\x97 esgota 12:30 \xC2\xB7 30m");
}
