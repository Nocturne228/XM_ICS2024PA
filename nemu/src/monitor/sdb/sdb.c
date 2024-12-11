/***************************************************************************************
 * Copyright (c) 2014-2024 Zihao Yu, Nanjing University
 *
 * NEMU is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan
 *PSL v2. You may obtain a copy of Mulan PSL v2 at:
 *          http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 *KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 *NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
 *
 * See the Mulan PSL v2 for more details.
 ***************************************************************************************/

#include "sdb.h"

#include <cpu/cpu.h>
#include <isa.h>
#include <readline/history.h>
#include <readline/readline.h>

#include "memory/paddr.h"

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

word_t vaddr_read(vaddr_t addr, int len);

/* We use the `readline' library to provide more flexibility to read from stdin.
 */
static char *rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}

static int cmd_q(char *args) {
  nemu_state.state = NEMU_QUIT;
  return -1;
}

// 24/11/24
// Single instruction execute
// Usage: si or si <N>
static int cmd_si(char *args) {
  int step = 0;
  if (args == NULL) {
    step = 1;
  } else {
    sscanf(args, "%d", &step);  // scan in and parse
  }
  cpu_exec(step);
  return 0;
}

// 24/11/24
// Print Program Status
// Usage: info r(egister) or info w(atchpoint)
static int cmd_info(char *args) {
  char *arg = strtok(NULL, " ");
  if (arg == NULL) {
    printf("Arg required: <r> or <w>\n");
  } else if (strcmp(args, "r") == 0) {
    isa_reg_display();
  } else if (strcmp(args, "w") == 0) {
    // TODO: watchpoint
    // printf("sdb_watchpoint_display() called\n");
    display_wp();
  } else {
    printf("Wrong arg or number exceeds\n");
  }
  return 0;
}

// 24/11/24
// Scan virtual memory
// Usage: x N Expr
// TODO error process (fatal when out of bound)
static int cmd_x(char *args) {
  char *cnt_str = strtok(args, " ");
  // printf("count received: %s\n", cnt_str);
  if (cnt_str != NULL) {
    int cnt = atoi(cnt_str);
    char *addr_str = strtok(NULL, "");

    bool success = false;
    // printf("addr received: %s\n", addr_str);
    int addr = expr(addr_str, &success);
    // printf("addr evaluated: 0x%x\n", addr);
    if (success == false) {
      printf("Invalid Expression\n");
      return 0;
    }

    printf("%-14s%-28s%-s\n", "Address", "Hexadecimal", "Decimal");
    for (int i = 0; i < cnt; ++i) {
      printf("0x%-12x0x%02x  0x%02x  0x%02x  0x%02x", (addr),
             vaddr_read(addr, 1), vaddr_read(addr + 1, 1),
             vaddr_read(addr + 2, 1), vaddr_read(addr + 3, 1));
      printf("\t  %04d  %04d  %04d  %04d\n", vaddr_read(addr, 1),
             vaddr_read(addr + 1, 1), vaddr_read(addr + 2, 1),
             vaddr_read(addr + 3, 1));
      addr += 4;
    }
  }
  return 0;
}

// 24/11/25
// Evaluate expression
// // TODO: Bug fix: Fixed
// A single number parenthesed will be treated as invalid syntax
// Can't handle long expression

static int cmd_p(char *args) {
  if (args == NULL) return 0;

  bool success = false;
  int res = expr(args, &success);
  // printf("Received expression: %s\n", args);
  if (success == false)
    printf("Invalid Expression\n");
  else {
    printf("%-20s%-s\n", "Decimal", "Hexadecimal");
    printf("%-20u%#-10x\n", res, res);
  }
  return 0;
}

static int cmd_w(char *args) {
  if (args == NULL) return 0;
  if (new_wp(args) == NULL) printf("the watch_point_pool is full!\n");
  return 0;
}

static int cmd_d(char *args) {
  if (args == NULL) return 0;

  free_wp(atoi(args));
  return 0;
}

static int cmd_test(char *args) {
  int correct_count = 0;
  FILE *input_file =
      fopen("/home/xiaoma/ics2024/nemu/tools/gen-expr/input", "r");
  if (input_file == NULL) {
    perror("Error opening input file");
    return 1;
  }

  char record[1024];
  unsigned real_val;
  char buf[1024];

  int tests = 100;
  if (args == NULL) {
    tests = 100;
  } else {
    sscanf(args, "%d", &tests);  // scan in and parse
  }

  for (int i = 0; i < tests; i++) {
    if (fgets(record, sizeof(record), input_file) == NULL) {
      perror("Error reading input file");
      break;
    }

    char *token = strtok(record, " ");
    if (token == NULL) {
      printf("Invalid record format\n");
      continue;
    }
    real_val = atoi(token);

    strcpy(buf, "");
    while ((token = strtok(NULL, "\n")) != NULL) {
      strcat(buf, token);
      strcat(buf, " ");
    }

    // printf("Real Value: %u, Expression: %s\n", real_val, buf);
    bool flag = false;
    unsigned res = expr(buf, &flag);
    if (res == real_val) {
      correct_count++;
    } else {
      printf("\033[1;32mExpression\033[0m: %s\n\033[1;31mGot\033[0m: %u, \033[1;31mexpected\033[0m: %u\n\n", buf, res, real_val);
    }
  }

  float accurrency = (double)correct_count / tests;

  printf("%d samples tested, \033[1;32maccuracy\033[0m is %d/%d = \033[1;32m%f%%\033[0m\n", tests, correct_count, tests, accurrency * 100);
  if (accurrency == 1.0) {
    printf("All tests passed!\n");
  }
  fclose(input_file);
  return 0;
}

static int cmd_help(char *args);

static struct {
  const char *name;
  const char *description;
  int (*handler)(char *);
} cmd_table[] = {
    {"help", "Display information about all supported commands", cmd_help},
    {"c", "Continue the execution of the program", cmd_c},
    {"q", "Exit NEMU", cmd_q},

    /* TODO: Add more commands */
    {"si", "Step through <N> instructions", cmd_si},
    {"info", "Print status of program (Registers & Watchpoint)", cmd_info},
    {"x", "Scan virtual memory based on expr and length", cmd_x},
    {"p", "Evaluate expression", cmd_p},
    {"w",
     "Suspend program execution when the value of the expression EXPR changes",
     cmd_w},
    {"d", "Delete the watchpoint with sequence number n", cmd_d},
    {"test", "Test the correctness of evalution command", cmd_test},
};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  } else {
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

void sdb_set_batch_mode() { is_batch_mode = true; }

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL;) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) {
      continue;
    }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) {
          return;
        }
        break;
      }
    }

    if (i == NR_CMD) {
      printf("Unknown command '%s'\n", cmd);
    }
  }
}

void init_sdb() {
  /* Compile the regular exprs. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
