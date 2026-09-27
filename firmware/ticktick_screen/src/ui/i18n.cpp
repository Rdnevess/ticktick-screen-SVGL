#include "i18n.h"

// Padrao: portugues (idioma do autor). O Plano B le a escolha da NVS no boot.
static bool s_en = false;

void lang_set_en(bool en) { s_en = en; }
bool lang_is_en() { return s_en; }
