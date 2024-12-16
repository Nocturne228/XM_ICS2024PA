#include <klib-macros.h>
#include <klib.h>
#include <stdint.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)

size_t strlen(const char *s) {
  // panic("Not implemented");
  const char *sc;
  for (sc = s; *sc != '\0'; ++sc) ;
  
  return sc - s;
}

char *strcpy(char *dst, const char *src) {
  // panic("Not implemented");
  char *tmp = dst;
  while ((*dst++ = *src++) != '\0') ;
  return tmp;
}

char *strncpy(char *dst, const char *src, size_t n) {
  // panic("Not implemented");
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


char *strcat(char *dst, const char *src) {
  // panic("Not implemented");
  char *tmp = dst;
  while (*dst) {
    dst++;
  }

  while ((*dst++ = *src++) != '\0');

  return tmp;
}

/**
 * strcmp - Compare two strings
 * @s1: One string
 * @s2: Another string
 */
int strcmp(const char *s1, const char *s2) {
  // panic("Not implemented");
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
 * @s1: One string
 * @s2: Another string
 * @n: The maximum number of bytes to compare
 */
int strncmp(const char *s1, const char *s2, size_t n) {
  // panic("Not implemented");
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
 * @s: Pointer to the start of the area.
 * @c: The byte to fill the area with
 * @count: The size of the area.
 *
 * Do not use memset() to access IO space, use memset_io() instead.
 */
void *memset(void *s, int c, size_t n) {
  // panic("Not implemented");
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
 * @n: The size of the area.
 *
 * Unlike memcpy(), memmove() copes with overlapping areas.
 */
void *memmove(void *dst, const void *src, size_t n) {
  // panic("Not implemented");
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
 * @n: The size of the area.
 *
 * You should not use this function to access IO space, use memcpy_toio()
 * or memcpy_fromio() instead.
 */
void *memcpy(void *out, const void *in, size_t n) {
  //  panic("Not implemented");
  char *tmp = out;
  const char *s = in;
  while (n--) *tmp++ = *s++;
  return out;
}

/**
 * memcmp - Compare two areas of memory
 * @s1: One area of memory
 * @s2: Another area of memory
 * @n: The size of the area.
 */
int memcmp(const void *s1, const void *s2, size_t n) {
  // panic("Not implemented");
  const unsigned char *su1, *su2;
  int res = 0;

  for (su1 = s1, su2 = s2; 0 < n; ++su1, ++su2, n--) {
    if ((res = *su1 - *su2) != 0) break;
  }
  return res;
}

#endif
