/***************************************************************************************
* Copyright (c) 2014-2022 Zihao Yu, Nanjing University
*
* NEMU is licensed under Mulan PSL v2.
* You can use this software according to the terms and conditions of the Mulan PSL v2.
* You may obtain a copy of Mulan PSL v2 at:
*          http://license.coscl.org.cn/MulanPSL2
*
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
* EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
* MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
*
* See the Mulan PSL v2 for more details.
***************************************************************************************/

#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>
#include <stdio.h>
#include <string.h>
#include "debug.h"

enum {
  TK_DECIMAL = 16,
  TK_NOTYPE = 256,
  TK_EQ,
  TK_PLUS,
  TK_MINUS,
  TK_MULTIPLY,
  TK_DIVIDE,
  TK_NEGTIVE,
  TK_LEFT_PA,
  TK_RIGHT_PA,
  /* TODO: Add more token types */

};

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", TK_PLUS},         // plus
  {"==", TK_EQ},        // equal
  {"\\-",TK_MINUS},
  {"\\*", TK_MULTIPLY},
  {"/", TK_DIVIDE},
  {"\\(", TK_LEFT_PA},
  {"\\)", TK_RIGHT_PA},
  {"[[:digit:]]+",TK_DECIMAL},
};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
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
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add codes
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        switch (rules[i].token_type) {
          case TK_NOTYPE: break;
          case TK_DECIMAL: Assert(substr_len <= 32, "%s", "tokens len over flow");
             strncpy(tokens[nr_token].str, substr_start,substr_len);
          default: tokens[nr_token].type = rules[i].token_type;
              nr_token++;
        }

        break;
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  //regnize negtive operator
  for (i = 0; i < nr_token; i++) {
      if ((i == 0 && tokens[i].type == TK_MINUS)// first
          || (tokens[i].type == TK_MINUS && tokens[i-1].type >= TK_PLUS
              && tokens[i-1].type <= TK_NEGTIVE)){
          tokens[i].type = TK_NEGTIVE;
      }
  }
  return true;
}

int findMainOperator(Token* tokens, int start, int end) {
    int pos = start;
    int in_parenthes = 0;
    for(int i = start; i <= end; i++){
        if (tokens[i].type == TK_DECIMAL || tokens[i].type == TK_NOTYPE)
            continue;
        if (tokens[i].type == TK_LEFT_PA) {
            in_parenthes = 1;
            continue;
        }else if (tokens[i].type == TK_RIGHT_PA) {
            in_parenthes = 0;
            continue;
        }else if (in_parenthes == 1) {
            continue;
        }
        if (pos == start){
            pos = i;
        } else {
            if (tokens[i].type <= tokens[pos].type)
                pos = i;
        }
    }

    return pos;
}

int check_parentheses(Token* tokens,int p, int q) {
//    Log("check path p %d, q %d t %d", p,q, tokens[p].type);
    if (tokens[p].type != TK_LEFT_PA || tokens[q].type != TK_RIGHT_PA)
        return -1;
    int s = p + 1, e = q -1;
    while (s < e) {
      for(;tokens[s].type != TK_LEFT_PA; s++)
        if (s == q) break;
      for(;tokens[e].type != TK_RIGHT_PA;e--)
        if (e == p) break;
        //nested parentheses
      if(tokens[s].type == TK_LEFT_PA && tokens[e].type == TK_RIGHT_PA) {
        s++;
        e--;
      }
      // no more parentheses
      if (s >= e && s != q && e != p)
        return -1;
    }

    if (s == q && e == p)
        return 0;
    else
        return -1;
}

uint32_t eval(Token* tokens,int p, int q, bool *succ) {
  if (p > q) {
    /* Bad expression */
    *succ = false;
    Assert(-1,"bad expressions for eval");
  }
  else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */

    uint32_t num = 0;
    if (sscanf(tokens[p].str, "%d", &num) < 0){
        Assert(-1,"wrong args for eval numbers %s", tokens[p].str);
    }

    return num;
  } else if (check_parentheses(tokens,p, q) == 0) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
      return eval(tokens, p + 1, q - 1,succ);
  }
  else {
    int op = findMainOperator(tokens,p,q);
    //  Log("main pos is %d", op);
    if (op == p && tokens[op].type == TK_NEGTIVE) {
        return (-1) * eval(tokens, op+1, q,succ);
    }else {
        uint32_t val1 = eval(tokens, p, op - 1,succ);
        uint32_t val2 = eval(tokens, op + 1, q,succ);
        //  Log("op type %d, pos %d", tokens[op].type, op);
      switch (tokens[op].type) {
        case TK_PLUS: return val1 + val2;
        case TK_MINUS: return val1 - val2;
        case TK_MULTIPLY: return val1 * val2;
        case TK_DIVIDE: return val1 / val2;
        default: assert(0);
      }
    }
  }
  return 0;
  // Assert(0, "should not reach here");
}

word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  return eval(tokens, 0, nr_token-1, success);

}
