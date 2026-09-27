# Plano C — Relógio, Pomodoro, Settings e Publicação — Desenho

**Data:** 2026-09-26
**Base:** `docs/superpowers/specs/2026-09-24-ticktick-screen-design.md` (a spec principal). Este
documento é um **adendo**: descreve o que muda ou entra de novo no Plano C e registra as
decisões tomadas desde a spec. Onde não diz nada, vale a spec principal.
**Estado de partida:** Planos A e B mesclados na `master` (merge `9b840df`), verificados na
placa com dados reais.

---

## 1. Escopo

O Plano C fecha as fases 9 a 11 da spec (pomodoro, Status e Settings, publicação) e acrescenta
um pedido novo do usuário: **o horário no header e uma tela de relógio**.

Continua fora (spec 10): pausas e contagem de ciclos do pomodoro, criar ou editar tarefas,
multi-conta, som. Fica fora também, no relógio: segundos, formato 12 h, abrir sozinho como
descanso de tela e brilho noturno.

## 2. Horário no header e tela do relógio (novo)

### 2.1 Header

Da esquerda para a direita: título da tela · contador do dia (`1 / 53`; durante o pomodoro,
o contador dele, como na spec 7.6) · … · selo "desatualizado" · **horário** · engrenagem.

- Horário `HH:MM`, 24 h nos dois idiomas, `font_pt_18`, cor secundária (`COL_DIM`), logo à
  esquerda da engrenagem. O selo "desatualizado" passa para a esquerda do horário.
- Antes da primeira sincronização de hora mostra `--:--`.
- Área de toque ampliada com `lv_obj_set_ext_click_area`, como a engrenagem.
- Atualiza quando o minuto vira (o shell compara o minuto local a cada tick e só reescreve o
  label quando ele muda).
- **Tocar abre a tela do relógio.**

### 2.2 Tela do relógio (`Page::Clock`)

- Fora do swipe, como o Settings. Abre pelo toque no horário do header, a partir de qualquer
  uma das três telas.
- Conteúdo: `HH:MM` no centro com dígitos de ~120 px e, embaixo, a data por extenso em
  `font_pt_24`, cor secundária: "sábado, 26 de setembro" (PT) ou "Saturday, September 26" (EN).
- Qualquer toque volta para a tela principal, na mesma tela do swipe em que estava.
- Atualiza quando o minuto vira. Antes da primeira sincronização mostra `--:--` e nenhuma data.
- **A agenda de refresh continua rodando com o relógio aberto** (a regra da Task 11 do Plano B
  passa a ser "tela principal **ou** relógio"), para que os dados estejam frescos ao voltar.
- Se um pomodoro termina com o relógio aberto, o diálogo de fim aparece por cima (ver 3.3).

### 2.3 Fonte dos dígitos

`tools/gen_fonts.sh` ganha uma `font_clock_120`: Montserrat Medium, 120 px, bpp 4, **só**
`0123456789:` e o espaço. Custa poucos KB de flash e mantém o estilo do resto. Alternativas
descartadas: ampliar a de 48 px (serrilha) e desenhar sete segmentos (estilo destoante,
código novo).

### 2.4 Onde mora a regra

A formatação vai para o núcleo (`core/clock_format.{h,cpp}`), sem Arduino e com testes no PC:

- `HH:MM` a partir do epoch local (`--:--` quando a hora não é conhecida).
- Data por extenso a partir do epoch local, em PT e EN: dia da semana, dia e mês. Os nomes
  ficam no próprio módulo, em duas tabelas (o núcleo não usa `TRS`; recebe o idioma como
  parâmetro).
- Casos testados: virada de ano, 29 de fevereiro, dia 1 de cada mês, domingo e sábado nos dois
  idiomas, epoch local negativo não aparece (a hora do aparelho é sempre pós-2024).

## 3. Pomodoro (spec 5.6 e 7.6, sem mudança de regra)

O núcleo (`core/pomodoro`) existe desde o Plano A. O Plano C faz a casca.

### 3.1 Iniciar

- **Foco:** o segundo alvo de 56×56 em cada card (spec 7.2), ao lado do concluir.
- **Hoje:** a folha de ação passa a ter **Concluir · Pomodoro · Fixar** (spec 7.3). O
  "Fechar" sai; a folha continua fechando com um toque fora dela.
- Duração: `settings().pomoMin` (NVS `cfg/pomo`, padrão 25).
- **Um ciclo por vez:** com um pomodoro rodando, iniciar outro abre uma confirmação
  "Trocar o pomodoro atual?" com **Trocar** e **Manter**.

### 3.2 Durante

- O contador vive no header no lugar do contador do dia: `18:42` com um ícone de relógio
  (glifo da fonte de fábrica). Tocar **expande** para uma sobreposição em tela cheia com a
  contagem em `font_pt_48` e o título da tarefa; tocar de novo recolhe.
- A contagem é redesenhada no tick (procedural, spec 7.6), nunca por `lv_anim`.
- A cada refresh, se a tarefa do pomodoro não está mais no conjunto do dia, o módulo chama
  `pomo_mark_task_gone`. Concluir a tarefa pelo ✓ enquanto o pomodoro dela roda também a
  marca como saída.

### 3.3 Fim

- Diálogo em tela cheia com **Renovar · Cancelar · Concluir**, sem expirar. Com a tarefa
  saída do dia, **Concluir** não aparece e o diálogo diz por quê.
- **Concluir** usa a conclusão otimista existente (`sync_complete`).
- O diálogo aparece **em qualquer página** (principal, relógio, Settings, Status). Como a troca
  de página limpa o `lv_layer_top()` (spec 7.6), a sobreposição do pomodoro é **desenhada a
  partir do estado**: um módulo `ui/pomo_overlay` recria o que o estado pede depois de cada
  troca de página. A folha de ação do Hoje fecha quando o diálogo abre.
- Reboot no meio perde o ciclo (spec 5.6).

## 4. Settings (spec 7.5, com acréscimos)

`Page::Settings`, pela engrenagem, fora do swipe, com um botão **‹ Voltar** no topo. Lista
rolável de linhas de 44 px (rótulo à esquerda, valor ou controle à direita), em dois grupos.

**Preferências**
- **Idioma:** PT / EN. Salva e reconstrói a página (spec 7.7).
- **Fuso** *(acréscimo; desvio 4 do Plano B)*: de UTC−12:00 a UTC+14:00 em passos de 15 min,
  com botões − e +. Padrão UTC−3.
- **Atualizar a cada:** 1, 2, 5, 10, 15 ou 30 min.
- **Pomodoro:** 15, 20, 25, 30, 45 ou 50 min.

**Conta e aparelho**
- **Listas do dia:** abre a página de listas existente, que ao salvar volta ao Settings.
- **Trocar rede WiFi** *(acréscimo)*: abre a página de WiFi com um botão **‹ Voltar**. A rede
  nova entra na frente das salvas; as outras continuam.
- **Parear de novo:** abre a página de pareamento. As credenciais atuais só são trocadas
  depois que as novas forem validadas.
- **PIN:** "Definir PIN" (sem PIN) ou "Remover PIN" (com PIN). Remover pede o PIN atual.
  Definir regrava as credenciais em uso cifradas com o PIN novo.
- **Verificação do certificado:** "cadeia embutida" / "desligada (inseguro)", com aviso em
  vermelho quando desligada.
- **Sobre:** versão do firmware e "Licença MIT".
- **Reset de fábrica:** confirmação em tela cheia ("Apagar tudo e reiniciar?"). Apaga os
  namespaces `tt`, `cfg` e `wifi` e reinicia.

## 5. Status completo (spec 7.4)

Acrescenta ao Status atual: a **URL do portal** (`http://ticktick-screen.local` e o IP), e dois
botões: **Atualizar agora** (mesmo efeito do toque na barra) e **Abrir portal** (mesmo efeito
de "Parear de novo"). A hora do Status passa a atualizar quando o minuto vira.

## 6. Pendências das revisões do Plano B

Entram no Plano C como uma task própria:

- Conta com 64 listas ou mais: um 404 numa lista marcada vira "pular a lista" em vez de
  derrubar o ciclo.
- O motivo em vermelho da página de pareamento é limpo em `pairing_finish`, não só pela
  página.
- Concluir uma tarefa fixada solta o pin na confirmação, sem esperar o próximo refresh.
- O nome da lista no card do Foco é truncado com reticências.
- A tela de WiFi volta a "Toque na sua rede" depois de uma tentativa automática que falhou.
- O contador do dia zera na virada da meia-noite mesmo sem refresh (o tick do header já
  acompanha o minuto).
- Um registro cifrado de formato incompatível é tratado como corrompido (volta ao pareamento)
  e não conta tentativa de PIN.
- `helper/pair.py` mostra mensagens de erro no lugar de tracebacks (rede, tempo esgotado,
  autorização negada) e trata `expires_in: 0` como ausente.

## 7. Publicação (fase 11)

### 7.1 Captura de tela e modo demonstração

- Comando de console **`shot`**: envia pela serial o último quadro do buffer de render
  (480×320 RGB565), comprimido por RLE e codificado em base64, entre marcadores de início e fim.
  `tools/shot2png.py` (biblioteca padrão) extrai o bloco da saída do console e grava um PNG.
- Comando **`demo on|off`**: carrega no store um conjunto fixo de tarefas de exemplo (títulos
  genéricos, em PT ou EN conforme o idioma) e **pausa** o refresh; `off` volta ao normal e pede
  um refresh. As capturas do README usam o modo demo: **nenhuma tarefa real vai para o
  repositório**.
- Comando **`go foco|hoje|status|relogio|settings`**: navega sem tocar na tela, para as
  capturas.

### 7.2 README bilíngue

`README.md` (EN) e `README.pt-BR.md` (PT), no padrão do projeto vizinho: o que é, hardware,
build (`arduino-cli`, versões travadas), testes, pareamento — **pelo portal e pelo console**,
com a explicação de redes com isolamento de clientes, onde o portal não é alcançável —, uso das
telas, e as capturas.

### 7.3 Licença e privacidade

- Licença **MIT** (`LICENSE` na raiz).
- Nas fixtures e na spec, o fuso das tarefas capturadas passa a ser `America/La_Paz`
  (UTC−4, sem horário de verão, como o original), e `CAPTURA.txt` continua coerente.

### 7.4 Faxina do Plano A

`LV_COLOR_16_SWAP` morto no `lv_conf.h`, caminhos absolutos da máquina do autor no cabeçalho
das fontes geradas (limpos pelo `gen_fonts.sh`), `partitions.csv` e spec 4.1 alinhados, e os
resquícios de texto do `bringup.ino`.

## 8. Testes

- No PC: `core/clock_format` (seção 2.4) e `core/settings_values` — as sequências de valores
  do Settings (intervalo de refresh, duração do pomodoro) com o passo seguinte/anterior, e o
  fuso em passos de 15 min limitado a UTC−12:00…UTC+14:00, com a formatação `UTC−3:00`.
- Na placa, com o console: `go`, `demo` e `shot` servem também de verificação automatizada
  das telas novas; o toque (pomodoro, Settings) continua como verificação manual com o usuário.

## 9. Decisões registradas desde a spec principal

| Decisão | Onde |
|---|---|
| Dia inteiro chega como meia-noite no fuso da tarefa; regra `utc + fuso + 12h` | Plano B, Task 3 |
| A Entrada é acessível por `/project/inbox/data` | Plano B, Task 3 |
| Worker de rede em task FreeRTOS; credenciais num registro JSON; `src/app/` | Plano B, desvios 1-3 |
| Fuso em `cfg/tz`; todo ciclo começa com `GET /project` | Plano B, desvios 4-5 |
| CSRF do portal resolvido por checagem de `Origin` | Plano B, Task 8 |
| Bloqueio do PIN sobrevive ao reboot | Plano B, Task 9 |
| Horário no header e tela do relógio | este documento, seção 2 |
| Settings ganha fuso e troca de rede WiFi | este documento, seção 4 |
| Licença MIT; fixtures com `America/La_Paz` | este documento, seção 7.3 |
