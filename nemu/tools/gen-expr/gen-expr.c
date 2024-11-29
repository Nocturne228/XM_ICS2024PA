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

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// this should be enough
static char buf[65536] = {};
static char output_buf[65536] = {};
static char code_buf[65536 + 128] = {};  // a little larger than `buf`
static char *code_format =
    "#include <stdio.h>\n"
    "int main() { "
    "  unsigned result = %s; "
    "  printf(\"%%u\", result); "
    "  return 0; "
    "}";

uint32_t perform(uint32_t n1, uint32_t n2, char op) {
  switch (op) {
    case '+':
      return n1 + n2;
    case '-':
      return n1 - n2;
    case '*':
      return n1 * n2;
    case '/':
      return n1 / n2;
    default:
      return 0;
  }
}

uint32_t choose(uint32_t n) { return rand() % n; }

void gen(char c) {
  char char_buffer[2] = {c, '\0'};
  strcat(buf, char_buffer);
}

char gen_rand_op() {
  switch (choose(4)) {
    case 0:
      gen('+');
      return '+';
    case 1:
      gen('-');
      return '-';
    case 2:
      gen('*');
      return '*';
    case 3:
      gen('/');
      return '/';
  }
  return ' ';
}

uint32_t gen_num() {
  char num_buffer[1024];
  num_buffer[0] = '\0';
  uint32_t number = rand() % 100 + 1;
  sprintf(num_buffer, "%du", number);
  strcat(buf, num_buffer);
  return number;
}

void generate_output() {
  int j = 0;
  for (int i = 0; buf[i] != '\0'; ++i) {
    if (buf[i] != 'u') {
      output_buf[j++] = buf[i];
    }
  }
  output_buf[j] = '\0';
}

#define INSERT_BLANK

static void gen_rand_blank() {
#ifdef INSER_BLANK
  switch (choose(7)) {
    case 0:
    case 1:
    case 2:
    case 3:
      break;
    case 4:
    case 5:
      strcat(buf, " ");
      break;
    case 6:
      strcat(buf, " ");
      break;
  }
#endif
}

static void gen_rand_expr(int depth) {
  // buf[0] = '\0';
  if (strlen(buf) > 65536 - 10000 || depth > 15) {
    gen('(');
    gen_rand_blank();
    gen_num();
    gen_rand_blank();
    gen(')');
    return;
  }

  switch (choose(3)) {
    case 0:
      gen_num();
      gen_rand_blank();
      break;

    case 1:
      gen('(');
      gen_rand_blank();
      gen_rand_expr(depth + 1);
      gen_rand_blank();
      gen(')');
      break;

    default: {
      gen_rand_expr(depth + 1);
      gen_rand_blank();
      gen_rand_op();
      gen_rand_blank();
      gen_rand_expr(depth + 1);
      break;
    }
  }
}

// Bug report:
// Much too long expression
// Too many trivial expression (like single number)
// Negative number
int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i++) {
    buf[0] = '\0';
    gen_rand_expr(0);
    generate_output();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) {
      i--;
      continue;
    }

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    uint32_t result = 0u;
    int b = fscanf(fp, "%u", &result);
    pclose(fp);

    if (b != 1) {
      i--;
      continue;
    }

    printf("%u %s\n", result, output_buf);
  }
  return 0;
}
