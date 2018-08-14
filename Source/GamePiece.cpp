#include "StdAfx.h"
#include "GamePiece.h"

void GamePieceWeenie::Move(Position const& to)
{
    MovementParameters params;
    params.distance_to_object = 0.1;
    params.use_spheres        = 0;
    params.use_final_heading  = 1;
    params.desired_heading    = to.frame.get_heading();

    MovementStruct mvs;
    mvs.type   = MoveToPosition;
    mvs.pos    = to;
    mvs.params = &params;

    last_move_was_autonomous = false;
    movement_manager->PerformMovement(mvs);
}
