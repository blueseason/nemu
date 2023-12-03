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

#include "sdb.h"

#define NR_WP 32


static WP wp_pool[NR_WP] = {};
static WP *head = NULL, *free_ = NULL;

void init_wp_pool() {
  int i;
  for (i = 0; i < NR_WP; i ++) {
    wp_pool[i].NO = i;
    wp_pool[i].next = (i == NR_WP - 1 ? NULL : &wp_pool[i + 1]);
    wp_pool[i].expr = NULL;
    wp_pool[i].value = 0;
    wp_pool[i].changed = 0;
  }

  head = NULL;
  free_ = wp_pool;
}

/* TODO: Implement the functionality of watchpoint */

WP* new_wp(char* e) {
    if ( free_ == NULL) {
        Assert(0, "watch point used up");
    }

    WP* wp = free_;
    free_ = free_->next;

    wp -> next = NULL;

    if (head == NULL) {
        head = wp;
    }else {
        WP* current = head;
        while (current -> next != NULL) current = current->next;
        current -> next = wp;
    }

    int len = strlen(e);
    wp->expr = malloc(len+1);
    strncpy(wp->expr, e, len);
    bool succ = 0;
    wp->value = expr(wp->expr,&succ);

    return wp;
}

void free_wp(WP* wp) {
    Assert(head != NULL,"watch point head is null");
    WP* curr = head, *before = NULL;
    while (curr != NULL) {
        if (curr == wp) {
            if (before == NULL) {
                head = curr->next;
            }else {
                before->next = curr->next;
            }
            curr->next = NULL;
            break;
        }
        before = curr;
        curr = curr->next;
    }

    if (free_ == NULL) {
        free_ = curr;
    }else {
        WP* c = free_;
        while (c->next != NULL) c=c->next;
        c->next = curr;
    }

    free(curr->expr);
}

int delete_wp(int n) {
    WP* curr = head;
    while (curr != NULL) {
        if (curr->NO == n)
            break;
        curr = curr->next;
    }

    free_wp(curr);

    return 0;
}

int check_wp() {
    int changed = 0;
    bool succ;
    WP* curr = head;
    while (curr != NULL) {
        if(curr->value != expr(curr->expr, &succ)){
            changed = 1;
            curr->changed = 1;
        }

        curr = curr -> next;
    }

    return changed;
}

void display_wp() {
    WP* curr = head;
    if (curr == NULL) {
        printf("No watchpoints\n");
        return;
    }else {
        printf("NO      Expr          Old Value\n");
    }
    while (curr != NULL) {
        printf("%d%10s%10d\n",curr->NO, curr->expr, curr->value);
        curr = curr->next;
    }
}
