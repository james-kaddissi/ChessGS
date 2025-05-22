# ChessGS

A bitboard-based chess engine in C++ with an SDL2 GUI.
UCI-compatible, currently plays at a level above amateurs. I lose most games.

The longer-term goal of this project is to use it as a research vehicle for
testing whether learned positional heuristics can add measurable strength on top of a classical alpha-beta search.

## Features

- Bitboard move generation, fully legal.
- Iterative deepening alpha-beta with quiescence search.
- Transposition table with mate-distance correction and depth-preferred
  replacement.
- Null-move pruning, killer moves, history heuristic, MVV-LVA capture
  ordering, per-move delta pruning in qsearch.
- Tapered evaluation: material + PSTs (mg/eg), pawn structure, king safety,
  mobility, bishop-pair bonus, early-queen development penalty.
- Repetition and insufficient-material draw detection.
- Polyglot opening book reader (see caveat below).
- UCI loop and a basic SDL2 GUI for play against the engine.
- Self-play harness for testing.

## Running

```bash
chessgs                    # start GUI
chessgs uci                # UCI mode
chessgs perft 5            # perft to depth 5 from startpos
chessgs selfplay 10 6      # 10 self-play games at depth 6
chessgs selfplay 4 0 time 1000   # 4 games, 1000ms per move
chessgs benchmark          # node count / NPS over fixed positions
chessgs testsuite tests.epd
```

## Known issues / things I haven't gotten to

- **No 50-move rule.** `PositionManager` doesn't track the halfmove clock,
  so this isn't enforced. Won't matter in normal play but will let the
  engine miss draw claims in pathological endgames.
- **Polyglot zobrist mismatch.** The engine's hashing uses a custom seed
  rather than the published Polyglot scheme, so real `.bin` book files will
  load fine but probe lookups almost never hit. Plain text books work.
  Fixing this means maintaining two hashes in parallel — TODO.
- **No SEE in qsearch.** Capture ordering is MVV-LVA only, which means
  the engine sometimes chases bad captures during quiescence. Adding
  static exchange evaluation is the obvious next step.
- **No aspiration windows or LMR.** Both standard, both worth maybe
  50–100 Elo each, neither implemented yet.
- **Time management is naive.** Allocates a flat fraction of remaining
  time per move without considering position complexity. Fine for
  fixed-time-per-move modes; worse for tournament time controls.
- **No multi-threaded search.** Single-threaded only. Lazy SMP would be
  the natural fit but is non-trivial.
- **GUI is minimal.** Drag-and-drop works, move highlighting works, but
  there's no analysis panel, no PV display, no eval graph. The UCI mode
  paired with an external GUI is the better experience for serious use.


## Learned heuristic component

The idea is a set of small specialized models, each trained on a narrow
task that the engine genuinely can't compute cheaply itself:

- **Eval correction.** Predict the delta between shallow and deep eval, so
  the engine can apply a learned correction without searching deeper. This
  is the most engine-relevant of the five and where I'd expect the most
  measurable gain.
- **Position-type embeddings.** Contrastive encoder mapping positions to a
  shared embedding space, used as a learned plan/structure lookup.
- **Fortress detection.** Binary classifier flagging positions where
  material advantage isn't actually convertible.
- **Prophylaxis / opponent threat prediction.** What the opponent wants to
  do, used to bias root move ordering toward prophylactic moves.
- **Long-horizon plan prediction.** Predict the position 10 plies ahead
  under best play, used as a strategic anchor.

Each is a separately-trained head, much smaller than a general-purpose LLM
(target 10–200M params each). They serve via a persistent Python HTTP
service, queried from C++ via libcurl with a hard timeout. The engine
treats their outputs as advisory bonuses on root move scores never as
the primary decision-maker.

Each head will be independently toggleable through UCI options
(`UseEvalCorrection`, `UseFortressDetection`, etc.) so they can be A/B
tested in match play. The integration is designed around the assumption
that some of them will work and some won't, and we'll know which from
match results, not from training metrics.