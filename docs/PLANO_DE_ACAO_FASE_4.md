# Project Freedom — Plano de Ação (Fases 0 a 6)

> **Este documento é um PLANO, não uma implementação.** Nenhum código novo foi escrito para ele.
> Foi preparado para que **qualquer modelo de IA** possa executá-lo, uma tarefa por vez, sem ter visto a conversa original.
> Ele **complementa** o `PROJECT_FREEDOM_SPEC.md` (arquitetura, build, regras de tempo real, pegadinhas).
> **Onde houver conflito de ordem/prioridade, este plano vale mais que o §14 do SPEC** (porque incorpora o feedback do Marco 3).

- **Base de código:** Marco 3 (arranjo com várias trilhas, cadeia de plugins, piano roll estilo Live).
- **Origem das demandas:** feedback do dono após testar o Marco 3 em Windows com plugins reais (Nexus etc.), biblioteca de ícones SVG v1, logo, manual de referência do Live (PDF — **é o Live 12**; o alvo é Live 10/11).
- **Idioma de trabalho com o dono:** português (Brasil), linguagem simples; ele vem de Java/POO e **não conhece C++**.

---

## 0. Como usar este plano

### 0.1 Fluxo obrigatório (por tarefa)
1. Ler a tarefa (seção 6) **e** as regras do SPEC (§5 arquitetura, §10 tempo real, §12 pegadinhas).
2. Mostrar ao dono um **mini-plano** (arquivos que vai tocar + testes que vai escrever) — *antes* de codar.
3. Lógica pura → `core/` **com testes primeiro**. UI/áudio → `app/`.
4. Compilar (Windows: ver SPEC §3.1) e rodar `roll_tests`. **Nunca** declarar pronto sem compilar.
5. Entregar `.zip` do projeto + resumo no formato do SPEC §18.4 (**o que foi e o que NÃO foi testado**).
6. Atualizar o SPEC (matriz §4, dívida §13) e marcar a tarefa aqui (`- [x]`).

### 0.2 Prompt de abertura para colar em outro modelo
```text
Você vai continuar o "Project Freedom" (DAW open source, C++20 + JUCE 8.0.15 + CMake, inspirado no
Ableton Live 10/11, só Arrangement View). Leia PROJECT_FREEDOM_SPEC.md e PLANO_DE_ACAO_FASE_4.md inteiros.
Execute UMA tarefa por vez, na ordem da seção 5 (comece pela primeira não marcada). Antes de codar,
mostre um mini-plano (arquivos + testes). Responda em português (Brasil), simples, com comandos exatos de
PowerShell. Diga sempre o que NÃO foi testado (áudio real/plugins). Não invente APIs do JUCE: confira no
código-fonte em build/_deps/juce-src. Não copie texto/ícones/nome do Live. Respeite a seção 2 (o que preservar).
```

### 0.3 Convenções deste documento
- **P0** = bloqueia o uso / regressão grave · **P1** = pedido forte do dono · **P2** = desejável · **P3** = futuro.
- **Tamanho:** S (≤ meio dia) · M (1–2 dias) · L (3–5 dias) · XL (> 1 semana).
- **DEC-n** = decisão de design com **padrão recomendado** (o executor segue o padrão se o dono não responder).
- **VERIFICAR NO LIVE** = comportamento que o manual não resolve; **pedir ao dono para testar no Live dele** e relatar.

---

## 1. Entradas recebidas e pendências

| Entrada | Estado |
|---|---|
| Feedback do Marco 3 (lista longa do dono) | ✅ Triado na seção 3 |
| Biblioteca de ícones `project_freedom_icons_v1.zip` (24 SVGs) | ✅ Recebida e inspecionada (seção 7) |
| Logo `first_logo.jpg` (F/R/E/E) | ✅ Recebido (só raster com textura — ver SPEC §19.5) |
| Manual de referência do Live (PDF) | ✅ Usado: capítulos *Using Grooves* (cap. 13), *Locators* (6.3), *Using the Editing Grid* (6.9), *Automation and Editing Envelopes* (cap. 21), *Editing MIDI Notes and Velocities* (cap. 10) |
| **Imagem: menu de contexto (botão direito) do piano roll** | ✅ Recebida — descrita em palavras na **seção 13** e usada em F1.8/F1.9/G5 |
| **Imagem: navegador do Live (Collections / Categories / Places)** | ✅ Recebida — estrutura descrita na **seção 13** e usada em F1.1/U3 |
| **Imagem: Plug-Ins → VST3 → pastas de fornecedores** | ✅ Recebida — **mudou a DEC-4** (agrupar por **pasta**) e a F0.8 |
| Ícones adicionais (lista na seção 7.3) | ⏳ Pedir ao dono numa 2ª rodada |
| Logo vetorial/PNG transparente + versão clara + `.ico` | ⏳ Pedir ao dono (SPEC V5) |

---

## 2. O que está BOM e deve ser PRESERVADO (não quebrar)

O dono elogiou explicitamente. **Qualquer mudança que regrida isto é rejeitada.**

1. **Play/Pause (Espaço), voltar ao início, Stop e Loop** — "excelentes". Só melhorar *feedback visual* (cursor/tooltip).
2. **Piano roll** — "estado da arte, até melhor que o do Ableton". Preservar: **Scale + Destacar + Fold + Fold escala** (resolve a dor de ter que desativar notas e mover para fora), atalhos já implementados (SPEC §8.3), criação por duplo clique, teclas que acendem, zoom, velocity lane.
3. **Cadeia de plugins aparecendo** e **lista completa de ~175 plugins** (agora visível).
4. **Criação de clips e notas** funciona ("selecionar uma região e dar duplo clique") — ver F1.6/F1.9: o fluxo é correto mas **pouco descobrível**; melhorar sem trocar.

---

## 3. Triagem do feedback (tudo que o dono pediu, 1 a 1)

| # | Pedido do dono (resumido) | Tipo | Prio | Tarefa |
|---|---|---|---|---|
| 1 | Programa e janelas de plugin **ultrapassam as bordas do monitor**; precisa tamanho padrão + redimensionar fácil | **Bug** | **P0** | F0.1 |
| 2 | Plugin minimizado **não consigo recuperar**; não sei onde está; saber se está aberto e quantos | **Bug/UX** | **P0** | F0.2 |
| 3 | Trocar de canal com **1 clique** (hoje só clicando no indicador de volume); duplo clique = renomear/M/S etc. | **Bug/UX** | **P0** | F0.3 |
| 4 | **Teclado do computador como MIDI** (A–L, W E T Y U, Z/X oitava, C/V velocity ±20), **também com janela de plugin aberta** | Feature | **P0** | F0.7 |
| 5 | Ajustar **BPM arrastando o mouse** para cima/baixo | Feature | P1 | F0.5 |
| 6 | **Metrônomo** | Feature | P1 | F0.6 |
| 7 | Plugins **organizados por fornecedor (pastas, como no Live)**, expandir/ocultar | Feature | P1 | F0.8 |
| 8 | **Arrastar e soltar plugin** no canal (cursor indica o que acontece) | Feature | P1 | F1.3 |
| 9 | Lista de plugins do canal **embaixo à esquerda**, cresce para a direita, mostrar/ocultar; clicar no canal mostra | Feature | P1 | F1.2 |
| 10 | **Duplo clique no canal** abre todas as informações + lista de plugins abertos | Feature | P1 | F1.2 |
| 11 | **Navegador no lado DIREITO** (única diferença deliberada do Live: dá identidade) | Layout | P1 | F1.1 |
| 12 | Poder **arrastar/redimensionar** partes do DAW (painéis, trilhas…) | Feature | P1 | F1.4 |
| 13 | **Cursor do mouse** mostra o que está acontecendo (hover, arrastar, copiar, bloqueado) | Design | P1 | F1.5 |
| 14 | **Selecionar região** da trilha → botão direito → **Inserir clip MIDI vazio** | Feature | P1 | F1.6 |
| 15 | **Selecionar região e fazer loop** (Ctrl+L) | Feature | P1 | F1.6 |
| 16 | **Locators** (cue points) no arranjo, ir/voltar fácil | Feature | P1 | F1.7 |
| 17 | Menu de contexto do piano roll com seção **Fixed Grid** (imagem recebida — F1.8) | Feature | P1 | F1.8 |
| 18 | Adicionar/remover notas como **blocos 1/4, 1/8, 1/16…**, esticar/encolher arrastando — **não só no modo desenho** | Feature | P1 | F1.9 |
| 19 | **Ctrl+U** posicionar notas perfeitamente na grade (como no Live) | **Bug?** | P1 | G5 (VERIFICAR) |
| 20 | **GROOVE** (MUITO IMPORTANTE): swing/síncope/ritmo/velocity automáticos e ajustáveis, **grooves pré-prontos** (hi-hat, shaker, synth, hip-hop, 8th, 16th…), igual ao Live | **Feature núcleo** | **P1+** | Fase 2 |
| 21 | **Automação** (volume, ligar/desligar plugin em um trecho, etc.) — **indispensável** | **Feature núcleo** | **P1+** | Fase 4 |
| 22 | **Áudio:** canais de áudio, navegador de pastas (raiz, criar pasta, favoritos), arrastar áudio para canal de áudio, **pré-escuta** com volume, editar/renomear/excluir arquivos | **Épico** | P1 | Fase 5 |
| 23 | Interação **áudio→trilha MIDI** (ver DEC-1) | Decisão | — | DEC-1 |
| 24 | Pasta de samples do dono: `D:\music-production\Samples` (favorito padrão sugerido) | Config | P2 | U3 |
| 25 | Tocar notas com teclado de **plugin aberto** (foco) | **Risco técnico** | P0 | F0.7 (spike) |
| 26 | Ícones SVG integrados + paleta oficial + estados (hover/pressed/disabled) | Visual | P1 | Trilha V |

> **Observação do dono para não confundir:** o texto dele mistura *o que falta* com *o que já está bom*. A seção 2 lista o que está bom. Tudo da tabela acima é o que **falta ou está com problema**.

---

## 4. Decisões de design (com padrão recomendado)

| ID | Questão | Padrão recomendado | Por quê |
|---|---|---|---|
| **DEC-1** | Arrastar **áudio** para trilha **MIDI**: converter a trilha ou bloquear? O dono descreve as duas coisas. | **Bloquear** (cursor "proibido" + dica "arraste para uma trilha de áudio ou para o espaço vazio para criar uma"). Arrastar para a **área vazia** cria uma trilha de áudio nova. Opcional futuro: comando explícito "Converter trilha" com confirmação (destrutivo). | É o comportamento do Live (citado pelo dono) e evita perder dados da trilha MIDI. |
| **DEC-2** | Formato de grooves | **Formato próprio `.pfg` (JSON).** **Não** importar `.agr` do Live e **não** empacotar a biblioteca de grooves do Live. | Direitos autorais + complexidade. Grooves embutidos serão **originais** (ver G2). |
| **DEC-3** | Posição do navegador | **Lado direito**, com abas Plugins / Amostras / Grooves. | Pedido explícito do dono. |
| **DEC-4** *(revisada com a imagem de referência)* | Como agrupar plugins | **Por PASTA, como o Live:** raiz **VST3** → **uma pasta por fornecedor** (1º nível de subpastas dentro da pasta de plugins VST3 do sistema) → plugins. Plugins soltos na raiz aparecem no mesmo nível das pastas. **Alternância opcional** "Agrupar por: Pasta / Fabricante (metadado)". Nomes exatamente como estão no disco. | A imagem do Live mostra **"iZotope" e "iZotope, Inc." como grupos separados** — comportamento típico de **nomes de pasta**, não de um metadado normalizado. O caminho vem de `PluginDescription::fileOrIdentifier`. |
| **DEC-5** | Áudio sem warp | **v1: áudio toca na velocidade original** (sem esticar para o BPM). Warp/time-stretch = épico futuro. | Escopo controlável. Avisar o dono na UI ("sem warp"). |
| **DEC-6** | Escopo da automação v1 | Volume, pan, **ligar/desligar dispositivo**, **parâmetros de plugin**. Desenho/edição de breakpoints; **sem gravar automação** ao vivo na v1. | Resolve os pedidos do dono; gravação entra depois (A5). |
| **DEC-7** | Teclas do computador com janela de plugin focada | Fazer **spike** (F0.7b). Fallback: abrir editor do plugin **dentro** do painel de dispositivos (sem janela separada) ou devolver o foco à janela principal ao passar o mouse. | Janelas nativas de plugin (HWND) recebem teclas sozinhas; não há garantia de repasse. |
| **DEC-8** | Persistência antes ou depois de groove/automação? | **Persistência (Fase 3) logo após a Fase 1**, com `formatVersion` e **campos opcionais**; cada feature nova (groove, locators, automação, áudio) **estende o serializador** como parte do seu aceite. | O app é inutilizável sem salvar; não esperar todas as features. |

---

## 5. Ordem de execução (e dependências)

```
FASE 0  Correções críticas ───────────────┐  (rápidas; liberam o uso real)
  F0.1 janela/DPI   F0.2 plugin minimizado   F0.3 clique na trilha
  F0.5 BPM arrastável   F0.6 metrônomo   F0.7 teclado MIDI   F0.8 plugins por fabricante
                                           │
FASE 1  Layout e interação estilo Live ────┤
  F1.1 navegador à direita   F1.2 cadeia embaixo/inspetor   F1.3 arrastar-e-soltar plugin
  F1.4 painéis redimensionáveis   F1.5 cursores/tooltips   F1.6 seleção de região + menu
  F1.7 locators   F1.8 menu de grade   F1.9 ferramentas/blocos de nota
                                           │
FASE 3  Persistência (ex-Marco 4) ─────────┤  ← ANTES do groove/automação (DEC-8)
FASE 2  Groove + Quantize (NÚCLEO) ────────┤  depende de F1.8/F1.9 (grade/blocos) e F3 (salvar)
FASE 4  Automação ─────────────────────────┤  depende de F1.6 (seleção de tempo), F3
FASE 5  Áudio (trilhas, clips, navegador) ─┤  depende de F1.1, F1.3, F3
FASE 6  Entrada MIDI/gravação + restante do SPEC (undo global, export…) 
TRILHA V (ícones/paleta) ── paralela; fazer V1+V2 cedo (antes da F1.1) para evitar retrabalho de UI
```

**Ordem recomendada:** `V1+V2` → **F0.1 → F0.2 → F0.3** → F0.5 → F0.6 → F0.8 → F0.7 (+spike) → F1.1 → F1.2 → F1.3 → F1.5 → F1.4 → F1.6 → F1.7 → F1.8 → F1.9 → **Fase 3** → **Fase 2** → Fase 4 → Fase 5 → Fase 6.

> Se só houver tempo para **uma sessão curta**: faça **F0.1, F0.2, F0.3** (os três P0 de bug) — resolvem o que impede o uso diário.

---

## 6. Especificação das tarefas

### FASE 0 — Correções críticas

#### F0.1 — Janela principal e janelas de plugin dentro da tela (P0, M)
**Problema:** a janela abre com tamanho fixo (hoje 1360×800 em `Main.cpp`/`MainComponent`) e, com escala do Windows (125/150%), **passa das bordas do monitor**; o dono precisa esconder a barra de tarefas para alcançar minimizar/maximizar. Plugins também abrem fora da tela.
**Especificação**
- **Tamanho padrão** = ~85% da **área útil** do monitor principal (`Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea`, que já está em coordenadas lógicas com DPI), **limitado** a no mínimo 1000×620 e nunca maior que a área útil. Centralizar.
- `setResizable(true, true)` + `setResizeLimits(min, max = área útil do monitor onde a janela está)`.
- **Persistir** posição/tamanho/estado (maximizada) em `ApplicationProperties`/`PropertiesFile` (pasta `%APPDATA%\ProjectFreedom`). Ao abrir: se a posição salva estiver **fora de todos os monitores atuais**, recentralizar.
- Barra de título **nativa** (já é) com minimizar/maximizar/fechar visíveis; duplo clique na barra maximiza.
- **Janelas de plugin:** ao abrir, **limitar à área útil** do monitor e posicionar de forma que a barra de título fique visível; **cascatear** posições para várias janelas. Se o editor for maior que a tela: tentar `editor->setScaleFactor()` (reduz se o plugin suportar) e, se não, mostrar num `Viewport` com rolagem.
- Menu/atalho **"Restaurar layout da janela"**.
**Aceite:** em um monitor de 1366×768 e em 1920×1080 com escala 150%, a janela abre **inteira visível**; tamanho/posição persistem entre execuções; plugin com editor grande abre visível; barra de título sempre alcançável.
**Testes:** função pura `clampToDisplay(bounds, userAreas, minSize)` testável (pode ficar em `core/` sem JUCE usando structs simples).
**Arquivos:** `Main.cpp`, `MainComponent.*`, novo `WindowPlacement.h/.cpp`.

#### F0.2 — Recuperar plugin minimizado + estado "aberto/fechado" (P0, M)
**Problema (causa provável):** `openDeviceEditor` faz `toFront(true)` mas **não** chama `setMinimised(false)`; a janela minimizada não tem como voltar.
**Especificação**
- Clicar em **"Abrir"** / duplo clique no dispositivo: se a janela existe → `setMinimised(false)` + `toFront(true)` + garantir que está na área visível.
- **Indicador de estado** em cada caixa de dispositivo: ● aberto / ○ fechado / ▭ minimizado (**ícone + texto**, nunca só cor).
- **Resumo no cabeçalho da trilha:** "N plugins · M abertos".
- **Menu "Janelas" > "Plugins abertos"** (lista) e atalho para **trazer todos à frente / fechar todos**.
- Fechar a janela do plugin (X) **não remove** o plugin da cadeia (apenas fecha a UI).
**Aceite:** abrir plugin → minimizar → clicar na caixa dele ou no menu → janela volta; contagem correta ao abrir/fechar/remover.
**Arquivos:** `MainComponent.*` (mapa `windows`), `DeviceView.*`, `ArrangementView.cpp` (cabeçalho).

#### F0.3 — Selecionar trilha com 1 clique em qualquer ponto do cabeçalho (P0, S)
**Causa provável:** o `Label` do nome (e outros filhos) **consomem** o clique; só áreas "livres" do cabeçalho chegam ao `Header::mouseDown`.
**Especificação**
- **1 clique em qualquer ponto** do cabeçalho (inclusive sobre nome, resumo, medidor, slider, botões M/S) **seleciona a trilha**; M/S/slider continuam fazendo sua ação **e** selecionam a trilha.
- `Label` do nome: `setInterceptsMouseClicks(false,false)` e tratar **duplo clique no nome** (retângulo do nome) → renomear. **Duplo clique no resto** do cabeçalho → abre o **inspetor da trilha** (F1.2).
- Clique em área vazia da faixa (lane) já seleciona a trilha (manter).
**Aceite:** clicar em qualquer pixel do cabeçalho troca a seleção; renomear continua por duplo clique no nome; M/S/volume funcionam.
**Arquivos:** `ArrangementView.cpp` (`Header`).

#### F0.5 — BPM arrastável (P1, S)
**Spec:** o campo BPM funciona como "valor arrastável" do Live: **mouse down + arrastar vertical** muda o BPM (≈ 1 BPM a cada 2–3 px; **Shift** = passo fino 0,01–0,1; **Ctrl** = passo grande); **duplo clique** digita; roda do mouse opcional; cursor vira "setas ↑↓" durante o arraste; limites 20–999. Mostrar 2 casas.
**Aceite:** arrastar para cima aumenta, para baixo diminui; valor mostrado atualiza em tempo real e o andamento muda sem estalos (BPM é atômico no grafo).
**Arquivos:** `TransportBar.*` (novo `DragValueLabel`).

#### F0.6 — Metrônomo (P1, M)
**Spec (core testável):** `core/Metronome` gera **eventos de clique** a partir do `BlockPlan` (um clique por beat; **acento no 1º tempo do compasso**); lógica pura com testes (posição ±1 amostra, respeita loop/seek).
**Áudio:** nó `MetronomeSource` no grafo, somando **na saída master** (não passa pelo mixer das trilhas), som gerado por síntese (seno curto/ruído filtrado; **sem arquivo externo**), volume próprio, liga/desliga atômico.
**UI:** botão com o ícone `metronome.svg` na barra de transporte (estado ligado visível por **forma/fundo**, não só cor) + **pré-contagem** opcional (0/1/2 compassos) — pode ficar para F6 junto da gravação.
**Aceite:** com play, clique em cada beat, acento no tempo 1; acompanha mudanças de BPM; desligar silencia imediatamente.
**Arquivos:** `core/Metronome.*` (+ teste), `AudioEngine.*`, `TransportBar.*`.

#### F0.7 — Teclado do computador como MIDI + spike de foco (P0, M + spike)
**Mapa (confirmar tudo no Live do dono; **não** copiar texto do manual):**
- **A S D F G H J K L** = teclas brancas **C3 D3 E3 F3 G3 A3 B3 C4 D4**; **W E T Y U** = sustenidos (C#3 D#3 F#3 G#3 A#3) (+ **O P** para C#4/D#4 se existirem no Live).
- **Z** = oitava abaixo; **X** = oitava acima (a base volta a ser C3 ao reiniciar/ao apertar atalho de reset, conforme o Live — VERIFICAR).
- **C** = velocity −20; **V** = velocity +20 (limites 1–127; mostrar o valor na UI).
- **Ativar/desativar:** botão com o ícone `keyboard.svg` na barra + tecla **M** (como no Live — VERIFICAR). **Enquanto ativo, as letras acima deixam de ser atalhos** (B, K, 0… do piano roll) — mostrar claramente o estado (**indicador "MIDI teclado ligado"**).
**Conflito conhecido com o menu do piano roll (imagem do dono):** no Live, **Z** = *Zoom to Clip Selection* e **X** = *Zoom Out Clip Completely*. Com o teclado MIDI **ligado**, **Z/X passam a ser oitava** (como o dono descreve); com ele **desligado**, valem como zoom (ver F1.8).
**Roteamento:** notas vão para a **trilha selecionada** (pré-escuta já existe: `AudioEngine::previewNote`/`TrackSource::preview`), com velocity configurável (hoje fixo 0,8). Reter nota enquanto a tecla estiver pressionada (note-off ao soltar; tratar **auto-repeat** do teclado ignorando repetições; liberar tudo se a janela perder o foco).
**Spike F0.7b (DEC-7):** com **janela de plugin focada**, as teclas **não chegam** ao `MainComponent`. Investigar, nesta ordem: (1) `KeyListener` na própria `PluginWindow` encaminhando ao mesmo gerenciador; (2) devolver o foco à janela principal quando o ponteiro sair/entrar; (3) **editor do plugin embutido** no painel de dispositivos (sem janela separada); (4) hook de teclado do Windows limitado ao processo (último recurso; avaliar risco). **Documentar a decisão** no SPEC.
**Aceite:** tocar A–L produz som no instrumento da trilha selecionada; Z/X mudam oitava; C/V mudam velocity; **funciona com a UI do plugin aberta** (ou fallback documentado); soltar a tecla encerra a nota; sem notas presas ao trocar de janela.
**Testes (core):** `ComputerKeyboardMap` (tecla → nota, deslocamento de oitava, limites 0–127, velocity clamp) como lógica pura.
**Arquivos:** novo `app/ComputerKeyboard.*` + `core/ComputerKeyboardMap.*`; `MainComponent.*`, `MainComponent::PluginWindow`.

#### F0.8 — Navegador de plugins agrupado por pasta de fornecedor (P1, M)
**Referência (imagem do dono, descrita em palavras):** na categoria **Plug-Ins**, o Live mostra os nós **VST** e **VST3**; abrindo **VST3** aparece **uma pasta por fornecedor**, em ordem alfabética sem diferenciar maiúsculas (ex.: 194dB Audio, Antares, Cableguys, Celemony, DarkEffekt, Exo Audio, FabFilter, Heavyocity, Higher Plane Software, Illformed, iZotope, "iZotope, Inc.", Minimal, Native Instruments, oeksound, Plugin Alliance, Plugins That Knock, reFX, Sugar Bytes, Tritik, u-he, UJAM, Ultra.Audio, Valhalla DSP LLC, Waves, Xfer Records). Cada pasta tem **seta de expandir/recolher**; o item selecionado fica com realce.
**Spec:**
- Trocar o `ListBox` por `juce::TreeView` com raiz **VST3** (o nó **VST** só aparece se um dia houver suporte; hoje **não** mostrar).
- **Agrupamento por pasta (DEC-4):** derivar do caminho do plugin o **1º nível de pasta** abaixo da pasta VST3 em que foi encontrado (`C:\Program Files\Common Files\VST3\<Fornecedor>\...`). Plugins no nível raiz ficam **soltos** ao lado das pastas. Mostrar **contagem** por pasta. Alternância opcional **Pasta / Fabricante**.
- Instrumentos e efeitos continuam filtráveis (abas Todos/Instrumentos/Efeitos) **sem** quebrar a árvore; marca roxa nos efeitos.
- **Busca (Ctrl+F)** expande os grupos com resultado; limpar a busca restaura o estado anterior.
- **Estado de expansão persistido** (por nome de pasta).
- Duplo clique/Enter carrega; **arrastar** (F1.3).
**Aceite:** com ~175 plugins, a árvore reproduz a organização por pastas do disco; expandir/recolher e busca funcionam; estado persiste; sem regressão no carregamento.
**Testes (core):** `groupByFolder(paths, rootPath)` (função pura: caminhos → grupos ordenados, raiz solta, nomes com vírgula/espaço).
**Arquivos:** `BrowserPanel.*` (+ função de agrupamento no `core/` ou em header sem JUCE).

---

### FASE 1 — Layout e interação estilo Live

#### F1.1 — Navegador à direita com abas (P1, M)
**Estrutura de referência (imagem do dono):** o navegador do Live tem **barra de busca (Ctrl+F)** no topo, três blocos à esquerda — **Collections** (marcadores coloridos: Vocals/Mix/Synths/Drums/Misc), **Categories** (Sounds, Drums, Instruments, Audio Effects, MIDI Effects, Max for Live, **Plug-Ins**, Clips, **Samples**, **Grooves**, Templates) e **Places** (Packs, User Library, **Current Project**, **Add Folder…**) — e à direita a **lista de conteúdo** com coluna "Name" ordenável e pastas expansíveis; um botão de **pré-escuta** fica no canto inferior esquerdo. **Mapeamento para o Project Freedom (no lado direito):** *Categorias:* **Plug-Ins** (F0.8), **Samples** (U3), **Grooves** (G3), **Clips** (futuro); *Locais:* **Favoritos** (pastas do usuário — equivale ao "Add Folder…"), **Projeto atual**, **Este computador**; *Coleções:* **favoritos coloridos** (P2, opcional); botão de **pré-escuta** com volume (U3). **Não** replicar Packs/Push/Max for Live/Templates.
Mover `BrowserPanel` para o **lado direito** (DEC-3), largura **redimensionável** (F1.4), abas **Plugins | Amostras | Grooves** (as duas últimas ficam desabilitadas até existirem). Layout: arranjo ocupa o centro/esquerda. Cabeçalhos de trilha continuam à esquerda do arranjo. **Aceite:** layout estável em 1366×768; sem sobreposição; atalho para mostrar/ocultar o navegador.

#### F1.2 — Cadeia de dispositivos na base + inspetor da trilha (P1, M)
- **Selecionar uma trilha** → o painel inferior mostra a **cadeia de dispositivos dela** (instrumento, depois efeitos **sempre acrescentados à direita**), no canto **inferior esquerdo**, expandindo para a direita conforme entram plugins; **mostrar/ocultar** o painel (atalho; **VERIFICAR NO LIVE** qual atalho o Live usa).
- **Selecionar um clip** → painel inferior mostra o **Clip (piano roll)**; alternar manualmente continua possível (abas atuais, Shift+Tab/F12).
- **Duplo clique no cabeçalho** (fora do nome) → **inspetor da trilha**: nome, tipo, nº de plugins, estado de cada um (aberto/fechado/bypass), volume/pan, entrada/saída (informativo).
**Aceite:** clicar numa trilha mostra imediatamente seus plugins; adicionar um plugin faz a caixa nova surgir **no fim (direita)**; ocultar/mostrar funciona.

#### F1.3 — Arrastar e soltar plugin no canal (P1, M–L)
- `BrowserPanel` vira **origem** (`DragAndDropContainer` na janela principal; `startDragging` com **imagem fantasma** do nome do plugin).
- **Alvos** (`DragAndDropTarget`): cabeçalho da trilha, faixa (lane) da trilha, painel da cadeia (**posição de inserção** entre dispositivos), área vazia do arranjo (**cria trilha nova** com o plugin, se instrumento).
- **Feedback obrigatório:** alvo válido → **realce** + cursor de "copiar"; alvo inválido → cursor **proibido** (X) + dica; **linha de inserção** na cadeia.
- Regras: instrumento solta numa trilha com instrumento → **substitui** (confirmar com aviso discreto); efeito → entra na posição solta (hoje só no fim).
- **Reordenar** dispositivos arrastando dentro da cadeia (o engine precisa de `moveDevice(trackId, deviceId, newIndex)` + `rewire`).
**Aceite:** arrastar plugin da lista até uma trilha carrega nela; cursor muda conforme o alvo; soltar fora cancela sem efeito; reordenar altera a ordem do áudio.
**Testes:** lógica de "pode soltar aqui?" e de índice de inserção como funções puras.

#### F1.4 — Painéis e trilhas redimensionáveis (P1, M)
Barras divisórias **arrastáveis** com cursor próprio: navegador↔arranjo, arranjo↔painel inferior (já existe, tornar persistente), **largura do cabeçalho**, **altura de cada trilha** (arrastar a borda inferior do cabeçalho; mínimo/máximo; "altura padrão" no menu). Persistir tamanhos. **Aceite:** todas as divisões respondem ao mouse, respeitam limites e persistem.
*(Dúvida para o dono, prioridade baixa: "arrastar partes do DAW" inclui **janelas flutuantes/destacáveis**? Padrão: **não** nesta fase.)*

#### F1.5 — Cursores e dicas contextuais (P1, M)
- `CursorManager` central: mapa **contexto → cursor**: seta (padrão), **mão aberta/fechada** (mover clip/nota), **setas ←→** (borda de redimensionar), **setas ↑↓** (BPM, zoom da régua), **copiar** (Ctrl durante arraste), **proibido** (drop inválido), **lápis** e **borracha** (ferramentas; usar os ícones `pencil.svg`/`eraser.svg` renderizados como `MouseCursor` com *hotspot* correto), **cruz** (seleção de região).
- `TooltipWindow` em todos os controles com **nome + atalho** (ex.: "Metrônomo (…)" ), via `SettableTooltipClient`.
- Cursor **muda ao passar** sobre a zona sensível (borda do clip, borda da nota, brace de loop, divisores), não só durante o arraste.
**Aceite:** em cada interação listada, o cursor reflete a ação; tooltips aparecem após ~600 ms; nada de cursor "preso" ao sair do componente.

#### F1.6 — Seleção de região de tempo + menu de contexto do arranjo (P1, L)
**Modelo:** `TimeSelection {startBeat, endBeat, trackIds[]}` no `ProjectModel` (o **marcador de inserção** do piano roll já existe como ponto).
**Interação:** arrastar em área **vazia** de uma ou mais trilhas seleciona um **intervalo** (snap à grade; Shift estende); mostrar região sombreada.
**Menu de contexto (botão direito na seleção):** **Inserir clip MIDI vazio** (um por trilha selecionada, com o comprimento da seleção, respeitando a regra de não sobreposição); **Loop da seleção (Ctrl+L)** (define o brace e liga o loop — **pedido do dono**); Dividir (Ctrl+E); Consolidar (Ctrl+J); Apagar tempo; Duplicar tempo; Renomear. (Conferir no manual, seção 6.8 *Editing the arrangement*, e no Live do dono.)
**Aceite:** selecionar região → botão direito → "Inserir clip MIDI vazio" cria o clip exato; Ctrl+L ajusta o loop para a região e o transporte repete nela.
**Testes (core):** `TimeSelection` (normalização, interseção com clips), `Track::splitAt`, `insertEmptyClip`.

#### F1.7 — Locators / cue points (P1, M)
Conforme o manual (cap. 6.3) — **descrever com palavras próprias**:
- Modelo `Locator {id, beat, name, info}` no projeto (`LocatorList`, ordenada, em `core/`, testável).
- **Adicionar** no ponto do marcador/posição atual (botão "Set" + item de menu + atalho — **VERIFICAR NO LIVE**); posição **quantizada** pela quantização global.
- **Triângulo** na régua do arranjo; clique **salta** para o locator; **duplo clique** salta e **toca**; botões **Anterior/Próximo**; mover arrastando ou com setas; **renomear (Ctrl+R)**; apagar (Backspace); menu de contexto: **Loop até o próximo locator**, "Definir como início da música".
- Anterior/Próximo além dos extremos pulam para início/fim do arranjo.
**Aceite:** criar 3 locators, saltar entre eles com os botões, renomear, mover, apagar; "Loop até o próximo" liga o loop entre dois locators.
**Persistência:** estender `.pfp` (Fase 3).

#### F1.8 — Menu de contexto do piano roll (botão direito) e grade (P1, M)
**Referência (imagem do dono, em palavras).** O menu do Live, de cima para baixo, em **grupos separados por linhas**:
1. **Quantize** *(Ctrl+U)* · **Quantize Settings…** *(Ctrl+Shift+U)* · **Activate/Deactivate Note(s)** *(0; cinza se nada selecionado)* · **Extract Groove(s)**
2. **Crop Clip**
3. **Set 1.1.1 Here** *(VERIFICAR NO LIVE o efeito exato: parece redefinir a origem de tempo do clip no marcador)*
4. **Loop Selection** *(Ctrl+L)* · **Zoom to Clip Selection** *(Z)* · **Zoom Out Clip Completely** *(X)*
5. **Adaptive Grid:** Widest · Wide · Medium · Narrow · Narrowest *(5 níveis)*
6. **Fixed Grid:** 8 Bars · 4 Bars · 2 Bars · 1 Bar · 1/2 · 1/4 · 1/8 · **1/16 (marcado)** · 1/32 · **Off**
7. **Triplet Grid** *(Ctrl+3)*
8. **Draw Mode (Pitch Lock Off)** *(B)* — **novo requisito:** o modo desenho tem **Pitch Lock** (ligado = o desenho horizontal mantém o mesmo tom; desligado = "pincel melódico" livre). O texto do item mostra o estado atual.
9. **Highlight Scales** *(K)* — item com **✓** quando ativo.
**Spec:**
- Implementar o menu **com esses grupos, rótulos (traduzidos) e atalhos**; itens indisponíveis ficam **desabilitados**; o menu também deve aparecer no **botão direito do arranjo** com os itens aplicáveis.
- **Grade fixa** = passo escolhido (de **8 compassos até 1/32**; hoje o código só vai de 1/64 a **1 compasso** → estender o limite superior para 8 compassos = 32 beats); **Off** = sem grade (equivale a snap desligado); **Grade adaptativa** = passo recalculado pelo zoom (`pixelsPerBeat`) com 5 níveis de densidade. **Ctrl+5** alterna fixa/adaptativa; **Ctrl+1/2** estreita/alarga; **Ctrl+3** tripleto; **Ctrl+4** snap.
- Mostrar o **espaçamento atual da grade** no canto inferior direito do piano roll (ex.: "1/16").
- **Z / X:** zoom até a seleção do clip / zoom total do clip (quando o teclado MIDI do computador estiver desligado — ver F0.7).
- Itens que dependem de outras tarefas ficam desabilitados até existirem: **Quantize/Settings** (G5), **Extract Groove(s)** (G3), **Crop Clip** (aparar o clip ao intervalo selecionado — implementar em `core`), **Loop Selection** (F1.6).
**Aceite:** o menu reproduz os grupos acima; ✓ reflete o estado; Ctrl+1…5, B, K, 0, Z, X se comportam como no menu; o indicador de espaçamento acompanha.
**Testes (core):** `adaptiveGrid(pixelsPerBeat, level, triplet)` e `fixedGridBeats(option)`; `cropClip(clip, startBeat, endBeat)`.

#### F1.9 — Ferramentas de edição e "blocos" de nota (P1, M)
**Pedido:** adicionar/remover notas como **blocos de 1/4, 1/8, 1/16…**, **esticar/encolher arrastando**, **não só** no modo desenho.
**Spec:**
- Barra de ferramentas do piano roll com **Ponteiro / Lápis / Borracha** (ícones `pointer`, `pencil`, `eraser`) + **seletor de valor de nota** (1/1 … 1/64, tripleto) ligado à grade.
- **Lápis:** clique cria um **bloco do valor escolhido**; arrastar horizontalmente **estica em passos de grade**; arrastar vertical move a altura; Alt = sem snap. **Borracha:** clique/arraste apaga.
- **Ponteiro (padrão):** como hoje (duplo clique cria; arrastar borda redimensiona). **Mostrar uma dica discreta** no piano roll vazio ("duplo clique cria nota; B = lápis").
- **Draw Mode com Pitch Lock** (imagem do dono): **Pitch Lock ligado** = arrastar na horizontal cria/estica notas **no mesmo tom**; **desligado** = desenho **melódico** (o tom acompanha o mouse). O menu de contexto mostra o estado (F1.8). *(VERIFICAR NO LIVE 10/11 o padrão e como alternar, ex.: tecla modificadora.)*
- Atalhos: **B** alterna lápis (já existe), **E** borracha (VERIFICAR no Live), **V**/Esc ponteiro.
**Aceite:** com lápis em 1/8, clicar 4 vezes cria 4 blocos de colcheia alinhados; arrastar estica por passos; borracha apaga; atalhos funcionam.

---

### FASE 3 — Persistência (ex-Marco 4 do SPEC) — fazer ANTES da Fase 2 (DEC-8)
Executar **T4.1–T4.4 do SPEC §14** (unificar estado; formato `.pfp` JSON versionado com nlohmann/json no `core/`; Salvar/Abrir/Novo (Ctrl+S/O/N); autosave) **com estes acréscimos obrigatórios**:
- Campos **opcionais** e `formatVersion` para: `locators[]`, `window/layout` (tamanhos de painéis, F0.1/F1.4), `track.kind` ("midi"/"audio"), `clip.groove {id, base, quantize, timing, random, velocity}`, `grooves[]` (pool), `automation[]`, `audioClips[]`, favoritos do navegador, estado de expansão do navegador.
- Regra: **arquivo de versão futura** → abrir em modo leitura com aviso, sem perder dados.
- Cada feature posterior (Fases 2, 4, 5) **só é "pronta" quando salva e reabre igual**.

---

### FASE 2 — Groove e Quantize (NÚCLEO DO PRODUTO — "muito importante")

> Objetivo do dono: tirar o aspecto **quadrado/mecânico** das notas, dar **swing, síncope, ritmo e dinâmica** de forma **automática e ajustável**, com **grooves prontos** (hi-hat, shaker, synth, hip-hop, 8th, 16th…), **igual ao Live**. Referência: manual, **cap. 13 *Using Grooves*** (parâmetros) e **cap. 10** (quantizar). Reimplementar **do zero**; **não** copiar a biblioteca de grooves do Live (DEC-2).

#### Conceitos (do manual, em palavras próprias)
- Um **groove** é um padrão de **desvios de tempo e de velocity** medidos contra uma grade (**Base**).
- Aplicado a um **clip** (MIDI), move as notas na direção do padrão, **de forma não destrutiva e em tempo real**; pode ser **confirmado** ("commit") escrevendo o resultado nas notas.
- **Parâmetros por groove:** **Base** (resolução: 1/4, 1/8, 1/8T, 1/16, 1/16T, 1/32…) · **Quantize** (0–100%: quanto as notas são "endireitadas" para a grade **antes** de aplicar o groove) · **Timing** (intensidade do desvio de tempo) · **Random** (jitter aleatório; **por voz**, para "humanizar") · **Velocity** (−100…+100: intensidade/inversão do efeito de dinâmica).
- **Global Amount** (0–130%): escala **Timing, Random e Velocity** de **todos** os grooves; aparece também na barra de transporte.
- Notas do groove que caem **exatamente na grade não se movem**.
- **Quantização não destrutiva:** Timing=Random=Velocity=0 e só Quantize/Base → comporta-se como quantizador em tempo real.
- **Extrair groove** de um clip; **hot-swap** (testar grooves enquanto toca).

#### G1 — Núcleo `core/Groove` + algoritmo (P1+, L) — **testes primeiro**
**Estruturas**
```cpp
struct GrooveStep { double timeOffsetBeats; double velocityOffset; };   // por passo da Base, relativo à grade
struct Groove {
  std::string name;
  double baseBeats;                 // passo da Base em beats (1/16 = 0.25, 1/8T = 1/3 ...)
  double patternBeats;              // comprimento do padrão (ex.: 1 compasso = 4.0)
  std::vector<GrooveStep> steps;    // patternBeats / baseBeats passos
};
struct GrooveParams { double quantize, timing, random, velocity; /* 0..1, velocity -1..1 */ };
double globalAmount;                // 0..1.3
```
**Algoritmo (proposto; fiel ao manual, números a validar contra o Live do dono)**
Para cada nota `n` (início `s`, velocity `v`, tom `p`):
1. `g = round(s / B) * B` (posição de grade mais próxima, `B = baseBeats`); `k = round(s / B)`.
2. **Quantize:** `s1 = s + Q * (g - s)`.
3. **Groove:** passo `i = k mod nSteps`; `s2 = s1 + T * G * steps[i].timeOffsetBeats`.
4. **Random:** `s3 = s2 + R * G * u * (B * 0.5)`, `u ∈ [-1,1]` de um **PRNG determinístico** semeado por (id do clip, id da nota, `seed` do clip) → resultado **estável** entre repetições/renders; botão "Re-sortear" muda a `seed`. *VERIFICAR NO LIVE:* escala real do Random e se muda a cada volta do loop.
5. **Velocity:** `v' = clamp(v + Vamt * G * steps[i].velocityOffset * 127, 1, 127)` (`Vamt` pode ser negativo).
6. **Comprimento da nota:** **mantido** (VERIFICAR NO LIVE se o fim acompanha).
7. `s3 = max(0, s3)`; não deixar uma nota ultrapassar o início da próxima do **mesmo tom** (evitar nota presa).
**Aplicação:** função pura `applyGroove(const Clip&, const Groove&, params, globalAmount, seed) -> std::vector<Note>` chamada em **`flatten()`** (o engine recebe notas já "grooveadas"; sem custo no thread de áudio).
**Testes (obrigatórios):** swing 66% em 1/8 desloca as colcheias pares na proporção esperada; Quantize 100% + Timing 0 = notas exatamente na grade; Timing 0/Random 0/Velocity 0 = notas **inalteradas** (com Quantize 0); notas **na grade** do groove não se movem; Global Amount 0 = sem efeito, 130% amplifica; Velocity negativo **inverte**; determinismo do Random (mesma seed → mesmo resultado); clamp de velocity 1–127; **nenhuma nota com início < 0**; `commit` produz as mesmas notas que o tempo-real.

#### G2 — Grooves embutidos **originais** (P1+, M)
Catálogo próprio (nomes e valores criados aqui — **não** derivados dos `.agr` do Live), agrupados em pastas no navegador (aba Grooves):
- **Swing 8th** e **Swing 16th**: 50 (reto), 54, 58, 62, 66 (tercina), 71, 75 % — deslocamento do tempo fraco = `(p − 50%) × 2B` (ex.: 16th, 62% ≈ +0,03 beat).
- **Shuffle** (tercina marcada) e **Half-time swing**.
- **Hi-hats:** "Hats 16th leve", "Hats 16th empurrado" (notas fracas ligeiramente à frente + acento no contratempo), "Hats trap 1/16T".
- **Shaker/Percussão:** "Shaker 16th solto", "Shaker 8th".
- **Hip-hop:** "Boom-bap laid-back" (backbeat levemente atrasado), "Lo-fi bamba" (swing alto + velocity irregular).
- **Synth/Teclas:** "Pad humanizado", "Arpejo com swing".
- **Humanize (Random + velocity):** "Humano leve / médio / solto".
Cada groove: arquivo `.pfg` + metadados (nome, categoria, descrição). **Afinar de ouvido com o dono** (ele compara com sua experiência no Live). Tabela inicial de números fica em `docs/grooves.md` (criar).
**Aceite:** ≥ 20 grooves carregam; trocar entre eles muda audivelmente o feel.

#### G3 — Groove Pool + seletor no clip + controles (P1+, L)
- **Aba Grooves** no navegador (lista em pastas) e **Groove Pool** (painel): lista dos grooves em uso/carregados, com os 5 parâmetros por groove (sliders com valor numérico, duplo clique digita) e **Global Amount** (também na barra de transporte).
- **Clip → "Groove" (seletor):** escolhe o groove do clip; **Commit** (escreve nas notas; seletor volta a "Nenhum"); **hot-swap** (botão) que percorre os grooves **enquanto toca**; menu do clip: **Extrair groove**.
- Arrastar groove da lista **para um clip** (arranjo) aplica; também para o piano roll.
- Indicador visual no clip (ícone de groove) quando ativo.
**Aceite:** aplicar groove, mexer nos parâmetros **ao vivo** e ouvir; Commit muda as notas visíveis; Extrair cria groove novo na pool.

#### G4 — Integração no engine (P1+, M)
`flatten(track)` passa a aplicar o groove **por clip**; publicar ao engine apenas quando groove/parâmetros/notas mudarem (**debounce** durante arraste de slider). Sem alocação no áudio (mesmo padrão `publishNotes`). **Aceite:** mover slider de Timing durante o play altera o som sem estalos/notas presas.

#### G5 — Quantize completo (P1, M) — **VERIFICAR NO LIVE**
- **Menu de contexto (imagem do dono):** *Quantize* (Ctrl+U), *Quantize Settings…* (Ctrl+Shift+U), *Activate/Deactivate Note(s)* (0) e *Extract Groove(s)* ficam no **topo** do menu do piano roll (F1.8).
- **Ctrl+U** quantiza **o que está selecionado** (ou **o clip inteiro** se nada estiver selecionado — **conferir no Live**) com as **últimas opções**; deve deixar as notas **exatamente na grade** (o dono relata que no Live "posicionava perfeitamente": se o nosso comportamento atual não o faz, é **bug**). Verificar também **fim da nota** (quantizar início e/ou fim).
- **Ctrl+Shift+U** abre **diálogo**: grade (1/4…1/32, tripleto), **quantizar início/fim**, **intensidade (%)**, sem restrição de oitava.
- Lógica em `core/` (`quantizeNotes(notes, options)`), testada.
**Aceite:** nota fora da grade → Ctrl+U → cai na grade; 50% de intensidade → metade do caminho.

#### G6 — Formato `.pfg` e importar/exportar (P2, S)
JSON versionado `{formatVersion, name, category, baseBeats, patternBeats, steps[{t,v}]}`; botão importar/exportar; pasta do usuário `…\ProjectFreedom\Grooves`. (**Sem** suporte a `.agr`, DEC-2.)

---

### FASE 4 — Automação (indispensável)
> Referência: manual, **cap. 21 *Automation and Editing Envelopes*** (desenhar envelopes, editar breakpoints, esticar/inclinar, simplificar, inserir formas, travar) e **interruptores "Device Activator"** (ligar/desligar dispositivo é automatizável). **VERIFICAR NO LIVE** atalhos e fluxo.

#### A1 — Núcleo `core/Envelope` (P1+, M) — testes primeiro
`Envelope { std::vector<Breakpoint{beat, value, curve}> }`, `valueAt(beat)`, inserir/mover/remover ponto, interpolação **linear** (curvas depois), valores normalizados 0–1 mapeados pelo alvo. Invariantes: pontos ordenados; sem dois pontos no mesmo beat.
**Testes:** interpolação, extremos, inserção ordenada, remoção, `simplify`.

#### A2 — Modelo e alvos (P1+, M)
`AutomationTarget { trackId, kind: Volume | Pan | DeviceBypass(deviceId) | PluginParam(deviceId, paramIndex) }`; `AutomationLane { target, Envelope }` por trilha. **Persistir** no `.pfp`.

#### A3 — UI: faixas de automação (P1+, L)
- Botão com o ícone `automation.svg` (ou tecla **A**, **VERIFICAR**) alterna **Modo Automação** no arranjo; cada trilha ganha uma **faixa de automação** abaixo, com **seletor de parâmetro** em 2 níveis: **Dispositivo → Parâmetro** (volume/pan/"Device On" primeiro).
- **Desenhar:** clique cria ponto; arrastar move (snap à grade; Alt sem snap; Shift fino); duplo clique apaga ponto; **modo desenho** (B) pinta; arrastar segmento move 2 pontos; seleção múltipla; **trava de envelope** (opcional).
- **Ligar/desligar plugin num trecho:** envelope de **"Device On"** (degrau). Exibição clara de estado (**forma + cor**).
**Aceite:** desenhar rampa de volume e ouvir o fade; desenhar degrau de "Device On" e ouvir o plugin entrar/sair no trecho.

#### A4 — Engine (P1+, L) — **tempo real**
- **Volume/Pan:** `TrackMixer` lê o valor do envelope **por bloco com interpolação** (ou por amostra) a partir de uma cópia local (mesmo padrão try-lock de `TrackSource`).
- **Device On/Bypass:** nó de **bypass por dispositivo** (ou `getBypassParameter()` quando existir) com **crossfade curto** (evitar clique).
- **Parâmetros de plugin:** aplicar `AudioProcessorParameter::setValue` **no início do bloco** com suavização/`beginChangeGesture` quando necessário. **Risco:** nem todo plugin tolera `setValue` do thread de áudio → **spike**: medir com 3 plugins reais; alternativa: aplicar via fila no thread de mensagem a ~100–200 Hz (menos preciso). Documentar.
- Automação **respeita o loop e o seek** (valor correto ao saltar para o meio).
**Testes (core):** amostragem do envelope por bloco = valores esperados em pontos de controle, inclusive atravessando o fim do loop.

#### A5 — (depois) Gravar automação mexendo no parâmetro (P3)

---

### FASE 5 — Áudio (trilhas, clips e navegador de amostras)

#### U1 — Modelo: tipos de trilha e clips de áudio (P1, L)
- `Track.kind = Midi | Audio`. `Placement` ganha variante **AudioClip {arquivo, início na linha do tempo, comprimento, offset dentro do arquivo, ganho}**.
- Regras (DEC-1): clip de áudio **só** em trilha de áudio; MIDI **só** em MIDI. **Persistir** caminho **relativo** quando possível + detecção de arquivo ausente (aviso, sem crash).
- **Aceite:** modelo + serialização + testes de `core/` (anti-sobreposição também vale para áudio).

#### U2 — Engine de áudio: `AudioTrackSource` (P1, XL) — **tempo real**
Reproduzir clips de áudio sincronizados com o relógio: leitura em **thread de fundo** (`AudioFormatReader` + `BufferingAudioSource`/fila própria), **conversão de taxa** quando o arquivo ≠ taxa do dispositivo, respeita **loop/seek/pausa**, início exato (±1 amostra), **ganho** do clip. Sem I/O no thread de áudio. Efeitos na cadeia da trilha de áudio (sem instrumento). **Sem warp** (DEC-5).
**Aceite:** WAV 44,1k e 48k tocam no tom/velocidade corretos; loop sem clique audível; pausa/retomada corretas.
**Testes:** cálculo de posição dentro do arquivo a partir do `BlockPlan` (puro).

#### U3 — Navegador de amostras (aba Amostras, lado direito) (P1, L)
- **Locais:** **Favoritos** (o dono adiciona pastas; sugerir `D:\music-production\Samples` como primeiro), **Este computador** (raízes/discos via `File::findFileSystemRoots`) e pastas do usuário.
- **Operações:** **criar pasta**, **renomear** (F2), **excluir** (para a **Lixeira** — `File::moveToTrash` — **com confirmação**; nunca apagar definitivamente), **mostrar no Explorer**, **adicionar/remover favorito**, atualizar. **Sempre confirmar** operações destrutivas; **nunca** operar fora do que o usuário escolheu.
- **Pré-escuta:** clicar em um arquivo toca (**auto-preview** ligável), **Play/Stop**, **barra de volume da pré-escuta**, mini-forma de onda, indicador de progresso. Player **separado** do transporte (não interfere no play do projeto; toca na saída padrão; mixagem simples).
- Formatos: o que `AudioFormatManager::registerBasicFormats()` lê (WAV, AIFF, FLAC, OGG…; **MP3 só se habilitado** no build do JUCE — verificar).
- Busca por nome; ordenar; filtro por tipo.
**Aceite:** navegar até D:\music-production\Samples, ouvir arquivos, ajustar volume da pré-escuta, criar/renomear/excluir com confirmação.
**Risco:** operações de arquivo e permissões do Windows; testar com pasta de teste.

#### U4 — Arrastar áudio para trilhas (P1, M)
- Arrastar do navegador para **trilha de áudio** cria clip no ponto solto (snap); para **área vazia** cria **trilha de áudio** nova; para **trilha MIDI** → **proibido** com cursor X (**DEC-1**) e dica.
- Arrastar **plugin** para trilha de áudio: só **efeitos** aceitos (instrumento → proibido).
**Aceite:** os três casos se comportam como descrito; sem crash ao soltar em alvos inválidos.

#### U5 — Forma de onda no clip (P1, M)
`AudioThumbnail` com cache em disco; desenho dentro do clip no arranjo (cor da trilha), atualiza ao redimensionar; carregamento **assíncrono** com placeholder.

#### U6 — Edição básica de clip de áudio (P2, M)
Mover, copiar, duplicar, apagar, **aparar** bordas (offset), dividir (Ctrl+E), ganho; **fades curtos** automáticos para evitar clique.

#### U7 — (futuro) Warp / time-stretch (P3, XL)
Fora de escopo até nova decisão do dono.

---

### FASE 6 — Entrada MIDI/gravação e o restante do SPEC
Executar **SPEC §14**: Marco 5 (desfazer global), **Marco 6** (MIDI de hardware, gravação, **pré-contagem**, tap tempo — *o metrônomo básico já veio na F0.6*), Marco 7/8 (paridade do piano roll/arranjo — **muita coisa foi antecipada aqui**: grade, locators, seleção de região, quantize, groove), Marco 9 (robustez: áudio, master, PDC, isolamento de crash), Marco 10 (exportação), Marco 11 (distribuição). **Atualizar o SPEC §14** marcando como feito o que esta fase antecipou.

---

## 7. Trilha V — Ícones e identidade visual (paralela)

### 7.1 Inspeção da biblioteca recebida (`project_freedom_icons_v1.zip`)
- **24 SVGs**, `viewBox 0 0 24 24`, `fill="none"`, `stroke="currentColor"`, `stroke-width="1.8"`, `stroke-linecap/linejoin="round"`; formas sólidas usam `fill="currentColor" stroke="none"`.
- **Todos os 24 usam `currentColor`** → o parser SVG do **JUCE não entende** `currentColor` (verificado no código-fonte). **Pré-processar:** substituir a string `currentColor` pelo hex do estado antes de `Drawable::createFromImageData`. `stroke-width`/`linecap`/`linejoin` são suportados.
- Cada SVG já tem `role="img"` e `aria-label` (aproveitar como nome acessível/tooltip base).

### 7.2 Tarefas (detalhes no SPEC, trilha V)
- **V1 Carregador (`Icons.h/.cpp`)**: `juce_add_binary_data` a partir de `assets/icons/**.svg` (copiar o conteúdo do zip para `assets/icons/{transport,editing,mixer}`); cache por (ícone, cor, tamanho); fallback "?" visível; teste de fumaça que carrega os 24.
- **V2 Paleta oficial no `Theme.h`** (SPEC §19.1) — **assumido**: verde `#55C982` substitui o verde-água em tudo que é "ligado/ativo"; vermelho **só** gravação; amarelo **só** alerta. (**Confirmar com o dono** — pergunta já feita.)
- **V3 Botões com ícones + estados** (SPEC §19.2): normal/hover/pressed/disabled/ativo; área clicável ≥ 36 px; **tooltip + rótulo** (não depender só de cor).
- **V5 Logo/ícone do app:** pedir ao dono o monograma vetorial/PNG transparente + versão clara + `.ico`.

### 7.3 Onde cada ícone entra
| Ícone | Uso no app |
|---|---|
| `play`, `pause`, `stop`, `rewind`, `record`, `loop`, `metronome` | Barra de transporte (F0.6 usa `metronome`) |
| `fast-forward` | Ir ao próximo locator (F1.7) / avançar |
| `pointer`, `pencil`, `eraser` | Ferramentas do piano roll e do arranjo (F1.9) |
| `cut`, `copy`, `move`, `split`, `select-region` | Menu de contexto/ferramentas do arranjo (F1.6) |
| `mixer`, `volume`, `mute`, `pan`, `level-meter` | Cabeçalho de trilha e inspetor (F0.3/F1.2) |
| `automation` | Alternar Modo Automação (A3) |
| `keyboard` | Teclado do computador como MIDI (F0.7) |
| `waveform` | Aba/pasta de áudio e clips de áudio (Fase 5) |

### 7.4 Ícones que faltam (pedir ao dono na 2ª rodada)
Solo · armar gravação (arm) · adicionar trilha · apagar (lixeira) · renomear · pasta · nova pasta · favorito (estrela) · plugin/dispositivo · instrumento × efeito · bypass/power · locator (bandeira) · groove · pré-escuta (fone/alto-falante) · buscar · atualizar/rescan · desfazer/refazer · setas/chevrons · zoom +/− · grade/snap · quantizar · escala/fold/destacar · abrir/salvar/novo · configurações · minimizar/fechar · ir ao início.

---

## 8. Impacto na arquitetura (o que muda no modelo)

| Mudança | Onde | Observação |
|---|---|---|
| `Track.kind` (Midi/Audio) e clips de áudio | `core/Arrangement`, `ProjectModel`, engine | `flatten` só serve MIDI; áudio tem caminho próprio |
| `TimeSelection`, `LocatorList` | `core/` + `ProjectModel` | Testáveis; entram no `.pfp` |
| `Groove`, `applyGroove`, `Quantize` | `core/` | Aplicados em `flatten()`; **sem** custo no áudio |
| `Envelope`, `AutomationLane` | `core/` + `ProjectModel` + engine | Engine lê cópias locais (padrão try-lock) |
| `Metronome` | `core/` + engine | Soma na saída master |
| `ComputerKeyboardMap` | `core/` + `app/` | Lógica pura testável |
| Cursores/tooltips | `app/CursorManager` | Transversal |
| Persistência de layout | `ApplicationProperties`/`.pfp` | Tamanhos, expansão, favoritos |
| **Estado fora do `ProjectModel`** (BPM, loop, dispositivos) | engine | **Unificar na Fase 3** (SPEC D3) |

**Regras de ouro mantidas:** `core/` sem JUCE; **nada** de alocação/lock bloqueante no áudio; mudanças de grafo só no thread de mensagem; **sem acentos** em nomes de arquivo/teste (strings de UI via `theme::ui`).

---

## 9. QA manual adicional (rodar no Windows com plugins reais)

- [ ] Janela abre inteira em 1366×768 e em 1920×1080 com 150%; plugin grande abre visível; tamanhos persistem.
- [ ] Minimizar plugin → recuperar pela caixa/menu; contagem e estado corretos.
- [ ] 1 clique em **qualquer ponto** do cabeçalho seleciona a trilha; duplo clique no nome renomeia; duplo clique fora abre inspetor.
- [ ] A–L tocam o instrumento da trilha selecionada; Z/X oitava; C/V velocity; funciona com UI do plugin aberta (ou fallback).
- [ ] BPM por arraste; metrônomo com acento; locators; seleção de região → "Inserir clip MIDI vazio" e **Ctrl+L**.
- [ ] Arrastar plugin da lista para a trilha (cursor correto); reordenar na cadeia.
- [ ] **Groove:** aplicar 5 grooves diferentes a um hi-hat 1/16 e **comparar de ouvido com o Live**; mexer Timing/Random/Velocity/Global ao vivo; Commit; Ctrl+U deixa **exatamente na grade**.
- [ ] **Automação:** rampa de volume; degrau de "Device On"; parâmetro de plugin; loop/seek corretos.
- [ ] **Áudio:** navegar em `D:\music-production\Samples`, pré-escuta com volume, arrastar para trilha de áudio, X ao soltar em trilha MIDI, forma de onda, salvar/abrir.
- [ ] Salvar/abrir reproduz tudo (incluindo groove, locators, automação, áudio, layout).

---

## 10. Riscos e spikes

| Risco | Mitigação |
|---|---|
| Teclas não chegam com janela de plugin focada (F0.7) | Spike F0.7b (DEC-7) + fallback de editor embutido |
| `setValue` de parâmetro no thread de áudio (A4) | Spike com 3 plugins; fallback por fila a 100–200 Hz |
| Streaming de áudio em tempo real (U2) | Buffer em thread de fundo; testes com arquivos longos; medir *underruns* |
| Groove "soar como o do Live" | Afinar **de ouvido com o dono** (G2); manter parâmetros expostos; documentar valores |
| Operações de arquivo destrutivas (U3) | Lixeira + confirmação; nunca deletar definitivo |
| Escopo enorme | Respeitar a ordem da seção 5; entregar **incrementos compiláveis** |
| Plugins instáveis derrubam o app | SPEC M9 (isolamento) — **não** nesta fase |
| Nunca testado áudio com plugins reais no ambiente de dev | QA do dono (seção 9); registrar resultados |

---

## 11. Perguntas pendentes ao dono (não bloqueiam; seguir os padrões)
1. ~~Imagens de referência~~ — **recebidas** (seção 13). Falta apenas confirmar no Live: o que faz **"Set 1.1.1 Here"** e como alternar o **Pitch Lock** do modo desenho.
2. **Verde `#55C982`** substitui o verde-água em tudo que é "ligado"? (padrão: sim)
3. **DEC-1:** áudio solto numa trilha MIDI → **bloquear** (padrão) ou converter?
4. Atalhos que ele usa no Live para: **adicionar locator**, **mostrar/ocultar a vista de detalhe**, **borracha** — para conferir no Live dele.
5. "Arrastar partes do DAW" inclui **janelas destacáveis/flutuantes**? (padrão: não)
6. Lista dos **grooves que ele mais usa** (hi-hat 16th, shaker, hip-hop…) para priorizar a afinação em G2.

---

## 12. Checklist-mestre (marcar ao concluir)

**Fase 0:** - [ ] F0.1 · - [ ] F0.2 · - [ ] F0.3 · - [ ] F0.5 · - [ ] F0.6 · - [ ] F0.7 (+F0.7b) · - [ ] F0.8
**Fase 1:** - [ ] F1.1 · - [ ] F1.2 · - [ ] F1.3 · - [ ] F1.4 · - [ ] F1.5 · - [ ] F1.6 · - [ ] F1.7 · - [ ] F1.8 · - [ ] F1.9
**Fase 3:** - [ ] T4.1 · - [ ] T4.2 · - [ ] T4.3 · - [ ] T4.4 (SPEC §14)
**Fase 2:** - [ ] G1 · - [ ] G2 · - [ ] G3 · - [ ] G4 · - [ ] G5 · - [ ] G6
**Fase 4:** - [ ] A1 · - [ ] A2 · - [ ] A3 · - [ ] A4 · - [ ] A5
**Fase 5:** - [ ] U1 · - [ ] U2 · - [ ] U3 · - [ ] U4 · - [ ] U5 · - [ ] U6
**Trilha V:** - [ ] V1 · - [ ] V2 · - [ ] V3 · - [ ] V4 · - [ ] V5 · - [ ] V6
**Fase 6:** SPEC §14 Marcos 5–11 (descontando o antecipado)

*Fim do plano. Última revisão: baseada no código do Marco 3 e no feedback do dono (Marco 3).*

---

## 13. Referências visuais do Live (recebidas do dono) — descrição em palavras

> **Regra:** as capturas de tela do Live **não entram no repositório** (direitos autorais, SPEC §16). Guardar fora do Git; este documento as descreve para quem não as viu.

1. **Menu de contexto do piano roll** — conteúdo completo em **F1.8** (9 grupos: quantizar/ativar/extrair groove; cortar clip; "Set 1.1.1 Here"; loop da seleção e zoom Z/X; grade adaptativa (5 níveis); grade fixa (8 compassos … 1/32, Off); tripleto; modo desenho com Pitch Lock; destacar escalas).
2. **Navegador (visão "Audio Effects")** — Collections / Categories / Places, busca no topo (Ctrl+F), lista com pastas expansíveis (ex.: Delay & Loop, Drive & Color, Dynamics, EQ & Filters, Modulators, Pitch & Modulation, Reverb & Resonance, Utilities) — estrutura em **F1.1**. *(Os efeitos nativos do Live não existem no Project Freedom; só a estrutura interessa.)*
3. **Navegador (categoria "Plug-Ins")** — nós VST e VST3; VST3 expandido = **pastas por fornecedor** em ordem alfabética (inclui "iZotope" e "iZotope, Inc." separados) — origem da **DEC-4 revisada** e da **F0.8**.

**Itens de interface vistos nas imagens que ainda não têm tarefa própria (P2):** botão de **pré-escuta** no rodapé do navegador; coluna "Name" **ordenável**; **Collections** coloridas (favoritos por cor); controle **Tap** e campo de **compasso (4/4)** ao lado do BPM; caixa de **quantização global** (ex.: "1 Bar") na barra de controle; botão de **Key/MIDI** e indicador de **CPU** no canto superior direito.
