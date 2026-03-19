/**
 * @file gi_io.h
 * @brief Dynamic buffer management for ghostimg.
 *
 * This module provides a simple container for binary data that
 * grows automatically during processing.
 */

#ifndef GI_IO_H
#define GI_IO_H

#include <stddef.h>

/**
 * @struct GhostimgBuffer
 * @brief Container for auto-growing binary data.
 */
typedef struct {
  unsigned char *data; /**< Pointer to allocated memory block. */
  size_t size;         /**< Current number of bytes stored. */
  size_t cap;          /**< Total allocated capacity in bytes. */
} GhostimgBuffer;

/**
 * @brief Initializes a buffer with an initial capacity.
 * @param buf Pointer to the buffer structure.
 * @param initial_cap The starting capacity in bytes.
 * @return 0 on success, -1 if memory allocation fails.
 */
int gi_buf_init(GhostimgBuffer *buf, size_t initial_cap);

/**
 * @brief Appends n bytes to the buffer, resizing if necessary.
 * @param buf Pointer to the destination buffer.
 * @param src Pointer to the source data.
 * @param n Number of bytes to copy.
 * @return 0 on success, -1 on allocation failure.
 */
int gi_buf_append(GhostimgBuffer *buf, const unsigned char *src, size_t n);

/**
 * @brief Frees the internal memory of the buffer and resets its fields.
 * @param buf Pointer to the buffer to free.
 */
void gi_buf_free(GhostimgBuffer *buf);

#endif /* GI_IO_H */
