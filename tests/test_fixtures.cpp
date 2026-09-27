// Passa respostas REAIS (anonimizadas, tests/fixtures/) pelo parser. Os testes
// sinteticos provam as regras; este prova que o formato real chega nelas.
#include "tinytest.h"
#include "payload.h"

#include <cstdio>
#include <string>

static std::string fixture_dir() {
    std::string f = __FILE__; // run.sh passa caminhos absolutos ao compilador
    return f.substr(0, f.find_last_of("/\\")) + "/fixtures/";
}

static bool read_file(const std::string &path, std::string &out) {
    FILE *fp = std::fopen(path.c_str(), "rb");
    if (!fp) return false;
    char buf[4096];
    size_t n = 0;
    out.clear();
    while ((n = std::fread(buf, 1, sizeof(buf), fp)) > 0) out.append(buf, n);
    std::fclose(fp);
    return true;
}

// Linha 1 de CAPTURA.txt: "<agora local> <fuso em segundos>" no momento da
// captura. Assim o filtro ve o mesmo "hoje" que o app mostrava.
static bool capture_clock(int64_t *nowLocal, int32_t *tz) {
    std::string txt;
    if (!read_file(fixture_dir() + "CAPTURA.txt", txt)) return false;
    long long now = 0;
    long off = 0;
    if (std::sscanf(txt.c_str(), "%lld %ld", &now, &off) != 2) return false;
    *nowLocal = now;
    *tz = (int32_t)off;
    return true;
}

TEST(fixture_da_lista_de_projetos_e_lida) {
    std::string j;
    CHECK(read_file(fixture_dir() + "projects.json", j));
    static char ids[64][TASK_ID_BYTES + 1];
    static char names[64][PROJECT_NAME_BYTES + 1];
    CHECK(payload_parse_projects(j.data(), j.size(), ids, names, 64) > 0);
}

TEST(fixtures_de_listas_reais_passam_pelo_parse) {
    int64_t now = 0;
    int32_t tz = 0;
    CHECK(capture_clock(&now, &tz));

    const char *files[] = {"project_data_1.json", "project_data_2.json",
                           "project_data_3.json", "inbox_data.json"};
    int lidos = 0;
    int doDia = 0;
    for (const char *f : files) {
        std::string j;
        if (!read_file(fixture_dir() + f, j)) continue;
        lidos++;
        static Task buf[TASK_LIST_MAX];
        TaskTop top;
        top_init(top, buf, TASK_LIST_MAX);
        const int n = payload_parse_project(j.data(), j.size(), now, tz, top);
        CHECK(n >= 0);
        doDia += n > 0 ? n : 0;
        for (int i = 0; i < top.count; i++) {
            CHECK(top.items[i].id[0] != '\0');
            CHECK(top.items[i].projectId[0] != '\0');
            const int p = top.items[i].priority;
            CHECK(p == 0 || p == 1 || p == 3 || p == 5);
        }
    }
    CHECK(lidos >= 1);
    // Conferencia humana (Step 5): tem de bater com a visao Hoje do app para
    // as listas capturadas, no momento da captura.
    std::printf("  (fixtures: %d arquivos, %d tarefas do dia)\n", lidos, doDia);
}
