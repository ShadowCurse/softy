#ifndef SOFTY_PRIMITIVES
#define SOFTY_PRIMITIVES

#include "log.h"
#include "math.h"
#include "memory.h"

#include <fcntl.h>
#include <stdlib.h>
#include <sys/stat.h>

typedef struct {
  V2 min;
  V2 max;
} AABB;

f32 aabb_width(AABB *a) { return a->max.x - a->min.x; }
f32 aabb_hight(AABB *a) { return a->max.y - a->min.y; }

u32 aabb_width_u32(AABB *a) {
  return f32_to_u32_round_up(a->max.x) - f32_to_u32_round_down(a->min.x);
}
u32 aabb_hight_u32(AABB *a) {
  return f32_to_u32_round_up(a->max.y) - f32_to_u32_round_down(a->min.y);
}

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
  V3 position;
  V3 normal;
  V2 uv;
} Vertex;

typedef struct {
  V3 v0;
  V3 v1;
  V3 v2;
  Vertex *v0_vertex;
  Vertex *v1_vertex;
  Vertex *v2_vertex;
} Triangle;

typedef enum {
  CW,
  CCW,
  None,
} CullMode;

AABB triangle_aabb(Triangle *triangle) {
  AABB result = {
      .min = {MIN(MIN(triangle->v0.x, triangle->v1.x), triangle->v2.x),
              MIN(MIN(triangle->v0.y, triangle->v1.y), triangle->v2.y)},
      .max = {MAX(MAX(triangle->v0.x, triangle->v1.x), triangle->v2.x),
              MAX(MAX(triangle->v0.y, triangle->v1.y), triangle->v2.y)},
  };
  return result;
}

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
             (v0_position.y + 1.0) / 2.0 * window_hight, v0_position.z},
      .v0_vertex = v0,
      .v1 = {(v1_position.x + 1.0) / 2.0 * window_width,
             (v1_position.y + 1.0) / 2.0 * window_hight, v1_position.z},
      .v1_vertex = v1,
      .v2 = {(v2_position.x + 1.0) / 2.0 * window_width,
             (v2_position.y + 1.0) / 2.0 * window_hight, v2_position.z},
      .v2_vertex = v2,
  };

  return t;
}

typedef struct {
  Vertex *vertices;
  u32 vertices_num;
  u32 *indices;
  u32 indices_num;
} Model;

typedef struct {
  u32 position_index;
  u32 uv_index;
  u32 normal_index;
} ModelFace;

Model load_model(Memory *memory, const char *obj_path) {
  i32 fd = open(obj_path, O_RDONLY);
  ASSERT((0 < fd), "Failed to open font file 2: %s", obj_path);

  struct stat sb;
  ASSERT((fstat(fd, &sb) != -1), "Failed to get a font file %s size", obj_path);

  u8 *file_mem = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT(file_mem, "Failed to mmap font file: %s", obj_path);

#define SKIP_BEYOND(line, x)                                                   \
  while (*line != x) {                                                         \
    line++;                                                                    \
  }                                                                            \
  line++;

  const char *line = (const char *)file_mem;
  char *end;

  u32 positions_num = 0;
  u32 normals_num = 0;
  u32 uvs_num = 0;
  u32 faces_num = 0;

  while (*line) {
    switch (*line) {
    case 'v':
      line++;
      switch (*line) {
      case ' ':
        positions_num++;
        SKIP_BEYOND(line, '\n')
        break;
      case 'n':
        normals_num++;
        SKIP_BEYOND(line, '\n')
        break;
      case 't':
        uvs_num++;
        SKIP_BEYOND(line, '\n')
        break;
      default:
        SKIP_BEYOND(line, '\n')
        break;
      }
      break;
    case 'f':
      faces_num += 3;
      SKIP_BEYOND(line, '\n')
      break;
    default:
      SKIP_BEYOND(line, '\n')
      break;
    }
  }

  V3 *positions = frame_alloc_array(memory, V3, positions_num);
  V3 *normals = frame_alloc_array(memory, V3, normals_num);
  V2 *uvs = frame_alloc_array(memory, V2, uvs_num);
  ModelFace *faces = frame_alloc_array(memory, ModelFace, faces_num);

  line = (const char *)file_mem;

  positions_num = 0;
  normals_num = 0;
  uvs_num = 0;
  faces_num = 0;

  while (*line) {
    switch (*line) {
    case 'v':
      line++;
      switch (*line) {
      case ' ':
        positions[positions_num].x = strtof(line, &end);
        line = end;
        positions[positions_num].y = strtof(line, &end);
        line = end;
        positions[positions_num].z = strtof(line, &end);
        positions_num++;
        SKIP_BEYOND(line, '\n')
        break;
      case 'n':
        line++;
        normals[normals_num].x = strtof(line, &end);
        line = end;
        normals[normals_num].y = strtof(line, &end);
        line = end;
        normals[normals_num].z = strtof(line, &end);
        normals_num++;
        SKIP_BEYOND(line, '\n')
        break;
      case 't':
        line++;
        uvs[uvs_num].x = strtof(line, &end);
        line = end;
        uvs[uvs_num].y = strtof(line, &end);
        uvs_num++;
        SKIP_BEYOND(line, '\n')
        break;
      default:
        SKIP_BEYOND(line, '\n')
        break;
      }
      break;
    case 'f':
      line++;
      faces[faces_num].position_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num].uv_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num].normal_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num + 1].position_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num + 1].uv_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num + 1].normal_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num + 2].position_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num + 2].uv_index = strtoul(line, &end, 10);
      line = end + 1;
      faces[faces_num + 2].normal_index = strtoul(line, &end, 10);
      faces_num += 3;
      SKIP_BEYOND(line, '\n')
      break;
    default:
      SKIP_BEYOND(line, '\n')
      break;
    }
  }

  u32 vertices_num = faces_num;
  Vertex *vertices = perm_alloc_array(memory, Vertex, vertices_num);
  u32 indices_num = faces_num;
  u32 *indices = perm_alloc_array(memory, u32, indices_num);

  for (u32 i = 0; i < faces_num; i++) {
    ModelFace *face = &faces[i];
    vertices[i].position = positions[face->position_index - 1];
    vertices[i].normal = normals[face->normal_index - 1];
    vertices[i].uv = uvs[face->uv_index - 1];
    indices[i] = i;
  }

  munmap(file_mem, sb.st_size);
  INFO("Loaded model %s with %d vertices", obj_path, vertices_num);

  Model model = {
      .vertices = vertices,
      .vertices_num = vertices_num,
      .indices = indices,
      .indices_num = indices_num,
  };

  return model;
}

#endif
