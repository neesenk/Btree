/**
 * author: Zhiyong Liu<neesenk@gmail.com>
 */

#ifndef _BTREE_H_
#define _BTREE_H_

#include <stdlib.h>
#include <stdio.h>
#include <strings.h>

#define VALTYPE	long
#define TYPE_CMP(a, b) (*(a) > *(b) ? 1 : (*(a) == *(b)) ? 0 : -1)

#ifndef BTREE_BSEARCH_MIN_SIZE
#define BTREE_BSEARCH_MIN_SIZE 16
#endif

#define BTREE_ITER_MAX_DEPTH	(sizeof(long) * 8)

typedef VALTYPE val_t;
typedef struct bnode btree_node_t;

typedef struct {
	unsigned short min_degree; // Btree的最小度数
	btree_node_t   *root;	   // Btree的根节点
} btree_t;

typedef struct  {
	int		size;				// 迭代器保存节点的深度
	int		idx[BTREE_ITER_MAX_DEPTH];	// 当前遍历节点的位置
	btree_node_t	*sub[BTREE_ITER_MAX_DEPTH];	// 当前遍历的节点指针
} btree_iter_t;

btree_t *btree_create(unsigned short min_degree);
int btree_insert(btree_t *btree, val_t *key_val);
int btree_delete(btree_t *btree, val_t *key);
val_t *btree_search(btree_t *btree, val_t *key);
void btree_destroy(btree_t *btree);
val_t *btree_min(btree_t *btree);
val_t *btree_max(btree_t *btree);
val_t *btree_first(btree_t *btree, val_t *v, btree_iter_t *iter);
val_t *btree_last(btree_t *btree, val_t *v, btree_iter_t *iter);
val_t *btree_next(btree_t *btree, btree_iter_t *iter);
val_t *btree_prev(btree_t *btree, btree_iter_t *iter);

#define BTREE_FOREACH(btree, beg, end, iter)					\
	for (end = btree_first(btree, end, iter), beg = btree_first(btree, beg);\
	     beg && beg != end;							\
	     beg = btree_next(btree, iter))

#endif
