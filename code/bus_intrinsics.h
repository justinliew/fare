#ifndef __NETRUNNER_INTRINSICS_H_
#define __NETRUNNER_INTRINSICS_H_

#include "math.h"
// TODO - convert these to platform-efficient versions and remove math.h

inline real32 roundf(real32 x)
{
   return x >= 0.0f ? floorf(x + 0.5f) : ceilf(x - 0.5f);
}

inline int32 RoundReal32ToInt32(real32 Value)
{
    return (int32)roundf(Value);
}

inline uint32 RoundReal32ToUInt32(real32 Value)
{
    return (uint32)roundf(Value);
}

inline int32 TruncateReal32ToInt32(real32 Value)
{
    return (int32)Value;
}

inline uint32 TruncateReal32ToUInt32(real32 Value)
{
    return (uint32)Value;
}

inline int32 FloorReal32ToInt32(real32 Value)
{
    return (int32)floorf(Value);
}

inline uint32 FloorReal32ToUInt32(real32 Value)
{
    return (uint32)floorf(Value);
}

inline real32 Sin(real32 Angle)
{
	return sinf(Angle);
}

inline real32 Cos(real32 Angle)
{
	return cosf(Angle);
}

inline real32 Atan2(real32 Y, real32 X)
{
	return atan2f(Y,X);
}

#endif
