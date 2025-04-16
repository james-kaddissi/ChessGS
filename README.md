The Bitboard is a 64 bit unisgned integer. Each bit represents one square.

The Square enum is an integer repesentation for every square (C++ treats enums as lists of integers)

### 3 Bitboards Representation:

1. **pieces** is a 2d array of 2x6 bitboards that each represent one specific piece from the format [color][type]
2. **colorBitboards** is an array of 2 bitboards that each represent one colors pieces
3. **occupiedSquares** is a single bitboard that represents all pieces on the board

### SDL2 For Graphics

window.h and window.cpp contains all code for handling the GUI


### Missing from Core Implementation:
- Special rules:
-- Checkmate (although the game does stop its not a proper "end" right now)
-- Draw
-- Insufficient Material
-- 50 Move Rule
-- Threefold Repitition

- Protocols for Connection/Testings:
-- FEN String support (needed for testing and certain apis)
-- UCI Handler (needed for most chess related libraries and tools)

- Time Control