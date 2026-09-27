# TickTick Screen — Spec de Design

**Data:** 2026-09-24
**Placa:** Guition JC4832W535 (ESP32-S3, AXS15231B QSPI 480×320, touch I²C)
**Base de hardware:** `docs/REFERENCIA-HARDWARE-LVGL.md` (receita validada de display/touch/LVGL)
**Projeto de referência:** `../claude-usage-stick-SVGL` (firmware funcionando na mesma placa)

---

## 1. Objetivo

Um painel de mesa de 3,5" que mostra as tarefas do dia do TickTick e permite agir sobre
elas sem tocar no computador ou no celular: concluir uma tarefa e rodar um pomodoro.

O aparelho é **autônomo** — fala direto com a API do TickTick, sem ponte nem serviço
ligado 24/7. O único auxílio externo é um helper de **uso único** que faz a dança do
OAuth na primeira configuração.

O projeto será **publicado** como repositório aberto (mesmo modelo do claude-usage-stick),
o que torna a UI bilíngue (PT/EN) e a portabilidade do build requisitos, não enfeites.

## 2. Decisões tomadas

| # | Decisão | Por quê |
|---|---|---|
| 1 | Painel do dia **+ concluir tarefa** + pomodoro | é o que se usa de fato num aparelho de mesa |
| 2 | **Tela de Foco** com no máximo 2 tarefas, fonte maior, preenchendo a área | pedido explícito: filtrar o dia ao que importa agora |
| 3 | Gadget **autônomo** + **helper de uso único** para o OAuth | sem dependência permanente de PC ou nuvem |
| 4 | Pomodoro **local**, diálogo final **Renovar / Cancelar / Concluir** | a API do TickTick não tem endpoint de foco (ver seção 3) |
| 5 | "O dia" = **vence hoje + atrasadas** | corresponde ao dia real de trabalho; atrasadas são as que mais precisam ser vistas |
| 6 | Somente as **listas escolhidas** pelo usuário | custo de rede é proporcional ao número de listas |
| 7 | As 2 do Foco: **automático + pin** por cima | zero gesto no dia a dia, com controle manual quando quiser |
| 8 | Telas por swipe: **Foco / Hoje / Status**; Settings pela engrenagem | cobre o pedido e dá lugar ao diagnóstico de rede |
| 9 | Credenciais entram por **portal web no gadget**; **PIN opcional** | colar do PC é viável; digitar token de 40+ chars na tela não é |
| 10 | Poll de **5 min** (ajustável) + **conclusão otimista** | dado muda devagar; a tela responde no instante do toque |
| 11 | **Bilíngue PT/EN** com **fonte acentuada** | títulos de tarefa vêm do TickTick com acento |
| 12 | Estrutura **núcleo puro + casca de UI**, build `arduino-cli` | lógica testável no PC sem placa; toolchain já validado nesta placa |

## 3. Restrições da API do TickTick (verificado)

Base: `https://api.ticktick.com/open/v1`, autenticação `Authorization: Bearer <token>`.

**Existe:**

| Método | Caminho | Uso |
|---|---|---|
| GET | `/project` | lista as listas do usuário |
| GET | `/project/{projectId}/data` | a lista com suas tarefas em aberto |
| GET | `/project/{projectId}/task/{taskId}` | uma tarefa |
| POST | `/task` | criar |
| POST | `/task/{taskId}` | editar |
| POST | `/project/{projectId}/task/{taskId}/complete` | **concluir** (sem corpo) |
| DELETE | `/project/{projectId}/task/{taskId}` | remover |

**Não existe** — e cada ausência molda o desenho:

- **Nenhuma listagem cross-project e nenhum filtro por data.** "As tarefas de hoje" tem
  que ser montado no cliente: `GET /project` e depois uma `GET /project/{id}/data` por
  lista escolhida, filtrando `dueDate` localmente.
- **Nada de pomodoro / tempo de foco / hábitos.** O pomodoro do gadget é local: conta na
  tela e não gera registro de foco no TickTick. O que ele consegue fazer de verdade é
  **concluir** a tarefa ao fim do ciclo.
- **Nenhum endpoint de tarefas concluídas.** Não há como montar histórico do dia.

**OAuth2 (authorization code):** autorização em `https://ticktick.com/oauth/authorize`
(`client_id`, `scope`, `state`, `redirect_uri`, `response_type=code`), troca em
`POST https://ticktick.com/oauth/token` com Basic auth `client_id:client_secret`.
Escopos: `tasks:read tasks:write`. O access token dura ~165–180 dias.

> **Ponto não confirmado na documentação oficial:** a renovação por refresh token. A doc
> oficial documenta apenas `access_token`; o `refresh_token` aparece na prática das
> bibliotecas da comunidade. O desenho **não aposta nele** (ver seção 6.4).

**Prioridade** no TickTick é `0` / `1` / `3` / `5` (nenhuma / baixa / média / alta) — não
é uma escala 0–3.

## 4. Arquitetura

### 4.1 A restrição de build que define o layout

O `arduino-cli` compila os fontes da **raiz do sketch** e, recursivamente, os de **`src/`**.
Pastas de outros nomes na raiz não entram no build. Logo a separação núcleo/casca mora
embaixo de `src/`.

```
ticktick_screen_SVGL/
├─ firmware/
│  ├─ bringup/                      # copiado do vizinho: valida cor + touch nesta máquina
│  └─ ticktick_screen/
│     ├─ ticktick_screen.ino        # ~150 linhas: setup, loop, máquina de telas
│     ├─ config.h                   # pinos e constantes da placa
│     ├─ lv_conf.h                  # LV_COLOR_DEPTH 16 + LV_FONT_CUSTOM_DECLARE
│     ├─ partitions.csv             # app0/app1 3 MB (OTA) + spiffs ~9,9 MB + coredump
│     ├─ build.sh                   # compila / grava / monitora, porta por SO
│     └─ src/
│        ├─ core/                   # SEM LVGL, SEM Arduino — compila no PC
│        │   task.h
│        │   task_store.{h,cpp}
│        │   day_filter.{h,cpp}
│        │   task_order.{h,cpp}
│        │   pomodoro.{h,cpp}
│        │   payload.{h,cpp}
│        ├─ net/
│        │   oauth_store.{h,cpp}    # NVS: credenciais, cifra opcional
│        │   ticktick_api.{h,cpp}   # HTTPS: varredura, complete, refresh
│        │   certs.{h,cpp}          # cadeia raiz embutida
│        ├─ ui/
│        │   theme.{h,cpp}  i18n.h
│        │   screen_focus.{h,cpp}  screen_today.{h,cpp}
│        │   screen_status.{h,cpp}  screen_settings.{h,cpp}
│        │   overlay_pomodoro.{h,cpp}  overlay_dialog.{h,cpp}
│        │   onboarding_web.{h,cpp} # WebServer + páginas de pareamento
│        ├─ platform/
│        │   display.{h,cpp}  touch.h  wifi_manager.h  clock.{h,cpp}
│        └─ assets/
│            font_pt_14.c … font_pt_48.c   icons.h
├─ tests/          run.sh + test_*.cpp       # roda no PC, sem placa
├─ helper/         pair.py                   # OAuth de uso único (stdlib)
├─ tools/          gen_fonts.sh  gen_assets.py
└─ docs/           REFERENCIA-HARDWARE-LVGL.md + superpowers/specs/
```

### 4.2 A regra do `core/`

Nenhum arquivo em `core/` inclui `lvgl.h`, `Arduino.h` ou `WiFi.h`, nem usa `millis()`,
`Serial` ou variável global. Quem precisa do tempo **recebe o instante como parâmetro**.

É isso que permite compilar esses arquivos no PC — e é onde vive tudo que muda com mais
frequência: a definição de "hoje", a ordem das tarefas, o pin, o ciclo do pomodoro.

Efeito colateral bem-vindo: a armadilha documentada em
`docs/REFERENCIA-HARDWARE-LVGL.md` seção 14 ("protótipos automáticos do `.ino` quebram
com tipos próprios") deixa de morder. Ela só afeta código que mora no `.ino`; com o
`.ino` reduzido a `setup`/`loop`/troca de telas, o pré-processador do Arduino não tem o
que estragar.

### 4.3 Build e testes

- **Firmware:** `build.sh`, mesmo FQBN do vizinho
  (`esp32:esp32:esp32s3:PSRAM=opi,FlashSize=16M,PartitionScheme=custom,CDCOnBoot=cdc,USBMode=hwcdc,FlashMode=qio`)
  e `-DLV_CONF_INCLUDE_SIMPLE -I<sketch>` para o LVGL achar nosso `lv_conf.h`.
  Porta padrão detectada por SO: `COMx` (Windows), `/dev/cu.*` (macOS), `/dev/ttyACM*` (Linux).
- **Testes:** `tests/run.sh` detecta o compilador — usa `g++` ou `clang++` se existirem
  (Linux, macOS, contribuidores) e cai no **MSVC via `vcvars64.bat`** quando só ele
  existe, que é o caso desta máquina (VS 18 BuildTools, MSVC 14.51, já instalado).
  Compila `src/core/*.cpp` + os testes e imprime o resultado. Sem placa, sem upload.
- **Fontes:** `tools/gen_fonts.sh` chama `npx lv_font_conv` (Node 24 presente).

### 4.4 Ordem de ataque

O `firmware/bringup/` primeiro, copiado do projeto vizinho onde já é conhecido-bom.
Rodá-lo aqui prova que placa, cabo, `arduino-cli` e libs desta máquina estão de pé
**antes** de escrevermos UI nova. Cor errada nesse ponto é problema de ambiente, não de
código nosso.

## 5. O núcleo

### 5.1 Modelo

`Task` é struct de tamanho fixo (sem heap, sem fragmentação), em array na PSRAM com teto
de **64 tarefas por ciclo**. Passando de 64, mantém as 64 primeiras pela ordenação e a UI
mostra um "+N" discreto.

| campo | origem | nota |
|---|---|---|
| `id`, `projectId` | API | 24–32 chars; ambos necessários para concluir |
| `title` | API | truncado em 80 bytes UTF-8, **com corte em fronteira de caractere** (cortar no meio de um acento produz byte inválido na tela) |
| `dueTs` | derivado | instante **local** em `time_t`, já resolvido |
| `isAllDay` | API | muda a regra de comparação (5.2) |
| `priority` | API | `0/1/3/5` |
| `sortOrder` | API | desempate final; respeita a ordem arrastada no app |
| `overdue` | derivado | calculado pelo filtro |
| `pending` | interno | conclusão otimista em voo (5.5) |

### 5.2 Filtro do dia

Entrada: array de `Task` + o instante local `now`. Saída: as tarefas de hoje e atrasadas,
com `overdue` marcado.

- **Tarefas com hora:** converte o `dueDate` (ISO 8601 com offset) para o fuso local.
  `overdue` se `dueTs < inícioDeHoje`; entra no dia se `dueTs <= fimDeHoje`.
- **Tarefas de dia inteiro — a pegadinha:** o TickTick devolve dia inteiro como um
  instante, não como data. **Observado em 2026-09-25 (tests/fixtures):** meia-noite do
  fuso da tarefa convertida para UTC, por exemplo `2026-09-25T04:00:00.000+0000` com
  `timeZone` = `America/La_Paz`. Converter esse instante ingenuamente para o fuso local
  pode jogar a tarefa para o dia anterior. **Regra:** a data da tarefa é a data civil de
  `utc + fuso local + 12h`, que acerta tanto a convenção meia-noite UTC quanto a meia-noite
  no fuso da tarefa para qualquer |fuso| < 12h. Comparar datas de calendário: menor que
  hoje = atrasada, igual = do dia, maior = fora.
- **Para ordenação**, tarefa de dia inteiro recebe `dueTs` = 23:59:59 local da sua data.
- **Sem `dueDate`:** fora do dia (decisão 5).
- **Defesa:** descarta `status != 0` mesmo a API prometendo devolver só tarefas em aberto.
- **Hora é pré-requisito:** sem SNTP não existe "hoje" (e o TLS também não valida
  certificado). Antes da primeira sincronização a UI mostra "sincronizando hora", nunca
  uma lista vazia — que pareceria "nada pra fazer" e seria mentira.

### 5.3 Ordenação

Uma cadeia única, sem ramo especial:

1. atrasada antes de hoje
2. prioridade decrescente (5 > 3 > 1 > 0)
3. `dueTs` **crescente**
4. `sortOrder` do TickTick

O passo 3 serve aos dois grupos ao mesmo tempo: nas atrasadas põe a mais antiga no topo;
nas de hoje, a mais próxima primeiro. Como dia inteiro recebe 23:59:59, essas tarefas
caem naturalmente no fim do seu grupo de prioridade.

### 5.4 Pin

Até **2 pins**, guardados na NVS como par `(projectId, taskId)`. Os slots do Foco são
preenchidos pelos pins primeiro (na ordem em que foram fixados) e o restante pela ordem
automática. Um pin **se apaga sozinho** quando a tarefa é concluída ou sai do dia — senão
o aparelho insiste, depois de um reboot, numa tarefa que já morreu.

### 5.5 `task_store` e conclusão otimista

Dono do array e do estado de sincronização:

- `markPending(id)` — tira a tarefa da visão no instante do toque.
- `confirmPending(id)` — o `POST …/complete` voltou 200; remove de vez.
- `revertPending(id)` — falhou; a tarefa **volta** para a lista com marcador de erro.
- `markStale()` — o refresh não completou; o conjunto anterior continua válido e a UI
  exibe o selo "desatualizado".

Sem esse módulo, esse estado vazaria para dentro do código de UI.

### 5.6 Máquina do pomodoro

`PARADO → RODANDO(tarefa, início, duração) → TERMINADO(diálogo) → PARADO`

- O instante é sempre injetado (`tick(now)`); o módulo nunca lê `millis()`.
- `TERMINADO` **espera indefinidamente** pela escolha; o diálogo não expira.
- `Renovar` reinicia um ciclo na mesma tarefa com a mesma duração. `Cancelar` volta a
  `PARADO` sem concluir. `Concluir` dispara o `POST …/complete`.
- **Um ciclo por vez:** iniciar outro pergunta antes de descartar o corrente.
- **Reboot no meio perde o ciclo** (não guardamos timer na NVS).
- **Tarefa sumiu no meio do ciclo** (concluída no celular): o contador continua rodando,
  mas o diálogo final **omite** `Concluir` e explica que a tarefa saiu do dia.

## 6. Rede e pareamento

### 6.1 Helper de uso único (`helper/pair.py`)

Python de biblioteca padrão, sem dependências externas:

1. Lê `client_id` e `client_secret` de `helper/.env` (barrado pelo `.gitignore`) ou pergunta.
2. Sobe servidor local em `127.0.0.1:8080` e abre o navegador na URL de autorização com
   `scope=tasks:read tasks:write`, `response_type=code` e um `state` aleatório.
3. Recebe o `code` no retorno e troca por token em `POST /oauth/token` com Basic auth.
4. **Imprime** o que colar no portal do gadget. Se achar o aparelho na rede, oferece
   enviar direto — mas o caminho principal é colar, porque mDNS no Windows é instável e o
   pareamento não pode depender disso.

Não persiste nada e não é serviço: terminou, fechou.

**Premissa explícita:** o helper usa `redirect_uri = http://127.0.0.1:8080/callback`. O
cadastro do app no `developer.ticktick.com` precisa ter exatamente essa URI — o TickTick
exige correspondência exata. Se o cadastro existente usa outra, ajustar `REDIRECT_URI`
em `helper/.env`.

### 6.2 Portal do gadget

Depois do WiFi conectado (teclado LVGL, senha curta — `wifi_manager.h` já resolve), o
aparelho sobe `WebServer` na porta 80 e anuncia `ticktick-screen.local`. A tela de Status
mostra **também o IP numérico**, para quando o mDNS falhar.

Página (tema escuro, bilíngue) com **um campo só**: o *blob de pareamento* que o helper
imprimiu — base64 de um JSON compacto com `cid`, `csec`, `atok`, `rtok` e `exp`. Um campo
único em vez de quatro porque cada campo extra é uma chance de colar no lugar errado; um
botão "avançado" revela os quatro campos individuais para quem precisar corrigir só um.

O gadget **valida antes de salvar** chamando `GET /open/v1/project`; com 200, pede o PIN na
tela com "definir" e "pular" lado a lado.

Duas decisões de segurança explícitas:

- A página trafega em **HTTP simples na LAN**. Aceitável para pareamento local, e é o elo
  mais fraco da cadeia.
- O portal **não fica ligado depois de parear**. Sobe no onboarding e quando aberto pelo
  Settings, com desligamento automático. Um formulário permanentemente exposto na rede
  aceitando credenciais não tem razão de existir.

### 6.3 NVS

| namespace | chaves |
|---|---|
| `wifi` | até 3 redes — `wifi_manager.h` do vizinho, reaproveitado |
| `tt` | `cid`, `csec`, `atok`, `rtok`, `exp`; cifrados quando houver PIN |
| `cfg` | `lists` (ids escolhidos), `poll`, `pomo`, `lang`, `pin1`, `pin2`, `tls` |

### 6.4 Renovação do token

O gadget tenta renovar quando faltarem menos de **7 dias** para `exp`, ou imediatamente
ao receber **401**: `POST /oauth/token` com `grant_type=refresh_token` e Basic auth.

**Quando `exp` não é conhecido:** a resposta do token pode não trazer `expires_in` (a doc
oficial não o documenta). Nesse caso o helper grava `exp = agora + 150 dias` — conservador
diante dos ~165–180 observados. `exp` é só uma dica de quando tentar renovar; a autoridade
real é o **401**, que dispara a renovação independentemente do prazo gravado.

Como a renovação **não está confirmada na documentação oficial** (seção 3), ela falha de
forma alta e recuperável: o Status mostra "token expirado, refaça o pareamento" e o
portal reabre sozinho. No pior caso é um ritual de 2 minutos, duas vezes por ano —
aceitável; falha silenciosa não seria.

### 6.5 Varredura

`GET /project` traz todas as listas (o Settings usa isso para os checkboxes). Depois, uma
`GET /project/{id}/data` por lista marcada. Três cuidados decidem se o ciclo leva 2 ou 15
segundos:

- **Uma conexão só:** `WiFiClientSecure` mantido vivo com `setReuse(true)` — **um**
  handshake TLS por ciclo em vez de N.
- **Parse com filtro:** `DeserializationOption::Filter` do ArduinoJson deixando passar
  apenas os campos do `Task`. O `/data` de uma lista traz descrição, subtarefas e
  lembretes; filtrar corta o uso de memória em cerca de uma ordem de grandeza e evita
  depender de um buffer gigante na PSRAM.
- **Certificado:** três raízes embutidas (USERTrust RSA, Sectigo Public Server
  Authentication Root R46 e ISRG Root X1) em vez de `setInsecure()`, com um interruptor no
  Settings para cair em `setInsecure()` se uma rotação de CA quebrar tudo.

**Custo visível ao usuário:** o ciclo é proporcional ao número de listas marcadas. A tela
de seleção mostra a contagem para que o custo seja sentido na hora da escolha.

**Lista apagada:** todo ciclo começa com `GET /project`; lista marcada que sumiu dele sai da
seleção. Com 64 listas ou mais a resposta vem cortada, e então um 404 no `/data` de uma
lista marcada faz o ciclo pular essa lista em vez de falhar.

**Entrada:** GET /project não a lista, mas GET /project/inbox/data responde (verificado em
2026-09-25). As tarefas dela trazem projectId "inbox<número>", que é o que o complete usa.

### 6.6 Conclusão

`POST /open/v1/project/{projectId}/task/{taskId}/complete`, sem corpo. Sucesso agenda um
**refresh imediato** — é assim que mudanças feitas no celular aparecem rápido.

## 7. UI

480×320 paisagem, USB à esquerda: flush girando 270° CW e touch `rotation = 3`,
exatamente como `docs/REFERENCIA-HARDWARE-LVGL.md` prescreve. Display e touch sempre em par.

### 7.1 Esqueleto comum

| faixa | altura | conteúdo |
|---|---|---|
| header | 40 px | da esquerda para a direita: título da tela · contador do dia (`3 / 8`, ou o do pomodoro em `COL_ACCENT` enquanto ele roda, 7.6) · … · selo "desatualizado" · horário `HH:MM` · engrenagem, 58×40 com `lv_obj_set_ext_click_area(12)` |
| barra de refresh | 4 px | conta para o próximo ciclo; **tocada, atualiza na hora** |
| área útil | **252 px** | a tela em si |
| pontinhos | 24 px | posição no swipe; o ativo virando pílula |

Total: 40 + 4 + 252 + 24 = 320.

O horário fica em `font_pt_18`, cor `COL_DIM`, com área de toque ampliada como a da
engrenagem; antes da primeira sincronização de hora mostra `--:--` e só reescreve
quando o minuto local vira. **Tocar no horário abre a tela do relógio** (7.10);
tocar no contador do pomodoro expande a sua sobreposição (7.6).

Swipe por `LV_EVENT_GESTURE` com reconstrução da tela (padrão `render_state()` do
vizinho). Se um refresh chegar enquanto a lista de Hoje está sendo rolada, a reconstrução
espera — perder a posição de rolagem sozinho é irritante.

### 7.2 Foco (tela inicial no boot)

Dois cards de **480×120** com 12 px de folga: preenchem os 252 px da área útil.

- **Título em 28 px**, até 2 linhas, quebra por palavra.
- Linha inferior em 14 px: hora (ou `ATRASADA` em coral) · nome da lista · prioridade como
  palavra colorida (`ALTA` / `MÉDIA` / `BAIXA` na cor da tabela 7.8; prioridade nenhuma é
  omitida em vez de escrita).
- À direita, dois alvos de 56×56: **concluir** e **pomodoro**.
- Tarefa fixada exibe selo de pin; **toque longo** fixa/solta.
- **Uma tarefa só:** o card único ocupa os 252 px e o título sobe para 36 px.
- **Nenhuma:** estado de dia limpo, sem card falso.

### 7.3 Hoje

Lista rolável, linhas de 44 px (~5,7 visíveis): faixa de prioridade (4 px) · hora em
14 px · título em 18 px com elipse · botão concluir à direita.

Toque no botão conclui direto. Toque no **corpo da linha** abre folha de ação com três
botões grandes — **Concluir · Pomodoro · Fixar**. Preferido a empilhar toque longo: em
linha de 44 px o toque errado é fácil, e o pin ganha lugar óbvio em vez de gesto escondido.

### 7.4 Status

Sete linhas, agrupadas para caber sem rolagem nos 252 px da área útil:

1. **Rede:** SSID · IP · RSSI em dBm (ou "desconectado" em vermelho).
2. **Hora:** `HH:MM` (ou "sincronizando…") · fuso no mesmo formato do Settings
   (`UTC-3:00`, 7.5 "Fuso"). Reescrita quando o minuto vira, junto com o
   horário do header.
3. **Último refresh:** hora do último ciclo bem-sucedido e "ok", ou o código de
   status se o refresh mais recente falhou depois de um que deu certo, ou
   "ainda não" antes do primeiro.
4. **Listas:** quantas estão marcadas · intervalo do poll (`a cada N min`).
5. **Token:** validade em dias restantes (aviso a menos de 7), "expirado",
   "prazo desconhecido" (pareado mas sem `exp`) ou "não pareado".
6. **Portal:** `ticktick-screen.local · <IP>` quando há rede, para quando o
   mDNS falhar; "sem rede" quando não há.
7. **Firmware:** versão · RAM livre · PSRAM livre, em KB.

Dois botões: **Atualizar agora** (mesmo efeito da barra de refresh) e **Abrir
portal** (mesmo efeito de "Parear de novo" do Settings).

### 7.5 Settings (engrenagem, fora do swipe)

`Page::Settings`, com um botão **‹ Voltar** no topo. Lista rolável de linhas de
**44 px** (rótulo à esquerda, valor ou controle à direita), em dois grupos.

**Preferências**
- **Idioma:** PT / EN. Salva e reconstrói a página.
- **Fuso:** de UTC−12:00 a UTC+14:00 em passos de 15 min, com botões − e +
  (`UTC-3:00`, hífen ASCII — a fonte não tem o sinal de menos). Um valor fora do
  passo (por exemplo vindo de fora do Settings) alinha no múltiplo de 15 mais
  próximo na direção do toque, em vez de saltar direto para o passo cheio.
- **Atualizar a cada:** 1, 2, 5, 10, 15 ou 30 min.
- **Pomodoro:** 15, 20, 25, 30, 45 ou 50 min.

**Conta e aparelho**
- **Listas do dia:** abre a página de listas existente, que ao salvar volta ao
  Settings.
- **Trocar rede WiFi:** abre a página de WiFi com um botão **‹ Voltar**. Se a
  rede nova falhar, o aparelho tenta as redes salvas de volta sozinho antes de
  devolver a tela de "Toque na sua rede".
- **Parear de novo:** abre a página de pareamento. As credenciais atuais só são
  trocadas depois que as novas forem validadas.
- **PIN:** "ativo · remover" ou "desligado · definir", conforme o estado atual.
  Remover pede o PIN atual; definir regrava as credenciais em uso cifradas com
  o PIN novo.
- **Verificar certificado:** interruptor com o texto "cadeia embutida" (ligado)
  ou "desligada: inseguro" (desligado, em vermelho).
- **Sobre:** "TickTick Screen v\<versão\> · MIT".
- **Reset de fábrica:** confirmação em tela cheia ("Apagar tudo e reiniciar?").
  Apaga os namespaces `tt`, `cfg` e `wifi` da NVS e reinicia.

### 7.6 Pomodoro

O contador vive **no header**, no lugar do contador do dia, em `18:42` na cor
`COL_ACCENT` — sem ícone de relógio: nem as `font_pt_*` nem os `LV_SYMBOL_*` de
fábrica têm esse glifo, e a cor de destaque já o distingue do contador normal.
Sempre visível, em qualquer tela, sem empurrar layout e sem cobrir a navegação —
que era o problema de um scrim de tela cheia. **Tocar expande** para uma
sobreposição em tela cheia com a contagem em 48 px, o título da tarefa e um botão
**Cancelar** que cancela o ciclo — sem ele não haveria como parar um ciclo antes
do fim; tocar fora do botão recolhe.

Início: no Foco, o segundo alvo de 56×56 de cada card, ao lado do concluir; no
Hoje, a folha de ação do corpo da linha tem **Concluir · Pomodoro · Fixar**. Com
um pomodoro já rodando, iniciar outro pergunta antes: **"Trocar o pomodoro
atual?"** com **Trocar** e **Manter**.

Ao zerar: **diálogo em tela cheia com Renovar · Cancelar · Concluir**, esperando
sem expirar. Se a tarefa saiu do dia, `Concluir` não aparece e o diálogo explica
o motivo. Esse diálogo (e a sobreposição expandida) aparece **em qualquer
página** — principal, relógio, Settings ou Status: como a troca de página limpa
o `lv_layer_top()`, a sobreposição é **desenhada a partir do estado** a cada
troca, por um módulo próprio (`ui/pomo_overlay`) que recria o que o estado do
pomodoro pede depois de cada troca de tela. A folha de ação do Hoje fecha quando
o diálogo abre.

Duas armadilhas do doc de referência respeitadas:

- Animação **procedural por `clock_uptime_ms()`** no tick, nunca `lv_anim` com
  `exec_cb` apontando para objeto que pode ser destruído por um toque.
- A sobreposição anterior é apagada (`lv_obj_delete`) antes de desenhar a
  próxima, a cada troca de estado ou de página, senão overlay vaza entre telas.

### 7.7 Fonte acentuada e i18n

`tools/gen_fonts.sh` chama `npx lv_font_conv` e gera Montserrat nos tamanhos
**14 / 18 / 24 / 28 / 36 / 48** com os intervalos `0x20-0x7F` e `0xA0-0xFF` (todo o
Latin-1 Supplement: á à â ã é ê í ó ô õ ú ü ç) mais os pontos de código `0x2022` (bullet),
`0x00B0` (grau), `0x2014` (travessão), `0x2026` (elipse) e `0x201C`/`0x201D` (aspas
curvas). **Custo medido: ~90 KB de flash** para os seis tamanhos com bpp 4 (5 KB no 14,
10,6 KB no 24, 28,7 KB no 48) — irrelevante nos 16 MB, e é o que faz "Revisão do
contrato" aparecer escrito corretamente. O TTF vem da própria lib lvgl 9.2.2
(`scripts/built_in_font/Montserrat-Medium.ttf`), o mesmo arquivo com que a LVGL gera suas
fontes de fábrica, então as nossas saem visualmente idênticas. As fontes são declaradas no
`lv_conf.h` via `LV_FONT_CUSTOM_DECLARE`.

Uma consequência que obriga a manter uma fonte de fábrica ligada: os `LV_SYMBOL_*` do LVGL
(engrenagem, WiFi) são glifos na faixa `0xF000+` das Montserrat internas, e as nossas não
os têm. Portanto `LV_FONT_MONTSERRAT_18` fica habilitada e os labels de ícone a usam,
enquanto todo texto usa as `font_pt_*`.

Onde cada tamanho é usado — nenhuma fonte é gerada sem destino:

| tamanho | uso |
|---|---|
| 14 | linha inferior do card de Foco, hora na lista de Hoje, campos do Status |
| 18 | título da tela no header, título das linhas de Hoje, itens do Settings |
| 24 | botões da folha de ação e do diálogo do pomodoro |
| 28 | título do card de Foco (dois cards) |
| 36 | título do card de Foco quando há uma tarefa só |
| 48 | contagem do pomodoro expandido |

i18n pela macro `TRS(pt, en)` inline (padrão do vizinho) — sem tabela paralela para
desalinhar. Trocar idioma salva na NVS e reconstrói a tela atual. Títulos de tarefa
passam intactos.

### 7.8 Paleta

| papel | cor |
|---|---|
| fundo | `#0E1116` |
| superfície | `#161B22` |
| acento (azul TickTick) | `#4772FA` |
| concluído / sucesso | `#3FB950` |
| atrasada / prioridade alta | `#F85149` |
| prioridade média | `#D29922` |
| prioridade baixa | `#4772FA` |
| texto primário | `#E6EDF3` |
| texto secundário / prioridade nenhuma | `#8B949E` |

### 7.9 Primeiro boot

```
boot → sem WiFi salvo?             → tela WiFi (scan + teclado)
     → conectado, sem credenciais? → "abra ticktick-screen.local" + IP na tela
     → validou (GET /project 200)  → PIN? (definir / pular)
     → sem listas escolhidas?      → tela de seleção de listas
     → primeiro refresh            → FOCO
```

### 7.10 Relógio

`Page::Clock`, fora do swipe (como o Settings). Abre pelo toque no horário do
header (7.1), a partir de qualquer uma das três telas.

- Conteúdo: `HH:MM` no centro, dígitos de ~120 px em `font_clock_120`
  (Montserrat Medium, 120 px, bpp 4, só os glifos `0123456789:` e o espaço,
  gerada por `tools/gen_fonts.sh`); embaixo, a data por extenso em `font_pt_24`,
  cor `COL_DIM`: "sábado, 26 de setembro" (PT) ou "Saturday, September 26" (EN).
- Antes da primeira sincronização de hora mostra `--:--` e nenhuma data.
- Atualiza quando o minuto local vira, junto com o horário do header.
- **Qualquer toque** na tela volta para a tela principal, na mesma tela do
  swipe em que estava.
- A agenda de refresh continua rodando com o relógio aberto, tanto quanto na
  tela principal, para que os dados estejam frescos ao voltar. Se um pomodoro
  termina com o relógio aberto, o diálogo de fim (7.6) aparece por cima.
- A formatação — `HH:MM` e a data por extenso nos dois idiomas, a partir do
  epoch local — mora no núcleo (`core/clock_format.{h,cpp}`), sem Arduino,
  testada no PC: virada de ano, 29 de fevereiro, dia 1 de cada mês, domingo e
  sábado nos dois idiomas.

## 8. Estados e falhas

| situação | comportamento |
|---|---|
| SNTP pendente | tela "sincronizando hora" (nunca lista vazia) |
| WiFi caiu | último dado bom + selo **desatualizado**; Status diz o motivo |
| 401 | tenta renovar uma vez; falhando, "refazer pareamento" + portal reabre |
| 429 | recuo exponencial, dobrando o intervalo até 30 min; avisa no Status |
| timeout / 5xx | mantém o dado anterior e tenta no ciclo seguinte |
| `complete` falhou | tarefa **volta** à lista com marcador de erro + aviso passageiro |
| dia sem tarefas | estado "dia limpo" |
| mais de 64 tarefas | 64 primeiras pela ordenação + "+N" discreto |
| tarefa do pomodoro sumiu | contador segue; diálogo final sem `Concluir` |

## 9. Testes (no PC, sem placa)

Cobrindo o `core/`:

- **Filtro:** tarefa de dia inteiro de hoje em UTC−3 não é classificada como atrasada
  (a pegadinha da meia-noite UTC); a mesma tarefa em UTC+5:30 também é de hoje; atrasada
  de ontem às 23h vs. tarefa de hoje às 8h; tarefa sem `dueDate` fica fora; `status != 0`
  descartado.

  > O núcleo trabalha em **epoch local** (= UTC + offset fornecido pela casca), então ele é
  > alheio a horário de verão: uma transição é só um offset diferente entrando. Por isso os
  > testes exercitam **offsets distintos** em vez de simular a virada.
- **Ordenação:** atrasada antes de hoje mesmo com prioridade menor; prioridade
  `5 > 3 > 1 > 0`; dia inteiro após as com hora do mesmo grupo; desempate por `sortOrder`.
- **Título:** truncagem em 80 bytes respeita fronteira de UTF-8 (não parte acento).
- **Pin:** slots preenchidos por pins primeiro; pin apontando para tarefa ausente é
  limpo; dois pins ocupam os dois slots.
- **`task_store`:** `markPending` remove da visão; `revertPending` reinstala com erro;
  `markStale` preserva o conjunto anterior.
- **Pomodoro:** todas as transições; `TERMINADO` não expira; `Renovar` reinicia com a
  mesma duração; tarefa ausente suprime `Concluir`.
- **`payload`:** parse com filtro extrai exatamente os campos do `Task`; JSON malformado
  não corrompe o array anterior.

## 10. Fora de escopo (YAGNI explícito)

Não entram nesta versão, e cada um tem motivo:

- **Pausa e ciclos do pomodoro** (pausa curta/longa, contagem de ciclos) — a pausa
  automática briga com o diálogo de fim, e a contagem de ciclos não tem onde ser
  persistida com sentido. Dá para acrescentar depois.
- **Criar ou editar tarefas no gadget** — teclado de 3,5" não é lugar para isso.
- **Subtarefas, tags, hábitos, histórico de concluídas** — a API não oferece o suficiente.
- **Sincronizar o foco com o TickTick** — não existe endpoint.
- **Multi-conta** (o vizinho tem 4 slots) — uma conta resolve o caso de uso.
- **Tarefas sem data e janela de 2 dias** — afogam a ideia de "o que eu faço agora".
- **LED WS2812B, som** — não há buzzer validado nesta placa no doc de referência.
- **PlatformIO** — trocaria um toolchain validado nesta placa por outro.

## 11. Riscos

| risco | mitigação |
|---|---|
| Refresh token não documentado oficialmente | falha alta e recuperável: portal reabre, 2 min de ritual |
| Rotação de CA quebra o TLS | interruptor para `setInsecure()` no Settings |
| Muitas listas deixam o ciclo lento | conexão reusada + contagem visível na seleção |
| mDNS instável no Windows | IP numérico sempre visível no Status |
| Teto de 64 tarefas | indicador "+N"; o Foco mostra só 2, então não afeta o uso principal |
| Sem `g++` nativo nesta máquina | `tests/run.sh` cai no MSVC 14.51 já instalado |
| Fontes acentuadas inflam o flash | ~200 KB em 16 MB; medir antes de fechar os tamanhos |

## 12. Sequência de implementação sugerida

A implementação saiu em **três planos** (o desenho original de 2026-09-24 previa
dois; a divisão real, registrada no adendo de 2026-09-26, separou o pomodoro, o
Status/Settings completos e a publicação num terceiro):

- **Plano A — fases 1 a 4:** bring-up, núcleo testado no PC e esqueleto de UI. Verificável
  de ponta a ponta **sem nenhuma credencial do TickTick**, o que faz dele um bom ponto de
  parada e revisão.
- **Plano B — fases 5 a 8:** WiFi, pareamento, API e as telas com dados reais.
- **Plano C — fases 9 a 11: feitas.** Pomodoro, Status e Settings completos, e o
  material de publicação. Acrescentou também, fora do escopo original, o horário
  no header e a tela do relógio (7.1, 7.10).

A ordem pensada para validar o difícil cedo:

1. `bringup/` nesta máquina — cor, orientação e touch conferidos.
2. `core/` com testes: `task`, `day_filter`, `task_order`, `payload`, `task_store`,
   `pomodoro`. Tudo no PC, sem placa.
3. `platform/` (display, touch, clock/SNTP) + `.ino` mínimo desenhando um "hello" com a
   fonte acentuada gerada.
4. `ui/theme` + `i18n` + esqueleto de navegação (3 telas vazias com header e pontinhos).
5. WiFi (`wifi_manager.h` adaptado) + tela de WiFi.
6. `helper/pair.py` + `onboarding_web` + `oauth_store` — pareamento ponta a ponta.
7. `ticktick_api` — varredura com conexão reusada e parse filtrado; tela de seleção de listas.
8. Foco e Hoje com dados reais + conclusão otimista.
9. Pomodoro (overlay no header, expansão, diálogo de fim) — **feito**.
10. Status, Settings, estados de erro e selo de desatualizado — **feito**.
11. README bilíngue, mockups e documentação de build para publicação — **feito**.

O console de depuração (`platform/console.{h,cpp}`) ganhou, ao longo do Plano C,
os comandos `pin <dígitos>` (desbloqueia pela serial), `go
foco|hoje|status|relogio|settings` (navega sem tocar, usado pelas verificações e
pelas capturas), `pomo start|stop|fim` (atalhos do ciclo do pomodoro), `demo
on|off` (carrega tarefas de exemplo e pausa o refresh, para capturas sem dado
real) e `shot <nome>` (envia o último quadro renderizado pela serial); `lang
pt|en` já existia desde o Plano A/B. `tools/shot2png.py` (biblioteca padrão)
converte a saída do `shot` num PNG, e `tools/capture_readme.sh` encadeia esses
comandos numa sessão de console só para gerar as capturas do README.
