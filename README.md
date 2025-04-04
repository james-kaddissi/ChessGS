The Bitboard is a 64 bit unisgned integer. Each bit represents one square.

The Square enum is an integer repesentation for every square (C++ treats enums as lists of integers)


### 3 Bitboards Representation:

1. **pieces** is a 2d array of 2x6 bitboards that each represent one specific piece from the format [color][type]
2. **colorBitboards** is an array of 2 bitboards that each represent one colors pieces
3. **occupiedSquares** is a single bitboard that represents all pieces on the board
