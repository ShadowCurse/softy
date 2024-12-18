#include "SDL2/SDL_surface.h"
#include "defines.h"
#include "log.h"
#include "math.h"
#include "memory.h"
#include "primitives.h"
#include <SDL2/SDL.h>

#include "stb_image.h"
#include "stb_truetype.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>

#define WINDOW_WIDTH 1280
#define WINDOW_HIGHT 720

typedef struct {
  stbtt_bakedchar *char_info;
  u8 *bitmap;
  u32 bitmap_width;
  u32 bitmap_hight;
} Font;

Font load_font(Memory *memory, const char *font_path, f32 font_size,
               u32 bitmap_width, u32 bitmap_hight) {
  i32 fd = open(font_path, O_RDONLY);
  ASSERT((0 < fd), "Failed to open font file 2: %s", font_path);

  struct stat sb;
  ASSERT((fstat(fd, &sb) != -1), "Failed to get a font file %s size",
         font_path);

  u8 *file_mem = mmap(NULL, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
  ASSERT(file_mem, "Failed to mmap font file: %s", font_path);

  stbtt_fontinfo stb_font;
  stbtt_InitFont(&stb_font, file_mem, stbtt_GetFontOffsetForIndex(file_mem, 0));

  Font font = {
      .char_info =
          perm_alloc_array(memory, stbtt_bakedchar, stb_font.numGlyphs),
      .bitmap = perm_alloc_array(memory, u8, bitmap_width * bitmap_hight),
      .bitmap_width = bitmap_width,
      .bitmap_hight = bitmap_hight,
  };

  stbtt_BakeFontBitmap(file_mem, 0, font_size, font.bitmap, bitmap_width,
                       bitmap_hight, 0, stb_font.numGlyphs, font.char_info);

  munmap(file_mem, sb.st_size);

  INFO("Loaded font %s with %d glyphs", font_path, stb_font.numGlyphs);

  return font;
}

typedef struct {
  u32 width;
  u32 hight;
  u32 channels;
  u8 *data;
} BitMap;

BitMap load_bitmap(Memory *memory, const char *filename) {
  i32 x;
  i32 y;
  i32 n;
  u8 *data = stbi_load(filename, &x, &y, &n, 0);
  ASSERT(data, "Failed to load bitmap from %s", filename);

  BitMap bm = {
      .width = (u32)x,
      .hight = (u32)y,
      .channels = (u32)n,
      .data = perm_alloc_array(memory, u8, x * y * n),
  };
  memcpy(bm.data, data, x * y * n);

  stbi_image_free(data);

  return bm;
}

Rect bitmap_full_rect(BitMap *bm) {
  Rect r = {
      .pos = {bm->width / 2.0, bm->hight / 2.0},
      .width = bm->width,
      .hight = bm->hight,
  };
  return r;
}

// Set `dst` bitmap region `rect_dst` at (rect.x, rect.y) position to `color`
// assuming `dst` top left corner is at (0,0)
void blit_color_rect(BitMap *dst, Rect *rect_dst, u32 color, Rect *rect) {
  ASSERT((rect_dst->width <= dst->width), "Invalid blit dest rect");
  ASSERT((rect_dst->hight <= dst->hight), "Invalid blit dest rect");

  AABB aabb_src = rect_aabb(rect);
  AABB aabb_dst = rect_aabb(rect_dst);
  if (!aabb_intersect(&aabb_src, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_src, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 src_start_offset = v2_sub(intersection.min, aabb_src.min);
  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);

  u8 *dst_start = dst->data + (u32)dst_start_offset.x * dst->channels +
                  (u32)dst_start_offset.y * (dst->width * dst->channels);
  for (u32 y = 0; y < copy_area_hight; y++) {
    u32 *dst_row = (u32 *)(dst_start + y * (dst->width * dst->channels));
    u32 count = copy_area_width;
    while (count--)
      *dst_row++ = color;
  }
}

// Copy `src` bitmap region `rect_src` into a `dst` bitmap region `rect_dst`
// at (pos_x, pos_y) position assuming `dst` top left corner is at (0,0)
// Apply tint when used with 1 channel src
void blit_bitmap(BitMap *dst, Rect *rect_dst, BitMap *src, Rect *rect_src,
                 V2 pos, u32 tint) {
  AABB aabb_src;
  u8 *src_start;
  if (rect_src) {
    ASSERT((rect_src->width <= src->hight), "Invalid blit rect_src");
    ASSERT((rect_src->hight <= src->hight), "Invalid blit rect_src");
    aabb_src = aabb_from_parts(pos, (V2){rect_src->width, rect_src->hight});
    src_start = src->data + (u32)(rect_src->pos.x - rect_src->width / 2.0) +
                (u32)(rect_src->pos.y - rect_src->hight / 2.0) * src->width *
                    src->channels;
  } else {
    aabb_src = aabb_from_parts(pos, (V2){src->width, src->hight});
    src_start = src->data;
  }

  AABB aabb_dst;
  u8 *dst_start;
  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");
    aabb_dst = rect_aabb(rect_dst);
    dst_start = dst->data + (u32)(rect_dst->pos.x - rect_dst->width / 2.0) +
                (u32)(rect_dst->pos.y - rect_dst->hight / 2.0) * dst->width *
                    dst->channels;
  } else {
    aabb_dst = (AABB){{0.0, 0.0}, {dst->width, dst->hight}};
    dst_start = dst->data;
  }

  if (!aabb_intersect(&aabb_src, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_src, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 src_start_offset = v2_sub(intersection.min, aabb_src.min);
  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);

  src_start += (u32)src_start_offset.x * src->channels +
               (u32)src_start_offset.y * (src->width * src->channels);

  dst_start += (u32)dst_start_offset.x * dst->channels +
               (u32)dst_start_offset.y * (dst->width * dst->channels);

  f32 tint_mul_a = (f32)((tint >> 24) & 0xFF) / 255.0;
  f32 tint_mul_r = (f32)((tint >> 16) & 0xFF) / 255.0;
  f32 tint_mul_g = (f32)((tint >> 8) & 0xFF) / 255.0;
  f32 tint_mul_b = (f32)((tint >> 0) & 0xFF) / 255.0;

  if (src->channels == dst->channels)
    for (u32 y = 0; y < copy_area_hight; y++) {
      u8 *src_row = src_start + y * (src->width * src->channels);
      u8 *dst_row = dst_start + y * (dst->width * dst->channels);
      for (u32 x = 0; x < copy_area_width; x++) {
        u32 *src_color = (u32 *)(src_row + x * src->channels);
        f32 src_a = (f32)((*src_color >> 24) & 0xFF) / 255.0;
        f32 src_r = (f32)((*src_color >> 16) & 0xFF);
        f32 src_g = (f32)((*src_color >> 8) & 0xFF);
        f32 src_b = (f32)((*src_color >> 0) & 0xFF);

        src_a *= tint_mul_a;
        src_r *= tint_mul_r;
        src_g *= tint_mul_g;
        src_b *= tint_mul_b;

        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        f32 dst_r = (f32)((*dst_color >> 16) & 0xFF);
        f32 dst_g = (f32)((*dst_color >> 8) & 0xFF);
        f32 dst_b = (f32)((*dst_color >> 0) & 0xFF);

        u32 out_r = (u32)(lerp(dst_r, src_r, src_a));
        u32 out_g = (u32)(lerp(dst_g, src_g, src_a));
        u32 out_b = (u32)(lerp(dst_b, src_b, src_a));

        *dst_color = out_r << 16 | out_g << 8 | out_b << 0;
      }
    }
  else if (src->channels == 1 && dst->channels == 4)
    for (u32 y = 0; y < copy_area_hight; y++) {
      u8 *src_row = src_start + y * (src->width * src->channels);
      u8 *dst_row = dst_start + y * (dst->width * dst->channels);
      for (u32 x = 0; x < copy_area_width; x++) {
        u8 src_color = *(src_row + x);
        f32 src_a = (f32)(src_color) / 255.0;
        f32 src_r = src_color;
        f32 src_g = src_color;
        f32 src_b = src_color;

        src_a *= tint_mul_a;
        src_r *= tint_mul_r;
        src_g *= tint_mul_g;
        src_b *= tint_mul_b;

        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        f32 dst_r = (f32)((*dst_color >> 16) & 0xFF);
        f32 dst_g = (f32)((*dst_color >> 8) & 0xFF);
        f32 dst_b = (f32)((*dst_color >> 0) & 0xFF);

        u32 out_r = (u32)(lerp(dst_r, src_r, src_a));
        u32 out_g = (u32)(lerp(dst_g, src_g, src_a));
        u32 out_b = (u32)(lerp(dst_b, src_b, src_a));

        // *(dst_row + x) = a << 24 | r << 16 | g << 8 | b << 0;
        *dst_color = out_r << 16 | out_g << 8 | out_b << 0;
      }
    }
  else
    ASSERT(false,
           "No implementation for blit_bitmap from src %d channels to dst %d "
           "channels",
           src->channels, dst->channels);
}

void draw_aabb(BitMap *dst, Rect *rect_dst, AABB *aabb, u32 color) {
  AABB aabb_dst;
  u8 *dst_start;
  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");
    aabb_dst = rect_aabb(rect_dst);
    dst_start = dst->data + (u32)(rect_dst->pos.x - rect_dst->width / 2.0) +
                (u32)(rect_dst->pos.y - rect_dst->hight / 2.0) * dst->width *
                    dst->channels;
  } else {
    aabb_dst = (AABB){{0.0, 0.0}, {dst->width, dst->hight}};
    dst_start = dst->data;
  }

  if (!aabb_intersect(aabb, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(aabb, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);
  dst_start += (u32)dst_start_offset.x * dst->channels +
               (u32)dst_start_offset.y * (dst->width * dst->channels);

  for (u32 y = 0; y < copy_area_hight; y++) {
    u8 *dst_row = dst_start + y * (dst->width * dst->channels);
    if (y == 0 || y == copy_area_hight - 1) {
      for (u32 x = 0; x < copy_area_width; x++) {
        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        *dst_color = color;
      }
    } else {
      u32 *dst_color = (u32 *)(dst_row);
      *dst_color = color;

      dst_color += copy_area_width;
      *dst_color = color;
    }
  }
}

bool triangle_ccw(Triangle *triangle) {
  f32 area =
      0.5 *
      ((triangle->v2.x - triangle->v0.x) * (triangle->v1.y - triangle->v0.y) -
       (triangle->v1.x - triangle->v0.x) * (triangle->v2.y - triangle->v0.y));
  return 0.0 < area;
}

V3 calculate_interpolation(Triangle *triangle, V2 p) {
#if 1
  f32 total_area = 0.5 * v3_len(v3_cross(v3_sub(triangle->v2, triangle->v0),
                                         v3_sub(triangle->v1, triangle->v0)));
  f32 u =
      (p.x * (triangle->v0.y - triangle->v2.y) +
       p.y * (triangle->v2.x - triangle->v0.x) +
       (triangle->v0.x * triangle->v2.y - triangle->v2.x * triangle->v0.y)) /
      (2.0 * total_area);
  f32 v =
      (p.x * (triangle->v1.y - triangle->v0.y) +
       p.y * (triangle->v0.x - triangle->v1.x) +
       (triangle->v1.x * triangle->v0.y - triangle->v0.x * triangle->v1.y)) /
      (2.0 * total_area);

  f32 r = 1.0 - u - v;
  return (V3){r, u, v};
#else
  f32 w0 =
      ((triangle->v1.y - triangle->v2.y) * (p.x - triangle->v2.x) +
       (triangle->v2.x - triangle->v1.x) * (p.y - triangle->v2.y)) /
      ((triangle->v1.y - triangle->v2.y) * (triangle->v0.x - triangle->v2.x) +
       (triangle->v2.x - triangle->v1.x) * (triangle->v0.y - triangle->v2.y));
  f32 w1 =
      ((triangle->v2.y - triangle->v0.y) * (p.x - triangle->v2.x) +
       (triangle->v0.x - triangle->v2.x) * (p.y - triangle->v2.y)) /
      ((triangle->v1.y - triangle->v2.y) * (triangle->v0.x - triangle->v2.x) +
       (triangle->v2.x - triangle->v1.x) * (triangle->v0.y - triangle->v2.y));
  f32 w2 = 1.0 - w0 - w1;
  return (V3){w0, w1, w2};
#endif
}

void draw_triangle_flat_bottom(f32 *depthbuffer, BitMap *dst, AABB *aabb_dst,
                               u32 color, Triangle *triangle,
                               Triangle *orig_triangle) {
  AABB aabb_tri = triangle_aabb(triangle);
  if (!aabb_intersect(&aabb_tri, aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_tri, aabb_dst);
  u32 copy_area_hight = aabb_hight_u32(&intersection);

  f32 inv_slope_1 =
      (triangle->v1.x - triangle->v0.x) / (triangle->v1.y - triangle->v0.y);
  f32 inv_slope_2 =
      (triangle->v2.x - triangle->v0.x) / (triangle->v2.y - triangle->v0.y);

  f32 x1 = triangle->v0.x;
  f32 x2 = triangle->v0.x;
  if (triangle->v0.y < intersection.min.y) {
    x1 += inv_slope_1 * (intersection.min.y - triangle->v0.y);
    x2 += inv_slope_2 * (intersection.min.y - triangle->v0.y);
  }

  u8 *dst_start =
      dst->data + f32_to_u32_round_down(intersection.min.x) * dst->channels +
      f32_to_u32_round_down(intersection.min.y) * dst->width * dst->channels;
  depthbuffer += f32_to_u32_round_down(intersection.min.x) +
                 f32_to_u32_round_down(intersection.min.y) * dst->width;
  for (u32 y = 0; y < copy_area_hight; y++) {
    u8 *dst_row = dst_start;
    f32 *depth_row = depthbuffer;
    f32 x1_bound = MIN(MAX(x1, intersection.min.x), intersection.max.x);
    f32 x2_bound = MIN(MAX(x2, intersection.min.x), intersection.max.x);
    f32 line_start = MIN(x1_bound, x2_bound);
    f32 line_end = MAX(x1_bound, x2_bound);
    dst_row +=
        f32_to_u32_round_down(line_start - intersection.min.x) * dst->channels;
    depth_row += f32_to_u32_round_down(line_start - intersection.min.x);
    u32 line_width = f32_to_u32_round_down(line_end - line_start);
    for (u32 x = 0; x < line_width; x++) {
      V2 p = {line_start + (f32)x, intersection.min.y + (f32)y};
      V3 w = calculate_interpolation(orig_triangle, p);
      f32 depth = w.x * orig_triangle->v0.z + w.y * orig_triangle->v1.z +
                  w.z * orig_triangle->v2.z;
      f32 *current_depth = depth_row + x;
      if (*current_depth < depth) {
        *current_depth = depth;

        V3 normal =
            v3_add(v3_add(v3_mul(orig_triangle->v0_vertex->normal, w.x),
                          v3_mul(orig_triangle->v1_vertex->normal, w.y)),
                   v3_mul(orig_triangle->v2_vertex->normal, w.z));
        u32 normal_color = (u32)(fabs(normal.x * 255.0)) << 16 |
                           (u32)(fabs(normal.y * 255.0)) << 8 |
                           (u32)(fabs(normal.z * 255.0)) << 0;

        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        // *dst_color = color;
        *dst_color = normal_color;
      }
    }
    x1 += inv_slope_1;
    x2 += inv_slope_2;
    dst_start += dst->width * dst->channels;
    depthbuffer += dst->width;
  }
}

void draw_triangle_flat_top(f32 *depthbuffer, BitMap *dst, AABB *aabb_dst,
                            u32 color, Triangle *triangle,
                            Triangle *orig_triangle) {
  AABB aabb_tri = triangle_aabb(triangle);
  if (!aabb_intersect(&aabb_tri, aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_tri, aabb_dst);
  u32 copy_area_hight = aabb_hight_u32(&intersection);

  f32 inv_slope_1 =
      (triangle->v2.x - triangle->v0.x) / (triangle->v2.y - triangle->v0.y);
  f32 inv_slope_2 =
      (triangle->v2.x - triangle->v1.x) / (triangle->v2.y - triangle->v1.y);

  f32 x1 = triangle->v2.x;
  f32 x2 = triangle->v2.x;
  if (intersection.max.y < triangle->v2.y) {
    x1 -= inv_slope_1 * (triangle->v2.y - intersection.max.y);
    x2 -= inv_slope_2 * (triangle->v2.y - intersection.max.y);
  }

  u8 *dst_start =
      dst->data + f32_to_u32_round_down(intersection.min.x) * dst->channels +
      f32_to_u32_round_down(intersection.max.y) * dst->width * dst->channels;
  depthbuffer += f32_to_u32_round_down(intersection.min.x) +
                 f32_to_u32_round_down(intersection.max.y) * dst->width;
  for (u32 y = copy_area_hight; 0 < y; y--) {
    u8 *dst_row = dst_start;
    f32 *depth_row = depthbuffer;
    f32 x1_bound = MIN(MAX(x1, intersection.min.x), intersection.max.x);
    f32 x2_bound = MIN(MAX(x2, intersection.min.x), intersection.max.x);
    f32 line_start = MIN(x1_bound, x2_bound);
    f32 line_end = MAX(x1_bound, x2_bound);
    dst_row +=
        f32_to_u32_round_down(line_start - intersection.min.x) * dst->channels;
    depth_row += f32_to_u32_round_down(line_start - intersection.min.x);

    u32 line_width = f32_to_u32_round_down(line_end - line_start);
    for (u32 x = 0; x < line_width; x++) {
      V2 p = {line_start + (f32)x, intersection.min.y + (f32)y};
      V3 w = calculate_interpolation(orig_triangle, p);
      f32 depth = w.x * orig_triangle->v0.z + w.y * orig_triangle->v1.z +
                  w.z * orig_triangle->v2.z;
      f32 *current_depth = depth_row + x;
      if (*current_depth < depth) {
        *current_depth = depth;

        V3 normal =
            v3_add(v3_add(v3_mul(orig_triangle->v0_vertex->normal, w.x),
                          v3_mul(orig_triangle->v1_vertex->normal, w.y)),
                   v3_mul(orig_triangle->v2_vertex->normal, w.z));
        u32 normal_color = (u32)(fabs(normal.x * 255.0)) << 16 |
                           (u32)(fabs(normal.y * 255.0)) << 8 |
                           (u32)(fabs(normal.z * 255.0)) << 0;

        u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
        // *dst_color = color;
        *dst_color = normal_color;
      }
    }
    x1 -= inv_slope_1;
    x2 -= inv_slope_2;
    dst_start -= dst->width * dst->channels;
    depthbuffer -= dst->width;
  }
}

// Draw a triangle assuming vertices are in the CCW order.
void draw_triangle_standard(f32 *depthbuffer, BitMap *dst, Rect *rect_dst,
                            u32 color, Triangle triangle, CullMode cullmode) {
  bool is_ccw = triangle_ccw(&triangle);
  switch (cullmode) {
  case CCW:
    if (!is_ccw)
      return;
    break;
  case CW:
    if (is_ccw)
      return;
    break;
  case None:
    break;
  }

  AABB aabb_tri = triangle_aabb(&triangle);

  AABB aabb_dst;
  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");
    aabb_dst = rect_aabb(rect_dst);
  } else {
    aabb_dst = (AABB){{0.0, 0.0}, {dst->width, dst->hight}};
  }

  if (!aabb_intersect(&aabb_tri, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_tri, &aabb_dst);

  V3 s_v0;
  V3 s_v1;
  V3 s_v2;

  if (triangle.v0.y < triangle.v1.y) {
    if (triangle.v1.y < triangle.v2.y) {
      s_v0 = triangle.v0;
      s_v1 = triangle.v1;
      s_v2 = triangle.v2;
    } else {
      if (triangle.v0.y < triangle.v2.y) {
        s_v0 = triangle.v0;
        s_v1 = triangle.v2;
        s_v2 = triangle.v1;
      } else {
        s_v0 = triangle.v2;
        s_v1 = triangle.v0;
        s_v2 = triangle.v1;
      }
    }
  } else {
    if (triangle.v0.y < triangle.v2.y) {
      s_v0 = triangle.v1;
      s_v1 = triangle.v0;
      s_v2 = triangle.v2;
    } else {
      if (triangle.v1.y < triangle.v2.y) {
        s_v0 = triangle.v1;
        s_v1 = triangle.v2;
        s_v2 = triangle.v0;
      } else {
        s_v0 = triangle.v2;
        s_v1 = triangle.v1;
        s_v2 = triangle.v0;
      }
    }
  }
  ASSERT((s_v0.y <= s_v1.y && s_v1.y <= s_v2.y),
         "Vertices are not sorted: s_v2.y: %f, s_v1.y: %f, s_v0.y: %f", s_v2.y,
         s_v1.y, s_v0.y);

  Triangle sorted_triangle = {
      .v0 = s_v0,
      .v1 = s_v1,
      .v2 = s_v2,
  };
  if (s_v1.y == s_v2.y) {
    draw_triangle_flat_bottom(depthbuffer, dst, &intersection, color,
                              &sorted_triangle, &triangle);
    return;
  }
  if (s_v0.y == s_v1.y) {
    draw_triangle_flat_top(depthbuffer, dst, &intersection, color,
                           &sorted_triangle, &triangle);
    return;
  }

  V3 v4 = {
      .x = s_v0.x + ((s_v1.y - s_v0.y) / (s_v2.y - s_v0.y)) * (s_v2.x - s_v0.x),
      .y = s_v1.y,
      .z = 0.0,
  };

  V3 w = calculate_interpolation(&sorted_triangle, v4.xy);
  v4.z = s_v0.z * w.x + s_v1.z * w.y + s_v2.z * w.z;

  Triangle flat_bottom = {
      .v0 = s_v0,
      .v1 = s_v1,
      .v2 = v4,
  };
  draw_triangle_flat_bottom(depthbuffer, dst, &intersection, color,
                            &flat_bottom, &triangle);
  Triangle flat_top = {
      .v0 = s_v1,
      .v1 = v4,
      .v2 = s_v2,
  };
  draw_triangle_flat_top(depthbuffer, dst, &intersection, color, &flat_top,
                         &triangle);
}

void draw_triangle_barycentric(f32 *depthbuffer, BitMap *dst, Rect *rect_dst,
                               u32 color, Triangle triangle,
                               CullMode cullmode) {
  bool is_ccw = triangle_ccw(&triangle);
  switch (cullmode) {
  case CCW:
    if (!is_ccw)
      return;
    break;
  case CW:
    if (is_ccw)
      return;
    break;
  case None:
    break;
  }

  AABB aabb_tri = triangle_aabb(&triangle);

  AABB aabb_dst;
  u8 *dst_start;
  if (rect_dst) {
    ASSERT((rect_dst->width <= dst->width), "Invalid blit rect_dst");
    ASSERT((rect_dst->hight <= dst->hight), "Invalid blit rect_dst");
    aabb_dst = rect_aabb(rect_dst);
    dst_start = dst->data + (u32)(rect_dst->pos.x - rect_dst->width / 2.0) +
                (u32)(rect_dst->pos.y - rect_dst->hight / 2.0) * dst->width *
                    dst->channels;
  } else {
    aabb_dst = (AABB){{0.0, 0.0}, {dst->width, dst->hight}};
    dst_start = dst->data;
  }

  if (!aabb_intersect(&aabb_tri, &aabb_dst))
    return;

  AABB intersection = aabb_intersection(&aabb_tri, &aabb_dst);

  u32 copy_area_width = aabb_width(&intersection);
  u32 copy_area_hight = aabb_hight(&intersection);
  if (copy_area_width == 0 && copy_area_hight == 0)
    return;

  V2 dst_start_offset = v2_sub(intersection.min, aabb_dst.min);
  dst_start += (u32)dst_start_offset.x * dst->channels +
               (u32)dst_start_offset.y * (dst->width * dst->channels);

  for (u32 y = 0; y < copy_area_hight; y++) {
    u8 *dst_row = dst_start + y * (dst->width * dst->channels);
    for (u32 x = 0; x < copy_area_width; x++) {
      V2 p = {intersection.min.x + (f32)x, intersection.min.y + (f32)y};

      V2 v0v1 = v2_sub(triangle.v1.xy, triangle.v0.xy);
      V2 v0p = v2_sub(p, triangle.v0.xy);

      V2 v1v2 = v2_sub(triangle.v2.xy, triangle.v1.xy);
      V2 v1p = v2_sub(p, triangle.v1.xy);

      V2 v2v0 = v2_sub(triangle.v0.xy, triangle.v2.xy);
      V2 v2p = v2_sub(p, triangle.v2.xy);

      f32 c1 = v2_perp_dot(v0v1, v0p);
      f32 c2 = v2_perp_dot(v1v2, v1p);
      f32 c3 = v2_perp_dot(v2v0, v2p);

      bool render = false;
      switch (cullmode) {
      case CW:
        render = c1 >= 0.0 && c2 >= 0.0 && c3 >= 0.0;
        break;
      case CCW:
        render = c1 <= 0.0 && c2 <= 0.0 && c3 <= 0.0;
        break;
      case None:
        render = (c1 >= 0.0 && c2 >= 0.0 && c3 >= 0.0) ||
                 (c1 <= 0.0 && c2 <= 0.0 && c3 <= 0.0);
        break;
      }
      if (render) {
        V3 w = calculate_interpolation(&triangle, p);
        f32 depth =
            w.x * triangle.v0.z + w.y * triangle.v1.z + w.z * triangle.v2.z;

        f32 *current_depth = depthbuffer + (u32)(intersection.min.x) +
                             (u32)(intersection.min.y) * dst->width + x +
                             y * dst->width;
        if (*current_depth < depth) {
          *current_depth = depth;

          V3 normal = v3_add(v3_add(v3_mul(triangle.v0_vertex->normal, w.x),
                                    v3_mul(triangle.v1_vertex->normal, w.y)),
                             v3_mul(triangle.v2_vertex->normal, w.z));
          u32 normal_color = (u32)(fabs(normal.x * 255.0)) << 16 |
                             (u32)(fabs(normal.y * 255.0)) << 8 |
                             (u32)(fabs(normal.z * 255.0)) << 0;

          u32 *dst_color = (u32 *)(dst_row + x * dst->channels);
          // *dst_color = color;
          *dst_color = normal_color;
        }
      }
    }
  }
}

void draw_char(BitMap *dst, Rect *rect_dst, Font *font, char c, u32 color,
               V2 pos) {
  BitMap font_bm = {
      .data = font->bitmap,
      .width = font->bitmap_width,
      .hight = font->bitmap_hight,
      .channels = 1,
  };
  Rect char_rect = {
      .pos = {(font->char_info[c].x1 + font->char_info[c].x0) / 2.0,
              (font->char_info[c].y1 + font->char_info[c].y0) / 2.0},
      .width = (f32)(font->char_info[c].x1 - font->char_info[c].x0),
      .hight = (f32)(font->char_info[c].y1 - font->char_info[c].y0),
  };
  blit_bitmap(dst, rect_dst, &font_bm, &char_rect, pos, color);
}

void draw_text(BitMap *dst, Rect *rect_dst, Font *font, const char *text,
               u32 color, V2 pos) {
  while (*text) {
    draw_char(dst, rect_dst, font, *text, color, pos);
    pos.x += (f32)(font->char_info[*text].xadvance);
    text++;
  }
}

typedef struct {
  V3 position;
  f32 speed;
  V3 velocity;
  f32 mouse_sense;
  f32 pitch;
  f32 yaw;
  bool is_active;
} Camera;

#define CAMERA_FORWARD                                                         \
  (V3) { 0.0, 0.0, 1.0 }
#define CAMERA_UP                                                              \
  (V3) { 0.0, -1.0, 0.0 }
#define CAMERA_RIGHT                                                           \
  (V3) { 1.0, 0.0, 0.0 }

void camera_init(Camera *camera) {
  camera->position = (V3){0.0, -8.0, 0.0};
  camera->speed = 10.0;
  camera->velocity = (V3){0.0, 0.0, 0.0};
  camera->is_active = false;
  camera->mouse_sense = 0.1;
  camera->pitch = 0.0;
  camera->yaw = 0.0;
}

void camera_handle_event(Camera *camera, SDL_Event *sdl_event, f32 dt) {
  switch (sdl_event->type) {
  case SDL_KEYDOWN:
    switch (sdl_event->key.keysym.sym) {
    case SDLK_w:
      camera->velocity.y = 1.0;
      break;
    case SDLK_s:
      camera->velocity.y = -1.0;
      break;
    case SDLK_a:
      camera->velocity.x = -1.0;
      break;
    case SDLK_d:
      camera->velocity.x = 1.0;
      break;
    case SDLK_SPACE:
      camera->velocity.z = 1.0;
      break;
    case SDLK_LCTRL:
      camera->velocity.z = -1.0;
      break;
    default:
      break;
    }
    break;
  case SDL_KEYUP:
    switch (sdl_event->key.keysym.sym) {
    case SDLK_w:
      camera->velocity.y = 0.0;
      break;
    case SDLK_s:
      camera->velocity.y = 0.0;
      break;
    case SDLK_a:
      camera->velocity.x = 0.0;
      break;
    case SDLK_d:
      camera->velocity.x = 0.0;
      break;
    case SDLK_SPACE:
      camera->velocity.z = 0.0;
      break;
    case SDLK_LCTRL:
      camera->velocity.z = 0.0;
      break;
    default:
      break;
    }
    break;
  case SDL_MOUSEBUTTONDOWN:
    camera->is_active = true;
    break;
  case SDL_MOUSEBUTTONUP:
    camera->is_active = false;
    break;
  case SDL_MOUSEMOTION:
    if (camera->is_active) {
      camera->yaw -= sdl_event->motion.xrel * camera->mouse_sense * dt;
      camera->pitch -= sdl_event->motion.yrel * camera->mouse_sense * dt;
    }
    break;
  default:
    break;
  }
}

Mat4 camera_translation(Camera *camera) {
  Mat4 translation = mat4_idendity();
  mat4_translate(&translation, camera->position);
  return translation;
}

Mat4 camera_rotation(Camera *camera) {
  // These are in world space
  Mat4 camera_rotation_pitch =
      mat4_rotation((V3){1.0, 0.0, 0.0}, camera->pitch);
  Mat4 camera_rotation_yaw = mat4_rotation((V3){0.0, 0.0, 1.0}, camera->yaw);
  return mat4_mul(&camera_rotation_yaw, &camera_rotation_pitch);
}

Mat4 camera_transform(Camera *camera) {
  Mat4 c_rotation = camera_rotation(camera);
  // Camera space is:
  // X right
  // Y down
  // Z forward
  // While world space is:
  // X right
  // Y forward
  // Z up
  Mat4 c_coords = {
      .i = {1.0, 0.0, 0.0, 0.0},
      .j = {0.0, 0.0, -1.0, 0.0},
      .k = {0.0, 1.0, 0.0, 0.0},
      .t = {0.0, 0.0, 0.0, 1.0},
  };
  c_rotation = mat4_mul(&c_rotation, &c_coords);

  Mat4 c_translation = camera_translation(camera);
  Mat4 camera_transform = mat4_mul(&c_translation, &c_rotation);
  camera_transform = mat4_inverse(&camera_transform);

  return camera_transform;
}

void camera_update(Camera *camera, f32 dt) {
  Mat4 rotation = camera_rotation(camera);

  V3 camera_vel = v3_mul(camera->velocity, camera->speed * dt);
  V4 camera_vel_v4 = v3_to_v4(camera_vel, 1.0);

  V4 camera_vel_v4_rotated = mat4_mul_v4(&rotation, camera_vel_v4);
  camera->position = v3_add(camera->position, v4_to_v3(camera_vel_v4_rotated));
}

Mat4 calculate_mvp(Camera *camera, Mat4 *model_transform) {
  Mat4 c_transform = camera_transform(camera);
  Mat4 perspective = mat4_perspective(
      70.0 / 180.0 * 3.14, (f32)WINDOW_WIDTH / (f32)WINDOW_HIGHT, 0.1, 1000.0);

  Mat4 model_view = mat4_mul(&c_transform, model_transform);
  return mat4_mul(&perspective, &model_view);
}

typedef struct {
  Memory memory;

  SDL_Window *window;

  SDL_Surface *surface;
  BitMap surface_bm;
  Rect surface_rect;

  bool stop;
  clock_t time_old;
  clock_t time_new;
  f64 dt;

  f64 r;

  Rect rect;
  V2 rect_vel;

  Camera camera;
  enum { Standard, Barycentric } triangle_mode;
  bool draw_depth;

  BitMap bm;
  Font font;

  Model model;
  f32 model_rotation;
  Mat4 model_transform;
} Game;

void update_window_surface(Game *game) {
  game->surface = SDL_GetWindowSurface(game->window);
  ASSERT(game->surface, "SDL error: %s", SDL_GetError());

  BitMap surface_bm = {
      .data = game->surface->pixels,
      .width = game->surface->w,
      .hight = game->surface->h,
      .channels = 4,
  };
  game->surface_bm = surface_bm;

  Rect surface_rect = {
      .pos = {(f32)game->surface->w / 2.0, (f32)game->surface->h / 2.0},
      .width = game->surface->w,
      .hight = game->surface->h,
  };
  game->surface_rect = surface_rect;
}

void init(Game *game) {
  if (!init_memory(&game->memory)) {
    exit(1);
  }

  perm_alloc((&game->memory), u64[2]);
  perm_alloc((&game->memory), u32[4]);
  frame_alloc((&game->memory), u64[1]);
  frame_alloc((&game->memory), struct {
    bool b;
    f32 f;
  });
  frame_alloc((&game->memory), u8[3]);
  frame_alloc((&game->memory), u64);
  printf("perm_mem end %lu\n", game->memory.perm_memory.end);
  printf("frame_mem end %lu\n", game->memory.frame_memory.end);
  frame_reset(&game->memory);
  printf("frame_mem end %lu\n", game->memory.frame_memory.end);

  INFO("test %d", 69);
  WARN("test %d", 69);
  ERROR("test %d", 69);
  DEBUG("test %d", 69);

  SDL_Init(SDL_INIT_VIDEO);

  game->window =
      SDL_CreateWindow("softy", SDL_WINDOWPOS_UNDEFINED,
                       SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HIGHT, 0);
  ASSERT(game->window, "SDL error: %s", SDL_GetError());

  update_window_surface(game);

  game->dt = FRAME_TIME_S;

  game->r = 0.0;

  Rect rect = {
      .pos = {0.0, 0.0},
      .width = 100.0,
      .hight = 150.0,
  };
  game->rect = rect;
  game->rect_vel = (V2){1.2, 2.1};

  camera_init(&game->camera);
  game->triangle_mode = Standard;
  game->draw_depth = false;

  game->bm = load_bitmap(&game->memory, "assets/a.png");
  game->font = load_font(&game->memory, "assets/font.ttf", 32.0, 512, 512);
  game->model = load_model(&game->memory, "assets/monkey.obj");
  game->model_rotation = 0.0;
  game->model_transform = mat4_idendity();
}

void destroy(Game *game) {
  SDL_DestroyWindow(game->window);
  SDL_Quit();
}

#ifndef __EMSCRIPTEN__
void cap_fps(Game *game) {
  game->time_new = clock();
  game->dt = (f64)(game->time_new - game->time_old) / CLOCKS_PER_SEC;
  game->time_old = game->time_new;

  if (game->dt < FRAME_TIME_S) {
    f64 sleep_s = FRAME_TIME_S - game->dt;
    u64 sec = (u64)sleep_s;
    u64 nsec = (sleep_s - (f64)sec) * NS_PER_SEC;
    struct timespec req = {
        .tv_sec = sec,
        .tv_nsec = nsec,
    };

    while (nanosleep(&req, &req) == -1) {
    }

    game->dt = FRAME_TIME_S;
  }
}
#endif

void run(Game *game) {
  frame_reset(&game->memory);

  SDL_Event sdl_event;
  while (SDL_PollEvent(&sdl_event) != 0) {
    switch (sdl_event.type) {
    case SDL_QUIT:
      game->stop = true;
      break;
    case SDL_WINDOWEVENT:
      switch (sdl_event.window.event) {
      case SDL_WINDOWEVENT_RESIZED:
      case SDL_WINDOWEVENT_SIZE_CHANGED:
        update_window_surface(game);
        break;
      default:
        break;
      }
      break;
    case SDL_KEYDOWN:
      switch (sdl_event.key.keysym.sym) {
      case SDLK_q:
        game->model_rotation += game->dt;
        game->model_transform = mat4_rotation_z(game->model_rotation);
        break;
      case SDLK_e:
        game->model_rotation -= game->dt;
        game->model_transform = mat4_rotation_z(game->model_rotation);
        break;
      case SDLK_1:
        game->triangle_mode = Standard;
        break;
      case SDLK_2:
        game->triangle_mode = Barycentric;
        break;
      case SDLK_3:
        game->draw_depth = !game->draw_depth;
        break;
      }
      break;
    default:
      break;
    }
    camera_handle_event(&game->camera, &sdl_event, game->dt);
  }
  camera_update(&game->camera, game->dt);

#ifndef __EMSCRIPTEN__
  cap_fps(game);
#endif

  game->r += game->dt;
  if (1.0 < game->r) {
    game->r = 0;
  }

  game->rect.pos = v2_add(game->rect.pos, game->rect_vel);
  if (game->rect.pos.x < 0 || game->surface->w < game->rect.pos.x) {
    game->rect_vel.x *= -1;
  }
  if (game->rect.pos.y < 0 || game->surface->h < game->rect.pos.y) {
    game->rect_vel.y *= -1;
  }

  f32 *depthbuffer = frame_alloc_array((&game->memory), f32,
                                       game->surface->w * game->surface->h);
  memset(depthbuffer, 0, game->surface->w * game->surface->h * 4);

  SDL_FillRect(game->surface, 0, 0);

  Mat4 c_transform = camera_transform(&game->camera);
  Mat4 perspective = mat4_perspective(
      70.0 / 180.0 * 3.14, (f32)WINDOW_WIDTH / (f32)WINDOW_HIGHT, 0.1, 1000.0);

  Mat4 mvp = calculate_mvp(&game->camera, &game->model_transform);
  for (u32 i = 0; i < game->model.vertices_num; i += 3) {
#if 0
    V4 v0 = v3_to_v4(game->model.vertices[i].position, 1.0);
    V4 v1 = v3_to_v4(game->model.vertices[i + 1].position, 1.0);
    V4 v2 = v3_to_v4(game->model.vertices[i + 2].position, 1.0);
    INFO("model v0 (%f %f %f), v1 (%f %f %f), v2 (%f %f %f)", v0.x, v0.y,
    v0.z,
         v1.x, v1.y, v1.z, v2.x, v2.y, v2.z);

    V4 v0_world = mat4_mul_v4(&game->model_transform, v0);
    V4 v1_world = mat4_mul_v4(&game->model_transform, v1);
    V4 v2_world = mat4_mul_v4(&game->model_transform, v2);
    INFO("world v0 (%f %f %f), v1 (%f %f %f), v2 (%f %f %f)", v0_world.x,
         v0_world.y, v0_world.z, v1_world.x, v1_world.y, v1_world.z,
         v2_world.x, v2_world.y, v2_world.z);

    V4 v0_camera = mat4_mul_v4(&c_transform, v0_world);
    V4 v1_camera = mat4_mul_v4(&c_transform, v1_world);
    V4 v2_camera = mat4_mul_v4(&c_transform, v2_world);
    INFO("camera v0 (%f %f %f), v1 (%f %f %f), v2 (%f %f %f)", v0_camera.x,
         v0_camera.y, v0_camera.z, v1_camera.x, v1_camera.y, v1_camera.z,
         v2_camera.x, v2_camera.y, v2_camera.z);

    V4 v0_clip = mat4_mul_v4(&perspective, v0_world);
    V4 v1_clip = mat4_mul_v4(&perspective, v1_world);
    V4 v2_clip = mat4_mul_v4(&perspective, v2_world);
    INFO("clip v0 (%f %f %f), v1 (%f %f %f), v2 (%f %f %f)", v0_clip.x,
         v0_clip.y, v0_clip.z, v1_clip.x, v1_clip.y, v1_clip.z, v2_clip.x,
         v2_clip.y, v2_clip.z);
#endif

    Triangle t = vertices_to_triangle(
        &game->model.vertices[i], &game->model.vertices[i + 1],
        &game->model.vertices[i + 2], &mvp, WINDOW_WIDTH, WINDOW_HIGHT);
    u32 color =
        (f32)(0xFFAA33FF) * (f32)(i + 1) / (f32)(game->model.vertices_num + 1);
    switch (game->triangle_mode) {
    case Standard:
      draw_triangle_standard(depthbuffer, &game->surface_bm, NULL, color, t,
                             CCW);
      break;
    case Barycentric:
      draw_triangle_barycentric(depthbuffer, &game->surface_bm, NULL, color, t,
                                CCW);
      break;
    }
  }

  if (game->draw_depth) {
    for (u32 y = 0; y < game->surface_rect.hight; y++) {
      for (u32 x = 0; x < game->surface_rect.width; x++) {
        u32 *pixel = (u32 *)(game->surface->pixels) + x + y * game->surface->w;
        f32 *depth = depthbuffer + x + y * game->surface->w;
        u32 d = (u32)(*depth * 255.0);
        *pixel = d << 16 | d << 8 | d << 0;
      }
    }
  }

  blit_color_rect(&game->surface_bm, &game->surface_rect, 0xFF666666,
                  &game->rect);

  blit_bitmap(&game->surface_bm, NULL, &game->bm, NULL, game->rect.pos,
              0xFF0033EE);

  {
    char *buf = frame_alloc((&game->memory), char[70]);
    snprintf(buf, 70, "FPS: %.02f dt: %.5f", 1.0 / game->dt, game->dt);
    draw_text(&game->surface_bm, &game->surface_rect, &game->font, buf,
              0xFF00FF00, (V2){20.0, 20.0});
  }

  char *buf = frame_alloc((&game->memory), char[70]);
  snprintf(buf, 70, "Camera: x: %.02f y: %.02f z: %.02f",
           game->camera.position.x, game->camera.position.y,
           game->camera.position.z);
  draw_text(&game->surface_bm, &game->surface_rect, &game->font, buf,
            0xFF00FF00, (V2){20.0, game->surface_rect.hight - 50.0});

  SDL_UpdateWindowSurface(game->window);
}
