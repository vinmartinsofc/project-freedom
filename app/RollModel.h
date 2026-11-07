#pragma once
#include <juce_events/juce_events.h>
#include "Clip.h"
#include "History.h"
#include "Scale.h"
#include <cstdint>
#include <functional>
#include <set>
#include <vector>

// Estado compartilhado pelo piano roll (grade, teclas, régua, velocity).
// Todas as edições passam por aqui: assim desfazer/refazer, o engine de áudio
// e as views ficam sempre sincronizados.
class RollModel : public juce::ChangeBroadcaster
{
public:
    RollModel();

    int rowHeight = 16;                         // zoom vertical

    // Clip em edição (pertence ao arranjo). Sem clip selecionado, o piano roll fica inativo.
    roll::Clip& clip()              { return *clipPtr; }
    const roll::Clip& clip() const  { return *clipPtr; }
    bool hasClip() const            { return clipPtr != &dummyClip; }
    void setClip (roll::Clip* c);
    double clipStartBeats = 0.0;                // posição do clip no arranjo (para o playhead)
    roll::History history;
    roll::Scale   scale { 0, roll::Mode::Major };

    bool   scaleOn   = false;    // "Scale Mode": escolhe raiz/escala do clip
    bool   highlight = true;     // tecla K: destaca as linhas da escala (vale com scaleOn)
    bool   fold      = false;    // Fold: só linhas que têm notas
    bool   foldScale = false;    // botão Scale ao lado do Fold: linhas da escala + linhas com notas
    bool   drawMode  = false;
    bool   snap      = true;
    bool   previewOn = true;     // ouvir as notas ao selecionar/mover
    bool   triplet   = false;
    double baseGrid      = 0.25;   // 1/16
    double gridBeats     = 0.25;   // grade efetiva (baseGrid, ou 2/3 dela em tripleto)
    double insertBeat    = 0.0;    // marcador de inserção (onde cola; de onde o play começa)
    double pixelsPerBeat = 56.0;

    std::set<int> selection;

    // teclas acesas: clique do mouse e notas tocando no playback
    int pressedPitch = -1;
    std::uint64_t activeLo = 0, activeHi = 0;
    bool isKeyLit (int p) const
    {
        return p == pressedPitch || (p < 64 ? ((activeLo >> p) & 1u) : ((activeHi >> (p - 64)) & 1u));
    }

    std::function<void()>          onNotesChanged;   // engine de áudio
    std::function<void()>          onLengthChanged;  // loop do transporte
    std::function<void (int, bool)> onPreview;       // pitch, ligar/desligar
    std::function<void (double)>   onInsertMarker;   // marcador de inserção mudou

    // --- layout vertical (linhas visíveis, da mais aguda para a mais grave) ---
    const std::vector<int>& rows() const { return rowPitches; }
    void rebuildRows();
    int  pitchToRow (int pitch) const;           // -1 se escondida
    int  rowToPitch (int row) const;             // -1 fora do intervalo
    int  totalHeight() const { return static_cast<int> (rowPitches.size()) * rowHeight; }
    double gridWidthPixels() const { return clip().length() * pixelsPerBeat; }

    double snapBeats (double b) const  { return snap ? roll::snapToGrid (b, gridBeats) : b; }
    double minLength() const           { return snap ? gridBeats : 1.0 / 16.0; }
    juce::String gridLabel() const;

    // --- edição (cada uma registra desfazer e notifica engine + views) ---
    void beginEdit();                         // snapshot para desfazer
    void notify (bool rebuildRowLayout);      // publica notas + atualiza views
    void undo();
    void redo();
    bool isSelected (int id) const { return selection.count (id) > 0; }

    void selectAll();
    void deleteSelected();
    void duplicateSelected();
    void nudgeSelected (double dBeats, int dPitch);
    void quantizeSelected();
    void toggleMuteSelected();
    void narrowGrid();
    void widenGrid();
    void toggleSnap();
    void toggleTriplet();
    void resizeSelected (double dBeats);
    void copySelected (bool cut);
    void paste();
    void setInsertMarker (double beats);
    void resolveOverlaps (const std::set<int>& ids);

private:
    void pruneSelection();
    roll::Clip dummyClip { 16.0 };
    roll::Clip* clipPtr = &dummyClip;
    void updateGrid();
    void extendToFit (double endBeat);
    std::vector<roll::Note> clipboard;
    std::vector<int> rowPitches;
};
