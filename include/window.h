#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include "chess_types.h"
#include "engine.h"

class Window {
public:
    Window(const std::string &title, int width, int height);
    ~Window();

    bool Initialize();
    void RenderLoop();
    void Shutdown();
    
private:
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
};