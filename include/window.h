#pragma once

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <SDL3_ttf/SDL_ttf.h>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include "chess_types.h"
#include "engine.h"

bool isCapture(const Move& move);
Square getTo(const Move& move);
Square getFrom(const Move& move);

class Window {
public:
    Window(const std::string &title, int width, int height);
    ~Window();

    bool Initialize();
    void RenderLoop();
    void Shutdown();

private:
    enum class EngineState {
        Idle,
        Thinking,
        ResultReady
    };

    void ProcessEvents();
    void Update();
    void Render();
    void LoadPiecesTextures();
    void DrawPieces(int squareWidth, int squareHeight);
    void DrawMoveHighlights(int squareWidth, int squareHeight);
    void DrawUI();
    void DrawMoveHistory();
    void DrawDebugPanel();

    void RenderText(const std::string& text, int x, int y, SDL_Color color);
    SDL_Texture* CreateTextTexture(const std::string& text, SDL_Color color);

    Square CoordinatesToSquare(int x, int y);
    void SquareToCoordinates(Square sq, int &x, int &y);

    void HandleBoardClick(int mouseX, int mouseY);
    void AddMoveToHistory(Move move);

    void StartEngineSearch(int timeMs);
    void StopEngineSearch();
    void SnapshotBoard();
    bool EngineIsBusy() const;

    SDL_Window* window;
    SDL_Renderer* renderer;
    TTF_Font* font;

    bool isRunning;
    int width;
    int height;
    int boardSize;
    int rightPanelWidth;
    int bottomPanelHeight;
    std::string title;

    SDL_Texture* pieceTextures[2][6] = {{nullptr}};
    bool texturesLoaded = false;

    ChessEngine engine;
    Square selectedSquare = NO_SQ;
    std::vector<Move> legalMoves;
    bool isPieceSelected = false;

    struct MoveHistoryEntry {
        int moveNumber;
        std::string whiteMove;
        std::string blackMove;
    };
    std::vector<MoveHistoryEntry> moveHistory;

    Uint32 lastFrameTime;

    std::thread engineThread;
    std::atomic<EngineState> engineState{EngineState::Idle};
    Move pendingEngineMove;

    PieceType snapshotPieces[64];
    Color snapshotColors[64];
    bool snapshotInCheck = false;

    std::vector<ChessEngine::IterationInfo> iterationLog;
    static constexpr size_t MAX_ITERATION_LOG_LINES = 64;
};