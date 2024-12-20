#include <klib-macros.h>
#include <klib.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

/**
 * strlen - Calculate the length of a string
 * @s: The string to measure
 *
 * Returns the number of characters in the string, excluding the terminating
 * null byte.
 */
size_t strlen(const char *s) {
  const char *sc;
  for (sc = s; *sc != '\0'; ++sc);
  return sc - s;
}

/**
 * strnlen - Calculate the length of a length-limited string
 * @s: The string to measure
 * @count: The maximum number of bytes to check
 *
 * Returns the number of characters in the string, excluding the terminating
 * null byte, but at most count.
 */
size_t strnlen(const char *s, size_t count) {
  const char *sc;
  for (sc = s; count-- && *sc != '\0'; ++sc);
  return sc - s;
}

/**
 * strcpy - Copy a string
 * @dst: Where to copy the string to
 * @src: Where to copy the string from
 *
 * Returns a pointer to the destination string dst.
 */
char *strcpy(char *dst, const char *src) {
  char *tmp = dst;
  while ((*dst++ = *src++) != '\0');
  return tmp;
}

/**
 * strncpy - Copy a length-limited string
 * @dst: Where to copy the string to
 * @src: Where to copy the string from
 * @n: The maximum number of bytes to copy
 *
 * Note: If src is less than n characters long, the remainder of dst is filled
 * with null bytes. Otherwise, dst is not terminated.
 *
 * Returns a pointer to the destination string dst.
 */
char *strncpy(char *dst, const char *src, size_t n) {
  char *tmp = dst;
  while (n) {
    if ((*tmp = *src) != 0) {
      src++;
    }
    tmp++;
    n--;
  }
  return dst;
}

/**
 * strcat - Concatenate two strings
 * @dst: The string to append to
 * @src: The string to append
 *
 * Appends the src string to the dst string, overwriting the terminating null
 * byte at the end of dst, and then adds a terminating null byte.
 *
 * Returns a pointer to the destination string dst.
 */
char *strcat(char *dst, const char *src) {
  char *tmp = dst;
  while (*dst) {
    dst++;
  }
  while ((*dst++ = *src++) != '\0');
  return tmp;
}

/**
 * strcmp - Compare two strings
 * @s1: First string
 * @s2: Second string
 *
 * Returns an integer less than, equal to, or greater than zero if s1 is found,
 * respectively, to be less than, to match, or be greater than s2.
 */
int strcmp(const char *s1, const char *s2) {
  unsigned char c1, c2;
  while (1) {
    c1 = *s1++;
    c2 = *s2++;
    if (c1 != c2) {
      return c1 < c2 ? -1 : 1;
    }
    if (!c1) break;
  }
  return 0;
}

/**
 * strncmp - Compare two length-limited strings
 * @s1: First string
 * @s2: Second string
 * @n: The maximum number of bytes to compare
 *
 * Returns an integer less than, equal to, or greater than zero if s1 (or the
 * first n bytes thereof) is found, respectively, to be less than, to match, or
 * be greater than s2.
 */
int strncmp(const char *s1, const char *s2, size_t n) {
  unsigned char c1, c2;
  while (n) {
    c1 = *s1++;
    c2 = *s2++;
    if (c1 != c2) {
      return c1 < c2 ? -1 : 1;
    }
    if (!c1) break;
    n--;
  }
  return 0;
}

/**
 * memset - Fill a region of memory with the given value
 * @s: Pointer to the start of the area
 * @c: The byte to fill the area with
 * @n: The size of the area to fill
 *
 * Do not use memset() to access IO space, use memset_io() instead.
 *
 * Returns a pointer to the memory area s.
 */
void *memset(void *s, int c, size_t n) {
  char *xs = s;
  while (n--) {
    *xs++ = c;
  }
  return s;
}

/**
 * memmove - Copy one area of memory to another
 * @dst: Where to copy to
 * @src: Where to copy from
 * @n: The size of the area to copy
 *
 * Unlike memcpy(), memmove() copes with overlapping areas.
 *
 * Returns a pointer to the destination memory area dst.
 */
void *memmove(void *dst, const void *src, size_t n) {
  char *tmp;
  const char *s;

  if (dst <= src) {
    tmp = dst;
    s = src;
    while (n--) *tmp++ = *s++;
  } else {
    tmp = dst;
    tmp += n;
    s = src;
    s += n;
    while (n--) *--tmp = *--s;
  }
  return dst;
}

/**
 * memcpy - Copy one area of memory to another
 * @dst: Where to copy to
 * @src: Where to copy from
 * @n: The size of the area to copy
 *
 * You should not use this function to access IO space, use memcpy_toio()
 * or memcpy_fromio() instead.
 *
 * Returns a pointer to the destination memory area dst.
 */
void *memcpy(void *out, const void *in, size_t n) {
  char *tmp = out;
  const char *s = in;
  while (n--) *tmp++ = *s++;
  return out;
}

/**
 * memcmp - Compare two areas of memory
 * @s1: First area of memory
 * @s2: Second area of memory
 * @n: The size of the area to compare
 *
 * Returns an integer less than, equal to, or greater than zero if the first n bytes
 * of s1 is found, respectively, to be less than, to match, or be greater than the
 * first n bytes of s2.
 */
int memcmp(const void *s1, const void *s2, size_t n) {
  const unsigned char *su1, *su2;
  int res = 0;

  for (su1 = s1, su2 = s2; 0 < n; ++su1, ++su2, n--) {
    if ((res = *su1 - *su2) != 0) break;
  }
  return res;
}

#endif
