// Sobreposicao do pomodoro no lv_layer_top (spec 7.6): contagem expandida,
// confirmacao de troca e (Task 4) dialogo de fim. Desenhada a partir de
// pomo_app_view(): a troca de pagina limpa o layer_top e o app chama
// pomo_overlay_invalidate() para ela ser redesenhada por cima da pagina nova.
#ifndef UI_POMO_OVERLAY_H
#define UI_POMO_OVERLAY_H

// O layer_top acabou de ser limpo: esquece os ponteiros (sem apagar nada) e
// redesenha no proximo tick.
void pomo_overlay_invalidate();

// Chamado por pomo_app_tick: aplica a acao pendente dos botoes, redesenha se a
// vista mudou e atualiza a contagem do expandido.
void pomo_overlay_tick();

// A vista nao mudou mas o conteudo sim (a tarefa saiu do dia com o dialogo de
// fim aberto): apaga e redesenha no proximo tick.
void pomo_overlay_rebuild();

#endif // UI_POMO_OVERLAY_H
