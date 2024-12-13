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

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

//? Update the parenthesis count (increment for '(' and decrement for ')')
#define UPDATE_PARENTHESIS(c, tokens, i) \
  do { ((tokens[i].type == '(') ? (c)++ : ((tokens[i].type == ')') ? (c)-- : 0)); } while (0)

//? Check if the count satisfies the given condition, return false if true
#define IF_COND(c, condition) \
  do { if (condition) return false; } while (0)

//? Check if the parentheses count is balanced (i.e., the count is zero)
#define IF_UNBALANCED(c) \
  do { if (c != 0) return false; } while (0)

//? for loop with 2 function
#define FOR_LOOP(p, q, FUN1, FUN2) \
  do { for (int i = p; i <= q; i++) { FUN1; FUN2; } } while (0)

//? Traverse the tokens from index p to q, update count, and check for mismatches
#define PROCESS_PARENS(p, q, c, condition) \
  do { FOR_LOOP(p, q, UPDATE_PARENTHESIS(c, tokens, i), IF_COND(c, condition)); } while (0)

#define CHECK(p, q, c, cond) \
  do { PROCESS_PARENS(p, q, c, cond); IF_UNBALANCED(c); } while (0)

enum {
  TK_NOTYPE = 256,
  /* TODO: Add more token types */
  // * Tokens already added:
  // * ==, !=, num, hex, neg num, &&, ||, *(deref), >=. <=
  TK_EQ,
  TK_NEQ,
  TK_NUMBER,
  TK_HEX,
  TK_REG,
  TK_NEG,
  TK_AND,
  TK_OR,
  TK_DEREF,
  TK_GEQ,
  TK_LEQ,
};

// * Regular rules for possible operators and expression fragments
// * and their corresponding precedence
static struct rule {
  const char *regex;
  int token_type;
  int priority;
} rules[] = {

    /*
     * Pay attention to the precedence level of different rules.
     */
    // TODO: Add more rules.
    {" +", TK_NOTYPE, 10},  // spaces
    {"\\(", '(', 10},
    {"\\)", ')', 10},
    // {"\\$[$,a-z,0-9]+", TK_REG, 10},
    {"\\$(a[0-7]|t[0-6]|s[0-9]|s1[0-1]|ra|sp|gp|tp|pc)", TK_REG, 10},
    {"0x[0-9,a-f,A-F]+", TK_HEX, 10},
    {"[0-9]+", TK_NUMBER, 10},  // number
    {"\\+", '+', 4},            // plus
    {"-", '-', 4},              // minus
    {"\\*", '*', 5},            // multiply
    {"/", '/', 5},              // divide
    {">=", TK_GEQ, 3},
    {"<=", TK_LEQ, 3},
    {"<", '<', 3},
    {">", '>', 3},
    {"==", TK_EQ, 2},  // equal
    {"!=", TK_NEQ, 2},
    {"&&", TK_AND, 1},
    {"\\|\\|", TK_OR, 0},
    {"!", '!', 6},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/*
 * Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
  int priority;
} Token;

static Token tokens[1024] __attribute__((used)) = {};
static int token_count __attribute__((used)) = 0;

//* Iterate through the input string and try to match all the regular expression
// rules
static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  token_count = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s", i,
        //     rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        switch (rules[i].token_type) {
          case TK_NOTYPE:
            break;
          case '+':
          case '-':
          case '*':
          case '/':
          case '(':
          case ')':
            tokens[token_count].type = rules[i].token_type;
            tokens[token_count].priority = rules[i].priority;
            token_count++;
            break;
          case TK_NUMBER:
          case TK_HEX:
            tokens[token_count].type = rules[i].token_type;
            tokens[token_count].priority = rules[i].priority;
            if (substr_len > 32) return false;
            strncpy(tokens[token_count].str, substr_start, substr_len);
            tokens[token_count].str[substr_len] = '\0';
            token_count++;
            break;
          case TK_REG:
            tokens[token_count].type = rules[i].token_type;
            tokens[token_count].priority = rules[i].priority;
            if (substr_len > 32) return false;
            strncpy(tokens[token_count].str, substr_start + 1, substr_len - 1);
            tokens[token_count].str[substr_len - 1] = '\0';
            token_count++;
            break;

          case TK_EQ:
          case TK_NEQ:
          case '>':
          case TK_GEQ:
          case '<':
          case TK_LEQ:
          case TK_AND:
          case TK_OR:
            tokens[token_count].type = rules[i].token_type;
            tokens[token_count].priority = rules[i].priority;
            token_count++;
            break;

          default:
            TODO();
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }
  return true;
}

bool check_subexpr(int p, int q);
int find_dominant_op(int p, int q);
uint32_t eval(int p, int q);
bool check_parentheses(int p, int q);

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  } else {
    *success = true;

    int i;
    for (i = 0; i < token_count; i++) {
      // printf("Get token: \033[1;32;45m%s\033[0m\n", tokens[i].str);
      if (tokens[i].type == '-' &&
          (i == 0 ||
           (tokens[i - 1].type != TK_NUMBER && tokens[i - 1].type != TK_HEX &&
            tokens[i - 1].type != TK_REG && tokens[i - 1].type != ')'))) {
        tokens[i].type = TK_NEG;
        tokens[i].priority = 6;
      }
      if (tokens[i].type == '*' &&
          (i == 0 ||
           (tokens[i - 1].type != TK_NUMBER && tokens[i - 1].type != TK_HEX &&
            tokens[i - 1].type != TK_REG && tokens[i - 1].type != ')'))) {
        tokens[i].type = TK_DEREF;
        tokens[i].priority = 6;
      }
    }

    uint32_t res = eval(0, token_count - 1);
    return res;
  }
}

#include <stdio.h>
#include <stdlib.h>

extern word_t vaddr_read(vaddr_t addr, int len);

uint32_t eval(int p, int q) {
  if (p > q || check_parentheses(p, q) == false) {
    /* Bad expression */
    return 1;
  } else if (p == q) {
    /*
     * Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    switch (tokens[p].type) {
      case TK_NUMBER: {
        uint32_t number;
        number = (uint32_t)atoi(tokens[p].str);
        return number;
      }
      case TK_HEX: {
        uint32_t hex_number;
        hex_number = (uint32_t)strtol(tokens[p].str, NULL, 16);
        return hex_number;
      }
      case TK_REG: {
        uint32_t reg_content;
        bool check_success = true;
        reg_content = isa_reg_str2val(tokens[p].str, &check_success);
        if (check_success == true) {
          return reg_content;
        } else {
          Assert(0, "Wrong register argument!\n");
        }
      }
      case TK_NEG:
      case TK_DEREF:
        return 0;
      default:
        TODO();
    }
  } else if (check_subexpr(p, q)) {
    /*
     * The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    return eval(p + 1, q - 1);
  } else {
    int op = find_dominant_op(p, q);
    uint32_t val1 = eval(p, op - 1);
    uint32_t val2 = eval(op + 1, q);
    // printf("%d\n",op);
    switch (tokens[op].type) {
      case '+':
        return val1 + val2;
      case '-':
        return val1 - val2;
      case '*':
        return val1 * val2;
      case '/':
        // TODO: Handle division by zero error
        if (val2 == 0) {
          printf("\033[1;31mDivision by zero error!\033[0m\n");
          return -1;
        }
        return val1 / val2;
      case TK_NEG: {
        int index_neg = op;
        while (tokens[index_neg].type == TK_NEG && index_neg >= 0) {
          val2 = -val2;
          index_neg--;
        }
        return val2;
      }
      case TK_DEREF: {
        int index_deref = op;
        while (tokens[index_deref].type == TK_DEREF && index_deref >= 0) {
          val2 = vaddr_read(val2, 4);
          index_deref--;
        }
        return val2;
      }
      case TK_AND:
        return val1 && val2;
      case TK_OR:
        return val1 || val2;
      case TK_EQ:
        return val1 == val2;
      case TK_NEQ:
        return val1 != val2;
      case TK_LEQ:
        return val1 <= val2;
      case TK_GEQ:
        return val1 >= val2;
      case '<':
        return val1 < val2;
      case '>':
        return val1 > val2;
      case '!':
        return !val2;
      default:
        assert(0);
    }
  }
}

// // TODO: check_parentheses, check_subexpr, find_next_parentheses are similar
/*
 * Check if the parentheses are legal
 * One-to-one match
 */
bool check_parentheses(int p, int q) {
  int flag = 0;
  CHECK(p, q, flag, flag < 0);
  // CHECK_PARENTHESIS_MATCH(flag);
  return true;
}

/*
 * Eliminating redundant brackets
 */
bool check_subexpr(int p, int q) {
  int check = 0;
  if (tokens[p].type != '(' || tokens[q].type != ')') return false;
  CHECK(p, q, check, check == 0 && i < q);
  // CHECK_PARENTHESIS_MATCH(check);
  return true;
}

int find_next_parenthesis(int pos);

int find_dominant_op(int p, int q) {
  int start = p;
  int end = q;
  int dominant_op = -1;
  int min_priority = INT8_MAX;
  while (start <= end) {
    if (tokens[start].type == '(') {
      start = find_next_parenthesis(start) + 1;
      continue;
    } else if (tokens[start].priority <= min_priority) {
      dominant_op = start;
      min_priority = tokens[start].priority;
    }
    start++;
  }
  return dominant_op;
}

int find_next_parenthesis(int pos) {
  int count = 0;
  for (int i = pos; i < token_count; i++) {
    UPDATE_PARENTHESIS(count, tokens, i);
    if (count == 0) return i;
  }
  return -1;
}

