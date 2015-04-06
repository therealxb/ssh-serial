
#include <assert.h>
#include <stdlib.h>

#include "buffer.h"

buffer_t *
buffer_make(size_t size)
{
  buffer_t *b;

  b = malloc(sizeof(struct buffer_struct));
  if (b == NULL) {
    return NULL;
  }

  b->buffer_start = malloc(size);
  if (b->buffer_start == NULL) {
    free(b);
    return NULL;
  }
  b->buffer_size = size;
  b->data_start = b->buffer_start;
  b->data_size = 0;

  return b;
}


void
buffer_unmake(buffer_t *b)
{
  assert(b != NULL);
  assert(b->buffer_start != NULL);

  free(b->buffer_start);
  free(b);
}
