#include "window.h"
#include <iostream>
#include <sstream>
#include <iomanip>

Window::Window(const std::string &title, int width, int height) : title(title), width(width), height(height), window(nullptr), renderer(nullptr), font(nullptr), isRunning(false), lastFrameTime(0), selectedSquare(NO_SQ), isPieceSelected(false)
{ // window layout design here
    rightPanelWidth = width / 3;  
    bottomPanelHeight = height / 6;
    boardSize = std::min(width - rightPanelWidth, height - bottomPanelHeight);
}

Window::~Window()
{
    Shutdown();
}

bool Window::Initialize()
{
    // initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return false;
    }
    // initialize image support
    if (IMG_Init(IMG_INIT_PNG) == 0)
    {
        std::cerr << "IMG_Init Error: " << IMG_GetError() << std::endl;
        SDL_Quit();
        return false;
    }
    // initalize text
    if (TTF_Init() != 0)
    {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    // setup the gui
    window = SDL_CreateWindow(title.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, SDL_WINDOW_SHOWN);
    if (!window)
    {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return false;
    }

    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
    {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        TTF_Quit();
        IMG_Quit();
        SDL_Quit();
        return false;
    }
    
    font = TTF_OpenFont("assets/Terminal.ttf", 18);
    if (!font)
    {
        std::cerr << "Failed to load font: " << TTF_GetError() << std::endl;
    }

    // start engine
    engine.resetToStartingPosition();
    
    isRunning = true;
    lastFrameTime = SDL_GetTicks();
    
    return true;
}

void Window::RenderLoop()
{
    while (isRunning) {
        ProcessEvents();
        Update();
        Render();
        SDL_Delay(16); // 60 fps
    }
}

void Window::Shutdown()
{
    for (int color = 0; color < 2; color++) {
        for (int pieceType = 0; pieceType < 6; pieceType++) {
            if (pieceTextures[color][pieceType]) {
                SDL_DestroyTexture(pieceTextures[color][pieceType]);
                pieceTextures[color][pieceType] = nullptr;
            }
        }
    }

    if (font)
    {
        TTF_CloseFont(font);
        font = nullptr;
    }

    if (renderer)
    {
        SDL_DestroyRenderer(renderer);
        renderer = nullptr;
    }

    if (window)
    {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void Window::ProcessEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        switch (event.type)
        {
            case SDL_QUIT:
                isRunning = false;
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    isRunning = false;
                }
                else if (event.key.keysym.sym == SDLK_u)
                {
                    engine.unmakeMove();
                    selectedSquare = NO_SQ;
                    isPieceSelected = false;
                    legalMoves.clear();
                    
                    if (!moveHistory.empty()) {
                        MoveHistoryEntry& last = moveHistory.back();
                        if (!last.blackMove.empty()) {
                            last.blackMove = "";
                        } else {
                            moveHistory.pop_back();
                        }
                    }
                }
                break;
                
            case SDL_MOUSEBUTTONDOWN:
                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    if (event.button.x < boardSize && event.button.y < boardSize) {
                        HandleBoardClick(event.button.x, event.button.y);
                    }
                }
                break;
                
            default:
                break;
        }
    }
}

void Window::Update()
{
    Uint32 currentFrameTime = SDL_GetTicks();
    Uint32 deltaTime = currentFrameTime - lastFrameTime;
    lastFrameTime = currentFrameTime;
}

void Window::Render()
{
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    SDL_RenderClear(renderer);

    int squareSize = boardSize / 8;

    // checkerboard
    for (int row = 0; row < 8; row++) {
        for (int col = 0; col < 8; col++) {
            if ((row + col) % 2 == 0) {
                SDL_SetRenderDrawColor(renderer, 234, 233, 210, 255);
            } else {
                SDL_SetRenderDrawColor(renderer, 75, 115, 153, 255);
            }
            
            SDL_Rect square = {
                col * squareSize,
                row * squareSize,
                squareSize,
                squareSize
            };
            
            SDL_RenderFillRect(renderer, &square);
        }
    }
    
    // highlighting
    DrawMoveHighlights(squareSize, squareSize);
    
    // piece rendering
    DrawPieces(squareSize, squareSize);
    
    // debug ui
    DrawUI();
    
    SDL_RenderPresent(renderer);
}

void Window::LoadPiecesTextures()
{
    const std::string pieceFiles[2][6] = {
        {
            "assets/white_pawn.png",
            "assets/white_knight.png",
            "assets/white_bishop.png",
            "assets/white_rook.png",
            "assets/white_queen.png",
            "assets/white_king.png"
        },
        {
            "assets/black_pawn.png",
            "assets/black_knight.png",
            "assets/black_bishop.png",
            "assets/black_rook.png",
            "assets/black_queen.png",
            "assets/black_king.png"
        }
    };

    for (int color = 0; color < 2; color++) {
        for (int pieceType = 0; pieceType < 6; pieceType++) {
            SDL_Surface* surface = IMG_Load(pieceFiles[color][pieceType].c_str());
            if (!surface) {
                std::cerr << "Failed to load piece texture: " << pieceFiles[color][pieceType] << std::endl;
                std::cerr << "IMG_Load Error: " << IMG_GetError() << std::endl;
                continue;
            }
            
            pieceTextures[color][pieceType] = SDL_CreateTextureFromSurface(renderer, surface);
            SDL_FreeSurface(surface);
            
            if (!pieceTextures[color][pieceType]) {
                std::cerr << "Failed to create texture from surface: " << SDL_GetError() << std::endl;
            }
        }
    }
    
    texturesLoaded = true;
}

void Window::DrawPieces(int squareWidth, int squareHeight)
{
    if (!texturesLoaded) {
        LoadPiecesTextures();
    }

    for (int rank = 0; rank < 8; rank++) {
        for (int file = 0; file < 8; file++) {
            Square sq = static_cast<Square>(rank * 8 + file);
            Color pieceColor;
            PieceType pieceType = engine.getPieceAt(sq, pieceColor);
            
            if (pieceType != NONE) {
                int screenRank = 7 - rank;
                
                SDL_Rect destRect = {
                    file * squareWidth,
                    screenRank * squareHeight,
                    squareWidth,
                    squareHeight
                };
                
                if (pieceTextures[pieceColor][pieceType]) {
                    SDL_RenderCopy(renderer, pieceTextures[pieceColor][pieceType], NULL, &destRect);
                }
            }
        }
    }
}

bool isCapture(const Move& move) {
    return (move.flags() & CAPTURE) != 0 || 
           move.flags() == EN_PASSANT || 
           move.flags() == PC_KNIGHT || 
           move.flags() == PC_BISHOP || 
           move.flags() == PC_ROOK || 
           move.flags() == PC_QUEEN;
}

Square getTo(const Move& move) {
    return move.to();
}

Square getFrom(const Move& move) {
    return move.from();
}

void Window::DrawMoveHighlights(int squareWidth, int squareHeight)
{
    if (isPieceSelected && selectedSquare != NO_SQ) {
        int x, y;
        SquareToCoordinates(selectedSquare, x, y);
        
        SDL_SetRenderDrawColor(renderer, 186, 202, 68, 255);
        SDL_Rect highlightRect = {
            x, y, squareWidth, squareHeight
        };
        SDL_RenderFillRect(renderer, &highlightRect);
    }
    
    // draw circle for legal moves
    for (const Move& move : legalMoves) {
        Square to = getTo(move);
        
        int x, y;
        SquareToCoordinates(to, x, y);
        
        int radius = squareWidth / 4;
        int centerX = x + squareWidth / 2;
        int centerY = y + squareHeight / 2;
        
        if (isCapture(move)) {
            SDL_SetRenderDrawColor(renderer, 209, 61, 61, 180);
            SDL_Rect captureRect = {
                x, y, squareWidth, squareHeight
            };
            
            int borderSize = 3;
            SDL_Rect outerRect = captureRect;
            SDL_RenderDrawRect(renderer, &outerRect);
            
            captureRect.x += borderSize;
            captureRect.y += borderSize;
            captureRect.w -= 2 * borderSize;
            captureRect.h -= 2 * borderSize;
            
            SDL_RenderDrawRect(renderer, &captureRect);
        } 
        else {
            SDL_SetRenderDrawColor(renderer, 186, 202, 68, 180);
            
            for (int w = -radius; w <= radius; w++) {
                for (int h = -radius; h <= radius; h++) {
                    if (w*w + h*h <= radius*radius) {
                        SDL_RenderDrawPoint(renderer, centerX + w, centerY + h);
                    }
                }
            }
        }
    }
}

void Window::DrawUI()
{
    // right panel
    SDL_SetRenderDrawColor(renderer, 44, 44, 44, 255);
    SDL_Rect rightPanel = { boardSize, 0, rightPanelWidth, height - bottomPanelHeight };
    SDL_RenderFillRect(renderer, &rightPanel);
    
    // debug panel
    SDL_SetRenderDrawColor(renderer, 44, 44, 44, 255);
    SDL_Rect bottomPanel = { 0, boardSize, width, bottomPanelHeight };
    SDL_RenderFillRect(renderer, &bottomPanel);
    
    SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);
    SDL_RenderDrawLine(renderer, boardSize, 0, boardSize, height - bottomPanelHeight);
    SDL_RenderDrawLine(renderer, 0, boardSize, width, boardSize);
    
    // write gamelog
    DrawMoveHistory();
    
    // Draw debug panel in the bottom panel
    DrawDebugPanel();
}

void Window::DrawMoveHistory()
{
    int startX = boardSize + 20;
    int startY = 20;
    int lineHeight = 24;
    
    SDL_Color white = {255, 255, 255, 255};
    RenderText("MOVE HISTORY", startX, startY, white);
    
    startY += lineHeight * 2;
    int y = startY;
    
    for (const auto& entry : moveHistory) {
        std::string moveNumberStr = std::to_string(entry.moveNumber) + ".";
        RenderText(moveNumberStr, startX, y, white);
        
        SDL_Color lightGray = {200, 200, 200, 255};
        RenderText(entry.whiteMove, startX + 40, y, lightGray);
        
        if (!entry.blackMove.empty()) {
            SDL_Color darkGray = {180, 180, 180, 255};
            RenderText(entry.blackMove, startX + 100, y, darkGray);
        }
        
        y += lineHeight;
    }
    
    // display current turn
    y += lineHeight * 2;
    std::string turnStr = "Turn: " + std::string(engine.getSideToMove() == WHITE ? "White" : "Black");
    SDL_Color yellow = {255, 255, 0, 255};
    RenderText(turnStr, startX, y, yellow);
}

void Window::DrawDebugPanel()
{
    int startX = 20;
    int startY = boardSize + 20;
    SDL_Color white = {255, 255, 255, 255};
    
    RenderText("DEBUG PANEL", startX, startY, white);
    
    startY += 30;
    
    std::string stateInfo = "Board state: ";
    if (engine.isInCheck(engine.getSideToMove())) {
        stateInfo += "Check!";
    } else {
        stateInfo += "Normal";
    }
    
    RenderText(stateInfo, startX, startY, white);

    // eval display
    int evaluation = engine.eval();
    // int evaluation = engine.search(1, -std::numeric_limits<int>::infinity(), std::numeric_limits<int>::infinity());
    std::ostringstream evalStream;
    evalStream << "EVAL: (" << (engine.getSideToMove() == WHITE ? "WHITE" : "BLACK") << ") " 
               << std::fixed << std::setprecision(2) << evaluation / 100.0;
    RenderText(evalStream.str(), startX, startY + 20, white);
}

void Window::RenderText(const std::string& text, int x, int y, SDL_Color color)
{
    if (!font) {
        SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
        SDL_Rect textRect = {
            x, y, static_cast<int>(text.length() * 8), 18
        };
        SDL_RenderDrawRect(renderer, &textRect);
        return;
    }
    
    SDL_Texture* textTexture = CreateTextTexture(text, color);
    if (textTexture) {
        int textWidth, textHeight;
        SDL_QueryTexture(textTexture, NULL, NULL, &textWidth, &textHeight);
        
        SDL_Rect destRect = {x, y, textWidth, textHeight};
        SDL_RenderCopy(renderer, textTexture, NULL, &destRect);
        
        SDL_DestroyTexture(textTexture);
    }
}

SDL_Texture* Window::CreateTextTexture(const std::string& text, SDL_Color color)
{
    if (!font || text.empty()) {
        return nullptr;
    }
    
    SDL_Surface* surface = TTF_RenderText_Blended(font, text.c_str(), color);
    if (!surface) {
        std::cerr << "Failed to render text: " << TTF_GetError() << std::endl;
        return nullptr;
    }
    
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    
    if (!texture) {
        std::cerr << "Failed to create texture from rendered text: " << SDL_GetError() << std::endl;
    }
    
    return texture;
}

Square Window::CoordinatesToSquare(int x, int y)
{
    int squareSize = boardSize / 8;
    
    int file = x / squareSize;
    int rank = 7 - (y / squareSize);
    
    if (file < 0 || file > 7 || rank < 0 || rank > 7) {
        return NO_SQ;
    }
    
    return static_cast<Square>(rank * 8 + file);
}

void Window::SquareToCoordinates(Square sq, int &x, int &y)
{
    if (sq == NO_SQ) {
        x = -1;
        y = -1;
        return;
    }
    
    int squareSize = boardSize / 8;
    
    int file = sq % 8;
    int rank = sq / 8;
    
    x = file * squareSize;
    y = (7 - rank) * squareSize;
}

void Window::HandleBoardClick(int mouseX, int mouseY)
{
    Square clickedSquare = CoordinatesToSquare(mouseX, mouseY);
    
    if (clickedSquare == NO_SQ) {
        return;
    }
    
    if (!isPieceSelected) {
        Color pieceColor;
        PieceType pieceType = engine.getPieceAt(clickedSquare, pieceColor);
        
        if (pieceType != NONE && pieceColor == engine.getSideToMove()) {
            selectedSquare = clickedSquare;
            isPieceSelected = true;
            
            std::vector<Move> allLegalMoves = engine.generateLegalMoves();
            legalMoves.clear();
            
            for (const Move& move : allLegalMoves) {
                if (getFrom(move) == selectedSquare) {
                    legalMoves.push_back(move);
                }
            }
        }
    }
    else { // selecting when a piece is already selected
        for (const Move& move : legalMoves) {
            if (getTo(move) == clickedSquare) {
                AddMoveToHistory(move);
                engine.makeMove(move);
                
                selectedSquare = NO_SQ;
                isPieceSelected = false;
                legalMoves.clear();
                return;
            }
        }
        
        Color pieceColor;
        PieceType pieceType = engine.getPieceAt(clickedSquare, pieceColor);
        
        if (pieceType != NONE && pieceColor == engine.getSideToMove()) {
            selectedSquare = clickedSquare;
            
            std::vector<Move> allLegalMoves = engine.generateLegalMoves();
            legalMoves.clear();
            
            for (const Move& move : allLegalMoves) {
                if (getFrom(move) == selectedSquare) {
                    legalMoves.push_back(move);
                }
            }
        } 
        else {
            selectedSquare = NO_SQ;
            isPieceSelected = false;
            legalMoves.clear();
        }
    }
}

void Window::AddMoveToHistory(Move move)
{
    std::string moveNotation = engine.moveToString(move);
    
    if (engine.getSideToMove() == WHITE) {
        MoveHistoryEntry entry;
        entry.moveNumber = moveHistory.size() + 1;
        entry.whiteMove = moveNotation;
        entry.blackMove = "";
        moveHistory.push_back(entry);
    }
    else {
        if (!moveHistory.empty()) {
            moveHistory.back().blackMove = moveNotation;
        }
    }
}