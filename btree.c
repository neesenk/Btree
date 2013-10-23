/**
 * author: Zhiyong Liu<neesenk@gmail.com>
 */

#include "btree.h"
#include <string.h>
#include <assert.h>
#include <errno.h>

#define ARRAY_MOVE(a,i,b,l) memmove((a)+(i),(a)+(b),sizeof(*a)*((l)-(b)))
#define ARRAY_INSERT(a,b,l,n) do {ARRAY_MOVE(a,(b)+1,b,l); (a)[b]=(n);} while(0)
#define ISLEAF(n) ((n)->subs == NULL)

typedef struct bnode {
        unsigned short	size;	    // 节点包含的元素数目
	unsigned short  min_degree; // 最小度
        struct bnode	**subs;	    // 下级子树, 可以包含2t个节点; 如果是页节点为NULL
        val_t		vals[];     // 节点元素
} node_t;

typedef btree_iter_t iter_t;

static inline node_t *btree_node_create(unsigned int min_degree, int is_leaf)
{
	node_t **subs = NULL;
	node_t *node = calloc(1, sizeof(node_t) + (2 * min_degree - 1) * sizeof(val_t));
	if (!is_leaf)
		subs = (node_t **) calloc(2 * min_degree, sizeof(node_t *));

	if (!node || !(is_leaf || subs)) {
		free(node);
		free(subs);
		return NULL;
	}

	node->subs = subs;
	node->min_degree = min_degree;
	node->size = 0;

	return node;
}

static inline void btree_node_destroy(node_t *node)
{
	free(node->subs);
	free(node);
}

static int btree_node_search(val_t *arr, int len, val_t *n, int *ret)
{
	int m = 0, d = 0, cmp = 0;
	if (len > BTREE_BSEARCH_MIN_SIZE) {
		while (m < len) {
			d = (len + m) / 2;
			if ((cmp = TYPE_CMP(arr + d, n)) == 0) {
				*ret = d;
				return 1;
			}
			if (cmp < 0)
				m = d + 1;
			else
				len = d;
		}
	} else {
		for (m = 0; m < len; m++) {
			if ((cmp = TYPE_CMP(arr + m, n)) >= 0)
				break;
		}

		if (m < len && cmp == 0) {
			*ret = m;
			return 1;
		}
	}
	*ret = m;
	return 0;
}

static int btree_split_child(int t, node_t *parent, int idx, node_t *child)
{
	node_t *n = btree_node_create(t, ISLEAF(child));
	if (!n)
		return -1;

	// 将child的右半部分拷贝到新的节点
	memcpy(n->vals, child->vals + t, sizeof(*n->vals) * (t - 1));
	if (n->subs)
		memcpy(n->subs, child->subs + t, sizeof(*n->subs) * t);
	n->size = t - 1;

	// 直接修改现有的元素个数就可以了
	child->size = t - 1;

	// 将分裂的新节点插入到正确位置
	ARRAY_INSERT(parent->vals, idx, parent->size, child->vals[t - 1]);
	ARRAY_INSERT(parent->subs, idx + 1, parent->size + 1, n);
	parent->size++;

	return 0;
}

int btree_insert(btree_t *btree, val_t *val)
{
	node_t *child = NULL, *node = btree->root;
	int t = btree->min_degree, idx;

	if (node->size == (2 * t - 1)) {
		if ((child = btree_node_create(t, 0)) == NULL)
			return -1;

		child->subs[0] = node, node = btree->root = child;
		if (btree_split_child(t, child, 0, child->subs[0]) < 0)
			return -1;
	}

	for (;;) {
		if (btree_node_search(node->vals, node->size, val, &idx))
			return -1;
		if (ISLEAF(node)) {
			ARRAY_INSERT(node->vals, idx, node->size, *val);
			node->size++;
			break;
		}

		if (node->subs[idx]->size == (2 * t - 1)) {
			if (btree_split_child(t, node, idx, node->subs[idx]) < 0)
				return -1;
			if (TYPE_CMP(val, node->vals + idx) > 0)
				idx++;
		}

		node = node->subs[idx];
	}

	return 0;
}

static inline val_t *__btree_max(node_t *subtree)
{
	assert(subtree && subtree->size > 0);
	for (;;) {
		if(ISLEAF(subtree) || !subtree->subs[subtree->size])
			return &subtree->vals[subtree->size - 1];
		subtree = subtree->subs[subtree->size];
	}
}

static inline val_t *__btree_min(node_t *subtree)
{
	assert(subtree && subtree->size > 0);
	for (;;) {
		if(ISLEAF(subtree) || !subtree->subs[0])
			return subtree->vals;
		subtree = subtree->subs[0];
	}
}

static inline node_t *btree_merge_siblings(btree_t *btree, node_t *parent, int idx)
{
	int t = btree->min_degree;
	node_t *n1, *n2;

	if (idx == parent->size)
		idx--;
	n1 = parent->subs[idx];
	n2 = parent->subs[idx + 1];

	assert(n1->size + n2->size + 1 < 2 * t);

	// 合并n1, parent->vals[idx], n2成一个大的节点
	memcpy(n1->vals + t, n2->vals, sizeof(*n1->vals) * (t - 1));
	if (n1->subs)
		memcpy(n1->subs + t, n2->subs, sizeof(*n1->subs) * t);
	n1->vals[t - 1] = parent->vals[idx];
	n1->size += n2->size + 1;

	// 将合并后的节点插入到parent的正确位置
	ARRAY_MOVE(parent->vals, idx, idx + 1, parent->size);
	ARRAY_MOVE(parent->subs, idx + 1, idx + 2, parent->size + 1);
	parent->subs[idx] = n1;
	parent->size--;

	// 如果root节点只有一个节点, 降低树高
	if (parent->size == 0 && btree->root == parent) {
		btree_node_destroy(parent);
		btree->root = n1;
	}

	btree_node_destroy(n2);
	return n1;
}

static void move_left_to_right(node_t *parent, int idx)
{
	// 将idx位置的数据移动到右节点最左边，从idx的左节点移动最大的key到idx位置，
	node_t *left = parent->subs[idx], *right = parent->subs[idx + 1];
	ARRAY_MOVE(right->vals, 1, 0, right->size);
	right->vals[0] = parent->vals[idx];
	parent->vals[idx] = left->vals[left->size - 1];

	if (right->subs) {
		ARRAY_MOVE(right->subs, 1, 0, right->size + 1);
		right->subs[0] = left->subs[left->size];
	}

	left->size--;
	right->size++;
}

static void move_right_to_left(node_t *parent, int idx)
{
	// 将idx位置的数据移动到左节点最右边，从idx的右节点移动最小的key到idx位置，
	node_t *left = parent->subs[idx], *right = parent->subs[idx + 1];
	left->vals[left->size] = parent->vals[idx];
	parent->vals[idx] = right->vals[0];
	ARRAY_MOVE(right->vals, 0, 1, right->size);

	if (right->subs) {
		left->subs[left->size + 1] = right->subs[0];
		ARRAY_MOVE(right->subs, 0, 1, right->size + 1);
	}

	right->size--;
	left->size++;
}

static int __btree_delete(btree_t *btree, node_t *sub, val_t *key)
{
	int idx, t = btree->min_degree;

	for (;;) {
		node_t *parent;
		if (btree_node_search(sub->vals, sub->size, key, &idx))
			break;
		if (ISLEAF(sub))
			return -1;

		parent = sub, sub = sub->subs[idx];
		assert(sub != NULL);
		if (sub->size > t - 1)
			continue;

		// 3: key不在内部节点中，必须保证其查询路径上的父节点的元素都大于t-1个
		// 3.a: 如果兄弟节点的元素个数大于t-1, 从兄弟节点提升一个节点到父节点
		// 在将父节点idx上的元素下降到这个节点上保证大于t-1个元素
		// 3.b: 兄弟节点的元素个数都是t-1, 合并成一个新的节点b:
		if (idx < parent->size && parent->subs[idx + 1]->size > t - 1)
			move_right_to_left(parent, idx);
		else if (idx > 0 && parent->subs[idx - 1]->size > t - 1)
			move_left_to_right(parent, idx - 1);
		else
			sub = btree_merge_siblings(btree, parent, idx);
	}
LOOP:
	if (ISLEAF(sub)) { // 1: 为页节点; a)在根节点上 b)元素个数大于t - 1
		assert(sub == btree->root || sub->size > t - 1);
		ARRAY_MOVE(sub->vals, idx, idx + 1, sub->size); // 将idx之后的数据往前移
		sub->size--;
	} else { // 2: 在内部节点上
		if (sub->subs[idx]->size > t - 1) {
			// 2.a: 小于Key的子树的根节点包含了大于t - 1个元素,找到子树的
			// 最大元素替换当前元素，并从子树中删除
			sub->vals[idx] = *__btree_max(sub->subs[idx]);
			__btree_delete(btree, sub->subs[idx], sub->vals + idx);
		} else if (sub->subs[idx + 1]->size > t - 1) {
			// 2.b: 大于Key的子树的根节点包含了大于t - 1个元素,找到子树的
			// 最小元素替换当前元素，并从子树中删除
			sub->vals[idx] = *__btree_min(sub->subs[idx + 1]);
			__btree_delete(btree, sub->subs[idx + 1], sub->vals + idx);
		} else {
			// 2.c: 小于和大于Key的子树都只包含了t - 1个元素, 那么将lt, key
			// 和gt都合并到一个节点上, 然后递归删除
			assert(sub->subs[idx]->size==t-1 && sub->subs[idx+1]->size == t-1);
			sub = btree_merge_siblings(btree, sub, idx);
			idx = t - 1;
			goto LOOP;
		}
	}

	return 0;
}

int btree_delete(btree_t *btree, val_t *key)
{
	return __btree_delete(btree, btree->root, key);
}

static void __btree_destroy(node_t *n)
{
	if (n) {
		if (!ISLEAF(n)) {
			int i = 0;
			for (i = 0; i < n->size + 1; i++)
				__btree_destroy(n->subs[i]);
		}
		btree_node_destroy(n);
	}
}

void btree_destroy(btree_t *btree)
{
	if (btree) {
		__btree_destroy(btree->root);
		free(btree);
	}
}

btree_t *btree_create(unsigned short min_degree)
{
	btree_t *btree = calloc(1, sizeof(*btree));
	if (btree) {
		btree->min_degree = min_degree;
		btree->root = btree_node_create(min_degree, 1);
	}
	return btree;
}

val_t *btree_search(btree_t *btree, val_t *key)
{
	node_t *node = btree->root;
	for (;;) {
		int i = 0;
		if (btree_node_search(node->vals, node->size, key, &i))
			return node->vals + i;
		if (ISLEAF(node))
			return NULL;
		node = node->subs[i];
	}

	return NULL;
}

val_t *btree_max(btree_t *btree)
{
	return __btree_max(btree->root);
}

val_t *btree_min(btree_t *btree)
{
	return __btree_min(btree->root);
}

static inline void btree_iter_init(btree_t *btree, val_t *v, iter_t *iter, int n)
{
	node_t *node = btree->root;
	iter->size = -1;
	for (;;) {
		int idx = 0;
		int r = btree_node_search(node->vals, node->size, v, &idx);

		iter->size++;
		assert(iter->size < BTREE_ITER_MAX_DEPTH);
		iter->sub[iter->size] = node;
		iter->idx[iter->size] = idx + !!(r && !n);

		if (ISLEAF(node) || r)
			return;
		node = node->subs[idx];
	}
}

static inline val_t *_btree_next(iter_t *iter)
{
	node_t *node = iter->sub[iter->size];
	int idx      = iter->idx[iter->size];

	iter->idx[iter->size]++;
	if (!ISLEAF(node) && idx < node->size) {
		node_t *sub = node->subs[idx + 1];
		for (;;) {
			iter->size++;
			assert(iter->size < BTREE_ITER_MAX_DEPTH);
			iter->sub[iter->size] = sub;
			iter->idx[iter->size] = 0;
			if (ISLEAF(sub))
				break;
			sub = sub->subs[0];
		}
	}

	return idx < node->size ? node->vals + idx : NULL;
}

static inline val_t *_btree_prev(iter_t *iter)
{
	node_t *node = iter->sub[iter->size];
	int idx      = iter->idx[iter->size];

	iter->idx[iter->size]--;
	if (!ISLEAF(node) && idx > 0) {
		node_t *sub = node->subs[idx - 1];
		for (;;) {
			iter->size++;
			assert(iter->size < BTREE_ITER_MAX_DEPTH);
			iter->sub[iter->size] = sub;
			iter->idx[iter->size] = sub->size;
			if (ISLEAF(sub))
				break;
			sub = sub->subs[sub->size];
		}
	}

	return idx > 0 ? node->vals + (idx - 1): NULL;
}

static inline val_t *_btree_iter(btree_t *btree, iter_t *iter, int lt)
{
	for (; iter->size >= 0; iter->size--) {
		val_t *r = lt ? _btree_next(iter) : _btree_prev(iter);
		if (r)
			return r;
	}

	return NULL;
}

val_t *btree_first(btree_t *btree, val_t *v, iter_t *iter)
{
	btree_iter_init(btree, v, iter, 1);
	return btree_next(btree, iter);
}

val_t *btree_last(btree_t *btree, val_t *v, iter_t *iter)
{
	btree_iter_init(btree, v, iter, 0);
	return btree_prev(btree, iter);
}

val_t *btree_next(btree_t *btree, iter_t *iter)
{
	return _btree_iter(btree, iter, 1);
}

val_t *btree_prev(btree_t *btree, iter_t *iter)
{
	return _btree_iter(btree, iter, 0);
}
