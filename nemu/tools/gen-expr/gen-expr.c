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

#include <readline/readline.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>
#include <setjmp.h>
#include <signal.h>
#include <fenv.h>

// this should be enough
static char buf[65536] = {};
static int len = 0;
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";


uint32_t choose(uint32_t n) {
    uint32_t res = rand() % n;
    //printf("gen_num %d \n", res);

    return res;
}

void gen_space(){
    int res = choose(2);
    if(res == 1) {
        int n = choose(20);
        char str[n+1];
        for(int i = 0; i < n; i++){
            str[i] = ' ';
        }
        str[n] = '\0';
        sprintf(buf + len, "%s", str);
        len += n;
    }
}

void gen_num() {
    int n = -1;
    if ((n = sprintf(buf + len, "%u", rand())) > 0)
        len += n;
    gen_space();
//    printf("gen_num %d %s \n",n, buf);
}

void gen(char c) {
    buf[len] = c;
    len++;
    //  printf("gen char %c buf %s \n",c, buf);
    gen_space();
}
void gen_rand_op(){
    int res = choose(4);
    switch (res) {
    case 0: gen('+');break;
    case 1: gen('-');break;
    case 2: gen('*');break;
    case 3: gen('/');break;
    }
//    printf("gen op %d buf %s \n",res, buf);
}

static void gen_rand_expr() {
//  buf[0] = '\0';
    //  int n = choose(3);
//    printf("choose %d \n", n);
    switch (choose(3)) {
    case 0: gen_num(); break;
    case 1: gen('('); gen_rand_expr(); gen(')');break;
    default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
  }
}

jmp_buf recovery;

void handle_divide_by_zero(int sig)
{
    // Re-assign the signal handler
    signal(SIGFPE, handle_divide_by_zero);
    printf("Error: Division by zero is not allowed.\n");
    // Jump to the recovery point
    longjmp(recovery, 1);
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  signal(SIGFPE, handle_divide_by_zero);
  for (i = 0; i < loop; i ++) {
    len = 0;
    int recovery_status;
    recovery_status = setjmp(recovery);
    if (recovery_status == 0) {
      // Clear all floating-point exceptions
      feclearexcept(FE_ALL_EXCEPT);

      gen_rand_expr();
      sprintf(code_buf, code_format, buf);

      FILE *fp = fopen("/tmp/.code.c", "w");
      assert(fp != NULL);
      fputs(code_buf, fp);
      fclose(fp);

      int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
      if (fetestexcept(FE_DIVBYZERO)) {
        // Raise the SIGFPE signal
         raise(SIGFPE);
      }
      if (ret != 0) continue;

      fp = popen("/tmp/.expr", "r");
      assert(fp != NULL);

      int result;
      ret = fscanf(fp, "%d", &result);
      pclose(fp);

      printf("%u %s\n", result, buf);
    }
  }
  return 0;
}
