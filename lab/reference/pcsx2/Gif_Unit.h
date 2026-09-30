#pragma once
#include "Common.h"
enum GIF_PATH { GIF_PATH_1=0 };
enum GIF_TRANSFER_TYPE { GIF_TRANS_XGKICK=0 };
enum { GIF_FLG_PACKED=0,GIF_FLG_REGLIST=1,GIF_FLG_IMAGE=2,GIF_FLG_IMAGE2=3,GIF_REG_A_D=14 };
// These slices are extracted byte-for-byte from the pinned upstream header.
#include "Gif_Tag.inc"
#include "Gif_incTag.inc"
struct ReferenceGifUnit
{
#include "Gif_GetGSPacketSize.inc"
    u32 TransferGSPacketData(GIF_TRANSFER_TYPE type,u8* memory,u32 size,bool aligned);
};
extern ReferenceGifUnit gifUnit;
