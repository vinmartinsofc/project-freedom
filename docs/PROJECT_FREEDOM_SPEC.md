# Project Freedom — Especificação, Estado Atual e Plano

> **Documento mestre para Spec-Driven Development (SDD).**
> Pensado para ser entregue a *qualquer* modelo de IA (ou pessoa) que vá continuar o projeto **sem ter visto a conversa original**.
> Tudo que importa está aqui: objetivo, restrições, arquitetura, o que já existe, o que falta, e tarefas com critérios de aceite.
> Salve como `docs/SPEC.md` na raiz do repositório e mantenha-o atualizado (ver §0.3).

- **Versão do documento:** 1.3 (corresponde ao código do *Marco 3*; inclui a trilha visual V)
- **⚠️ Ordem de trabalho vigente:** `PLANO_DE_ACAO_FASE_4.md` (feedback do Marco 3: correções críticas, layout, groove, automação, áudio). **Em caso de conflito com o §14 deste documento, vale o plano.**
- **Código-fonte:** ~4.800 linhas de C++20 (core + app + testes)
- **Idioma da conversa com o dono do projeto:** Português (Brasil)

---

## 0. Como usar este documento

### 0.1 Fluxo SDD (obrigatório para cada tarefa)

1. **Ler** a tarefa em §14 (objetivo, especificação, critérios de aceite).
2. **Ler** as seções de arquitetura (§5–§7) e as regras de tempo real (§10) relevantes.
3. **Escrever os testes primeiro** quando a lógica couber no `core/` (ver §11).
4. **Implementar** com o menor diff possível; não refatorar o que não faz parte da tarefa.
5. **Compilar e rodar os testes** (§3). Nunca declarar "pronto" sem compilar.
6. **Atualizar este documento:** marcar a tarefa como concluída em §14, ajustar §4 (matriz de estado) e §13 (dívida técnica) se mudou.
7. **Entregar** o diff (ou um `.zip`), explicando em linguagem simples o que mudou e **o que não foi testado**.

### 0.2 Regras de ouro para quem for implementar

- **Honestidade sobre testes:** diga explicitamente o que foi verificado (compilou? testes passaram? rodou a UI?) e o que **não** foi (som real, plugins reais).
- **Não invente APIs do JUCE.** A versão fixada é **JUCE 8.0.15**. Confira a assinatura no código-fonte em `build/_deps/juce-src/modules/` antes de usar.
- **O thread de áudio é sagrado** (§10): nada de alocação, locks bloqueantes, I/O nem `std::function` pesado dentro de `processBlock`.
- **Lógica musical pura vai no `core/`** (sem JUCE, testável). A UI e o áudio ficam em `app/`.
- **Sem acentos** em nomes de arquivos, pastas e **nomes de testes Catch2** (quebra o `ctest` no Windows). Acentos só dentro de strings de UI, sempre via `theme::ui("...")` ou `juce::String::fromUTF8` (§12).

### 0.3 Manutenção deste arquivo

Ao concluir uma tarefa: troque `- [ ]` por `- [x]` em §14, atualize a matriz de §4 e adicione uma linha ao **Changelog** (§17). Se descobrir um bug/pegadinha nova, registre em §12 ou §13.

### 0.4 Prompt de abertura para colar em outro modelo

```text
Você vai continuar o desenvolvimento do "Project Freedom", um DAW open source em C++20 + JUCE 8 + CMake,
inspirado no fluxo do Ableton Live 10/11 (somente Arrangement View). O documento docs/SPEC.md é a fonte
da verdade: leia-o inteiro antes de qualquer coisa. Siga o fluxo SDD da seção 0.1.
Trabalhe UMA tarefa da seção 14 por vez (comece pela primeira não concluída, a menos que eu indique outra).
Responda em português (Brasil), em linguagem simples: o dono está aprendendo C++ (vem de Java/OOP).
Dê comandos exatos de terminal (Windows PowerShell). Diga sempre o que foi e o que NÃO foi testado.
Antes de codar, me mostre um plano curto (arquivos que vai tocar + testes que vai escrever).
```

---

## 1. Visão, objetivos e restrições

### 1.1 O que é

**Project Freedom** é um DAW (Digital Audio Workstation) minimalista, gratuito e open source, focado em **produção MIDI com instrumentos/efeitos VST3 de terceiros**. É um **"Live-like"**: reproduz o *fluxo de trabalho*, os *atalhos* e o *comportamento* do Ableton Live 10/11, mas com **identidade visual e código próprios**.

### 1.2 Por que existe

O dono do projeto produz música há 13+ anos no Ableton Live e estuda programação. Quer **um único ambiente** para produzir e desenvolver, com um DAW **que ele controle** (código aberto, sem dependência de software proprietário). O projeto também serve como **peça de portfólio** (projeto real, usado no dia a dia, com testes e arquitetura limpa — não um protótipo descartável).

### 1.3 Escopo — o que o dono realmente usa (e portanto é prioridade)

- **Somente Arrangement View** (nunca usa Session View).
- **Piano roll** com: inserção/edição de notas, **atalhos idênticos ao Live**, navegação fácil.
- **Técnica de escala:** escolher a escala/campo harmônico, **Fold** para mostrar só as linhas da escala, destaque das notas da escala. (Foi elogiada como "espetacular": **preservar**.)
- **Hospedar plugins VST3 de terceiros** (instrumentos e efeitos). **Não usa** sons, bibliotecas, instrumentos ou efeitos nativos do Live.
- Várias trilhas, vários plugins por trilha, mixer básico.

### 1.4 Não-objetivos (por enquanto)

Session View, instrumentos/efeitos nativos, Max for Live, warping/time-stretch de áudio, gravação de áudio, automação (até o §14 chegar lá), VST2, AU, CLAP, suporte a macOS (o código é portável, mas **não é testado** lá).

### 1.5 Restrições

| Tema | Regra |
|---|---|
| Linguagem | C++20, CMake ≥ 3.22 |
| Framework | JUCE **8.0.15** (fixado em `CMakeLists.txt`) + Catch2 **v3.16.1** |
| Plataforma-alvo | **Windows 10/11 (MSVC, Visual Studio 2026)** primeiro; Linux compila e roda (usado para verificação automatizada) |
| Custo | O projeto deve ser compilável **só com ferramentas gratuitas** |
| Legal | Ver §16. Reproduzir *comportamento*, nunca *assets/código/texto/nome* do Live |
| UI | Tema próprio, escuro, **cinza grafite (não preto puro)**. Hoje `app/Theme.h` usa uma paleta provisória (acento verde-água); a paleta oficial está em §19 e deve substituí-la (tarefa V2) |

---

## 2. Contexto do dono do projeto (para adequar a comunicação)

- Apelido: **rjc**. Fala português (Brasil); entende inglês técnico.
- Vem de **Java / POO**; **não conhece C++** — explique conceitos novos quando aparecerem e evite jargão sem definição.
- Trabalha no **Windows** (PowerShell). Já tem Visual Studio 2026 Community com a carga "Desenvolvimento para desktop com C++".
- Prefere receber **o projeto completo como `.zip`** para sobrescrever a pasta, com instruções exatas de build.
- Dá feedback em listas curtas após testar; valorize **fidelidade ao Live** e **usabilidade**.
- Cuida **pessoalmente da identidade visual** (logo F/R/E/E, paleta grafite, biblioteca de ícones SVG): tratar o design dele como **especificação**, não como sugestão.
- Tem acesso ao **Ableton Live** instalado, então pode **conferir comportamentos duvidosos** lá — quando houver ambiguidade, peça para ele testar e relatar.
- Possui o **manual de referência do Live em PDF** (na verdade é o **Live 12**, mesmo com nome de arquivo "live11"). Use-o para validar atalhos/comportamentos, mas lembre: o alvo é **Live 10/11**; algumas coisas do 12 podem não existir no 11.
- Tem uso limitado de modelos de IA (limites de uso), por isso este documento existe: **cada sessão deve render o máximo possível** — seja objetivo, entregue incrementos compiláveis.

---

## 3. Ambiente, build e verificação

### 3.1 Windows (ambiente do dono)

Abrir **Developer PowerShell for VS** (menu Iniciar). O CMake do MSYS2/UCRT64 **não serve**.

```powershell
cd C:\FILES\project-freedom\rolldaw          # raiz do repositório (a pasta com CMakeLists.txt)
Remove-Item -Recurse -Force build            # só quando a estrutura de CMake/grafo mudou
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
.\build\tests\Release\roll_tests.exe          # testes do core
ctest --test-dir build -C Release             # (opcional)
.\build\app\ProjectFreedom_artefacts\Release\"Project Freedom.exe"
```

- Primeiro build baixa JUCE e Catch2 (alguns minutos).
- Se o gerador não for reconhecido: `cmake --help` e usar o nome exato listado em "Visual Studio ...".
- A pasta do projeto no disco ainda se chama `rolldaw` (nome antigo). O produto é **Project Freedom**.

### 3.2 Linux (usado para verificação automatizada)

```bash
sudo apt install build-essential cmake ninja-build pkg-config libasound2-dev libfreetype-dev \
  libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2          # JUCE usa MUITA RAM: -j8 mata o compilador (OOM). Use -j2.
./build/tests/roll_tests
```

Opções úteis do CMake: `-DROLL_BUILD_APP=OFF` (só core + testes, build em segundos), `-DROLL_BUILD_TESTS=OFF`.

### 3.3 Verificação visual headless (sem monitor)

Útil para um agente de IA validar a UI:

```bash
apt install xvfb imagemagick xdotool
Xvfb :77 -screen 0 1500x900x24 &            # em processo separado (use setsid)
DISPLAY=:77 "build/app/ProjectFreedom_artefacts/Release/Project Freedom" &
DISPLAY=:77 import -window root shot.png    # captura
DISPLAY=:77 xdotool mousemove X Y click --repeat 2 --delay 90 1   # duplo clique
```

**Pegadinhas dessa técnica:**
- Se a captura for **reduzida** ao ser exibida (ex.: 1500x900 → 1400x840), as coordenadas lidas na imagem precisam ser **multiplicadas pelo fator de escala** antes de ir ao `xdotool`. (Já causou falsos "bugs" de clique.)
- Sem gerenciador de janelas não há barra de título; sem placa de som aparece `ALSA lib ... seq_hw` (inofensivo).
- Processo de build em segundo plano com `nohup ... &` pode morrer junto com o shell; use `setsid`.
- **Nunca** use `pkill -f "Project Freedom"` dentro do mesmo comando de shell (o padrão casa com a própria linha de comando); use `pkill -x`.
- **O ambiente de verificação não tem placa de som nem plugins VST3**: áudio e hospedagem real **não podem ser testados** lá.

---

## 4. Estado atual (matriz de funcionalidades)

Legenda: ✅ implementado e verificado (compila + comportamento visto) · 🟡 implementado mas **não verificado com áudio/plugins reais** · ⬜ não implementado

### 4.1 Núcleo (`core/`, testado automaticamente)

| Funcionalidade | Estado |
|---|---|
| Modelo `Note` / `Clip` com ids estáveis | ✅ |
| Regra de sobreposição de notas do Live (`Clip::resolveOverlaps`) | ✅ |
| Escalas (9 modos) e `Scale::contains` | ✅ |
| Desfazer/refazer por snapshot **das notas de um clip** (`History`) | ✅ |
| Relógio de transporte com loop (`TransportClock`) | ✅ |
| Gerador de eventos MIDI por trilha com note-off correto (`Sequencer`) | ✅ |
| Arranjo: `Track`, `Placement`, `flatten()`, anti-sobreposição de clips | ✅ |
| **Assume compasso 4/4 fixo** (1 compasso = 4 beats) | ⚠️ limitação |

### 4.2 Aplicação (`app/`)

| Área | Funcionalidade | Estado |
|---|---|---|
| Plugins | Scan automático VST3 em segundo plano + lista persistida em XML | ✅ (lista) / 🟡 (scan em Windows real) |
| Plugins | Navegador com busca e filtros Todos/Instrumentos/Efeitos; duplo clique carrega | ✅ UI / 🟡 carregamento |
| Plugins | Cadeia por trilha (1 instrumento + N efeitos), UI do plugin em janela (minimizar+fechar) | 🟡 |
| Áudio | Grafo (`AudioProcessorGraph`): TrackSource → dispositivos → TrackMixer → saída | 🟡 |
| Áudio | Mixer por trilha: volume, pan (sem UI), mute, solo, medidor de pico | 🟡 |
| Transporte | Play/Pause (Espaço), Stop, Voltar ao início (Home), Loop, BPM, posição | ✅ UI / 🟡 áudio |
| Arranjo | Várias trilhas, clips (criar/mover/redimensionar/copiar/duplicar/apagar), régua, brace de loop | ✅ |
| Piano roll | Notas, seleção, mover/redimensionar, rubber band, copiar/colar, duplicar, quantizar | ✅ |
| Piano roll | Escala: Scale mode, Destacar (K), Fold, Fold escala, tônica enfatizada | ✅ |
| Piano roll | Régua, teclas (C3 = nota 60) que **acendem** ao clicar e durante playback, velocity lane | ✅ |
| Piano roll | Zoom H/V (botões, Ctrl+roda, Alt+roda, arrastar régua, +/−), grade 1/64–1 compasso + tripleto | ✅ |
| Piano roll | Marcador de inserção (clique no fundo), Preview on/off | ✅ |
| Projeto | **Salvar/abrir** | ⬜ (próxima tarefa, §14 M4) |
| Projeto | **Desfazer global** (arranjo/mixer/dispositivos) | ⬜ |
| Entrada | MIDI de teclado/controlador, teclado do computador, gravação | ⬜ |
| Exportação | MIDI / áudio (render) | ⬜ |
| Config. | Seleção de dispositivo de áudio, buffer, taxa de amostragem | ⬜ (usa o padrão do sistema) |
| Acessibilidade | Anel de foco, F6 alterna painéis, setas selecionam trilha | 🟡 parcial |

---

## 5. Arquitetura

### 5.1 Camadas

```
┌───────────────────────────── app/ (JUCE) ─────────────────────────────┐
│ UI (message thread)                                                     │
│  MainComponent ── TransportBar ── BrowserPanel ── ArrangementView       │
│       │                │               │                │               │
│       │                │               │          PianoRollEditor       │
│  ProjectModel ◄──── RollModel ─────────┘          (NoteGrid, KeyColumn,  │
│  (tracks, clips,     (estado do piano roll:        RulerBar, Velocity)  │
│   seleção, loop)      clip atual, grade, escala)   DeviceView           │
│       │                                                                 │
│  AudioEngine ── SequencerGraph (AudioProcessorGraph + TransportClock)   │
│       │            └─ por trilha: TrackSource → [plugins] → TrackMixer  │
│  PluginScanner (thread de scan)                                         │
└───────────────┬─────────────────────────────────────────────────────────┘
                │ usa (sem dependência de JUCE)
┌───────────────▼──────────────────── core/ ──────────────────────────────┐
│ Note, Clip, Scale, History, Sequencer (+TransportClock), Arrangement    │
└─────────────────────────────────────────────────────────────────────────┘
```

**Regra de dependência:** `core/` não inclui nada do JUCE. `app/` depende de `core/`. Testes dependem só de `core/`.

### 5.2 Threads

| Thread | O que roda | Regras |
|---|---|---|
| **Message thread** (UI) | Toda a UI, `ProjectModel`, `RollModel`, `AudioEngine` (API pública), criação/remoção de nós do grafo | Pode alocar e bloquear brevemente |
| **Thread de áudio** | `SequencerGraph::processBlock` → `TrackSource::processBlock` → plugins → `TrackMixer::processBlock` | **Tempo real** (§10) |
| **Thread de scan** | `PluginScanner::run` | Comunica com a UI via `MessageManager::callAsync` + flag `alive` |

### 5.3 Fluxo de dados das notas (UI → áudio)

1. Usuário edita notas no piano roll → `RollModel` altera o `roll::Clip` (que pertence a um `Placement` no `ProjectModel`).
2. `RollModel::notify()` → `onNotesChanged` → `MainComponent` chama `project.notifyTrackNotes(trackId)`.
3. `ProjectModel::onTrackNotes` → `roll::flatten(track)` (notas em **tempo absoluto**, cortadas ao fim do clip) → `AudioEngine::publishNotes(trackId, notes)`.
4. `TrackSource::publishNotes` copia para `pendingNotes` sob `SpinLock` e incrementa `notesVersion`.
5. No áudio, `TrackSource::processBlock` faz **try-lock**; se obtiver e a versão mudou, copia para `localNotes` (capacidade pré-reservada, **sem alocar**). Se não obtiver o lock, usa a cópia antiga (nunca bloqueia).

### 5.4 Fluxo do transporte

- `SequencerGraph::processBlock` (1× por bloco): lê flags atômicas (`playing`, `rewind`, `seek`, `bpm`, `loop`, `loopStart/End`), usa `TransportClock::plan()` para produzir um `BlockPlan` (até 8 `Segment`s — o loop pode dividir o bloco) e guarda em `BlockShared`.
- Cada `TrackSource` lê `BlockShared` (mesmo thread) e chama `Sequencer::process(notes, plan, events)`.
- `flush` (pausa/stop/seek): todos os `Sequencer`s soltam as notas ativas (note-off) no início do bloco.
- A posição atual sai por `positionAtomic` (UI lê a 30 Hz).

### 5.5 Grafo de áudio

```
TrackSource(MIDI) ──midi──► [instrumento] ──audio──► [efeito 1] ──► ... ──► TrackMixer ──► Output(0,1)
```
- Cada trilha: 1 `TrackSource`, 0..1 instrumento (sempre na posição 0), 0..N efeitos, 1 `TrackMixer`. Saídas das trilhas **somam** na saída (o grafo soma conexões no mesmo canal).
- `AudioEngine::rewire(track)` remove e refaz **todas** as conexões da trilha sempre que a cadeia muda.
- Assume **estéreo** (2 canais) em toda a cadeia. Plugin com 1 saída é duplicado nos dois canais.
- `TrackSource` produz MIDI (`producesMidi() = true`, sem canais de áudio). Ele também mistura o MIDI de **pré-escuta** (`MidiMessageCollector`) vindo do piano roll.

---

## 6. Especificação por módulo

### 6.1 `core/` (puro, testado)

| Arquivo | Responsabilidade | Pontos de atenção |
|---|---|---|
| `Note.h` | `struct Note {pitch, startBeats, lengthBeats, velocity, muted, id}` | Tempo em **beats** (semínimas). Ordem dos campos permite `{60, 1.0, 2.0, 100}` |
| `Clip.h/.cpp` | Lista de notas + `length()` em beats; `addNote` atribui id; `find`, `noteAt`, `removeNote`; `setNotes`; `resolveOverlaps(ids)`; `snapToGrid` | Ids nunca reaproveitados (`nextId`) |
| `Scale.h/.cpp` | `Scale(root 0..11, Mode)`, `contains(midiNote)`, 9 modos | Raiz é normalizada |
| `History.h/.cpp` | Pilhas de snapshots `vector<Note>` (máx. 200) | Hoje só cobre **notas de um clip** (ver M5) |
| `Sequencer.h/.cpp` | `TransportClock` (1 por projeto) e `Sequencer` (1 por trilha), `BlockPlan`, `Segment`, `MidiEvent`, `activeMask()` | **Sem alocação** no caminho normal (`active` reservado 1024; `events` reservado 4096 no app) |
| `Arrangement.h/.cpp` | `Placement`, `Track` (clips em `unique_ptr`), `flatten(track)`, `isFree`, `take/put` | `put()` atribui **novo id de clip** na trilha destino |

**Invariantes importantes**
- Clips da mesma trilha **não se sobrepõem** (encostar é permitido).
- `flatten` descarta notas com `start >= clip.length` e corta `length` ao fim do clip.
- Nota com `lengthBeats <= 1e-6` ou `muted` **não toca**.
- Em empate de offset, **note-off vem antes de note-on** (evita nota presa em notas repetidas).
- O loop corta notas que ultrapassam `loopEnd` (note-off no ponto do wrap) e dispara de novo na volta.

### 6.2 `app/` — Motor de áudio (`AudioEngine.h/.cpp`)

- `TrackSource`, `TrackMixer`, `SequencerGraph`, `AudioEngine` (ver §5).
- `AudioEngine` é dono do `AudioDeviceManager` (`initialiseWithDefaultDevices(0, 2)`), `AudioPluginFormatManager` (**somente VST3**), `AudioProcessorGraph`, `AudioProcessorPlayer`.
- API pública (message thread): `addTrack/removeTrack`, `publishNotes`, `setMixer`, `previewNote`, `activeMask`, `peak`, `addDevice(trackId, desc, cb(error, deviceId))`, `removeDevice`, `devices(trackId)`, `createDeviceEditor`.
- Regras de cadeia: instrumento novo **substitui** o anterior; efeito vai **ao fim**.
- Peak do mixer: máximo do bloco com decaimento suave (`*0.92`).
- `TrackMixer`: ganho com rampa linear dentro do bloco; pan linear simples (não *constant-power*).

### 6.3 `app/` — Projeto (`ProjectModel.h/.cpp`)

- Dono de `std::vector<std::unique_ptr<roll::Track>>`, seleção (`selectedTrackId`, `selectedClipId`), região de loop, `totalBeats` (cresce com os clips).
- Callbacks: `onTrackAdded/Removed/Notes`, `onMixerChanged`, `onSelectionChanged`, `onLoopChanged`.
- Garantias: sempre resta ao menos **1 trilha**; ao apagar clip/trilha selecionados, `onSelectionChanged` é disparado **antes** da destruição (para o piano roll soltar o ponteiro).
- `moveClip` entre trilhas **mantém o mesmo objeto `Placement`** (o ponteiro do `Clip` continua válido) mas muda o id do clip.

### 6.4 `app/` — Piano roll (`RollModel`, `PianoRollEditor`)

- `RollModel` guarda: ponteiro para o `Clip` atual (`clip()`; há um `dummyClip` quando nada está selecionado — `hasClip()` é falso), seleção (`std::set<int>` de ids), grade (`baseGrid`, `triplet`, `gridBeats` efetivo), `snap`, `drawMode`, `previewOn`, escala (`scaleOn`, `highlight`, `fold`, `foldScale`), zoom (`pixelsPerBeat`, `rowHeight`), `insertBeat`, clipboard, teclas acesas (`pressedPitch`, `activeLo/Hi`).
- Todas as edições passam pelo modelo: `beginEdit()` (snapshot) → mutação → `notify(rebuildRows)` (publica no engine + `sendChangeMessage`).
- `PianoRollEditor` compõe: `NoteGrid` (dentro de `SyncViewport`), `KeyColumn`, `RulerBar`, `VelocityLane`, com "holders" para sincronizar rolagem (padrão: componente-filho dentro de um `Component` pai que recorta).
- Linhas visíveis (`rows()`): todas (0–127), ou só as com notas (**Fold**), ou escala + notas (**Fold escala**, exige Scale mode).
- Posição do playhead no roll é **local ao clip**: `global - clipStartBeats`.

### 6.5 `app/` — Arranjo (`ArrangementView`)

- Classes internas: `Lanes` (clips + mouse), `Ruler` (régua + brace de loop + seek), `Header` (nome, M/S, volume, medidor, menu de contexto).
- `ppb` (pixels por beat) padrão 22; rolagem sincronizada por *polling* a 30 Hz (ver dívida técnica §13).
- Criar clip: duplo clique em trilha vazia → **1 compasso (4 beats)**, alinhado ao compasso.

### 6.6 `app/` — Demais

- `PluginScanner`: thread própria; `PluginDirectoryScanner` com `getDefaultLocationsToSearch()` do VST3; arquivo *dead-man's pedal* `scan-crash-guard.txt` bloqueia automaticamente plugins que travam o scan; lista salva em `%APPDATA%\ProjectFreedom\plugins.xml` (`userApplicationDataDirectory/ProjectFreedom`).
- `BrowserPanel`: `ListBoxModel`; filtros; marca roxa nos efeitos; Enter/duplo clique carrega.
- `DeviceView`: caixas de dispositivos com Abrir/remover.
- `TransportBar`: `IconButton` desenhado à mão (sem fontes/emoji); posição em "compasso . beat . 16avo".
- `MainComponent`: monta tudo; atalhos globais; divisor arrastável entre arranjo e detalhe; abas Clip/Dispositivos; mapeia posição global ↔ local do clip.

---

## 7. Modelo de dados e invariantes (resumo)

```
Project
 ├─ bpm (hoje: atômico no SequencerGraph; NÃO está no ProjectModel)
 ├─ loop {enabled (no TransportBar/graph), start, end}   // em beats
 └─ Track[]  {id, name, volume(0..1.5), pan(-1..1), mute, solo}
      ├─ Devices[] (no AudioEngine, não no ProjectModel!)  // instrumento primeiro
      └─ Placement[] {id, start, name, Clip}
            └─ Clip {length, Note[] {id,pitch,start,length,velocity,muted}}
```

> **Atenção para a tarefa de salvar/abrir (M4):** hoje parte do estado vive **fora** do `ProjectModel` (BPM e "loop ligado" no `SequencerGraph`/`TransportBar`; dispositivos no `AudioEngine`). M4 deve **unificar** isso em um modelo serializável.

- Tempo: **sempre em beats** (double). BPM só converte beats↔amostras no relógio.
- Pitch: 0–127, **C3 = 60** (convenção do Live; rótulo = `pitch/12 - 2`).
- Velocity: 1–127.
- Ids: únicos por clip (notas), por trilha (clips), por projeto (trilhas); ids de dispositivo são do engine.

---

## 8. Comportamento do piano roll e do arranjo (implementado)

### 8.1 Atalhos globais
| Atalho | Ação |
|---|---|
| Espaço | Play/Pause (botões **não** roubam o foco ao clicar, para o Espaço continuar funcionando) |
| Home | Volta ao início do projeto (fora do piano roll) |
| Ctrl+Shift+T | Nova trilha MIDI |
| Shift+Tab ou F12 | Alterna Clip ↔ Dispositivos |
| F6 | Foco: lista de plugins → arranjo → piano roll |

### 8.2 Arranjo
| Ação | Como |
|---|---|
| Criar clip | Duplo clique numa trilha vazia |
| Mover / mudar de trilha | Arrastar (snap 1 beat; **Alt** = sem snap) |
| Redimensionar | Arrastar a borda direita (mín. 1 beat) |
| Copiar | Ctrl + arrastar |
| Duplicar / apagar | Ctrl+D / Delete ou Backspace |
| Trilha acima/abaixo | ↑ / ↓ |
| Zoom | Ctrl+roda, `+` / `-` |
| Loop | Arrastar bordas/corpo do brace (faixa fina no topo da régua) |
| Seek | Clique na régua |
| Cabeçalho | Duplo clique no nome = renomear; M, S, slider de volume (duplo clique = 0.8); clique direito = nova/apagar trilha |

### 8.3 Piano roll
| Atalho | Ação |
|---|---|
| Duplo clique (vazio / nota) | Criar / apagar nota (comprimento = grade) |
| Clique no fundo | Define o **marcador de inserção** (parado: o play começa ali) |
| Arrastar no vazio | Seleção retangular (Shift soma) |
| Arrastar nota | Move (tempo e altura); **Alt** = sem snap; **Ctrl** = copia |
| Arrastar borda direita | Redimensiona (todas as selecionadas) |
| Ctrl+A / C / X / V | Selecionar tudo / copiar / recortar / colar no marcador |
| Ctrl+D | Duplicar seleção (estende o clip em compassos inteiros se passar do fim) |
| Delete / Backspace | Apagar seleção |
| ↑ ↓ | Transpor 1 semitom (**Shift** = oitava) |
| ← → | Mover pela grade (**Shift** = alongar/encurtar) |
| `0` | Ativar/desativar notas selecionadas |
| Ctrl+U | Quantizar início das notas à grade |
| `B` | Modo desenho (cria nota ao clicar e arrastar estica) |
| `K` | Destacar escala |
| Ctrl+1 / Ctrl+2 / Ctrl+3 / Ctrl+4 | Grade mais fina / mais larga / tripleto / snap |
| Esc | Desselecionar |
| Home / End | Marcador de inserção no início / fim |
| `+` / `-` | Zoom horizontal |
| Page Up/Down (+Ctrl = horizontal) | Rolar |
| Ctrl+roda / Alt+roda | Zoom H / V |
| Arrastar régua | Vertical = zoom, horizontal = rolar; clique = seek |
| Ctrl+Z / Ctrl+Y (Ctrl+Shift+Z) | Desfazer / refazer (**apenas notas do clip atual**) |

**Regra de sobreposição (igual ao Live):** nota nova/movida sobre outra do **mesmo pitch** → a coberta pelo início **some**; a coberta pela cauda é **encurtada** até o início da nova.

**Escala (como no manual do Live):** *Scale* liga o modo escala (raiz+tipo); *Destacar* (K) pinta linhas da escala e a tônica; *Fold* mostra só linhas com notas; *Fold escala* mostra linhas da escala + linhas com notas.

---

## 9. Referência de fidelidade ao Live

- **Fonte oficial para validar comportamento:** o manual de referência do Live (PDF do dono; capítulo **"Editing MIDI Notes and Velocities"** e o apêndice **"Computer Keyboard Shortcuts"**).
  - ⚠️ É o manual do **Live 12**. O alvo é **10/11**. Se algo parecer exclusivo do 12, **confirme com o dono** (ele tem o Live).
  - ⚠️ **Não copie texto do manual** para o repositório (direitos autorais). Descreva comportamentos com suas próprias palavras.
- **Ambiguidades do manual que só o Live real resolve** (peça ao dono para testar e relatar):
  1. `Alt + ← →`: o texto diz "nudge sem snap"; a tabela de atalhos diz "selecionar nota anterior/seguinte na mesma linha".
  2. Atalhos com `Ctrl + setas` para navegar entre notas: texto e tabela divergem.
  3. Modo desenho: o manual descreve "pincel melódico" (várias notas acompanhando o mouse); o projeto hoje só **estica** a nota.
- **Itens do manual ainda não implementados:** ver §14 (M7/M8): grade adaptativa (Ctrl+5), diálogo de quantização, edição de velocity (Alt+arrastar, digitar valor), seleção por intervalo de tempo e comandos "…Time", seleção entre notas por teclado, botões da seção **Notes** do clip (÷2, ×2, Reverse, Invert, Legato, Duplicate Loop), **Follow** (Ctrl+Shift+F), *loop selection* (Ctrl+L), dividir/consolidar (Ctrl+E/Ctrl+J), renomear (Ctrl+R).

---

## 10. Regras de tempo real (checklist de revisão)

Dentro de qualquer função chamada pelo thread de áudio (`processBlock`, `Sequencer::process`, `TransportClock::plan`):

- [ ] **Sem `new`/`malloc`/`std::vector` crescendo** (use capacidade pré-reservada; ver `reserve` nos construtores).
- [ ] **Sem `std::mutex`/locks bloqueantes**; só `SpinLock::ScopedTryLock` com *fallback* (usar dado antigo).
- [ ] **Sem I/O, logs, `DBG` pesado, nem chamadas à UI**.
- [ ] Comunicação UI→áudio por **atômicos** (`std::atomic`) ou **cópia sob try-lock** (padrão `publishNotes`).
- [ ] Comunicação áudio→UI por **atômicos** lidos por *timer* (`positionAtomic`, `maskLo/Hi`, `peakLevel`).
- [ ] Offsets de eventos MIDI dentro de `[0, numSamples-1]`.
- [ ] Toda mudança de topologia do grafo (add/remove/connect) ocorre na **message thread**.
- [ ] Destruir plugin **somente depois** de fechar sua janela/editor.

---

## 11. Convenções de código e estratégia de testes

### 11.1 Código
- C++20, `#pragma once`, namespaces: `roll::` no core; sem namespace no app (classes `PascalCase`, métodos `camelCase`, membros sem prefixo).
- Warnings tratados com seriedade (`juce_recommended_warning_flags`): o build **atual não tem warnings** — mantenha assim.
- Comentários em português, claros, explicando o **porquê**.
- UI strings com acento: `theme::ui("...")`.
- Tempo em beats; nunca misturar com amostras fora de `Sequencer`/`TransportClock`.

### 11.2 Testes (Catch2 v3)
- Hoje: **20 casos / ~194 asserções** (`ScaleTests`, `ClipTests`, `SequencerTests`, `ArrangementTests`).
- **Regra:** toda lógica nova que possa viver no `core/` ganha teste **antes** da implementação.
- Nomes de teste: **sem acentos**. Descrições em português simples.
- Testes de `Sequencer` usam o helper `run()` (relógio + sequenciador em blocos de 512, devolvendo eventos com tempo absoluto em amostras; tolerância de ±2 amostras).
- **Não há testes de UI/áudio automatizados.** Para essas partes: checklist manual (§15) + verificação headless (§3.3).

---

## 12. Pegadinhas conhecidas (já custaram tempo)

1. **JUCE exige `C` em `LANGUAGES`** (`project(... LANGUAGES C CXX)`), senão o CMake falha.
2. **JUCE compila com muita RAM**: usar `-j2` em máquinas modestas.
3. **Nomes de teste com acento quebram o `ctest` no Windows** (a linha de comando muda a codificação). Rodar o `roll_tests.exe` direto sempre funciona.
4. **Strings com acento na UI** viravam "dÃª": usar `theme::ui()` / `juce::String::fromUTF8`.
5. **Gerador do CMake** depende da versão do Visual Studio (`"Visual Studio 18 2026"`); usar o **Developer PowerShell**, não o terminal MSYS2.
6. **Botões roubam o foco do teclado** e passam a consumir **Espaço**. Todos os botões da toolbar usam `setMouseClickGrabsKeyboardFocus(false)`.
7. **Não destruir um `DocumentWindow` dentro do próprio `closeButtonPressed`**: usar `MessageManager::callAsync` para apagar (feito em `MainComponent::openDeviceEditor`).
8. **Componentes-filho vazam para fora do pai se não houver recorte**: o padrão "holder" (Component pai que recorta + filho deslocado) é usado para régua/teclas/velocity sincronizados com o scroll.
9. **`setPlayConfigDetails(0, 2, ...)` do grafo deve vir antes de adicionar os nós de E/S**; o projeto assume **2 canais de saída**.
10. **JUCE 9.x existe** (tags `9.0.x`), mas o projeto está **fixado em 8.0.15**. Atualizar exige revalidar APIs (não foi testado).
11. **Em `AudioProcessor` derivados**, adicionar `using juce::AudioProcessor::processBlock;` evita o warning de overload escondido (`double`).
12. **Captura de tela reduzida** → coordenadas do `xdotool` erradas (ver §3.3).
13. **O parser de SVG do JUCE não entende `currentColor`** (verificado no código-fonte: `juce_SVGParser.cpp` não trata esse valor). SVGs que usam `currentColor` precisam ter o texto substituído por uma cor explícita (`#RRGGBB`) **antes** de `Drawable::createFromImageData`/`createFromSVG`. `stroke-width`, `stroke-linecap` e `stroke-linejoin` **são** suportados.

---

## 13. Dívida técnica e bugs conhecidos

| # | Item | Impacto | Sugestão |
|---|---|---|---|
| D1 | `ArrangementView::timerCallback` usa `static juce::Point<int> last` local para detectar rolagem (frágil) | Baixo | Trocar por `Viewport` derivado com `visibleAreaChanged` (como `SyncViewport`) |
| D2 | Escala (raiz/modo/Scale/Fold) é **global no `RollModel`**, não por clip (no Live é por clip) | Médio | Mover para `Clip` + salvar (M7) |
| D3 | BPM, loop ligado e dispositivos fora do `ProjectModel` | Alto p/ M4 | Unificar no modelo serializável |
| D4 | Desfazer cobre só notas do clip atual; histórico zera ao trocar de clip | Médio | M5 |
| D5 | Compasso 4/4 fixo em toda a UI e no relógio | Médio | M8 (assinatura de tempo) |
| D6 | Pan sem UI; lei de pan linear | Baixo | M8/M9 |
| D7 | `Placement::name` sem UI de renomear | Baixo | M8 |
| D8 | Plugins rodam **no mesmo processo** — plugin instável derruba o programa; scan também (só mitigado pelo guard file) | Alto (robustez) | M9 |
| D9 | Sem seleção de dispositivo/buffer de áudio; usa o padrão do sistema | Médio | M9 |
| D10 | Chain/áudio **nunca testados com plugins reais** (ambiente de dev sem som/VST) | **Alto** | Rodar o checklist §15 no Windows e corrigir |
| D11 | Notas pré-escuta: `MidiMessageCollector::reset(sampleRate)` só em `prepareToPlay` do `TrackSource` — verificar que o grafo chama `prepareToPlay` ao adicionar o nó com o dispositivo já aberto | Médio | Validar no checklist |
| D12 | Sem limitador/volume no master; soma de trilhas pode clipar | Médio | M9 |
| D13 | Sem *plugin delay compensation* | Médio | M9 |
| D14 | Plugin em trilha **sem instrumento** (só efeito) não recebe áudio de entrada | Baixo (esperado) | Documentar |
| D15 | Redimensionar clip menor corta notas na reprodução mas **mantém** as notas no clip (não apaga) | Baixo (igual ao Live) | Manter |
| D16 | `lengthBox` do clip só tem 1/2/4/8/16/32 compassos; tamanhos intermediários mostram vazio | Baixo | Campo numérico |
| D17 | Versão no `CMakeLists.txt` ainda `0.2.0`; nome da pasta do projeto `rolldaw` | Cosmético | Atualizar para `0.3.0`; renomear pasta quando conveniente |

---

## 14. Roadmap — tarefas especificadas

> Ordem recomendada: **M4 → M5 → M6 → M7 → M8 → M9 → M10 → M11**. Cada tarefa deve ser entregue como incremento **compilável e testado**.

### Trilha paralela V — Identidade visual e ícones (independente dos marcos de funcionalidade)

> Pode ser feita a qualquer momento (inclusive por outro modelo, em paralelo), pois quase não toca no `core/` nem no áudio.
> **Entrada do dono:** biblioteca de ícones SVG (24 ícones: transporte, edição, mixer), paleta de referência e logo F/R/E/E. Ver §19.

- [ ] **V1 — Carregador de ícones SVG (`app/Icons.h/.cpp`)**
  - Embutir os SVGs no executável com `juce_add_binary_data` (CMake), a partir de `assets/icons/**/*.svg`; **sem arquivos soltos** em tempo de execução.
  - API sugerida: `Icons::get(IconId, juce::Colour, int sizePx) -> std::unique_ptr<juce::Drawable>` com **cache** por (ícone, cor).
  - Como o JUCE **não suporta `currentColor`** (§12.13), substituir o texto `currentColor` pela cor do estado antes de parsear.
  - Ícone ausente/inválido → desenhar um "?" visível (nunca crashar) e logar em modo debug.
  - **Aceite:** todos os 24 ícones renderizam nítidos em 16, 18, 24 e 32 px (em tela com escala 100% e 150%); trocar a cor muda o ícone; teste de fumaça que carrega todos os ids.
- [ ] **V2 — Paleta oficial em `Theme.h`** (valores em §19.1)
  - Substituir as cores provisórias; manter **funções por papel** (`background()`, `panel()`, `control()`, `iconPrimary()`, `textSecondary()`, `active()`, `record()`, `warning()`...), nunca cores soltas pela UI.
  - Ajustar o que hoje usa o acento verde-água (botões ligados, notas, brace de loop, anel de foco) para **verde `#55C982`** (ativo) — e **vermelho** só para gravação, **amarelo** só para alerta.
  - **Aceite:** nenhum literal `0xff......` fora de `Theme.h` (exceto cores de trilha geradas por HSV); captura de tela comparada com a paleta.
- [ ] **V3 — Substituir os botões desenhados à mão** (`IconButton` do transporte: play/pause/stop/rewind) e os textos "M", "S", "x", "+ Trilha MIDI" etc. por ícones **mantendo tooltip e rótulo acessível** (não depender só de cor — regra do design).
  - Estados obrigatórios: normal, hover, pressed, disabled, **ativo/ligado** (cor + forma/fundo, nunca só cor). Valores em §19.2.
  - Área clicável **≥ 36 px** mesmo com ícone de 24 px (ou 28–32 px em controles de destaque).
- [ ] **V4 — Ícones que faltam** (pedir ao dono em nova rodada; lista em §19.3). Enquanto não existirem, manter o texto.
- [ ] **V5 — Logo e ícone do aplicativo**
  - O logo atual (`first_logo.jpg`) é **escuro sobre fundo claro texturizado**; a UI é escura → solicitar ao dono: (a) versão **vetorial/PNG transparente** do monograma, (b) **versão clara** (para fundo grafite), (c) ícone **`.ico` multi-tamanho** (16, 24, 32, 48, 64, 128, 256) para janela/instalador/barra de tarefas.
  - Aplicar: ícone da janela (`DocumentWindow::setIcon`), tela "Sobre", e (futuro) instalador.
- [ ] **V6 — Verificação de acessibilidade visual:** contraste mínimo 4,5:1 para texto e 3:1 para ícones/controles (ver tabela em §19.1) e navegação por teclado com anel de foco visível.

### Marco 4 — Persistência de projeto (PRIORIDADE MÁXIMA)

**Por quê:** hoje fechar o programa perde tudo. Sem isso, o DAW não é usável.

- [ ] **T4.1 — Unificar o estado no modelo** (resolve D3)
  - Mover `bpm`, `loopEnabled` e a lista de dispositivos por trilha (descrição do plugin + estado) para uma estrutura serializável acessível pelo `ProjectModel`.
  - O `AudioEngine` passa a **refletir** o modelo (e não ser a fonte da verdade).
  - **Aceite:** nenhuma regressão de UI; testes existentes passam.
- [ ] **T4.2 — Formato de arquivo `.pfp` (JSON, versionado)** — *no `core/`, testável*
  - Biblioteca: **nlohmann/json** via `FetchContent` (header-only) no `core/`.
  - Esquema (v1):
    ```json
    {
      "app": "ProjectFreedom", "formatVersion": 1,
      "bpm": 120.0, "loop": { "enabled": true, "start": 0.0, "end": 16.0 },
      "tracks": [{
        "id": 1, "name": "1 MIDI", "volume": 0.8, "pan": 0.0, "mute": false, "solo": false,
        "devices": [{ "format": "VST3", "name": "Nexus", "pluginXml": "<PLUGIN .../>", "state": "<base64>" }],
        "clips": [{ "id": 1, "start": 0.0, "length": 4.0, "name": "",
          "notes": [{ "id": 1, "pitch": 60, "start": 0.0, "length": 1.0, "velocity": 100, "muted": false }] }]
      }]
    }
    ```
  - Funções: `std::string serialize(const ProjectData&)` / `ProjectData parse(const std::string&)` com **erros descritivos** (campo ausente, versão futura).
  - **Testes:** *round-trip* (serializar → parsear → igual), arquivo corrompido, versão desconhecida, campos opcionais ausentes (usar padrões).
- [ ] **T4.3 — Salvar/abrir na UI**
  - Ctrl+S / Ctrl+Shift+S / Ctrl+O / Ctrl+N (padrão do Live/Windows); `FileChooser` assíncrono; extensão `.pfp`.
  - Título da janela: `Project Freedom - nome.pfp *` (asterisco = alterações não salvas); confirmação ao fechar/abrir/novo com alterações.
  - Estado do plugin: `AudioPluginInstance::getStateInformation` → `MemoryBlock::toBase64Encoding()`; restaurar com `setStateInformation` **após** o plugin carregar.
  - Plugin ausente ao abrir: manter um dispositivo "plugin ausente" **preservando o estado** e avisar; não perder dados.
  - **Aceite:** salvar, fechar, abrir → mesmas trilhas, clips, notas, loop, BPM, mixer e **sons dos plugins** (preset recuperado).
- [ ] **T4.4 — Cópia de segurança / autosave**
  - A cada N minutos (padrão 2) grava `nome.pfp.bak` ao lado (ou em `%APPDATA%\ProjectFreedom\autosave`); oferecer recuperar após crash.

### Marco 5 — Desfazer/refazer global

- [ ] **T5.1 — Histórico de projeto** (resolve D4)
  - Substituir `roll::History` por um histórico no nível de `ProjectModel` (snapshot profundo de tracks/clips/mixer/loop **ou** padrão Command; escolher e justificar no PR). Limite 100 passos; **agrupar** gestos contínuos (arrastar = 1 passo).
  - Requer `Track`/`Placement` clonáveis (hoje `unique_ptr`; criar `clone()`).
  - Restaurar seleção **por ids**; o `RollModel` deve reapontar o clip sem crashar.
  - **Aceite:** desfazer/refazer de: criar/mover/apagar/duplicar clip, editar notas, mudar volume/mute/solo, mover loop, adicionar/remover trilha. Não cobre estado de plugins.
  - **Testes (core):** sequência de operações + undo/redo restaura estado idêntico.

### Marco 6 — Entrada MIDI e gravação

- [ ] **T6.1 — Entrada MIDI de hardware:** listar dispositivos (`MidiInput::getAvailableDevices`), habilitar/desabilitar em diálogo simples; MIDI vai para a **trilha armada** (padrão: selecionada).
- [ ] **T6.2 — Teclado do computador como MIDI (tecla `M`, como no Live):** mapa A W S E D F T G Y H U J K…; Z/X = oitava; C/V = velocity. (Confirmar mapa exato no manual do Live; não copiar o texto.)
- [ ] **T6.3 — Armar trilha + botão Gravar:** ao gravar com play, cria clip na trilha armada a partir da posição atual; notas com *timestamp* convertido para beats pelo relógio; note-off fechados ao parar; undo do take inteiro como 1 passo. Respeita loop (gravação em loop = sobrescreve/mescla; decidir e documentar).
- [ ] **T6.4 — Metrônomo e pré-contagem:** clique gerado no áudio (sem arquivo externo); on/off; 1–2 compassos de contagem.
- [ ] **T6.5 — Tap tempo** e campo de BPM com arrastar.
- **Aceite:** tocar teclado MIDI → ouvir o instrumento; gravar 4 compassos → notas aparecem no clip, alinhadas (±1 amostra de erro do relógio, sem deslocamento sistemático).

### Marco 7 — Paridade do piano roll com o Live

- [ ] **T7.1 — Escala por clip** (D2): raiz/modo/Scale/Destacar/Fold salvos no `Clip` e no `.pfp`; trocar de clip troca a escala exibida.
- [ ] **T7.2 — Botões "Notes" do clip:** `÷2`, `×2` (esticar/encolher tempo da seleção ou do clip), `Reverse`, `Invert`, `Legato`, `Duplicate Loop`. Lógica **no `core/`** com testes.
- [ ] **T7.3 — Grade adaptativa ao zoom** (Ctrl+5 alterna fixa/adaptativa) e grade em tripleto consistente.
- [ ] **T7.4 — Diálogo de quantização** (Ctrl+Shift+U): grade, quantizar fim, intensidade (%), sem limite de oitava; Ctrl+U usa as últimas opções.
- [ ] **T7.5 — Velocity:** Alt+arrastar a nota para mudar velocity; digitar valor + Enter; rampas arrastando linha; Ctrl+↑/↓ ±10 (confirmar no Live).
- [ ] **T7.6 — Modo desenho melódico** (pincel que cria notas ao arrastar). **Confirmar comportamento no Live 10/11 com o dono antes de implementar.**
- [ ] **T7.7 — Seleção por teclado entre notas** e por intervalo de tempo. **Resolver a ambiguidade de `Alt+←→` (§9) com o dono antes.**
- [ ] **T7.8 — Auto-scroll "Follow"** (Ctrl+Shift+F) seguindo o playhead.
- **Aceite geral:** cada item com testes de `core/` quando aplicável e comparação manual com o Live feita pelo dono.

### Marco 8 — Paridade do arranjo

- [ ] **T8.1 — Renomear clip/trilha** (Ctrl+R) e cor de clip.
- [ ] **T8.2 — Dividir (Ctrl+E) e consolidar (Ctrl+J) clips;** *loop selection* (Ctrl+L).
- [ ] **T8.3 — Seleção por tempo** arrastando em área vazia; comandos "Duplicar tempo", etc.
- [ ] **T8.4 — Reordenar trilhas** (arrastar cabeçalho); altura de trilha; "zoom para caber".
- [ ] **T8.5 — Pan na UI** (knob/slider); lei de pan *constant-power*; medidor com *clip indicator*.
- [ ] **T8.6 — Assinatura de compasso** (D5): `numerador/denominador` no projeto; régua, grade e relógio parametrizados (testes de `core/`).
- [ ] **T8.7 — Marcadores/localizadores** (opcional).

### Marco 9 — Robustez do áudio e dos plugins

- [ ] **T9.1 — Configuração de áudio:** diálogo com `AudioDeviceSelectorComponent` (dispositivo, tamanho de buffer, taxa); persistir; reagir a mudanças de taxa (re-`prepare`).
- [ ] **T9.2 — Master:** trilha master com volume e **limitador** simples; medidor.
- [ ] **T9.3 — Dispositivos:** bypass, reordenar por arrastar, restaurar estado, janelas de plugin **redimensionáveis** quando o plugin permitir.
- [ ] **T9.4 — Compensação de atraso de plugins (PDC)** via `getLatencySamples()`.
- [ ] **T9.5 — Isolamento de crash:** scan **em processo separado** (executável auxiliar) e, se viável, hospedagem fora de processo. Documentar a decisão (custo/benefício).
- [ ] **T9.6 — Indicador de CPU** e proteção contra *denormals* (`juce::ScopedNoDenormals`).
- [ ] **T9.7 — Pastas extras de plugins** (além do padrão) e botão "Procurar de novo" completo.
- **Aceite:** plugin que trava no scan não impede o programa de abrir; mudar buffer/taxa sem reiniciar.

### Marco 10 — Exportação

- [ ] **T10.1 — Exportar MIDI** (clip/trilha/projeto) com `juce::MidiFile` (PPQ 960).
- [ ] **T10.2 — Renderizar áudio (offline):** processar o grafo em blocos sem tempo real, do início ao fim (+ cauda configurável), escrever WAV 24-bit (`WavAudioFormat`); barra de progresso; respeitar mute/solo.
- **Aceite:** o WAV exportado soa igual ao tempo real (comparar pico/RMS aproximado).

### Marco 11 — Distribuição e qualidade (portfólio)

- [ ] **T11.1 — CI no GitHub Actions:** `windows-latest`: configurar, compilar (Release) e rodar `roll_tests.exe`; cache do `_deps`; badge no README.
- [ ] **T11.2 — Instalador** (Inno Setup, instalador `.exe`): ícone, atalhos, desinstalador; versão vinda do CMake. Publicar na aba **Releases**.
- [ ] **T11.3 — Licenciamento:** **verificar os termos atuais do JUCE** (há licença livre com condições e opção comercial; projetos open source normalmente usam GPL/AGPL) e escolher a licença do repositório de forma compatível; adicionar `LICENSE`, `THIRD_PARTY_NOTICES.md` (JUCE, Catch2, nlohmann/json, SDK do VST3 usado pelo JUCE).
- [ ] **T11.4 — Documentação de portfólio:** README com GIF/captura, arquitetura resumida, decisões técnicas (por que `core` separado, regras de tempo real), `CHANGELOG.md`, `CONTRIBUTING.md`.
- [ ] **T11.5 — Ícone e *branding* próprio** (sem semelhança com o Live).
- [ ] **T11.6 — Assinatura de código** (opcional, quando houver certificado; **nunca** commitar certificados/chaves — já cobertos pelo `.gitignore`).

### Backlog (sem prioridade definida)

Clips de áudio/samples; automação de parâmetros e de volume; efeitos MIDI (arpejador, acorde, escala); ferramentas de acorde/progressão; CLAP; VST2; macOS; tradução da UI (en/pt); temas claro/escuro configuráveis; tabela de atalhos configurável; Link/sincronização.

---

## 15. Checklist de QA manual (executar no Windows com plugins reais)

Obrigatório após qualquer mudança em áudio/plugins (D10). Registre o resultado no PR.

**Plugins**
- [ ] A lista mostra todos os plugins encontrados (compare `N plugins encontrados` com o número visível em **Todos**).
- [ ] Filtros Instrumentos/Efeitos separam corretamente; busca por nome e por fabricante funciona.
- [ ] Rescan não duplica entradas; abrir de novo mostra a lista instantaneamente (cache XML).

**Cadeia e áudio**
- [ ] Duplo clique em instrumento carrega, abre a UI e **toca ao clicar nas teclas do piano roll** (pré-escuta).
- [ ] Segundo instrumento em **outra trilha** toca simultaneamente.
- [ ] Instrumento novo na mesma trilha **substitui** o anterior (e a janela antiga fecha).
- [ ] Efeito adicionado **altera** o som da trilha; remover o efeito restaura.
- [ ] Mute/Solo/Volume por trilha funcionam; medidor reage.

**Transporte**
- [ ] Espaço toca/pausa **sem notas presas**; Home/botão voltam ao início; Stop volta ao início.
- [ ] Loop: a nota que cruza o fim do loop é cortada e recomeça sem estalo; arrastar o brace muda o loop em tempo real.
- [ ] Clique na régua move o playhead, tocando ou parado.
- [ ] BPM altera o andamento; valores inválidos são limitados (20–999).

**Piano roll**
- [ ] Teclas acendem durante o playback das notas e ao clicar.
- [ ] Criar/mover/redimensionar/copiar/colar/duplicar/quantizar; sobreposição de mesmo pitch segue a regra.
- [ ] Scale + Fold escala: só linhas da escala (+ linhas com notas fora da escala).
- [ ] Zoom (botões, Ctrl+roda, Alt+roda, arrastar régua).

**Janelas e foco**
- [ ] Janela do plugin tem **minimizar e fechar**; fechar não derruba o programa.
- [ ] Espaço funciona depois de clicar em qualquer botão; F6 alterna painéis.

---

## 16. Legal e licenças

- **Funcionalidade, fluxo de trabalho e atalhos** de um programa **não** são protegidos por direitos autorais; **código, texto do manual, ícones, imagens, skins e nome/marca** são. O projeto reimplementa comportamento **do zero**.
- **Nunca** incluir no repositório: trechos do manual do Live, capturas de tela do Live, ícones/fontes/temas do Live, ou o nome "Live"/"Ableton" em produto/UI (mencionar como inspiração em texto de README é aceitável, sem logotipos).
- **Dependências:** JUCE (verificar licença vigente — §14 T11.3), Catch2 (BSL-1.0), nlohmann/json (MIT, quando adicionado). O suporte a VST3 do JUCE traz o SDK da Steinberg — conferir os termos ao distribuir.
- **ASIO:** o SDK é da Steinberg e tem licença própria; **não** incluir sem verificar. Usar WASAPI/DirectSound ou drivers genéricos (ex.: FlexASIO) como alternativa.
- Certificados, chaves e `.env` **nunca** vão para o Git (`.gitignore` já cobre).

---

## 17. Changelog

| Versão | Conteúdo |
|---|---|
| **Marco 1** | Host VST3 mínimo + piano roll básico + modo escala; núcleo com testes |
| **Marco 2** | Transporte (play/pause/stop/loop/BPM), clip tocando no plugin, scanner automático de VST3, navegador, atalhos do Live (copiar/colar, marcador de inserção, tripleto, K, etc.), regra de sobreposição de notas |
| **Doc 1.3** | Recebidas as 3 imagens de referência do Live; **DEC-4 revisada** (plugins agrupados por **pasta**, não por fabricante), menu de contexto do piano roll especificado (F1.8), Pitch Lock no modo desenho, estrutura do navegador (Collections/Categories/Places) — tudo no plano, seção 13 |
| **Doc 1.2** | Criado `PLANO_DE_ACAO_FASE_4.md` (Fases 0–6) a partir do feedback do Marco 3; o plano define a ordem vigente e antecipa itens dos Marcos 7/8 (grade, locators, seleção de região, quantize) |
| **Doc 1.1** | Adicionada a trilha V (identidade visual) e a §19 (paleta, estados, ícones, logo) com base nos materiais do dono; nova pegadinha sobre `currentColor` no JUCE |
| **Marco 3** | Vista de arranjo com várias trilhas e clips, cadeia de plugins por trilha (instrumento + efeitos), mixer básico (volume/mute/solo/medidor), filtros Todos/Instrumentos/Efeitos, teclas que acendem, zoom do piano roll (botões/Alt+roda/arrastar régua), botão voltar ao início, F6/Shift+Tab/F12, janela de plugin com minimizar |

---

## 18. Apêndices

### 18.1 Estrutura de pastas

```
rolldaw/                      (raiz do repositório; renomear para project-freedom quando conveniente)
├─ CMakeLists.txt             (JUCE 8.0.15 + Catch2 v3.16.1 via FetchContent)
├─ README.md   .gitignore
├─ docs/SPEC.md               (este documento)
├─ core/                      (C++ puro)
│   ├─ Note.h  Clip.h/.cpp  Scale.h/.cpp  History.h/.cpp
│   ├─ Sequencer.h/.cpp       (TransportClock + Sequencer)
│   └─ Arrangement.h/.cpp     (Placement, Track, flatten)
├─ tests/                     (Catch2: Scale, Clip, Sequencer, Arrangement)
└─ app/                       (JUCE)
    ├─ Main.cpp  MainComponent.*  Theme.h
    ├─ AudioEngine.*          (SequencerGraph, TrackSource, TrackMixer, AudioEngine)
    ├─ ProjectModel.*         RollModel.*
    ├─ ArrangementView.*      PianoRollEditor.*   DeviceView.*   BrowserPanel.*   TransportBar.*
    └─ PluginScanner.*
```

### 18.2 Tamanhos aproximados (linhas)
`PianoRollEditor.cpp` 729 · `ArrangementView.cpp` 604 · `MainComponent.cpp` 479 · `AudioEngine.cpp` 350 · `RollModel.cpp` 240 · demais < 160.
> Se algum arquivo crescer demais (> ~800 linhas), **dividir por responsabilidade** (ex.: separar `NoteGrid`, `KeyColumn`, `RulerBar`, `VelocityLane` em arquivos próprios).

### 18.3 Glossário rápido
- **Beat:** semínima. **Compasso (bar):** 4 beats (4/4 fixo hoje). **Grade:** subdivisão para encaixe (snap). **Fold:** esconder linhas do piano roll. **Insert marker:** marcador de inserção (onde cola/onde o play começa quando parado). **Placement:** clip posicionado numa trilha. **Flatten:** converter clips em notas de tempo absoluto. **Block plan:** divisão do bloco de áudio em trechos de tempo (por causa do loop).

### 18.4 Contrato de entrega de cada sessão de trabalho (copiar na resposta final)

```text
✔ O que foi feito:        (lista curta)
✔ Arquivos alterados:     (lista)
✔ Testes automáticos:     (quantos, passaram?)
✔ Compilou? (SO/ferram.): sim/não
✔ Verificado visualmente: sim/não (como)
✘ NÃO testado:            (áudio real, plugins, Windows, etc.)
→ Próximo passo sugerido: (tarefa do §14)
→ Como atualizar:         (comandos exatos)
```

---

## 19. Identidade visual (especificação do dono)

> Fonte: README da biblioteca de ícones e logo fornecidos pelo dono. **Tratar como requisito.**

### 19.1 Paleta de referência

| Papel | Cor | Hex | Contraste do ícone/texto sobre o fundo `#343638` | sobre painel `#292B2D` |
|---|---|---|---|---|
| Fundo principal | Cinza grafite | `#343638` | — | — |
| Painéis | Grafite profundo | `#292B2D` | — | — |
| Botões/controles | Grafite escuro | `#242628` | — | — |
| Ícones principais | Cinza-claro | `#E6E6E6` | 9,7:1 | 11,4:1 |
| Texto secundário | Cinza médio | `#A5A7A8` | 5,0:1 | 5,9:1 |
| Reprodução/ativo | Verde | `#55C982` | 5,8:1 | 6,8:1 |
| Gravação | Vermelho | `#E85B5B` | **3,5:1** | 4,1:1 |
| Alertas | Amarelo | `#F2BD50` | 7,0:1 | 8,2:1 |

Observações de acessibilidade: o **vermelho** passa no mínimo de 3:1 para **ícones**, mas **não** atinge 4,5:1 como **texto** sobre `#343638` — usar vermelho em ícones/indicadores, e texto vermelho só sobre os painéis mais escuros ou em negrito/maior. **Não comunicar estado só por cor** (combinar com forma, fundo, tooltip ou texto).

### 19.2 Estados dos controles (proposta a validar com o dono)

| Estado | Ícone | Fundo do botão |
|---|---|---|
| Normal | `#E6E6E6` | `#242628` |
| Hover | `#FFFFFF` | `#2C2E31` (um passo mais claro) |
| Pressed | `#E6E6E6` | `#1D1F21` (um passo mais escuro) |
| Disabled | `#A5A7A8` a ~40% de opacidade | `#242628` |
| Ligado/ativo | `#55C982` **+** fundo/contorno distinto | `#242628` com borda `#55C982` |
| Gravando | `#E85B5B` **+** forma preenchida | `#242628` |

### 19.3 Biblioteca entregue (24 ícones, viewBox 24×24, traço 1,8, `currentColor`)

- **Transporte:** play, stop, pause, record, rewind, fast-forward, loop, metronome
- **Edição:** pointer, pencil, eraser, cut, copy, move, select-region, split
- **Mixer/produção:** mixer, volume, mute, pan, level-meter, automation, keyboard, waveform

**Ícones que a UI atual precisa e ainda não existem (pedir na próxima rodada):** solo; armar gravação (arm); adicionar trilha; apagar; plugin/dispositivo; instrumento vs efeito; bypass; zoom in / zoom out; escala (scale); fold; destacar escala; grade/snap; quantizar; pré-escuta (preview); desfazer / refazer; abrir / salvar / novo; configurações; buscar; atualizar/rescan; setas/chevrons (expandir/recolher); minimizar / fechar; ir ao início (go-to-start: pode reaproveitar `rewind` com barra).

### 19.4 Diretrizes de uso (do dono)

1. 24 px é o tamanho-base; 16–18 px em botões compactos; 28–32 px em destaque.
2. Área clicável maior que o desenho (botão de 36–40 px).
3. Preservar o **logo F/R/E/E** como marca; ícones funcionais **não** repetem o monograma.
4. Se o renderizador não herdar cor: gerar variantes com `stroke`/`fill` explícitos (é o caso do JUCE — ver §12.13).

### 19.5 Logo

Monograma em quadrantes (**F | R / E | E**) separado por uma cruz de linhas finas, com a legenda "PROJECT FREEDOM" em caixa alta espaçada; texto grafite sobre papel claro texturizado. Arquivo atual: `first_logo.jpg` (raster, com textura de papel — **não serve** como ícone de aplicativo; ver tarefa V5).
