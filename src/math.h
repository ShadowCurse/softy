#ifndef SOFTY_MATH
#define SOFTY_MATH

#include "defines.h"
#include <math.h>

#define MIN(a, b) (a < b ? a : b)
#define MAX(a, b) (a < b ? b : a)

f32 lerp(f32 a, f32 b, f32 t) { return a * (1.0 - t) + b * t; }

typedef struct {
  f32 x;
  f32 y;
} V2;

static inline f32 v2_dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }

static inline f32 v2_perp_dot(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }

static inline f32 v2_len_sq(V2 a) { return v2_dot(a, a); }

static inline V2 v2_add(V2 a, V2 b) {
  V2 result = {
      .x = a.x + b.x,
      .y = a.y + b.y,
  };
  return result;
}

static inline V2 v2_sub(V2 a, V2 b) {
  V2 result = {
      .x = a.x - b.x,
      .y = a.y - b.y,
  };
  return result;
}

static inline V2 v2_mul(V2 a, f32 v) {
  V2 result = {
      .x = a.x * v,
      .y = a.y * v,
  };
  return result;
}

typedef struct {
  union {
    struct {
      f32 x;
      f32 y;
      f32 z;
    };
    struct {
      V2 xy;
      f32 _z;
    };
    struct {
      f32 _x;
      V2 yz;
    };
    f32 v[3];
  };
} V3;

static inline f32 v3_dot(V3 a, V3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline f32 v3_len_sq(V3 a) { return v3_dot(a, a); }

static inline V3 v3_add(V3 a, V3 b) {
  V3 result = {
      .x = a.x + b.x,
      .y = a.y + b.y,
      .z = a.z + b.z,
  };
  return result;
}

static inline V3 v3_sub(V3 a, V3 b) {
  V3 result = {
      .x = a.x - b.x,
      .y = a.y - b.y,
      .z = a.z - b.z,
  };
  return result;
}

static inline V3 v3_mul(V3 a, f32 v) {
  V3 result = {
      .x = a.x * v,
      .y = a.y * v,
      .z = a.z * v,
  };
  return result;
}

static inline V3 v3_div(V3 a, f32 v) {
  V3 result = {
      .x = a.x / v,
      .y = a.y / v,
      .z = a.z / v,
  };
  return result;
}

typedef struct {
  f32 x;
  f32 y;
  f32 z;
  f32 w;
} V4;

static inline V4 v3_to_v4(V3 a, f32 w) {
  V4 result = {
      .x = a.x,
      .y = a.y,
      .z = a.z,
      .w = w,
  };
  return result;
}

static inline V3 v4_to_v3(V4 a) {
  V3 result = {
      .x = a.x,
      .y = a.y,
      .z = a.z,
  };
  return result;
}

static inline V4 v4_add(V4 a, V4 b) {
  V4 result = {
      .x = a.x + b.x,
      .y = a.y + b.y,
      .z = a.z + b.z,
      .w = a.w + b.w,
  };
  return result;
}

static inline V4 v4_sub(V4 a, V4 b) {
  V4 result = {
      .x = a.x - b.x,
      .y = a.y - b.y,
      .z = a.z - b.z,
      .w = a.w - b.w,
  };
  return result;
}

static inline V4 v4_mul(V4 a, f32 v) {
  V4 result = {
      .x = a.x * v,
      .y = a.y * v,
      .z = a.z * v,
      .w = a.w * v,
  };
  return result;
}

static inline V4 v4_div(V4 a, f32 v) {
  V4 result = {
      .x = a.x / v,
      .y = a.y / v,
      .z = a.z / v,
      .w = a.w / v,
  };
  return result;
}

static inline f32 v4_dot(V4 a, V4 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}

// Column based
typedef struct {
  V4 i;
  V4 j;
  V4 k;
  V4 t;
} Mat4;

static inline Mat4 mat4_idendity() {
  Mat4 result = {
      .i = {1.0, 0.0, 0.0, 0.0},
      .j = {0.0, 1.0, 0.0, 0.0},
      .k = {0.0, 0.0, 1.0, 0.0},
      .t = {0.0, 0.0, 0.0, 1.0},
  };
  return result;
}

static inline void mat4_translate(Mat4 *m, V3 translation) {
  m->t = v4_add(m->t, (V4){translation.x, translation.y, translation.z, 0.0});
}

static inline Mat4 mat4_perspective(f32 fovy, f32 aspect, f32 near, f32 far) {
  f32 g = 1.0 / tan(fovy / 2.0);
  f32 k = near / (near - far);
  Mat4 result = {
      .i = {.x = -g / aspect},
      .j = {.y = -g},
      .k = {.z = k, .w = far * k},
      .t = {.z = 1.0},
  };
  return result;
}

static inline Mat4 mat4_perspective_inf(f32 fovy, f32 aspect, f32 near) {
  f32 g = 1.0 / tan(fovy / 2.0);
  f32 e = 0.00000001;
  Mat4 result = {
      .i = {.x = g / aspect},
      .j = {.y = g},
      .k = {.z = e, .w = near * (1.0 - e)},
      .t = {.z = 1.0},
  };
  return result;
}

static inline Mat4 mat4_rotation(V3 axis, f32 angle) {
  f32 c = cos(angle);
  f32 s = sin(angle);
  f32 t = 1.0 - c;

  f32 sqr_norm = v3_len_sq(axis);
  if (sqr_norm == 0.0) {
    return mat4_idendity();
  } else if (fabs(sqr_norm - 1.0) > 0.0001) {
    f32 norm = sqrt(sqr_norm);
    return mat4_rotation(v3_div(axis, norm), angle);
  }

  f32 x = axis.x;
  f32 y = axis.y;
  f32 z = axis.z;

  Mat4 result = {
      .i = {.x = x * x * t + c,
            .y = y * x * t + z * s,
            .z = z * x * t - y * s,
            .w = 0.0},
      .j = {.x = x * y * t - z * s,
            .y = y * y * t + c,
            .z = z * y * t + x * s,
            .w = 0.0},
      .k = {.x = x * z * t + y * s,
            .y = y * z * t - x * s,
            .z = z * z * t + c,
            .w = 0.0},
      .t = {.x = 0.0, .y = 0.0, .z = 0.0, .w = 1.0},
  };
  return result;
}

static inline V4 v4_mul_mat4(V4 v, Mat4 *m) {
  V4 result = {
      .x = v4_dot(m->i, v),
      .y = v4_dot(m->j, v),
      .z = v4_dot(m->k, v),
      .w = v4_dot(m->t, v),
  };
  return result;
}

static inline V4 mat4_mul_v4(Mat4 *m, V4 b) {
  V4 result = {
      .x = m->i.x * b.x + m->j.x * b.y + m->k.x * b.z + m->t.x * b.w,
      .y = m->i.y * b.x + m->j.y * b.y + m->k.y * b.z + m->t.y * b.w,
      .z = m->i.z * b.x + m->j.z * b.y + m->k.z * b.z + m->t.z * b.w,
      .w = m->i.w * b.x + m->j.w * b.y + m->k.w * b.z + m->t.w * b.w,
  };
  return result;
}

static inline Mat4 mat4_mul(Mat4 *a, Mat4 *b) {
  Mat4 result =
      {
          .i =
              {
                  .x = b->i.x * a->i.x + b->i.y * a->j.x + b->i.z * a->k.x +
                       b->i.w * a->t.x,
                  .y = b->i.x * a->i.y + b->i.y * a->j.y + b->i.z * a->k.y +
                       b->i.w * a->t.y,
                  .z = b->i.x * a->i.z + b->i.y * a->j.z + b->i.z * a->k.z +
                       b->i.w * a->t.z,
                  .w = b->i.x * a->i.w + b->i.y * a->j.w + b->i.z * a->k.w +
                       b->i.w * a->t.w,
              },
          .j =
              {
                  .x = b->j.x * a->i.x + b->j.y * a->j.x + b->j.z * a->k.x +
                       b->j.w * a->t.x,
                  .y = b->j.x * a->i.y + b->j.y * a->j.y + b->j.z * a->k.y +
                       b->j.w * a->t.y,
                  .z = b->j.x * a->i.z + b->j.y * a->j.z + b->j.z * a->k.z +
                       b->j.w * a->t.z,
                  .w = b->j.x * a->i.w + b->j.y * a->j.w + b->j.z * a->k.w +
                       b->j.w * a->t.w,
              },
          .k =
              {
                  .x = b->k.x * a->i.x + b->k.y * a->j.x + b->k.z * a->k.x +
                       b->k.w * a->t.x,
                  .y = b->k.x * a->i.y + b->k.y * a->j.y + b->k.z * a->k.y +
                       b->k.w * a->t.y,
                  .z = b->k.x * a->i.z + b->k.y * a->j.z + b->k.z * a->k.z +
                       b->k.w * a->t.z,
                  .w = b->k.x * a->i.w + b->k.y * a->j.w + b->k.z * a->k.w +
                       b->k.w * a->t.w,
              },
          .t =
              {
                  .x = b->t.x * a->i.x + b->t.y * a->j.x + b->t.z * a->k.x +
                       b->t.w * a->t.x,
                  .y = b->t.x * a->i.y + b->t.y * a->j.y + b->t.z * a->k.y +
                       b->t.w * a->t.y,
                  .z = b->t.x * a->i.z + b->t.y * a->j.z + b->t.z * a->k.z +
                       b->t.w * a->t.z,
                  .w = b->t.x * a->i.w + b->t.y * a->j.w + b->t.z * a->k.w +
                       b->t.w * a->t.w,
              },
      };
  return result;
}

#endif
