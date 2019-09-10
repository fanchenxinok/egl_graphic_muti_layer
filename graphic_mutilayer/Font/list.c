#include <stdlib.h> 
#include <stdio.h> 
#include <errno.h> 
#include <string.h>
#include "list.h"

stList* list_create()
{
	stList *pList = (stList*)malloc(sizeof(stList));
	stListNode *pHead = (stListNode*)malloc(sizeof(stListNode));
	if(pList && pHead) {
	    memset(pHead, 0, sizeof(stListNode));
	    pHead->pNext = NULL;
	    pList->pHead = pHead;
	    pList->pRear = pHead;
	    pList->cnt = 0;
		lw_lock_init(&pList->lock);
	    return pList;
	}
	return NULL;
}

void list_destory(stList *pList)
{
	if(!pList) return;
	lw_lock(&pList->lock);
	stListNode *pHead = pList->pHead;
	stListNode *p = pHead;
	while(p != NULL){
		pHead= pHead->pNext;
		if(p->data) free(p->data);
		free(p);
		p = pHead;
	}
	lw_unlock(&pList->lock);
	free(pList);
	pList = NULL;
}

void list_insert_first(stList* pList, void** data)
{
    if(!pList || !data) return;
	lw_lock(&pList->lock);
    stListNode* pNew = (stListNode*)malloc(sizeof(stListNode));
    pNew->data = (void*)*data;
    pNew->pNext = pList->pHead->pNext;
    pList->pHead->pNext = pNew;
    pList->cnt++;
	lw_unlock(&pList->lock);
}

void list_insert_last(stList* pList, void **data)
{
    if(!pList || !data) return;
	lw_lock(&pList->lock);
    stListNode* pNew = (stListNode*)malloc(sizeof(stListNode));
    pNew->data = (void*)*data;
    pNew->pNext = NULL;
    pList->pRear->pNext = pNew;
    pList->pRear = pNew;
    pList->cnt++;
	lw_unlock(&pList->lock);
}

void list_delete_node(stList* pList, void **data)
{
	if(!pList || !data) return;
	lw_lock(&pList->lock);
	stListNode *pPrev = pList->pHead;
	stListNode *pCur = pPrev->pNext;
	while(pCur){
		if(pCur->data == *data) {
			if(pCur->pNext == NULL) { /* if cur node is rear node */
				pList->pRear = pPrev;
			}
			pPrev->pNext = pCur->pNext;
			pList->cnt--;
			free(pCur->data);
			pCur->data = NULL;
			free(pCur);
			pCur = NULL;
			break;
		}
		pPrev = pCur;
		pCur = pCur->pNext;
	}
	lw_unlock(&pList->lock);
}
