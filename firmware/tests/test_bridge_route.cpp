// Testes de src/logic/bridge_route (rótulo -> slot do contrato do bridge, fase 4).
#include "check.h"
#include "logic/bridge_route.h"

namespace {
const int N = 4;
const size_t L = 17;   // ACCT_LBL_MAX
bool used[N] = {true, false, true, true};
char labels[N][L] = {"Caio", "Velho", "TI", "caio"};
}  // namespace

TEST(bridge_find_label_exato) {
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, "Caio"), 0);
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, "TI"), 2);
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, "caio"), 3);   // diferencia maiúsculas
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, "ti"), -1);
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, "Velho"), -1); // slot livre
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, "Cai"), -1);   // prefixo não vale
    CHECK_EQ(bridge_find_label(used, &labels[0][0], L, N, nullptr), -1);
}

TEST(bridge_target_slot_foco_e_rotulo) {
    // sem rótulo (nulo ou vazio) -> conta em foco
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, 2, nullptr), 2);
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, 2, ""), 2);
    // com rótulo -> o slot dele, mesmo que não seja o foco
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, 2, "Caio"), 0);
    // desconhecido -> -1 (404)
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, 2, "Pessoal"), -1);
    // foco inválido ou em slot livre -> -1
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, 1, ""), -1);
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, -1, nullptr), -1);
    CHECK_EQ(bridge_target_slot(used, &labels[0][0], L, N, 4, nullptr), -1);
}

TEST(json_escape_basico) {
    char b[32];
    CHECK_EQ((int)json_escape("TI", b, sizeof(b)), 2);
    CHECK_STR(b, "TI");
    json_escape("a\"b\\c", b, sizeof(b));
    CHECK_STR(b, "a\\\"b\\\\c");
    json_escape("x\ny", b, sizeof(b));
    CHECK_STR(b, "x\\u000ay");
    json_escape("A\xC3\xA7\xC3\xA3o", b, sizeof(b));   // UTF-8 passa como está
    CHECK_STR(b, "A\xC3\xA7\xC3\xA3o");
    json_escape(nullptr, b, sizeof(b));
    CHECK_STR(b, "");
}

TEST(json_escape_trunca_sem_cortar_escape) {
    char b[5];
    json_escape("ab\"cd", b, sizeof(b));   // "ab" + \" = 4 bytes + 0
    CHECK_STR(b, "ab\\\"");
    char c[4];
    json_escape("ab\"cd", c, sizeof(c));   // \" não cabe inteiro
    CHECK_STR(c, "ab");
}
