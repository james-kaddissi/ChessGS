#include "window.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

static SDL_FRect MakeFRect(float x, float y, float w, float h) {
  return SDL_FRect{x, y, w, h};
}

Window::Window(const std::string &title, int width, int height)
    : title(title), width(width), height(height), window(nullptr),
      renderer(nullptr), font(nullptr), isRunning(false), lastFrameTime(0),
      selectedSquare(NO_SQ), isPieceSelected(false) {
  rightPanelWidth = width / 3;
  bottomPanelHeight = height / 6;
  boardSize = std::min(width - rightPanelWidth, height - bottomPanelHeight);
}

Window::~Window() { Shutdown(); }

bool Window::Initialize() {
  if (!SDL_Init(SDL_INIT_VIDEO)) {
    std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
    return false;
  }

  if (!TTF_Init()) {
    std::cerr << "TTF_Init Error: " << SDL_GetError() << std::endl;
    SDL_Quit();
    return false;
  }

  window = SDL_CreateWindow(title.c_str(), width, height, 0);
  if (!window) {
    std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
    TTF_Quit();
    SDL_Quit();
    return false;
  }

  renderer = SDL_CreateRenderer(window, nullptr);
  if (!renderer) {
    std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
    SDL_DestroyWindow(window);
    TTF_Quit();
    SDL_Quit();
    return false;
  }

  SDL_SetRenderVSync(renderer, 1);

  font = TTF_OpenFont("assets/Terminal.ttf", 18);
  if (!font) {
    std::cerr << "Failed to load font: " << SDL_GetError() << std::endl;
  }

  engine.resetToStartingPosition();

  isRunning = true;
  lastFrameTime = SDL_GetTicks();

  return true;
}

void Window::RenderLoop() {
  while (isRunning) {
    ProcessEvents();
    Update();
    Render();
    SDL_Delay(16);
  }
}

void Window::Shutdown() {
  StopEngineSearch();

  for (int color = 0; color < 2; color++) {
    for (int pieceType = 0; pieceType < 6; pieceType++) {
      if (pieceTextures[color][pieceType]) {
        SDL_DestroyTexture(pieceTextures[color][pieceType]);
        pieceTextures[color][pieceType] = nullptr;
      }
    }
  }

  if (font) {
    TTF_CloseFont(font);
    font = nullptr;
  }

  if (renderer) {
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;
  }

  if (window) {
    SDL_DestroyWindow(window);
    window = nullptr;
  }

  TTF_Quit();
  SDL_Quit();
}

bool Window::EngineIsBusy() const {
  return engineState.load() != EngineState::Idle;
}

void Window::SnapshotBoard() {
  for (int i = 0; i < 64; i++) {
    Square sq = static_cast<Square>(i);
    Color c;
    PieceType pt = engine.getPieceAt(sq, c);
    snapshotPieces[i] = pt;
    snapshotColors[i] = c;
  }

  snapshotInCheck = engine.isInCheck(engine.getSideToMove());
}

void Window::StartEngineSearch(int timeMs) {
  SnapshotBoard();

  engineState.store(EngineState::Thinking);

  if (engineThread.joinable()) {
    engineThread.join();
  }

  engineThread = std::thread([this, timeMs]() {
    Move m = engine.getBestMoveWithTime(timeMs);
    pendingEngineMove = m;
    engineState.store(EngineState::ResultReady);
  });
}

void Window::StopEngineSearch() {
  if (engineThread.joinable()) {
    engine.stop();
    engineThread.join();
  }

  engineState.store(EngineState::Idle);
}

void Window::ProcessEvents() {
  SDL_Event event;

  while (SDL_PollEvent(&event)) {
    switch (event.type) {
    case SDL_EVENT_QUIT:
      isRunning = false;
      break;

    case SDL_EVENT_KEY_DOWN:
      if (event.key.key == SDLK_ESCAPE) {
        isRunning = false;
      } else if (event.key.key == SDLK_U && !EngineIsBusy()) {
        engine.unmakeMove();

        selectedSquare = NO_SQ;
        isPieceSelected = false;
        legalMoves.clear();

        if (!moveHistory.empty()) {
          MoveHistoryEntry &last = moveHistory.back();

          if (!last.blackMove.empty()) {
            last.blackMove = "";
          } else {
            moveHistory.pop_back();
          }
        }
      }
      break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
      if (event.button.button == SDL_BUTTON_LEFT && !EngineIsBusy()) {
        int mouseX = static_cast<int>(event.button.x);
        int mouseY = static_cast<int>(event.button.y);

        if (mouseX < boardSize && mouseY < boardSize) {
          HandleBoardClick(mouseX, mouseY);
        }
      }
      break;

    default:
      break;
    }
  }
}

void Window::Update() {
  Uint32 currentFrameTime = SDL_GetTicks();
  Uint32 deltaTime = currentFrameTime - lastFrameTime;
  lastFrameTime = currentFrameTime;
  (void)deltaTime;

  if (engineState.load() == EngineState::ResultReady) {
    if (engineThread.joinable()) {
      engineThread.join();
    }

    Move engineMove = pendingEngineMove;

    if (engineMove != Move()) {
      AddMoveToHistory(engineMove);
      engine.makeMove(engineMove);
    }

    engineState.store(EngineState::Idle);
  }

  auto fresh = engine.drainIterationLog();

  if (!fresh.empty()) {
    iterationLog.insert(iterationLog.end(),
                        std::make_move_iterator(fresh.begin()),
                        std::make_move_iterator(fresh.end()));

    if (iterationLog.size() > MAX_ITERATION_LOG_LINES) {
      iterationLog.erase(iterationLog.begin(),
                         iterationLog.begin() +
                             (iterationLog.size() - MAX_ITERATION_LOG_LINES));
    }
  }
}

void Window::Render() {
  SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
  SDL_RenderClear(renderer);

  int squareSize = boardSize / 8;

  for (int row = 0; row < 8; row++) {
    for (int col = 0; col < 8; col++) {
      if ((row + col) % 2 == 0) {
        SDL_SetRenderDrawColor(renderer, 234, 233, 210, 255);
      } else {
        SDL_SetRenderDrawColor(renderer, 75, 115, 153, 255);
      }

      SDL_FRect square = MakeFRect(
          static_cast<float>(col * squareSize),
          static_cast<float>(row * squareSize),
          static_cast<float>(squareSize),
          static_cast<float>(squareSize));

      SDL_RenderFillRect(renderer, &square);
    }
  }

  DrawMoveHighlights(squareSize, squareSize);
  DrawPieces(squareSize, squareSize);
  DrawUI();

  SDL_RenderPresent(renderer);
}

void Window::LoadPiecesTextures() {
  const std::string pieceFiles[2][6] = {
      {"assets/white_pawn.png", "assets/white_knight.png",
       "assets/white_bishop.png", "assets/white_rook.png",
       "assets/white_queen.png", "assets/white_king.png"},
      {"assets/black_pawn.png", "assets/black_knight.png",
       "assets/black_bishop.png", "assets/black_rook.png",
       "assets/black_queen.png", "assets/black_king.png"}};

  for (int color = 0; color < 2; color++) {
    for (int pieceType = 0; pieceType < 6; pieceType++) {
      SDL_Surface *surface = IMG_Load(pieceFiles[color][pieceType].c_str());

      if (!surface) {
        std::cerr << "Failed to load piece texture: "
                  << pieceFiles[color][pieceType] << std::endl;
        std::cerr << "IMG_Load Error: " << SDL_GetError() << std::endl;
        continue;
      }

      pieceTextures[color][pieceType] =
          SDL_CreateTextureFromSurface(renderer, surface);

      SDL_DestroySurface(surface);

      if (!pieceTextures[color][pieceType]) {
        std::cerr << "Failed to create texture from surface: "
                  << SDL_GetError() << std::endl;
      }
    }
  }

  texturesLoaded = true;
}

void Window::DrawPieces(int squareWidth, int squareHeight) {
  if (!texturesLoaded) {
    LoadPiecesTextures();
  }

  bool useSnapshot = EngineIsBusy();

  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 8; file++) {
      Square sq = static_cast<Square>(rank * 8 + file);
      Color pieceColor;
      PieceType pieceType;

      if (useSnapshot) {
        pieceType = snapshotPieces[sq];
        pieceColor = snapshotColors[sq];
      } else {
        pieceType = engine.getPieceAt(sq, pieceColor);
      }

      if (pieceType != NONE) {
        int screenRank = 7 - rank;

        SDL_FRect destRect = MakeFRect(
            static_cast<float>(file * squareWidth),
            static_cast<float>(screenRank * squareHeight),
            static_cast<float>(squareWidth),
            static_cast<float>(squareHeight));

        if (pieceTextures[pieceColor][pieceType]) {
          SDL_RenderTexture(renderer, pieceTextures[pieceColor][pieceType],
                            nullptr, &destRect);
        }
      }
    }
  }
}

bool isCapture(const Move &move) {
  return (move.flags() & CAPTURE) != 0 || move.flags() == EN_PASSANT ||
         move.flags() == PC_KNIGHT || move.flags() == PC_BISHOP ||
         move.flags() == PC_ROOK || move.flags() == PC_QUEEN;
}

Square getTo(const Move &move) { return move.to(); }

Square getFrom(const Move &move) { return move.from(); }

void Window::DrawMoveHighlights(int squareWidth, int squareHeight) {
  if (isPieceSelected && selectedSquare != NO_SQ) {
    int x, y;
    SquareToCoordinates(selectedSquare, x, y);

    SDL_SetRenderDrawColor(renderer, 186, 202, 68, 255);

    SDL_FRect highlightRect = MakeFRect(
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(squareWidth),
        static_cast<float>(squareHeight));

    SDL_RenderFillRect(renderer, &highlightRect);
  }

  for (const Move &move : legalMoves) {
    Square to = getTo(move);

    int x, y;
    SquareToCoordinates(to, x, y);

    int radius = squareWidth / 4;
    int centerX = x + squareWidth / 2;
    int centerY = y + squareHeight / 2;

    if (isCapture(move)) {
      SDL_SetRenderDrawColor(renderer, 209, 61, 61, 180);

      SDL_FRect outerRect = MakeFRect(
          static_cast<float>(x),
          static_cast<float>(y),
          static_cast<float>(squareWidth),
          static_cast<float>(squareHeight));

      SDL_RenderRect(renderer, &outerRect);

      int borderSize = 3;

      SDL_FRect innerRect = MakeFRect(
          static_cast<float>(x + borderSize),
          static_cast<float>(y + borderSize),
          static_cast<float>(squareWidth - 2 * borderSize),
          static_cast<float>(squareHeight - 2 * borderSize));

      SDL_RenderRect(renderer, &innerRect);
    } else {
      SDL_SetRenderDrawColor(renderer, 186, 202, 68, 180);

      for (int w = -radius; w <= radius; w++) {
        for (int h = -radius; h <= radius; h++) {
          if (w * w + h * h <= radius * radius) {
            SDL_RenderPoint(renderer,
                            static_cast<float>(centerX + w),
                            static_cast<float>(centerY + h));
          }
        }
      }
    }
  }
}

void Window::DrawUI() {
  SDL_SetRenderDrawColor(renderer, 44, 44, 44, 255);

  SDL_FRect rightPanel = MakeFRect(
      static_cast<float>(boardSize),
      0.0f,
      static_cast<float>(rightPanelWidth),
      static_cast<float>(height - bottomPanelHeight));

  SDL_RenderFillRect(renderer, &rightPanel);

  SDL_FRect bottomPanel = MakeFRect(
      0.0f,
      static_cast<float>(boardSize),
      static_cast<float>(width),
      static_cast<float>(bottomPanelHeight));

  SDL_RenderFillRect(renderer, &bottomPanel);

  SDL_SetRenderDrawColor(renderer, 200, 200, 200, 255);

  SDL_RenderLine(renderer,
                 static_cast<float>(boardSize),
                 0.0f,
                 static_cast<float>(boardSize),
                 static_cast<float>(height - bottomPanelHeight));

  SDL_RenderLine(renderer,
                 0.0f,
                 static_cast<float>(boardSize),
                 static_cast<float>(width),
                 static_cast<float>(boardSize));

  DrawMoveHistory();
  DrawDebugPanel();
}

void Window::DrawMoveHistory() {
  int startX = boardSize + 20;
  int startY = 20;
  int lineHeight = 24;

  SDL_Color white = {255, 255, 255, 255};
  RenderText("MOVE HISTORY", startX, startY, white);

  startY += lineHeight * 2;
  int y = startY;

  for (const auto &entry : moveHistory) {
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

  y += lineHeight * 2;

  std::string turnStr;

  if (EngineIsBusy()) {
    turnStr = "Turn: Engine (thinking)";
  } else {
    turnStr = "Turn: " +
              std::string(engine.getSideToMove() == WHITE ? "White" : "Black");
  }

  SDL_Color yellow = {255, 255, 0, 255};
  RenderText(turnStr, startX, y, yellow);
}

const int DEPTH = 3;

void Window::DrawDebugPanel() {
  int startX = 20;
  int startY = boardSize + 12;

  SDL_Color white = {255, 255, 255, 255};
  SDL_Color yellow = {255, 220, 100, 255};
  SDL_Color gray = {180, 180, 180, 255};
  SDL_Color dimGray = {130, 130, 130, 255};
  SDL_Color green = {120, 220, 120, 255};
  SDL_Color red = {220, 120, 120, 255};

  int lineH = 18;

  bool busy = EngineIsBusy();
  bool inCheck =
      busy ? snapshotInCheck : engine.isInCheck(engine.getSideToMove());

  {
    std::ostringstream s;
    s << "DEBUG  |  " << (busy ? "ENGINE THINKING" : "IDLE") << "  |  "
      << (inCheck ? "CHECK" : "OK");

    RenderText(s.str(), startX, startY, busy ? yellow : white);
  }

  startY += lineH + 4;

  const auto &p = engine.progress();

  int curDepth = p.depth.load(std::memory_order_relaxed);
  int compDepth = p.completed_depth.load(std::memory_order_relaxed);
  uint64_t nodes = p.nodes.load(std::memory_order_relaxed);
  uint64_t qn = p.qnodes.load(std::memory_order_relaxed);
  int score = p.score_cp.load(std::memory_order_relaxed);
  uint32_t t0 = p.start_ms.load(std::memory_order_relaxed);
  uint32_t elapsed_ms = busy && t0 ? (SDL_GetTicks() - t0) : 0;
  double elapsed_s = elapsed_ms / 1000.0;
  uint64_t nps = elapsed_s > 0.001 ? static_cast<uint64_t>(nodes / elapsed_s) : 0;

  auto fmtK = [](uint64_t n) -> std::string {
    std::ostringstream s;

    if (n >= 1'000'000) {
      s << std::fixed << std::setprecision(2) << (n / 1'000'000.0) << "M";
    } else if (n >= 1'000) {
      s << std::fixed << std::setprecision(1) << (n / 1'000.0) << "k";
    } else {
      s << n;
    }

    return s.str();
  };

  {
    std::ostringstream s;

    s << "depth " << curDepth << "/" << compDepth << "  nodes " << fmtK(nodes)
      << " (q " << fmtK(qn) << ")"
      << "  nps " << fmtK(nps) << "  t " << std::fixed << std::setprecision(1)
      << elapsed_s << "s"
      << "  score " << std::showpos << score << std::noshowpos << "cp";

    RenderText(s.str(), startX, startY, busy ? white : gray);
  }

  startY += lineH + 6;

  RenderText("ITERATIONS", startX, startY, dimGray);
  startY += lineH;

  int linesAvailable = (height - startY - 4) / lineH;

  if (linesAvailable < 1) {
    return;
  }

  int firstIdx = std::max(0, static_cast<int>(iterationLog.size()) - linesAvailable);

  for (int i = firstIdx; i < static_cast<int>(iterationLog.size()); i++) {
    const auto &it = iterationLog[i];

    std::ostringstream s;

    s << "d" << std::setw(2) << it.depth << "  " << std::setw(8) << std::right
      << fmtK(it.nodes) << "  " << std::setw(5) << it.time_ms << "ms"
      << "  " << (it.score_cp >= 0 ? "+" : "") << it.score_cp << "cp"
      << "  hash " << std::setw(2) << it.hash_hit_pct << "%"
      << "  fhf " << std::setw(2) << it.fail_high_first_pct << "%"
      << "  bf " << std::fixed << std::setprecision(2)
      << (it.effective_branching_x100 / 100.0) << "  " << it.pv;

    SDL_Color c = (i == static_cast<int>(iterationLog.size()) - 1) ? white : gray;

    if (i == static_cast<int>(iterationLog.size()) - 1) {
      if (it.score_cp > 50) {
        c = green;
      } else if (it.score_cp < -50) {
        c = red;
      }
    }

    RenderText(s.str(), startX, startY, c);
    startY += lineH;
  }
}

void Window::RenderText(const std::string &text, int x, int y,
                        SDL_Color color) {
  if (!font) {
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);

    SDL_FRect textRect = MakeFRect(
        static_cast<float>(x),
        static_cast<float>(y),
        static_cast<float>(text.length() * 8),
        18.0f);

    SDL_RenderRect(renderer, &textRect);
    return;
  }

  SDL_Texture *textTexture = CreateTextTexture(text, color);

  if (textTexture) {
    float textWidth = 0.0f;
    float textHeight = 0.0f;

    SDL_GetTextureSize(textTexture, &textWidth, &textHeight);

    SDL_FRect destRect = MakeFRect(
        static_cast<float>(x),
        static_cast<float>(y),
        textWidth,
        textHeight);

    SDL_RenderTexture(renderer, textTexture, nullptr, &destRect);
    SDL_DestroyTexture(textTexture);
  }
}

SDL_Texture *Window::CreateTextTexture(const std::string &text,
                                       SDL_Color color) {
  if (!font || text.empty()) {
    return nullptr;
  }

  SDL_Surface *surface =
      TTF_RenderText_Blended(font, text.c_str(), text.length(), color);

  if (!surface) {
    std::cerr << "Failed to render text: " << SDL_GetError() << std::endl;
    return nullptr;
  }

  SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);

  SDL_DestroySurface(surface);

  if (!texture) {
    std::cerr << "Failed to create texture from rendered text: "
              << SDL_GetError() << std::endl;
  }

  return texture;
}

Square Window::CoordinatesToSquare(int x, int y) {
  int squareSize = boardSize / 8;

  int file = x / squareSize;
  int rank = 7 - (y / squareSize);

  if (file < 0 || file > 7 || rank < 0 || rank > 7) {
    return NO_SQ;
  }

  return static_cast<Square>(rank * 8 + file);
}

void Window::SquareToCoordinates(Square sq, int &x, int &y) {
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

void Window::HandleBoardClick(int mouseX, int mouseY) {
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

      for (const Move &move : allLegalMoves) {
        if (getFrom(move) == selectedSquare) {
          legalMoves.push_back(move);
        }
      }
    }
  } else {
    for (const Move &move : legalMoves) {
      if (getTo(move) == clickedSquare) {
        AddMoveToHistory(move);
        engine.makeMove(move);

        selectedSquare = NO_SQ;
        isPieceSelected = false;
        legalMoves.clear();

        StartEngineSearch(5000);
        return;
      }
    }

    Color pieceColor;
    PieceType pieceType = engine.getPieceAt(clickedSquare, pieceColor);

    if (pieceType != NONE && pieceColor == engine.getSideToMove()) {
      selectedSquare = clickedSquare;

      std::vector<Move> allLegalMoves = engine.generateLegalMoves();
      legalMoves.clear();

      for (const Move &move : allLegalMoves) {
        if (getFrom(move) == selectedSquare) {
          legalMoves.push_back(move);
        }
      }
    } else {
      selectedSquare = NO_SQ;
      isPieceSelected = false;
      legalMoves.clear();
    }
  }
}

void Window::AddMoveToHistory(Move move) {
  std::string moveNotation = engine.moveToString(move);

  if (engine.getSideToMove() == WHITE) {
    MoveHistoryEntry entry;
    entry.moveNumber = static_cast<int>(moveHistory.size()) + 1;
    entry.whiteMove = moveNotation;
    entry.blackMove = "";
    moveHistory.push_back(entry);
  } else {
    if (!moveHistory.empty()) {
      moveHistory.back().blackMove = moveNotation;
    }
  }
}