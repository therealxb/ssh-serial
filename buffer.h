#ifndef BUFFER_H
#define BUFFER_H

struct buffer_struct {
  char *buffer_start;
  size_t buffer_size;
  char *data_start;
  size_t data_size;
};
typedef struct buffer_struct buffer_t;

buffer_t *buffer_make(size_t size);
void buffer_unmake(buffer_t *t);

/* Intended use is
 *  if (buffer_remaining(b)) {
 *    readen = read(f, buffer_remaining_start(b), buffer_remaining_size(b));
 *    if (-1 != readen) {
 *      buffer_added_data(b, readen);
 *    }
 *  }
 */
int buffer_remaining(buffer_t *t);
void *buffer_remaining_start(buffer_t *t);
size_t buffer_remaining_size(buffer_t *t);
void buffer_added_data(buffer_t *t, ssize_t r);

/* Intended use is
 *  if (buffer_data(b)) {
 *    written = write(f, buffer_data_start(b), buffer_data_size(b));
 *    if (-1 != written) {
 *      buffer_removed_data(b, written);
 *    }
 *  }
 */
int buffer_data(buffer_t *t);
void *buffer_data_start(buffer_t *t);
size_t buffer_data_size(buffer_t *t);
void buffer_removed_data(buffer_t *t, ssize_t written);

#endif
