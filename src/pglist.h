/*
 *  mhuxd - mircoHam device mutliplexer/demultiplexer
 *  Copyright (C) 2012-2013  Matthias Moeller, DJ5QV
 *
 *  This program can be distributed under the terms of the GNU GPLv2.
 *  See the file COPYING
 */


#ifndef PGLIST_H
#define PGLIST_H


struct PGList {
  struct PGNode  *pl_Head;
  struct PGNode  *pl_Tail;
  struct PGNode *pl_TailPred;
};
struct PGNode {
  struct PGNode *pn_Succ;
  struct PGNode *pn_Pred;
};

void PG_NewList(struct PGList *);
void PG_AddTail(struct PGList *, struct PGNode *);
void PG_AddHead(struct PGList *, struct PGNode *);
void PG_Insert (struct PGList *, struct PGNode *new, struct PGNode *after);
void PG_Remove(struct PGNode *);
int PG_Contains(struct PGList *l, struct PGNode *n);
int PG_Count(struct PGList *l);
int PG_CheckedRemove(struct PGList *l, struct PGNode *n);

#define PG_SCANLIST(l, n) \
   for(n=(void*)(l)->pl_Head;     \
       NULL!=((struct PGNode *)n)->pn_Succ; \
       n=(void *)((struct PGNode *)n)->pn_Succ)

#define PG_LISTEMPTY(l) \
     ((l)->pl_Head->pn_Succ == NULL)

#define PG_FIRSTENTRY(l) \
     (PG_LISTEMPTY(l) ? NULL : (l)->pl_Head)

#define PG_LASTENTRY(l) \
     (PG_LISTEMPTY(l) ? NULL : (l)->pl_TailPred)



#endif // PGLIST_H
