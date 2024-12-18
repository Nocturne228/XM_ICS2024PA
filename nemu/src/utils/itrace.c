#include <common.h>
#include <elf.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_IRINGBUF 16

typedef struct {
  word_t pc;
  uint32_t inst;
} ItraceNode;

ItraceNode iringbuf[MAX_IRINGBUF];
int p_cur = 0;
bool full = false;

void trace_inst(word_t pc, uint32_t inst) {
  iringbuf[p_cur].pc = pc;
  iringbuf[p_cur].inst = inst;
  p_cur = (p_cur + 1) % MAX_IRINGBUF;
  full = full || p_cur == 0;
}

void display_inst() {
  if (!full && !p_cur) return;

  int end = p_cur;
  int i = full ? p_cur : 0;

  void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
  char buf[128];  // 128 should be enough!
  char *p;
  Log("Most recently executed instructions");
  do {
    p = buf;
    p += sprintf(buf, "%s" FMT_WORD ": %08x ",
                 (i + 1) % MAX_IRINGBUF == end ? " --> " : "     ",
                 iringbuf[i].pc, iringbuf[i].inst);
    disassemble(p, buf + sizeof(buf) - p, iringbuf[i].pc,
                (uint8_t *)&iringbuf[i].inst, 4);

    if ((i + 1) % MAX_IRINGBUF == end) printf(ANSI_FG_RED);
    puts(buf);
  } while ((i = (i + 1) % MAX_IRINGBUF) != end);
  puts(ANSI_NONE);
}

void display_pread(paddr_t addr, int len) {
  printf("pread at " FMT_PADDR " len=%d\n", addr, len);
}

void display_pwrite(paddr_t addr, int len, word_t data) {
  printf("pwrite at " FMT_PADDR " len=%d, data=" FMT_WORD "\n", addr, len,
         data);
}

typedef struct {
  char name[32];  // func name, 32 should be enough
  paddr_t addr;
  unsigned char info;
  Elf64_Xword size;
} SymEntry;

SymEntry *symbol_tbl = NULL;  // dynamic allocated
int symbol_tbl_size = 0;
int call_depth = 0;

static int find_symbol_func(paddr_t target, bool is_call) {
  int i;
  for (i = 0; i < symbol_tbl_size; i++) {
    if (ELF64_ST_TYPE(symbol_tbl[i].info) == STT_FUNC) {
      if (is_call) {
        if (symbol_tbl[i].addr == target) break;
      } else {
        if (symbol_tbl[i].addr <= target &&
            target < symbol_tbl[i].addr + symbol_tbl[i].size)
          break;
      }
    }
  }
  return i < symbol_tbl_size ? i : -1;
}

// * FTRACE_WRITE
void trace_func_call(paddr_t pc, paddr_t target) {
  if (symbol_tbl == NULL) return;

  ++call_depth;

  if (call_depth <= 2) return;  // ignore _trm_init & main

  int i = find_symbol_func(target, true);
  log_write(FMT_PADDR ": %*scall [%s@" FMT_PADDR "]\n", pc,
               (call_depth - 3) * 2, "", i >= 0 ? symbol_tbl[i].name : "???",
               target);
}

void trace_func_ret(paddr_t pc) {
  if (symbol_tbl == NULL) return;

  if (call_depth <= 2) return;  // ignore _trm_init & main

  int i = find_symbol_func(pc, false);
  log_write(FMT_PADDR ": %*sret [%s]\n", pc, (call_depth - 3) * 2, "",
            i >= 0 ? symbol_tbl[i].name : "???");

  --call_depth;
}