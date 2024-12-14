#ifndef SOFTY_PRIMITIVES
#define SOFTY_PRIMITIVES

#include "log.h"
#include "math.h"
#include "memory.h"

#include <fcntl.h>
#include <sys/stat.h>

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

typedef struct {
  V3 position;
} Vertex;

Triangle vertices_to_triangle(Vertex *v0, Vertex *v1, Vertex *v2, Mat4 *mvp,
                              f32 window_width, f32 window_hight) {

  V4 v0_position = v3_to_v4(v0->position, 1.0);
  v0_position = mat4_mul_v4(mvp, v0_position);
  v0_position = v4_div(v0_position, v0_position.w);

  V4 v1_position = v3_to_v4(v1->position, 1.0);
  v1_position = mat4_mul_v4(mvp, v1_position);
  v1_position = v4_div(v1_position, v1_position.w);

  V4 v2_position = v3_to_v4(v2->position, 1.0);
  v2_position = mat4_mul_v4(mvp, v2_position);
  v2_position = v4_div(v2_position, v2_position.w);

  Triangle t = {
      .v0 = {(v0_position.x + 1.0) / 2.0 * window_width,
             (v0_position.y + 1.0) / 2.0 * window_hight},
      .v1 = {(v1_position.x + 1.0) / 2.0 * window_width,
             (v1_position.y + 1.0) / 2.0 * window_hight},
      .v2 = {(v2_position.x + 1.0) / 2.0 * window_width,
             (v2_position.y + 1.0) / 2.0 * window_hight},
  };

  return t;
}

#endif
