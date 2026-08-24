/**
file bgroomtrans.c
*/

#include <ultra64.h>
#include "player.h"
#include "bg.h"
#include "bgroomtrans.h"
#include "matrixmath.h"


#define MAX_ROOM_MATRIX_SLOTS 300

u8 roomStatusFlags[MAX_ROOM_MATRIX_SLOTS];

s32 roomIndices[MAX_ROOM_MATRIX_SLOTS];
s32 roomOwners[MAX_ROOM_MATRIX_SLOTS];
Mtx roomMatrices[MAX_ROOM_MATRIX_SLOTS];


/**
 * Initialize room and player-related data structures.
 * Sets all rooms and players to an initial state.
 */
void initializeRoomData(void)
{
    s32 i;

    for (i = 0; i < getPlayerCount(); i++)
    {
        g_playerPointers[i]->curRoomIndex = -1;
    }

    for (i = 0; i < MAX_ROOM_MATRIX_SLOTS; i++)
    {
      roomIndices[i] = -1;
      roomStatusFlags[i] = 2;

      roomOwners[i] = -1;
    }

    for (i = 0; i < bgGetMaxNumRooms(); ++i)
    {
        g_BgRoomInfo[i].mtxid = -1;
    }
}


/**
 * Set the player's room.
 */
void setPlayerRoom(s32 roomIndex)
{
  g_CurrentPlayer->curRoomIndex = roomIndex;
}


/**
 * Assigns a room index to a specific room ID.
 */
void assignRoomIndexToRoomID(s32 mtx, s32 room)
{
    g_BgRoomInfo[room].mtxid = mtx;
    roomIndices[mtx] = room;
}


/**
 * Removes the room index assignment for a specific room ID.
 */
void removeRoomIndexFromRoomID(s32 mtx, s32 room)
{
    g_BgRoomInfo[room].mtxid = -1;
    roomIndices[mtx] = -1;
}


/**
 * Resets a room's state to its initial condition.
 */
void resetRoomState(s32 roomIndex)
{
    if (roomIndices[roomIndex] != -1) 
    {
        removeRoomIndexFromRoomID(roomIndex,roomIndices[roomIndex]);
    }

    roomStatusFlags[roomIndex] = 2;
    roomOwners[roomIndex] = -1;
}


/**
 * Finds and returns the first available room index.
 * Returns 0 if no available room is found.
 */
s32 findAvailableRoomIndex(void)
{
    s32 i;

    for (i = 0; i < MAX_ROOM_MATRIX_SLOTS; i++)
    {
        if (((s32) roomStatusFlags[i] >= 2) && (roomOwners[i] == -1))
        {
            return i;
        }
    }

    return 0;
}


/**
 * Updates the status flags for rooms, resetting those that are inactive.
 */
void updateRoomStatusFlags(void)
{
    s32 i;

    for(i = 0; i < MAX_ROOM_MATRIX_SLOTS; ++i)
    {
        if (roomOwners[i] > -1)
        {
            roomStatusFlags[i]++;

            if (roomStatusFlags[i] >= 2)
            {
                resetRoomState(i);
            }
        }
    }
}


/**
 * Manages room index allocation and matrix setup for a given room.
 */
s32 setupRoomTransformationMatrix(s32 room)
{
    s32 mtx;
    Mtxf roomTransformMatrix;

    mtx = g_BgRoomInfo[room].mtxid;

    if ((mtx == -1) || (g_CurrentPlayer->curRoomIndex != roomOwners[mtx]))
    {
        if (mtx != -1)
        {
            removeRoomIndexFromRoomID(mtx, room);
        }

        mtx = findAvailableRoomIndex();
        assignRoomIndexToRoomID(mtx, room);

        roomStatusFlags[mtx] = 0;
    }
    else
    {
        roomStatusFlags[mtx] = 0;

        return mtx;
    }

    roomOwners[mtx] = g_CurrentPlayer->curRoomIndex;

    matrix_4x4_set_identity(&roomTransformMatrix);

    // set room size according to level scaling
    roomTransformMatrix.m[0][0] = g_LevelInverseScale;
    roomTransformMatrix.m[1][1] = g_LevelInverseScale;
    roomTransformMatrix.m[2][2] = g_LevelInverseScale;

    // room translation to position it relative to the player
    roomTransformMatrix.m[3][0] = (ptr_bgdata_room_fileposition_list[room].pos.f[0] * g_LevelInverseScale) - g_CurrentPlayer->current_model_pos.f[0];
    roomTransformMatrix.m[3][1] = (ptr_bgdata_room_fileposition_list[room].pos.f[1] * g_LevelInverseScale) - g_CurrentPlayer->current_model_pos.f[1];
    roomTransformMatrix.m[3][2] = (ptr_bgdata_room_fileposition_list[room].pos.f[2] * g_LevelInverseScale) - g_CurrentPlayer->current_model_pos.f[2];

    matrix_4x4_f32_to_s32(&roomTransformMatrix, &roomMatrices[mtx]);

    return mtx;
}


/**
 * Updates the display list with the room matrix for a specific room roomID.
 */
Gfx * applyRoomMatrixToDisplayList(Gfx *gdl, s32 roomID)
{
    s32 roomIndex;

    roomIndex = setupRoomTransformationMatrix(roomID);
    gSPMatrix(gdl++, &roomMatrices[roomIndex], G_MTX_MODELVIEW|G_MTX_LOAD|G_MTX_NOPUSH);

    return gdl;
}


/**
 * Returns the position of a room by its roomID.
 */
struct coord3d* getRoomPositionByIndex(s32 roomID)
{
    return &ptr_bgdata_room_fileposition_list[roomID].pos;
}


/**
 * Retrieves and scales the position of a room by its roomID.
 */
void getRoomPositionScaledByIndex(s32 roomID, coord3d *scaledPos)
{
    scaledPos->x = ptr_bgdata_room_fileposition_list[roomID].pos.x * g_LevelInverseScale;
    scaledPos->y = ptr_bgdata_room_fileposition_list[roomID].pos.y * g_LevelInverseScale;
    scaledPos->z = ptr_bgdata_room_fileposition_list[roomID].pos.z * g_LevelInverseScale;
}
