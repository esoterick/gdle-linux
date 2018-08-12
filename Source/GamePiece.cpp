#include "StdAfx.h"
#include "GamePiece.h"

void GamePieceWeenie::Move(Position const& to)
{
    MovementParameters params;
    params.desired_heading = to.frame.get_heading();

    MovementStruct mvs;
    mvs.type   = MoveToPosition;
    mvs.pos    = to;
    mvs.params = &params;

    last_move_was_autonomous = false;
    movement_manager->PerformMovement(mvs);
}
