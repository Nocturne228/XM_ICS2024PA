#include <common.h>
#include <utils.h>

#define mtrace_write printf
#ifdef CONFIG_MTRACE
void display_pread(paddr_t addr, int len) {
  mtrace_write(ANSI_FG_CYAN "MTRACE: pread at " FMT_PADDR " len=%d\n" ANSI_NONE, addr, len);
}

void display_pwrite(paddr_t addr, int len, word_t data) {
  mtrace_write(ANSI_FG_CYAN "MTRACE: pwrite at " FMT_PADDR " len=%d, data=" FMT_WORD "\n" ANSI_NONE, addr, len, data);
}
#endif