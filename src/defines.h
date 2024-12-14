#ifndef SOFTY_DEFINES
#define SOFTY_DEFINES

#include <stdint.h>

#define bool uint8_t
#define true 1
#define false 0

#define u8 uint8_t
#define u16 uint16_t
#define u32 uint32_t
#define u64 uint64_t

#define i8 int8_t
#define i16 int16_t
#define i32 int32_t
#define i64 int64_t

#define f32 float
#define f64 double

#define NS_PER_SEC 1000 * 1000 * 1000

#define FPS 60
#define FRAME_TIME_S 1.0 / FPS
#define FRAME_TIME_NS FRAME_TIME_S *NS_PER_SEC

#endif
