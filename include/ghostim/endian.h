/**
 * @file gi_endian.h
 * @brief Portable byte-order helpers for image parsing.
 *
 * This module provides functions to read and write integers in both
 * Big Endian (Motorola) and Little Endian (Intel) formats.
 */

#ifndef GI_ENDIAN_H
#define GI_ENDIAN_H

/**
 * @brief Reads a 16-bit unsigned integer in Big Endian.
 * @param p Pointer to the data.
 * @return 16-bit value in host endianness.
 */
static inline unsigned short gi_read_be16(const unsigned char *p) {
  return (unsigned short)((p[0] << 8) | p[1]);
}

/**
 * @brief Reads a 32-bit unsigned integer in Big Endian.
 * @param p Pointer to the data.
 * @return 32-bit value in host endianness.
 */
static inline unsigned int gi_read_be32(const unsigned char *p) {
  return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) |
         ((unsigned int)p[2] << 8) | (unsigned int)p[3];
}

/**
 * @brief Reads a 16-bit unsigned integer in Little Endian.
 * @param p Pointer to the data.
 * @return 16-bit value in host endianness.
 */
static inline unsigned short gi_read_le16(const unsigned char *p) {
  return (unsigned short)((p[1] << 8) | p[0]);
}

/**
 * @brief Reads a 32-bit unsigned integer in Little Endian.
 * @param p Pointer to the data.
 * @return 32-bit value in host endianness.
 */
static inline unsigned int gi_read_le32(const unsigned char *p) {
  return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
         ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

/**
 * @brief Writes a 32-bit unsigned integer in Little Endian.
 * @param p Pointer to destination buffer.
 * @param v Value to write.
 */
static inline void gi_write_le32(unsigned char *p, unsigned int v) {
  p[0] = (unsigned char)v;
  p[1] = (unsigned char)(v >> 8);
  p[2] = (unsigned char)(v >> 16);
  p[3] = (unsigned char)(v >> 24);
}

#endif /* GI_ENDIAN_H */
