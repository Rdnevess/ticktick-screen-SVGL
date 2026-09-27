#include "page_pin.h"

#include <Arduino.h>
#include <cstdio>
#include <cstring>

#include "../../config.h"
#include "../app/app.h"
#include "../app/pairing.h"
#include "../net/net_worker.h"
#include "../net/oauth_store.h"
#include "../platform/clock.h"
#include "../platform/console.h"
#include "i18n.h"
#include "page_pair.h"
#include "theme.h"

enum class Mode : uint8_t { Choose, Set, Confirm, Enter, NewSet, NewConfirm, Verify };

static Mode s_mode = Mode::Choose;
static lv_obj_t *s_scr = nullptr;
static lv_obj_t *s_dots = nullptr;
static lv_obj_t *s_msg = nullptr;
static char s_entry[PIN_LEN + 1];
static char s_first[PIN_LEN + 1];
static bool s_submitPending = false;
static bool s_skipPending = false;
static bool s_choosePinPending = false;
static int64_t s_lockUntilMs = 0;
static int s_lockShown = -1;
static bool s_adminRemove = false; // modo do Settings pedido por prepare()
static bool s_backPending = false;

static bool admin_mode() {
    return s_mode == Mode::NewSet || s_mode == Mode::NewConfirm || s_mode == Mode::Verify;
}

static void on_back(lv_event_t *e) {
    (void)e;
    s_backPending = true;
}

// Teclado numerico em font_pt_24: "<" e "OK" em texto, porque LV_SYMBOL_*
// nao existem nas font_pt_*.
static const char *KEYMAP[] = {"1", "2", "3", "\n", "4", "5", "6", "\n",
                               "7", "8", "9", "\n", "<", "0", "OK", ""};

// Zera memoria pelo ponteiro volatile, que o otimizador nao remove (mesmo
// padrao de core/creds.cpp: os digitos do PIN sao segredo enquanto na RAM).
static void secure_zero(void *p, size_t n) {
    volatile unsigned char *vp = reinterpret_cast<volatile unsigned char *>(p);
    for (size_t i = 0; i < n; i++) vp[i] = 0;
}

static void wipe_entry() { secure_zero(s_entry, sizeof(s_entry)); }

static void update_dots() {
    char d[PIN_LEN * 2 + 1] = {0};
    const size_t n = std::strlen(s_entry);
    for (int i = 0; i < PIN_LEN; i++) {
        d[i * 2] = (size_t)i < n ? '*' : '_';
        d[i * 2 + 1] = (i < PIN_LEN - 1) ? ' ' : '\0';
    }
    lv_label_set_text(s_dots, d);
}

static void set_msg(const char *txt, uint32_t color) {
    lv_label_set_text(s_msg, txt);
    lv_obj_set_style_text_color(s_msg, theme_color(color), 0);
}

static bool locked() { return s_lockUntilMs != 0 && clock_uptime_ms() < s_lockUntilMs; }

static void on_key(lv_event_t *e) {
    if (s_submitPending || locked()) return;
    lv_obj_t *bm = (lv_obj_t *)lv_event_get_target(e);
    const char *txt =
        lv_buttonmatrix_get_button_text(bm, lv_buttonmatrix_get_selected_button(bm));
    if (!txt) return;
    const size_t len = std::strlen(s_entry);
    if (std::strcmp(txt, "<") == 0) {
        if (len > 0) s_entry[len - 1] = '\0';
    } else if (std::strcmp(txt, "OK") == 0) {
        if (len == PIN_LEN) s_submitPending = true;
    } else if (len < PIN_LEN) {
        s_entry[len] = txt[0];
        s_entry[len + 1] = '\0';
        if (len + 1 == PIN_LEN) s_submitPending = true; // envia ao completar
    }
    update_dots();
}

static void build_keypad(lv_obj_t *scr, const char *title, const char *msg) {
    lv_obj_t *t = ui_label(scr, title, &font_pt_24, COL_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_LEFT, 20, 16);

    s_dots = ui_label(scr, "", &font_pt_36, COL_ACCENT);
    lv_obj_align(s_dots, LV_ALIGN_TOP_LEFT, 20, 70);

    s_msg = ui_label(scr, msg, &font_pt_14, COL_DIM);
    lv_obj_set_width(s_msg, 180);
    lv_label_set_long_mode(s_msg, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_msg, LV_ALIGN_TOP_LEFT, 20, 130);

    lv_obj_t *bm = lv_buttonmatrix_create(scr);
    lv_buttonmatrix_set_map(bm, KEYMAP);
    lv_obj_set_size(bm, 260, 300);
    lv_obj_align(bm, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bm, 0, 0);
    lv_obj_set_style_text_font(bm, &font_pt_24, LV_PART_ITEMS);
    lv_obj_set_style_bg_color(bm, theme_color(COL_SURFACE), LV_PART_ITEMS);
    lv_obj_set_style_text_color(bm, theme_color(COL_TEXT), LV_PART_ITEMS);
    lv_obj_add_event_cb(bm, on_key, LV_EVENT_VALUE_CHANGED, nullptr);

    if (admin_mode()) { // pelo Settings: da para desistir
        lv_obj_t *back = ui_back_button(scr);
        lv_obj_align(back, LV_ALIGN_BOTTOM_LEFT, 20, -14);
        lv_obj_add_event_cb(back, on_back, LV_EVENT_CLICKED, nullptr);
    }
    wipe_entry();
    update_dots();
}

// Remonta a pagina num modo de teclado. So no tick: nunca de dentro do callback
// do objeto que vai ser destruido.
static void rebuild_as(Mode m, const char *title, const char *msg) {
    lv_obj_clean(s_scr);
    s_mode = m;
    build_keypad(s_scr, title, msg);
}

// ---- Set: escolher, definir, confirmar ----

static void on_choose_pin(lv_event_t *e) {
    (void)e;
    s_choosePinPending = true;
}

static void on_skip(lv_event_t *e) {
    (void)e;
    s_skipPending = true;
}

void page_pin_set_build(lv_obj_t *scr) {
    s_scr = scr;
    s_mode = Mode::Choose;
    secure_zero(s_first, sizeof(s_first));

    lv_obj_t *t = ui_label(scr, TRS("Proteger com PIN?", "Protect with a PIN?"), &font_pt_28, COL_TEXT);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 24);

    lv_obj_t *why = ui_label(scr,
        TRS("Com PIN, as credenciais ficam cifradas e o aparelho pede o PIN toda vez que "
            "ligar. Sem PIN, ele liga direto.",
            "With a PIN, the credentials are encrypted and the device asks for it every "
            "time it starts. Without one, it starts straight away."),
        &font_pt_18, COL_DIM);
    lv_obj_set_width(why, SCREEN_WIDTH - 60);
    lv_label_set_long_mode(why, LV_LABEL_LONG_WRAP);
    lv_obj_set_style_text_align(why, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(why, LV_ALIGN_TOP_MID, 0, 76);

    lv_obj_t *set = ui_button(scr, TRS("Definir PIN", "Set PIN"), 200, 64, COL_ACCENT);
    lv_obj_align(set, LV_ALIGN_BOTTOM_LEFT, 30, -30);
    lv_obj_add_event_cb(set, on_choose_pin, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *skip = ui_button(scr, TRS("Pular", "Skip"), 200, 64, COL_SURFACE);
    lv_obj_align(skip, LV_ALIGN_BOTTOM_RIGHT, -30, -30);
    lv_obj_add_event_cb(skip, on_skip, LV_EVENT_CLICKED, nullptr);
}

static void submit_set() {
    const bool admin = (s_mode == Mode::NewSet || s_mode == Mode::NewConfirm);
    if (s_mode == Mode::Set || s_mode == Mode::NewSet) {
        strlcpy(s_first, s_entry, sizeof(s_first));
        rebuild_as(admin ? Mode::NewConfirm : Mode::Confirm, TRS("Confirme o PIN", "Confirm the PIN"),
                   TRS("Digite de novo.", "Type it again."));
        return;
    }
    // Confirm / NewConfirm
    if (std::strcmp(s_first, s_entry) != 0) {
        secure_zero(s_first, sizeof(s_first));
        rebuild_as(admin ? Mode::NewSet : Mode::Set, TRS("Defina um PIN", "Set a PIN"), "");
        set_msg(TRS("Não bateu. Defina de novo.", "Didn't match. Set it again."), COL_LATE);
        return;
    }
    if (admin) {
        // Settings: cifra as credenciais em uso com o PIN novo.
        Creds c{};
        const bool ok = net_get_creds(c) && oauth_save_encrypted(c, s_entry);
        creds_wipe(c);
        secure_zero(s_first, sizeof(s_first));
        if (ok) {
            pairing_set_session_pin(s_entry);
            Serial.println("[pin] PIN definido pelo Settings");
            wipe_entry();
            app_back();
            return;
        }
        // Falha ao gravar: fica na tela em vez de voltar como se tivesse dado
        // certo (item 4b da revisao final).
        Serial.println("[pin] falha ao gravar");
        rebuild_as(Mode::NewSet, TRS("Novo PIN", "New PIN"), "");
        set_msg(TRS("Não deu para gravar. Tente de novo.", "Couldn't save. Try again."), COL_LATE);
        return;
    }
    pairing_finish(s_entry);
    secure_zero(s_first, sizeof(s_first));
    wipe_entry();
    app_advance();
}

// ---- Enter: desbloqueio no boot ----

static int lockout_seconds(int attempts) {
    int wait = LOCKOUT_BASE_SEC;
    for (int i = 1; i < attempts && wait < 3600; i++) wait *= 2;
    return wait > 3600 ? 3600 : wait;
}

void page_pin_enter_build(lv_obj_t *scr) {
    s_scr = scr;
    s_mode = Mode::Enter;
    s_lockShown = -1;
    build_keypad(scr, TRS("Digite o PIN", "Enter the PIN"),
                 TRS("Necessário para liberar as credenciais.", "Needed to unlock the credentials."));
    const int att = oauth_pin_attempts();
    if (att > 0) {
        char m[96];
        snprintf(m, sizeof(m), TRS("%d tentativa(s) errada(s) de %d.", "%d wrong attempt(s) of %d."),
                 att, MAX_PIN_ATTEMPTS);
        set_msg(m, COL_MED);
        // O bloqueio e por tentativas persistidas, nao por tempo de parede: sem
        // isso, desligar e ligar de novo dava uma tentativa nova de graca.
        s_lockUntilMs = clock_uptime_ms() + (int64_t)lockout_seconds(att) * 1000;
        s_lockShown = -1;
    }
}

static void submit_enter() {
    if (!oauth_blob_compatible()) {
        // Formato antigo: PIN nenhum abre. Nao e tentativa errada; e pareamento.
        Serial.println("[pin] registro cifrado incompativel: pareamento");
        oauth_wipe();
        net_clear_creds();
        pairing_set_session_pin("");
        page_pair_set_reason(TRS("As credenciais salvas são de uma versão antiga. Refaça o pareamento.",
                                 "The saved credentials are from an older version. Please pair again."));
        wipe_entry();
        app_advance();
        return;
    }

    // Conta a tentativa antes de decifrar (o KDF leva ~0,2 s): um corte de
    // energia no meio ainda deixa a tentativa registrada na NVS.
    const int att = oauth_pin_attempts() + 1;
    oauth_set_pin_attempts(att);

    Creds c{};
    if (oauth_unlock(s_entry, c)) {
        oauth_set_pin_attempts(0);
        net_set_creds(c);
        pairing_set_session_pin(s_entry);
        creds_wipe(c);
        wipe_entry();
        app_advance();
        return;
    }
    creds_wipe(c);
    wipe_entry();
    update_dots();
    if (att >= MAX_PIN_ATTEMPTS) {
        oauth_wipe();
        net_clear_creds();
        pairing_set_session_pin("");
        app_advance(); // sem credenciais: volta ao pareamento
        return;
    }
    s_lockUntilMs = clock_uptime_ms() + (int64_t)lockout_seconds(att) * 1000;
    s_lockShown = -1;
}

// Comparacao sem atalho: o tempo nao revela quantos digitos bateram.
static bool pin_equal(const char *a, const char *b) {
    unsigned diff = 0;
    for (int i = 0; i < PIN_LEN; i++) diff |= (unsigned)(a[i] ^ b[i]);
    return diff == 0 && a[PIN_LEN] == '\0' && b[PIN_LEN] == '\0';
}

// Remover o PIN pede o PIN atual, contando tentativa como no desbloqueio.
static void submit_verify() {
    const int att = oauth_pin_attempts() + 1;
    oauth_set_pin_attempts(att);
    if (pin_equal(s_entry, pairing_session_pin())) {
        oauth_set_pin_attempts(0);
        Creds c{};
        const bool ok = net_get_creds(c) && oauth_save_plain(c);
        creds_wipe(c);
        if (ok) {
            pairing_set_session_pin("");
            Serial.println("[pin] PIN removido");
            wipe_entry();
            app_back();
            return;
        }
        // Falha ao gravar: fica na tela em vez de voltar como se tivesse dado
        // certo (item 4b da revisao final).
        Serial.println("[pin] falha ao gravar");
        secure_zero(s_first, sizeof(s_first));
        rebuild_as(Mode::Verify, TRS("PIN atual", "Current PIN"),
                   TRS("Para remover a proteção.", "To remove the protection."));
        set_msg(TRS("Não deu para gravar. Tente de novo.", "Couldn't save. Try again."), COL_LATE);
        return;
    }
    wipe_entry();
    update_dots();
    if (att >= MAX_PIN_ATTEMPTS) {
        oauth_wipe();
        net_clear_creds();
        pairing_set_session_pin("");
        app_advance(); // sem credenciais: pareamento
        return;
    }
    s_lockUntilMs = clock_uptime_ms() + (int64_t)lockout_seconds(att) * 1000;
    s_lockShown = -1;
}

void page_pin_admin_prepare(bool remove) { s_adminRemove = remove; }

void page_pin_admin_build(lv_obj_t *scr) {
    s_scr = scr;
    s_lockShown = -1;
    s_backPending = false;
    secure_zero(s_first, sizeof(s_first));
    if (!s_adminRemove) {
        s_mode = Mode::NewSet;
        build_keypad(scr, TRS("Novo PIN", "New PIN"),
                     TRS("Ele será pedido toda vez que o aparelho ligar.",
                         "It will be asked every time the device starts."));
        return;
    }
    s_mode = Mode::Verify;
    build_keypad(scr, TRS("PIN atual", "Current PIN"),
                 TRS("Para remover a proteção.", "To remove the protection."));
    const int att = oauth_pin_attempts();
    if (att > 0) { // mesmo bloqueio persistido do desbloqueio
        s_lockUntilMs = clock_uptime_ms() + (int64_t)lockout_seconds(att) * 1000;
        s_lockShown = -1;
    }
}

void page_pin_tick() {
    if (!s_scr) return;

    if (s_backPending) {
        s_backPending = false;
        app_back();
        return;
    }
    if (s_choosePinPending) {
        s_choosePinPending = false;
        rebuild_as(Mode::Set, TRS("Defina um PIN", "Set a PIN"),
                   TRS("Você vai digitá-lo toda vez que o aparelho ligar.",
                       "You'll type it every time the device starts."));
        return;
    }
    if (s_skipPending) {
        s_skipPending = false;
        pairing_finish(nullptr);
        app_advance();
        return;
    }
    if (s_submitPending) {
        s_submitPending = false;
        if (s_mode == Mode::Enter) submit_enter();
        else if (s_mode == Mode::Verify) submit_verify();
        else submit_set();
        return;
    }
    // Contagem regressiva do bloqueio: reescreve so quando o segundo muda.
    if ((s_mode == Mode::Enter || s_mode == Mode::Verify) && s_lockUntilMs != 0) {
        if (locked()) {
            const int rem = (int)((s_lockUntilMs - clock_uptime_ms() + 999) / 1000);
            if (rem != s_lockShown) {
                s_lockShown = rem;
                char m[96];
                snprintf(m, sizeof(m), TRS("PIN errado (%d/%d). Aguarde %d s.", "Wrong PIN (%d/%d). Wait %d s."),
                         oauth_pin_attempts(), MAX_PIN_ATTEMPTS, rem);
                set_msg(m, COL_LATE);
            }
        } else {
            s_lockUntilMs = 0;
            set_msg(TRS("Pode tentar de novo.", "You can try again."), COL_DIM);
        }
    }
}

void page_pin_leave() {
    wipe_entry();
    secure_zero(s_first, sizeof(s_first));
    s_scr = s_dots = s_msg = nullptr;
    s_submitPending = s_skipPending = s_choosePinPending = false;
    s_lockUntilMs = 0;
    s_backPending = false;
}

static void cmd_pin(const char *a) {
    if (!s_scr || s_mode != Mode::Enter) {
        Serial.println("pin: a tela de desbloqueio nao esta aberta");
        return;
    }
    if (locked()) {
        Serial.println("pin: bloqueado; espere a contagem terminar");
        return;
    }
    const size_t len = std::strlen(a);
    bool digits = len == PIN_LEN;
    for (size_t i = 0; i < len && digits; i++) digits = a[i] >= '0' && a[i] <= '9';
    if (!digits) {
        Serial.printf("pin: use %d digitos\n", PIN_LEN);
        return;
    }
    strlcpy(s_entry, a, sizeof(s_entry));
    s_submitPending = true; // decifra no tick, como o teclado da tela
    Serial.println("pin: enviado");
}

void page_pin_begin() {
    console_add("pin", "pin <digitos>  desbloqueia pela serial", cmd_pin);
}
