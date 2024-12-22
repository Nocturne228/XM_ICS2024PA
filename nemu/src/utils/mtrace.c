#include <common.h>
#include <utils.h>

void display_pread(paddr_t addr, int len) {
  printf(ANSI_FG_CYAN "MTRACE: pread at " FMT_PADDR " len=%d\n" ANSI_NONE, addr, len);
}

void display_pwrite(paddr_t addr, int len, word_t data) {
  printf(ANSI_FG_CYAN "MTRACE: pwrite at " FMT_PADDR " len=%d, data=" FMT_WORD "\n" ANSI_NONE, addr, len, data);
}