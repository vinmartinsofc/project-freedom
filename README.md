# Project Freedom

DAW minimalista e open source, inspirado no fluxo de trabalho do Ableton Live 10/11:
**piano roll rápido, atalhos, trilha MIDI e hospedagem de plugins VST3**.
C++20, JUCE 8, CMake, testes com Catch2.

## Estado atual (Marco 3)
- **Vista de arranjo** (sem Session View): várias trilhas MIDI, clips na linha do tempo, régua com *loop brace*
- **Vários plugins:** cada trilha tem uma cadeia (instrumento + efeitos em sequência); duplo clique no navegador adiciona à trilha selecionada
- **Mixer por trilha:** volume, mute, solo e medidor de nível
- **Detecção automática de VST3**, em segundo plano, com lista salva em disco; filtros *Todos / Instrumentos / Efeitos*
- **Transporte:** voltar ao início, play/pause, stop, loop, BPM, posição (compasso . beat . 16avo)
- **Piano roll** estilo Live: régua, teclas que acendem (clique e playback), velocity, grade, snap, zoom (botões, Ctrl+roda, arrastar a régua)
- **Escala:** destaque, tônica enfatizada, *Fold* e *Fold escala*
- Desfazer/refazer de notas

## Atalhos (conforme o manual do Live)
| Atalho | Ação |
|---|---|
| Espaço | Play / Pause |
| Ctrl+Shift+T | Nova trilha MIDI |
| Shift+Tab ou F12 | Alterna Clip / Dispositivos |
| F6 | Muda o foco: plugins -> arranjo -> piano roll |
| Duplo clique numa trilha | Cria um clip de 1 compasso |
| Arrastar clip / borda direita | Mover (entre trilhas também) / redimensionar; Ctrl = copiar |
| Ctrl+D (no arranjo) | Duplicar clip |
| Delete (no arranjo) | Apagar clip |
| Alt + roda (piano roll) | Zoom vertical |
| Home / End | Piano roll: marcador de inserção no início / fim. Fora dele, Home volta ao início |
| Clique no fundo | Marcador de inserção (parado: o play começa ali) |
| Duplo clique (vazio / nota) | Criar / apagar nota |
| Arrastar nota / borda direita | Mover / redimensionar (**Alt** = sem snap) |
| **Ctrl + arrastar** | Copiar notas |
| Arrastar no vazio | Seleção em retângulo (Shift soma/remove) |
| Esc | Desselecionar tudo |
| Ctrl+A / Ctrl+C / Ctrl+X / Ctrl+V | Selecionar tudo / copiar / recortar / colar no marcador |
| Ctrl+D | Duplicar seleção |
| Delete / Backspace | Apagar seleção |
| Setas ↑ ↓ | Transpor 1 semitom (Shift = oitava) |
| Setas ← → | Mover pela grade; **Shift+← →** = encurtar/alongar |
| 0 | Ativar/desativar notas selecionadas |
| Ctrl+U | Quantizar |
| B | Modo desenho |
| **K** | Destacar a escala |
| Ctrl+1 / Ctrl+2 / **Ctrl+3** / Ctrl+4 | Grade mais fina / mais larga / tripleto / snap |
| + / - | Zoom horizontal |
| Page Up/Down (Ctrl = horizontal) | Rolar |
| Ctrl + roda | Zoom horizontal |
| Ctrl+Z / Ctrl+Y | Desfazer / refazer |

Regra de sobreposição do Live: nota nova sobre outra do mesmo pitch apaga a coberta pelo início
ou encurta a coberta pela cauda.

**Escala (como no manual):** *Scale* liga o modo escala (raiz + tipo); *Destacar* (K) pinta as linhas
da escala e a tônica; *Fold* mostra só linhas com notas; *Fold escala* mostra as linhas da escala
mais as linhas que têm notas.

## Compilar

Requisitos: CMake 3.22+, compilador C++20.

**Windows:** Visual Studio (Community) com "Desenvolvimento para desktop com C++". Num *Developer PowerShell*:
```
cmake -S . -B build -G "Visual Studio 18 2026"
cmake --build build --config Release
```
Executável: `build\app\ProjectFreedom_artefacts\Release\Project Freedom.exe`

**Linux (Ubuntu/Debian):**
```
sudo apt install build-essential cmake ninja-build pkg-config libasound2-dev libfreetype-dev \
  libfontconfig1-dev libx11-dev libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libgl1-mesa-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
```

Testes: `build\tests\Release\roll_tests.exe` (ou `ctest --test-dir build -C Release`).

## Estrutura
```
core/    modelo musical e sequenciador (Note, Clip, Scale, History, Sequencer) -- sem JUCE, testado
tests/   testes unitários do core
app/     aplicativo JUCE (janela, áudio, scanner de plugins, painel, transporte, piano roll)
```

## Roadmap
1. [x] Hospedar VST3 + piano roll básico + modo escala
2. [x] Transporte, clip tocando, scanner de plugins, atalhos principais
3. [x] Arranjo com várias trilhas, cadeia de plugins, mixer básico
4. [ ] Mais atalhos do Live (dividir, consolidar, grade adaptativa), desfazer global, pan e rename de clip
5. [ ] Salvar/abrir projeto (JSON) e estado dos plugins
6. [ ] Exportar MIDI e áudio
7. [ ] Plugins em processo separado (isolamento de crash), instalador
