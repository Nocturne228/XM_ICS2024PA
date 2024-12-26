#include <am.h>
#include <klib.h>
#include <klib-macros.h>

#if !defined(__ISA_NATIVE__) || defined(__NATIVE_USE_KLIB__)
static unsigned long int next = 1;

int rand(void) {
  // RAND_MAX assumed to be 32767
  next = next * 1103515245 + 12345;
  return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed) {
  next = seed;
}

int abs(int x) {
  return (x < 0 ? -x : x);
}

int atoi(const char* nptr) {
  int x = 0;
  while (*nptr == ' ') { nptr ++; }
  while (*nptr >= '0' && *nptr <= '9') {
    x = x * 10 + *nptr - '0';
    nptr ++;
  }
  return x;
}

void *malloc(size_t size) {
  // On native, malloc() will be called during initializaion of C runtime.
  // Therefore do not call panic() here, else it will yield a dead recursion:
  //   panic() -> putchar() -> (glibc) -> malloc() -> panic()
#if !(defined(__ISA_NATIVE__) && defined(__NATIVE_USE_KLIB__))
  // Initialize static program break pointer if first call
  static char *program_break = NULL;
  if (program_break == NULL) {
    program_break = (char *)heap.start;
  }

  // Round up size to 8-byte alignment
  size = (size_t)ROUNDUP(size, 8);
  
  // Store old break for return value
  char *old_break = program_break;
  char *new_break = program_break + size;

  // Check heap boundaries
  if ((uintptr_t)new_break > (uintptr_t)heap.end) {
    return NULL;  // Out of memory
  }

  // Update program break
  program_break = new_break;

  // Zero-initialize the allocated memory
  for (uint64_t *p = (uint64_t *)old_break; p < (uint64_t *)new_break; p++) {
    *p = 0;
  }

  return old_break;
#endif
  return NULL;
}

void free(void *ptr) {
}

#endif
