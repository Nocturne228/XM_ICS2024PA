# PA1.1

# 表达式求值

> 实现步骤：词法分析->递归求值

## 词法分析

词法分析的目的是识别出表达式中的元素，称为**token**，这是每个编译器的必备步骤。比如表达式：

```bash
0x80100000+   ($a0 +5)*4 - *(  $t1 + 8) + number
```

它包含更多的功能，例如十六进制整数(`0x80100000`)，小括号，访问寄存器(`$a0`)，指针解引用(第二个`*`)，访问变量(`number`)，还有一大堆括号和不规则的空格.

对于最简单的**算术表达式**，其token只会有：

- 十进制整数
- `+`，`-`，`*`，`/`
- `(`，`)`
- 空格串(一个或多个空格)

我们需要实现一个`make_token()`函数用于识别表达式中的token，它用`position`变量来指示当前处理到的位置，并且按顺序尝试用不同的规则来匹配当前位置的字符串。当一条规则匹配成功，并且匹配出的子串正好是`position`所在位置的时候，我们就成功地识别出一个token。

关于C语言的正则表达式库：**`&pmatch`**是一个指向 `regmatch_t` 结构体数组的指针，它用于存储匹配结果。`regmatch_t` 结构体包含两个字段：`rm_so`：匹配子串的起始位置、`rm_eo`：匹配子串的结束位置（不包括此位置）。 在这里，`pmatch` 会保存匹配到的子字符串的位置信息。

```c
static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  token_count = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
        
        // 从输入字符串 e 中，当前扫描的 position 位置开始
        // 逐个匹配正则表达式规则，每次最多匹配一个结果
        // 结果存储为pmatch，使用默认行为
        // pmatch.rm_so == 0 的条件确保匹配的子字符串是从 position 位置开始
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 &&
          pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        // Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
        //     i, rules[i].regex, position, substr_len, substr_len,
        //     substr_start);

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
            if (substr_len > 32) return false;	// overflow
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

```

识别出来的token会被记录在`Token`结构体中，其成员分别是代表token类型的`type`和记录token子串内容的字符数组，因为数值、解引用等token的具体信息无法直接从token类型中获取。

```c
typedef struct token {
  int type;
  char str[32];
} Token;
```

框架中给出了各个token类型的枚举定义，每一个扩充都会增强NEMU的表达式计算能力：

```c
enum {
  TK_NOTYPE = 256,
  /* TODO: Add more token types */
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
```

我们需要使用正则表达式分别编写用于识别这些token类型的规则。在框架代码中，一条规则是由正则表达式和token类型组成的二元组。为了便于后续的运算，我们给规则添加一个优先级的编址，这也是编译原理中用到的思想。我们定义的这些规则会在sdb初始化时通过框架提供的`init_regex`编译成一些用于进行pattern匹配的内部信息，因为其会被频繁反复使用，这样可以极大提高效率（空间换时间）。

```c
// * Regular rules for possible operators and expression fragments and their
// corresponding precedence
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
```



## BNF文法

算术表达式的归纳定义：

$$
\begin{aligned}
\text{<expr>}\ ::&=\ \text{<number>}\\
&|\quad\text{(<expr>)}\\
&|\quad\text{<expr>}\ +\ \text{<expr>}\\
&|\quad\text{<expr>}\ -\ \text{<expr>}\\
&|\quad\text{<expr>}\ *\ \text{<expr>}\\
&|\quad\text{<expr>}\ /\ \text{<expr>}\\
\end{aligned}
$$
既然长表达式是由短表达式构成的，我们就先对短表达式求值，然后再对长表达式求值。

## 求值

`eval` 函数负责计算一个给定区间内的表达式值，解析并计算一个由token组成的表达式，递归地分析表达式中的操作符和操作数，逐步将表达式求值并返回最终结果。

为了在token表达式中指示一个子表达式，我们可以使用两个整数`p`和`q`来指示这个子表达式的开始位置和结束位置，分别处理：`p > q`、`p == q`、存在括号、`p < q`四种情况，文档已经给出求值的框架代码。这里我们还要顺便实现一个`check_parentheses()`函数，用于判断表达式是否被一对匹配的括号括住，同时检查括号的合法性。

> 一个问题是，给出一个最左边和最右边不同时是括号的长表达式，我们要怎么正确地将它分裂成两个子表达式？

文档给出了“主运算符”的解决方法，参考手动求解表达式的运算顺序，在表达式人工求值时，最后一步进行运行的运算符即为主运算符。 它指示了表达式的类型（例如当一个表达式的最后一步是减法运算时，我们将其看作是一个减法表达式）根据经验，可以总结出以下几点：

- 非运算符的token不是主运算符.
- 出现在一对括号中的token不是主运算符. 注意到这里不会出现有括号包围整个表达式的情况, 因为这种情况已经在`check_parentheses()`相应的`if`块中被处理了.
- 主运算符的优先级在表达式中是最低的. 这是因为主运算符是最后一步才进行的运算符.
- 当有多个运算符的优先级都是最低时, 根据结合性, 最后被结合的运算符才是主运算符. 一个例子是`1 + 2 + 3`, 它的主运算符应该是右边的`+`.

## vscode变量定义

使用VScode时需要忍受编辑器对各种外部定义的变量标红，也没有自动补全，主要是因为VScode需要编译器来进行IntelliSense，可以在`.vscode`中的`c_cpp_properties.json`中配置：

```json
{
    "configurations": [
        {
            "name": "linux-gcc-x64",
            "includePath": [
                "${workspaceFolder}/**",
                "${workspaceFolder}/nemu/include"
            ],
            "defines": [],
            "compilerPath": "/usr/bin/gcc",
            "cStandard": "${default}",
            "cppStandard": "${default}",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ],
    "version": 4
}
```

