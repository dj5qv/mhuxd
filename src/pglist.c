/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#include <stdio.h>
#include <assert.h>
#include "pglist.h"

void PG_NewList(struct PGList *l) {
  l->pl_Head = (void *)&l->pl_Tail;
  l->pl_Tail = NULL;
  l->pl_TailPred = (void *)l;
  assert(l->pl_TailPred);
}

void PG_AddHead(struct PGList *l, struct PGNode *n) {
  n->pn_Pred = (void *)l;
  n->pn_Succ = (void *)l->pl_Head;
  n->pn_Succ->pn_Pred = n;
  l->pl_Head = n;
  assert(l->pl_TailPred);
}

void PG_AddTail(struct PGList *l, struct PGNode *n) {
  l->pl_TailPred->pn_Succ = n;
  n->pn_Pred = l->pl_TailPred;
  n->pn_Succ = (void *)&l->pl_Tail;
  l->pl_TailPred = n;
  assert(l->pl_TailPred);
}

void PG_Insert (struct PGList *l, struct PGNode *nnew, struct PGNode *after)
{
    nnew->pn_Pred = after;
    nnew->pn_Succ = after->pn_Succ;
    after->pn_Succ->pn_Pred = nnew;
    after->pn_Succ = nnew;
    assert(l->pl_TailPred);
}

void PG_Remove(struct PGNode *n) {
  n->pn_Succ->pn_Pred = n->pn_Pred;
  n->pn_Pred->pn_Succ = n->pn_Succ;
}

int PG_Contains(struct PGList *l, struct PGNode *n) {
	struct PGNode *ns;
	PG_SCANLIST(l, ns) {
		if(n == ns)
			return 1;
	}
	return 0;
}

int PG_Count(struct PGList *l) {
	struct PGNode *n;
	int cnt = 0;
	PG_SCANLIST(l,n){
		cnt++;
	}
	return cnt;
}

