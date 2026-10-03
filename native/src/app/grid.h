// The arithmetic of a grid of cards (the Theme picker): which row and column a card sits in. No graphics, so it can be tested.
#pragma once

namespace app {

struct GridCell { int row, col; };

// How many rows `count` cards need in `columns` columns (rows are added as needed).
inline int gridRows(int count, int columns) { return columns < 1 || count < 1 ? 0 : (count + columns - 1) / columns; }
// The row and column of card number `index` (0, 1, 2, ... left to right, then the next row).
inline GridCell gridCell(int index, int columns) { return columns < 1 ? GridCell{0, 0} : GridCell{index / columns, index % columns}; }

}  // namespace app
