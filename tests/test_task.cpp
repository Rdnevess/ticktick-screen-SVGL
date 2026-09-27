#include "tinytest.h"
#include "task.h"

#include <string>
#include <cstring>

TEST(titulo_ascii_curto_passa_intacto) {
    Task t{};
    task_set_title(t, "Pagar a conta de luz");
    CHECK_EQ_STR(t.title, "Pagar a conta de luz");
}

TEST(titulo_com_acento_cabe_intacto) {
    Task t{};
    task_set_title(t, "Revisão do contrato — ação urgente");
    CHECK_EQ_STR(t.title, "Revisão do contrato — ação urgente");
}

TEST(titulo_de_exatamente_80_bytes_cabe_inteiro) {
    std::string src(78, 'a');
    src += "ç"; // 2 bytes -> 80 no total
    Task t{};
    task_set_title(t, src.c_str());
    CHECK_EQ_INT(std::strlen(t.title), 80);
    CHECK_EQ_STR(t.title, src.c_str());
}

TEST(titulo_de_81_bytes_descarta_o_caractere_inteiro) {
    std::string src(79, 'a');
    src += "ç"; // 2 bytes -> 81 no total, o corte cairia no meio do c-cedilha
    Task t{};
    task_set_title(t, src.c_str());
    // 79 bytes: o c-cedilha sai INTEIRO, nao pela metade
    CHECK_EQ_INT(std::strlen(t.title), 79);
    CHECK_EQ_INT((unsigned char)t.title[78], (unsigned char)'a');
}

TEST(titulo_ascii_longo_corta_em_80) {
    std::string src(200, 'x');
    Task t{};
    task_set_title(t, src.c_str());
    CHECK_EQ_INT(std::strlen(t.title), 80);
}

TEST(titulo_nulo_ou_vazio_resulta_em_string_vazia) {
    Task t{};
    task_set_title(t, "");
    CHECK_EQ_STR(t.title, "");
    task_set_title(t, nullptr);
    CHECK_EQ_STR(t.title, "");
}

TEST(utf8_truncate_len_nunca_para_no_meio_de_caractere) {
    // "áéí" = 6 bytes (2 cada). Pedindo 5, deve devolver 4.
    CHECK_EQ_INT(utf8_truncate_len("áéí", 5), 4);
    CHECK_EQ_INT(utf8_truncate_len("áéí", 6), 6);
    CHECK_EQ_INT(utf8_truncate_len("áéí", 99), 6);
    // Travessao "—" = 3 bytes. Pedindo 2, devolve 0.
    CHECK_EQ_INT(utf8_truncate_len("—", 2), 0);
}

TEST(titulo_perde_emoji_e_colapsa_o_espaco_que_sobra) {
    // A fonte so tem Latin-1 e poucos simbolos: emoji viraria caixa vazia.
    Task t{};
    task_set_title(t, "Ligar \xF0\x9F\x93\x9E para João");
    CHECK_EQ_STR(t.title, "Ligar para João");
}

TEST(titulo_mantem_os_simbolos_que_a_fonte_tem) {
    Task t{};
    task_set_title(t, "Revisão — “final” • 50° …");
    CHECK_EQ_STR(t.title, "Revisão — “final” • 50° …");
}

TEST(titulo_com_utf8_malformado_descarta_so_os_bytes_ruins) {
    Task t{};
    // C3 sem byte de continuacao; FF nunca e valido em UTF-8. As literais
    // ficam separadas para o compilador nao ler "\xC3cd" como um hex so.
    task_set_title(t, "ab\xC3" "cd\xFF" "ef");
    CHECK_EQ_STR(t.title, "abcdef");
}

TEST(quebra_de_linha_e_tab_viram_um_espaco_e_as_pontas_somem) {
    Task t{};
    task_set_title(t, "  linha1\n\tlinha2  ");
    CHECK_EQ_STR(t.title, "linha1 linha2");
}

TEST(emoji_removido_nao_conta_no_limite_de_80_bytes) {
    std::string s(79, 'a');
    s += "\xF0\x9F\x98\x80";
    s += "b";
    Task t{};
    task_set_title(t, s.c_str());
    CHECK_EQ_INT(std::strlen(t.title), 80);
    CHECK(t.title[79] == 'b');
}

TEST(title_sanitize_com_buffer_pequeno_para_em_fronteira) {
    char buf[4];
    // "ação": a(1) ç(2) ã(2) o(1). Cabem 3 bytes: "aç".
    CHECK_EQ_INT(title_sanitize(buf, sizeof(buf), "ação"), 3);
    CHECK_EQ_STR(buf, "aç");
}
