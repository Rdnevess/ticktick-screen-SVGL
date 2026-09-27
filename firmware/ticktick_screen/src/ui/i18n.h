// Traducao inline. Sem tabela paralela: cada string carrega os dois idiomas,
// entao e impossivel acrescentar um texto e esquecer o par.
//
// Uso:  lv_label_set_text(lbl, TRS("Hoje", "Today"));
#ifndef UI_I18N_H
#define UI_I18N_H

void lang_set_en(bool en);
bool lang_is_en();

#define TRS(pt, en) (lang_is_en() ? (en) : (pt))

#endif // UI_I18N_H
