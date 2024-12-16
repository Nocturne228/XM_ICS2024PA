#include <am.h>
#include <klib-macros.h>
#include <klib.h>
#include <stdarg.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
typedef void (*putter_t)(char ch, void *buf, size_t idx, size_t maxlen);

static int _vsnprintf(putter_t, char *, const size_t, const char *, va_list);
static void _putter_out(char ch, void *buf, size_t idx, size_t maxlen);
static void _putter_buf(char ch, void *buf, size_t idx, size_t maxlen);
static void _put_literal(putter_t, void *, const char *, size_t *, size_t);

static void _itoa(putter_t, char *, int, int, size_t *, size_t, int);
static void _utoa(putter_t, char *, unsigned int, int, size_t *, size_t, int);
static void _reverse(char *str, int length);

int printf(const char *fmt, ...) {
  // panic("Not implemented");
  va_list args;
  va_start(args, fmt);
  char buf[1];
  int result = _vsnprintf(_putter_out, buf, (size_t)-1, fmt, args);
  va_end(args);
  return result;
}

int vsprintf(char *out, const char *fmt, va_list ap) {
  // panic("Not implemented");
  return vsnprintf(out, (size_t)-1, fmt, ap);
}

int sprintf(char *out, const char *fmt, ...) {
  // panic("Not implemented");
  va_list args;
  va_start(args, fmt);
  int result = vsprintf(out, fmt, args);
  va_end(args);
  return result;
}

int snprintf(char *out, size_t n, const char *fmt, ...) {
  // panic("Not implemented");
  va_list args;
  va_start(args, fmt);
  int result = vsnprintf(out, n, fmt, args);
  va_end(args);
  return result;
}

int vsnprintf(char *out, size_t n, const char *fmt, va_list ap) {
  // panic("Not implemented");
  return _vsnprintf(_putter_buf, out, n, fmt, ap);
}

// * -----------------------------------------------------  * //

static int _vsnprintf(putter_t put, char *buf, const size_t maxlen,
                      const char *fmt, va_list ap) {
  size_t idx = 0;

  while (*fmt != '\0') {
    if (*fmt != '%') {
      put(*fmt, buf, idx++, maxlen);
      fmt++;
      continue;
    }
    fmt++;

    int zpad_width = 0;

    switch (*fmt) {
      case '0':
        zpad_width = atoi(++fmt);
        while (*fmt >= '0' && *fmt <= '9') {
          fmt++;
        }
        break;
      default:
        break;
    }

    switch (*fmt) {
      case 's': {
        char *p = va_arg(ap, char *);
        while (*p != '\0') {
          put(*(p++), buf, idx++, maxlen);
        }
        fmt++;
        break;
      }
      case 'd': {
        int x = va_arg(ap, int);
        _itoa(put, buf, x, 10, &idx, maxlen, zpad_width);
        fmt++;
        break;
      }
      case 'u': {
        unsigned int u = va_arg(ap, unsigned int);
        _utoa(put, buf, u, 10, &idx, maxlen, zpad_width);
        fmt++;
        break;
      }
      case 'x': {
        unsigned int u = va_arg(ap, unsigned int);
        _utoa(put, buf, u, 16, &idx, maxlen, zpad_width);
        fmt++;
        break;
      }
      case 'p': {
        uintptr_t u = va_arg(ap, uintptr_t);
        if (u == (uintptr_t)NULL) {
          _put_literal(put, buf, "(null)", &idx, maxlen);
        } else {
          _put_literal(put, buf, "0x", &idx, maxlen);
          _utoa(put, buf, u, 16, &idx, maxlen, zpad_width);
        }
        fmt++;
        break;
      }
      case 'c': {
        char ch = (char)va_arg(ap, int);
        put(ch, buf, idx++, maxlen);
        fmt++;
        break;
      }
      default: {
        put(*fmt, buf, idx++, maxlen);
        fmt++;
        break;
      }
    }
  }

  put(0, buf, idx < maxlen ? idx : maxlen - 1, maxlen);
  return (int)idx;
}

static void _putter_out(char ch, void *buf, size_t idx, size_t maxlen) {
  (void)buf;
  (void)idx;
  (void)maxlen;
  if (ch) {
    putch(ch);
  }
}

static void _putter_buf(char ch, void *buf, size_t idx, size_t maxlen) {
  if (idx < maxlen) {
    ((char *)buf)[idx] = ch;
  }
}

static void _put_literal(putter_t put, void *buf, const char *str,
                         size_t *p_idx, size_t maxlen) {
  while (*str != '\0') {
    put(*str, buf, *p_idx, maxlen);
    str++;
    (*p_idx)++;
  }
}

static void _itoa(putter_t put, char *buf, int num, int base, size_t *p_idx,
                  size_t maxlen, int zpad_width) {
  bool is_neg = false;
  static char str[32] = {0};
  size_t idx = 0;
  zpad_width = zpad_width <= 0 ? 1 : zpad_width;
  if (num < 0 && base == 10) {
    is_neg = true;
    num = -num;
  }
  while (num != 0) {
    int rem = num % base;
    str[idx++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    num /= base;
  }
  while (idx < zpad_width - (is_neg ? 1 : 0)) {
    str[idx++] = '0';
  }
  if (is_neg) {
    str[idx++] = '-';
  }
  _reverse(str, idx);
  for (size_t i = 0; i < idx; i++) {
    put(str[i], buf, (*p_idx)++, maxlen);
  }
}

static void _utoa(putter_t put, char *buf, unsigned int num, int base,
                  size_t *p_idx, size_t maxlen, int zpad_width) {
  static char str[32] = {0};
  size_t idx = 0;
  zpad_width = zpad_width <= 0 ? 1 : zpad_width;
  while (num != 0) {
    unsigned int rem = num % base;
    str[idx++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
    num /= base;
  }
  while (idx < zpad_width) {
    str[idx++] = '0';
  }
  _reverse(str, idx);
  for (size_t i = 0; i < idx; i++) {
    put(str[i], buf, (*p_idx)++, maxlen);
  }
}

static void _reverse(char *str, int length) {
  int start = 0;
  int end = length - 1;
  while (start < end) {
    str[start] ^= str[end];
    str[end] ^= str[start];
    str[start] ^= str[end];
    end--;
    start++;
  }
}

#endif
