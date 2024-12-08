#include "defines.h"

#include <stdalign.h>
#include <stdio.h>
#include <sys/mman.h>

#define PERM_MEMORY_SIZE 1024 * 1024 * 32
#define FRAME_MEMORY_SIZE 1024 * 1024 * 4

typedef struct {
  u8 *memory;
  u64 end;
  u64 capacity;
} MemoryChunk;

typedef struct {
  MemoryChunk perm_memory;
  MemoryChunk frame_memory;
} Memory;

bool init_memory(Memory *memory) {
  u8 *perm_ptr = mmap(NULL, PERM_MEMORY_SIZE, PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (!perm_ptr) {
    return false;
  }

  u8 *frame_ptr = mmap(NULL, FRAME_MEMORY_SIZE, PROT_READ | PROT_WRITE,
                       MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (!frame_ptr) {
    return false;
  }

  memory->perm_memory.memory = perm_ptr;
  memory->perm_memory.end = 0;
  memory->perm_memory.capacity = PERM_MEMORY_SIZE;

  memory->frame_memory.memory = frame_ptr;
  memory->frame_memory.end = 0;
  memory->frame_memory.capacity = FRAME_MEMORY_SIZE;
  return true;
}

#define perm_alloc(memory, type)                                               \
  __bump_alloc(&memory->perm_memory, sizeof(type), alignof(type))

#define perm_alloc_array(memory, type, num)                                    \
  __bump_alloc(&memory->perm_memory, sizeof(type) * num, alignof(type))

#define frame_alloc(memory, type)                                              \
  __bump_alloc(&memory->frame_memory, sizeof(type), alignof(type))

#define frame_reset(memory) *memory.frame_memory.end = 0;

void *__bump_alloc(MemoryChunk *chunk, u64 size, u64 alignment) {
  u64 bytes_aligned =
      ((u64)(chunk->memory + chunk->end) + (alignment - 1)) & ~(alignment - 1);
  u64 bytes_diff = bytes_aligned - (u64)(chunk->memory + chunk->end);

  if (chunk->capacity < chunk->end + bytes_diff + size) {
    return NULL;
  }

  chunk->end += bytes_diff;
  u8 *r_ptr = chunk->memory + chunk->end;
  chunk->end += size;

  return r_ptr;
}
