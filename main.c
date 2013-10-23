#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include "btree.h"
#include <sys/time.h>

#define N 2000000/2
val_t vals[N];

int main(int argc,char * argv[])
{
	btree_t *tree;
	val_t i = 0, j = 0, *v;
	struct timeval tv[2];
	btree_iter_t iter[1];

	srandom(0);
	tree = btree_create(64);

	for (i = 0; i < N; i++)
		vals[i] = i;
	for (i = 0; i < N; i++) {
		int r = random() % N;
		long t = vals[r];
		vals[r] = vals[i], vals[i] = t;
	}
	gettimeofday(tv, NULL);
	for (i = 0; i < N; i++) {
		assert(btree_insert(tree, vals + i) == 0);
	}
	gettimeofday(tv + 1, NULL);
	printf("insert %lf\n", tv[1].tv_sec + tv[1].tv_usec/1000000.0 - tv[0].tv_sec - tv[0].tv_usec/1000000.0);

	gettimeofday(tv, NULL);
	for (i = 0, v = btree_first(tree, &i, iter); v ; v = btree_next(tree, iter), i++) {
		assert(i == *v);
	}
	assert(i == N);
	gettimeofday(tv + 1, NULL);
	printf("next %lf\n", tv[1].tv_sec + tv[1].tv_usec/1000000.0 - tv[0].tv_sec - tv[0].tv_usec/1000000.0);

	gettimeofday(tv, NULL);
	for (i = N - 1, v = btree_last(tree, &i, iter); v ; v = btree_prev(tree, iter), i--) {
		assert(i == *v);
	}
	assert(i == -1);
	gettimeofday(tv + 1, NULL);
	printf("prev %lf\n", tv[1].tv_sec + tv[1].tv_usec/1000000.0 - tv[0].tv_sec - tv[0].tv_usec/1000000.0);

	v = btree_max(tree);
	assert(v && *v == N-1);

	v = btree_min(tree);
	assert(v && *v == 0);

	gettimeofday(tv, NULL);
	for (i = 0; i < N; i++) {
		v = btree_search(tree, &i);
		assert(v && *v == i);
		v = btree_search(tree, vals + i);
		assert(v && *v == vals[i]);
	}
	gettimeofday(tv+1, NULL);
	printf("search %lf\n", tv[1].tv_sec + tv[1].tv_usec/1000000.0 - tv[0].tv_sec - tv[0].tv_usec/1000000.0);

	gettimeofday(tv, NULL);
	for (i = 0; i < N; i++) {
		v = btree_search(tree, vals + i);
		assert(v && *v == vals[i]);
	}
	gettimeofday(tv+1, NULL);
	printf("search random %lf\n", tv[1].tv_sec + tv[1].tv_usec/1000000.0 - tv[0].tv_sec - tv[0].tv_usec/1000000.0);


	for (i = N; i < N + 10000; i++) {
		v = btree_search(tree, &i);
		assert(v == NULL);
	}
	v = btree_max(tree);
	assert(v && *v == N-1);
	v = btree_min(tree);
	assert(v && *v == 0);

	for (i = N; i < N + 10000; i++) {
		assert(btree_delete(tree, &i) != 0);
	}

	gettimeofday(tv, NULL);
	for (i = 0; i < N; i++) {
		assert(btree_delete(tree, vals + i) == 0);
		v = btree_search(tree, vals + i);
		assert(v == NULL);
	}
	gettimeofday(tv+1, NULL);
	printf("delete & search %lf\n", tv[1].tv_sec + tv[1].tv_usec/1000000.0 - tv[0].tv_sec - tv[0].tv_usec/1000000.0);

	for (i = 0; i < N; i += 2) {
		assert(btree_insert(tree, vals + i) == 0);
	}

	for (i = 0; i < N; i += 2) {
		v = btree_search(tree, vals + i);
		assert(v && *v == vals[i]);
	}

	for (j = N-2, i = 1; i < N && j>= 0; i += 2, j -=2) {
		v = btree_search(tree, vals + j);
		assert(v && *v == vals[j]);
		assert(btree_delete(tree, vals + j) == 0);
		assert(btree_insert(tree, vals + i) == 0);
		v = btree_search(tree, vals + i);
		assert(v && *v == vals[i]);
	}

	btree_destroy(tree);
	return 0;
}
