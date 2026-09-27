#include "tinytest.h"
#include "creds.h"

#include <cstring>
#include <string>

// {"cid":"c1","csec":"s1","atok":"a1","rtok":"r1","exp":1790208000}
static const char *BLOB =
    "eyJjaWQiOiJjMSIsImNzZWMiOiJzMSIsImF0b2siOiJhMSIsInJ0b2siOiJyMSIsImV4cCI6MTc5MDIwODAwMH0=";

TEST(base64_decodifica_e_ignora_quebras_de_linha) {
    // O mesmo blob colado do terminal com quebra de linha e espacos.
    std::string quebrado = std::string(BLOB).substr(0, 20) + "\r\n  " +
                           std::string(BLOB).substr(20);
    uint8_t out[128];
    int n = base64_decode(quebrado.c_str(), out, sizeof(out));
    CHECK(n > 0);
    CHECK(std::memcmp(out, "{\"cid\":\"c1\"", 11) == 0);
}

TEST(base64_aceita_a_variante_url) {
    uint8_t a[4], b[4];
    CHECK_EQ_INT(base64_decode("+/8=", a, sizeof(a)), 2);
    CHECK_EQ_INT(base64_decode("-_8=", b, sizeof(b)), 2);
    CHECK_EQ_INT(a[0], 0xFB);
    CHECK_EQ_INT(a[1], 0xFF);
    CHECK(std::memcmp(a, b, 2) == 0);
}

TEST(base64_recusa_caractere_invalido_e_saida_pequena) {
    uint8_t out[64];
    CHECK_EQ_INT(base64_decode("ab$d", out, sizeof(out)), -1);
    CHECK_EQ_INT(base64_decode(BLOB, out, 4), -1);
    CHECK_EQ_INT(base64_decode(nullptr, out, sizeof(out)), -1);
}

TEST(blob_do_helper_vira_creds) {
    Creds c{};
    CHECK(creds_from_blob(BLOB, c));
    CHECK_EQ_STR(c.cid, "c1");
    CHECK_EQ_STR(c.csec, "s1");
    CHECK_EQ_STR(c.atok, "a1");
    CHECK_EQ_STR(c.rtok, "r1");
    CHECK_EQ_INT(c.exp, 1790208000LL);
    CHECK(creds_valid(c));
}

TEST(blob_sem_rtok_e_aceito) {
    // {"cid":"c1","csec":"s1","atok":"a1"}
    Creds c{};
    CHECK(creds_from_blob("eyJjaWQiOiJjMSIsImNzZWMiOiJzMSIsImF0b2siOiJhMSJ9", c));
    CHECK_EQ_STR(c.rtok, "");
    CHECK_EQ_INT(c.exp, 0);
}

TEST(blob_sem_secret_e_recusado) {
    // {"cid":"c1","atok":"a1"}
    Creds c{};
    CHECK(!creds_from_blob("eyJjaWQiOiJjMSIsImF0b2siOiJhMSJ9", c));
    CHECK(!creds_from_blob("isso nao e base64 de json!", c));
}

TEST(token_maior_que_o_campo_e_recusado_em_vez_de_truncado) {
    // Token truncado seria um token errado que parece certo.
    std::string grande(CRED_TOKEN_BYTES + 1, 'x');
    std::string json = "{\"cid\":\"c\",\"csec\":\"s\",\"atok\":\"" + grande + "\"}";
    Creds c{};
    CHECK(!creds_from_json(json.c_str(), json.size(), c));
}

TEST(json_da_nvs_ida_e_volta) {
    Creds a{};
    CHECK(creds_from_blob(BLOB, a));
    char buf[CREDS_JSON_MAX];
    int n = creds_to_json(a, buf, sizeof(buf));
    CHECK(n > 0);
    Creds b{};
    CHECK(creds_from_json(buf, (size_t)n, b));
    CHECK_EQ_STR(b.atok, "a1");
    CHECK_EQ_STR(b.rtok, "r1");
    CHECK_EQ_INT(b.exp, 1790208000LL);
}

TEST(json_que_nao_cabe_devolve_menos_um) {
    Creds a{};
    CHECK(creds_from_blob(BLOB, a));
    char pequeno[16];
    CHECK_EQ_INT(creds_to_json(a, pequeno, sizeof(pequeno)), -1);
}

TEST(resposta_de_token_com_tudo_atualiza_os_tres_campos) {
    Creds c{};
    CHECK(creds_from_blob(BLOB, c));
    const char *r = R"({"access_token":"novo","refresh_token":"rnovo","expires_in":1000})";
    CHECK(creds_apply_token_response(c, r, std::strlen(r), 5000));
    CHECK_EQ_STR(c.atok, "novo");
    CHECK_EQ_STR(c.rtok, "rnovo");
    CHECK_EQ_INT(c.exp, 6000);
}

TEST(resposta_de_token_sem_refresh_mantem_o_antigo_e_usa_150_dias) {
    Creds c{};
    CHECK(creds_from_blob(BLOB, c));
    const char *r = R"({"access_token":"novo"})";
    CHECK(creds_apply_token_response(c, r, std::strlen(r), 5000));
    CHECK_EQ_STR(c.rtok, "r1");
    CHECK_EQ_INT(c.exp, 5000 + CREDS_DEFAULT_LIFETIME_S);
}

TEST(resposta_de_token_sem_hora_deixa_exp_desconhecido) {
    Creds c{};
    CHECK(creds_from_blob(BLOB, c));
    const char *r = R"({"access_token":"novo","refresh_token":"rnovo","expires_in":1000})";
    CHECK(creds_apply_token_response(c, r, std::strlen(r), 0));
    CHECK_EQ_STR(c.atok, "novo");
    CHECK_EQ_INT(c.exp, 0);
}

TEST(resposta_de_token_de_erro_nao_toca_nas_creds) {
    Creds c{};
    CHECK(creds_from_blob(BLOB, c));
    const char *r = R"({"error":"invalid_grant"})";
    CHECK(!creds_apply_token_response(c, r, std::strlen(r), 5000));
    CHECK_EQ_STR(c.atok, "a1");
    CHECK_EQ_INT(c.exp, 1790208000LL);
}

TEST(wipe_zera_tudo) {
    Creds c{};
    CHECK(creds_from_blob(BLOB, c));
    creds_wipe(c);
    CHECK_EQ_STR(c.atok, "");
    CHECK_EQ_STR(c.csec, "");
    CHECK_EQ_INT(c.exp, 0);
    CHECK(!creds_valid(c));
}
