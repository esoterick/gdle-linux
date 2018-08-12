#ifndef GAME_PIECE_H
#define GAME_PIECE_H

#include "WeenieObject.h"
#include "Frame.h"

class GamePieceWeenie : public CWeenieObject
{
public:
    GamePieceWeenie* AsGamePiece() override { return this; }

    void Move(Position const& to);
};

#endif // GAME_PIECE_H
