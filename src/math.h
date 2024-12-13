#include "defines.h"

#define MIN(a, b) a < b ? a : b
#define MAX(a, b) a < b ? b : a

f32 lerp(f32 a, f32 b, f32 t) { return a * (1.0 - t) + b * t; }

typedef struct {
  f32 x;
  f32 y;
} V2;

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

f32 dot(V2 a, V2 b) { return a.x * b.x + a.y * b.y; }

f32 perp_dot(V2 a, V2 b) { return a.x * b.y - a.y * b.x; }

typedef struct {
  V2 min;
  V2 max;
} AABB;

f32 aabb_width(AABB *a) { return a->max.x - a->min.x; }

f32 aabb_hight(AABB *a) { return a->max.y - a->min.y; }

AABB aabb_from_parts(V2 center, V2 dim) {
  AABB result = {
      .min = {center.x - dim.x / 2.0, center.y - dim.y / 2.0},
      .max = {center.x + dim.x / 2.0, center.y + dim.y / 2.0},
  };
  return result;
}

bool aabb_intersect(AABB *a, AABB *b) {
  return !(a->max.x < b->min.x || b->max.x < a->min.x || b->max.y < a->min.y ||
           a->max.y < b->min.y);
}

AABB aabb_intersection(AABB *a, AABB *b) {
  AABB result = {
      .min =
          {
              MAX(a->min.x, b->min.x),
              MAX(a->min.y, b->min.y),
          },
      .max =
          {
              MIN(a->max.x, b->max.x),
              MIN(a->max.y, b->max.y),
          },
  };

  return result;
}

typedef struct {
  V2 pos;
  f32 width;
  f32 hight;
} Rect;

AABB rect_aabb(Rect *rect) {
  AABB aabb = {
      .min = {rect->pos.x - rect->width / 2.0, rect->pos.y - rect->hight / 2.0},
      .max = {rect->pos.x + rect->width / 2.0, rect->pos.y + rect->hight / 2.0},
  };
  return aabb;
}

typedef struct {
  V2 v0;
  V2 v1;
  V2 v2;
} Triangle;

AABB triangle_aabb(Triangle *triangle) {
  AABB result = {
      .min = {MIN(MIN(triangle->v0.x, triangle->v1.x), triangle->v2.x),
              MIN(MIN(triangle->v0.y, triangle->v1.y), triangle->v2.y)},
      .max = {MAX(MAX(triangle->v0.x, triangle->v1.x), triangle->v2.x),
              MAX(MAX(triangle->v0.y, triangle->v1.y), triangle->v2.y)},
  };
  return result;
}
