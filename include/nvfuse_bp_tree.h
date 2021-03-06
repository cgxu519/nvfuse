/*
*	NVFUSE (NVMe based File System in Userspace)
*	Copyright (C) 2016 Yongseok Oh <yongseok.oh@sk.com>
*	First Writing: 30/10/2016
*
* This program is free software; you can redistribute it and/or modify it
* under the terms and conditions of the GNU General Public License,
* version 2, as published by the Free Software Foundation.
*
* This program is distributed in the hope it will be useful, but WITHOUT
* ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
* FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
* more details.
*/

#include <stdlib.h>
#include <pthread.h>
#include "nvfuse_config.h"
#include "nvfuse_types.h"

#ifndef _BP_TREE_H
#define _BP_TREE_H

/***************************************************************/
//				RECORD DEFINITIONS
/***************************************************************/

#define BP_MAX_INDEX_NUM 0x7fffffff
#define BP_MAX_PER_GIGA_NODES (16*1024)
#define BP_MAX_NODES (4*1024*1024) // for 512GB

#define BP_NODE_SIZE (CLUSTER_SIZE/1)
#define BP_CLUSTER_PER_NODE (CLUSTER_SIZE/BP_NODE_SIZE)


/***************************************************************/
//				VARIABLE TYPES
/***************************************************************/

#define KEY_IS_INTEGER

#ifdef __linux__
#define __int64 long long
#endif

#ifdef KEY_IS_INTEGER
typedef u64 bkey_t;
#else
typedef unsigned char bkey_t;
#endif

typedef unsigned int bitem_t;
typedef int offset_t;


/***************************************************************/
//				INDEX NODE DEFINITIONS
/***************************************************************/

#ifdef KEY_IS_INTEGER
#	define INDEX_KEY_LEN	1
#else
#define USE_HALF_MD5
#ifdef USE_SHA1
#	define INDEX_KEY_LEN	20	//byte
#endif
#ifdef USE_SHA256
#	define INDEX_KEY_LEN	32	//byte
#endif
#ifdef USE_MD5
#	define INDEX_KEY_LEN	16	//byte
#endif
#ifdef USE_HALF_MD5
#	define INDEX_KEY_LEN	8	//byte
#endif

#endif

#define INDEX_FLAG		0

#ifdef KEY_IS_INTEGER
#define BP_KEY_SIZE sizeof(bkey_t)
#else
#define BP_KEY_SIZE INDEX_KEY_LEN
#endif
#define BP_ITEM_SIZE sizeof(bitem_t)
#define BP_PAIR_SIZE (BP_KEY_SIZE + BP_ITEM_SIZE)

typedef struct {
	bkey_t *i_key;
	bitem_t *i_item;
} key_pair_t;

#define INDEX_NODE_FREE 0
#define INDEX_NODE_USED 1

typedef struct master_node master_node_t;

typedef struct index_node {
	int i_root; //4
	int i_flag; //8
	int i_num;	//12
	int i_offset; //16
	int i_next_node; //20
	int i_prev_node; //24
	int i_status;	 //28
	char pad[8 + 8]; //40

	key_pair_t *i_pair;	//4086B
	char *i_buf;
	struct nvfuse_buffer_head *i_bh;
	master_node_t *i_master;
} index_node_t;

/* Master Node Layout */
#define BP_NODE_HEAD_SIZE	40
#define BP_BITMAP_START		BP_NODE_HEAD_SIZE
#define BP_BITMAP_SIZE		(CLUSTER_SIZE - BP_NODE_HEAD_SIZE) /* in bytes */
#define BP_NODES_PER_MASTER	(1 + (BP_BITMAP_SIZE << 3)) /* each master + normal nodes */
/* for testing purpose */
//#define BP_NODES_PER_MASTER	(512) /* each master + normal nodes */

/* B+tree Node Layout */
/*
|---------------|-----------|-----------|-----------|---------------|-----------|-----------|
|	MASTER		|	NODE	|	... 	|	NODE	|	MASTER		|	NODE	|	... 	|
|---------------|-----------|-----------|-----------|---------------|-----------|-----------|
*/
#define _FANOUT ((CLUSTER_SIZE - BP_NODE_HEAD_SIZE) / (BP_PAIR_SIZE))
#define FANOUT  ((_FANOUT % 2 == 0) ? (_FANOUT-1) : _FANOUT)

#define BP_KEY_START BP_NODE_HEAD_SIZE
#define BP_ITEM_START(m) (BP_KEY_START + FANOUT * BP_KEY_SIZE)

/***************************************************************/
//				DATA NODE DEFINITIONS (LEAF NODE)
/***************************************************************/
//#define	DATA_KEY_LEN	INDEX_KEY_LEN
#define DATA_FLAG		1

/***************************************************************/
//				MASTER NODE DEFINITIONS
/***************************************************************/
typedef struct _master_ondisk_node {
	offset_t m_root;			//4 root start cluster
	offset_t m_bitmap_free;		//8 bitmap start cluster
	offset_t m_bitmap_num;		//12 bitmap num clusters
	unsigned int m_max_nodes;	//16
	int m_alloc_block;			//20
	int m_dealloc_block;		//24
	int m_node_size;			//28  /* identical to CLUSTER_SIZE*/
	int m_fanout;				//32
	int m_last_allocated_sub_master; // 36
	int m_last_allocated_sub_offset; // 40

	char bitmap[BP_BITMAP_SIZE]; //4096
} master_ondisk_node_t;

#define MAX_STACK 128
typedef struct master_node {
	/* ondisk pointer */
	master_ondisk_node_t *m_ondisk;

	//struct in-memory
	struct nvfuse_inode_ctx *m_ictx;
	inode_t m_ino;

	struct nvfuse_superblock *m_sb;
	struct nvfuse_buffer_head *m_bh;
	int m_bitmap_ptr;
	index_node_t *m_cur;
	offset_t m_stack[MAX_STACK];
	int	m_sp;

	void *m_rec;
	int m_size; // nvfuse_malloc size
	int m_fsize; //file size
	char *m_buf;
	unsigned int m_key_count;

	index_node_t *(*alloc)(struct master_node *master, int flag, int offset, int is_new);
	int	(*dealloc)(struct master_node *master, index_node_t *p);
	int	(*insert)(struct master_node *master, bkey_t *key, bitem_t *value, bitem_t *cur_value,
			  int update);
	int	(*update)(struct master_node *master, bkey_t *key, bitem_t *value);
	int	(*search)(struct master_node *master, bkey_t *key, index_node_t **d);
	int	(*range_search)(struct master_node *master, bkey_t *start, bkey_t *end);
	int	(*remove)(struct master_node *master, bkey_t *key);
	int	(*get_pair)(index_node_t *dp, bkey_t *key);
	int	(*release)(struct master_node *master, index_node_t *p);
	int	(*release_bh)(struct nvfuse_buffer_head *bh);
	int	(*read)(struct master_node *master, index_node_t *p, int ofset, int sync, int rwlock);
	int	(*write)(struct master_node *master, index_node_t *p, int offset);
	void	(*push)(struct master_node *master, offset_t v);
	offset_t	(*pop)(struct master_node *master);
} master_node_t;


/*
* Master Node Context Structure
*/
typedef struct {
	master_node_t *master;
	struct nvfuse_buffer_head *bh;
	s32 master_id;
} master_ctx_t;


/***************************************************************/
//			 ETC
/***************************************************************/
#define NO_ENTRY -1
#define SUCCESS 1
#define UNDER_FLOW 0

/***************************************************************/
//			 operations
/***************************************************************/
/* allocation for b+tree node in memory and data block on disk*/
#define ALLOC_CREATE	1
/* allocation for b+tree node in memory without data block on disk*/
#define ALLOC_READ		0

#define HEAD_SYNC		1
#define HEAD_NOSYNC		0

#define LOCK			1
#define NOLOCK			0

#define B_SEARCH(m, k, d) m->search(m, k, d)
#define B_RSEARCH(m, k1, k2) m->range_search(m, k1, k2);
#define B_RSEARCH_RB(m, k1, k2, rb) m->range_search_rb(m, k1, k2, rb)

#define B_INSERT(m, k, v, c, u) m->insert(m, k, v, c, u)
#define B_UPDATE(m, k, v) m->update(m, k, v)

#define B_REMOVE(m, k) m->remove(m, k)
#define B_GET_PAIR(m,d, k) m->get_pair(d, k)
#define B_iALLOC(m, o, n) m->alloc(m, 0, o, n)
#define B_dALLOC(m, o, n) m->alloc(m, 1, o, n)
#define B_RELEASE(m, p) m->release(m, p)
#define B_RELEASE_BH(m, p) m->release_bh(p)

#define B_DEALLOC(m, p) m->dealloc(m, p)
#define B_WRITE(m, p, o) m->write(m, p, o)
#define B_READ(m, p, o, s, l) m->read(m, p, o, s, l)
#define B_POP(m) m->pop(m)
#define B_PUSH(m,p) m->push(m,p)

#define B_FLUSH_STACK(m) while(m->m_sp)B_POP(m);

void *bp_malloc(struct nvfuse_superblock *sb, int mempool_type, int num);
void bp_free(struct nvfuse_superblock *sb, int mempool_type, int num, void *ptr);

#ifdef KEY_IS_INTEGER
#define B_KEY_MAKE(b, n) (*b = n)

#define B_PAIR_COPY_N(a, b, i, j, n) \
			rte_memcpy((char *)B_KEY_PAIR(a, i),(char *) B_KEY_PAIR(b, j), (n) * sizeof(bkey_t));\
			rte_memcpy((char *)B_ITEM_PAIR(a, i),(char *) B_ITEM_PAIR(b, j), (n) * sizeof(bitem_t));

#define B_PAIR_COPY(a,b,i,j) \
			B_KEY_COPY(B_KEY_PAIR(a, i), B_KEY_PAIR(b, j)); \
			B_ITEM_COPY(B_ITEM_PAIR(a, i), B_ITEM_PAIR(b, j));

#define B_PAIR_INIT_N(a, i, n)\
			memset((char *)B_KEY_PAIR(a,i), 0x00, sizeof(bkey_t)*(n));\
			memset((char *)B_ITEM_PAIR(a,i), 0x00, sizeof(bitem_t)*(n));

#define B_PAIR_INIT(a,i)\
			B_KEY_INIT(B_KEY_PAIR(a,i));\
			B_ITEM_INIT(B_ITEM_PAIR(a,i));

#define B_KEY_COPY(x,y)	(*x = *y)
//#define B_KEY_CMP(x, y)	(bkey_t)(*x - *y)
#define B_KEY_CMP(x, y) key_compare(x, y, 0, 0, 0)
#define B_KEY_INIT(x)	(*x =  0x00)
#define B_KEY_ALLOC(sb, type, num) (bkey_t *)bp_malloc(sb, type, num);
#define B_KEY_FREE(p) bp_free((void *)(p))


#define B_ITEM_COPY(x,y)	(*x = *y)
#define B_ITEM_CMP(x,y)	(int)(*x - *y)
#define B_ITEM_INIT(x)	(*x =  0x00)
#define B_ITEM_ALLOC(sb, type, num) (bitem_t *)bp_malloc(sb, type, num);
#define B_ITEM_FREE(p) bp_free(p)

#define B_KEY_ISNULL(p) (*p == 0)
#define B_ITEM_ISNULL(p) (*p == 0)

#define B_KEY_GET(p, i) (&p->i_pair->i_key[i])
#define B_ITEM_GET(p, i) (&p->i_pair->i_item[i])

#define B_KEY_PAIR(p, i) (&p->i_key[i])
#define B_ITEM_PAIR(p, i) (&p->i_item[i])
#else

#if (BP_KEY_SIZE == 8)
#define B_KEY_MAKE(b, n) memcpy(b, &n, sizeof(n))
#endif

#define B_PAIR_COPY_N(a, b, i, j, n) \
			rte_memcpy((char *)B_KEY_PAIR(a, i),(char *) B_KEY_PAIR(b, j), (n) * BP_KEY_SIZE);\
			rte_memcpy((char *)B_ITEM_PAIR(a, i),(char *) B_ITEM_PAIR(b, j), (n) * BP_ITEM_SIZE);

#define B_PAIR_INIT_N(a, i, n)\
		memset((char *)B_KEY_PAIR(a,i), 0x00, (n) * BP_KEY_SIZE);\
		memset((char *)B_ITEM_PAIR(a,i), 0x00, (n) * BP_ITEM_SIZE);

#define B_PAIR_COPY(a,b, i, j) \
		B_KEY_COPY(B_KEY_PAIR(a, i), B_KEY_PAIR(b, j)); \
		B_ITEM_COPY(B_ITEM_PAIR(a, i), B_ITEM_PAIR(b, j));

#define B_PAIR_INIT(a,i)\
		B_KEY_INIT(B_KEY_PAIR(a,i));\
		B_ITEM_INIT(B_ITEM_PAIR(a,i));

#define B_KEY_COPY(x, y)	memcpy(x, y, BP_KEY_SIZE)
#define B_KEY_INIT(x)		memset(x, 0x00, BP_KEY_SIZE)
#define B_KEY_CMP(x, y)		memcmp(x, y, BP_KEY_SIZE)
#define B_KEY_ALLOC()		malloc(BP_KEY_SIZE);
#define B_KEY_FREE(p)		free(p)
#define B_KEY_ISNULL(p)		bp_key_is_null(p)

#define B_ITEM_COPY(x, y)	(*x = *y)
#define B_ITEM_INIT(x)		(*x =  0x00)
#define B_ITEM_CMP(x, y)	(int)(*x - *y)
#define B_ITEM_ALLOC()		(bitem_t *)malloc(sizeof(bitem_t));
#define B_ITEM_FREE(p)		free(p)

#define B_KEY_GET(p, i) (p->i_pair->i_key + BP_KEY_SIZE * i)
#define B_ITEM_GET(p, i) (p->i_pair->i_item + i)

#define B_KEY_PAIR(p, i) (p->i_key + BP_KEY_SIZE * i)
#define B_ITEM_PAIR(p, i) (p->i_item + i)
#endif

#define B_PARENT(p) ((index_node_t *)p)->i_parent
#define B_NEXT(p) p->i_next_node
#define B_PREV(p) p->i_prev_node
#define B_ISLEAF(p) (p->i_flag == DATA_FLAG)
#define B_ISROOT(p) (p->i_root)

int bp_remove_key(master_node_t *master, bkey_t *key);
int search_data_node(master_node_t *master, bkey_t *str, index_node_t **d);
int rsearch_data_node(master_node_t *master, bkey_t *s_key, bkey_t *e_key);
int bp_insert_key_tree(master_node_t *master, bkey_t *key, bitem_t *value, bitem_t *cur_value,
		       int update);
void bp_insert_value_tree(index_node_t *ip,
			  int index,
			  bkey_t *key,
			  bitem_t *value);
int bp_key_is_null(bkey_t *buf);

int bp_update_key_tree(master_node_t *master, bkey_t *key, bitem_t *value);
int get_pair_tree(index_node_t *dp, bkey_t *key);
index_node_t *traverse_empty(master_node_t *master, index_node_t *ip, bkey_t *key);
index_node_t *bp_alloc_node(master_node_t *master, int flag, int offset, int is_new);
int bp_release_node(master_node_t *master, index_node_t *p);
int bp_release_bh(struct nvfuse_buffer_head *bh);
int bp_bin_search(bkey_t *key, key_pair_t *pair, int max,
		  int(*compare)(void *, void *, void *start, int num, int mid));

struct nvfuse_buffer_head *bp_read_block(master_node_t *master, int offset, int rwlock);
int bp_read_node(master_node_t *master, index_node_t *node, int offset, int sync, int rwlock);
int bp_write_node(master_node_t *master, index_node_t *node, int offset);
int bp_distribute_node(index_node_t *p_ip, key_pair_t *pair);

void stack_push(master_node_t *master, offset_t v);
offset_t stack_pop(master_node_t *master);
void bp_init_pair(key_pair_t *node, int num);

int bp_dealloc_bitmap(master_node_t *master, index_node_t *p);
void bp_write_master(master_node_t *master);
void bp_init_root(master_node_t *master);
master_node_t *bp_init_master(struct nvfuse_superblock *sb);
index_node_t *bp_add_root_node(master_node_t *master, index_node_t *dp, bkey_t *key, bitem_t *value);

int key_compare(void *k1, void *k2, void *start, int num, int mid);
key_pair_t *bp_alloc_pair(master_node_t *master, int num);
int bp_split_tree(master_node_t *master, index_node_t *dp, bkey_t *key, bitem_t *value);
void bp_write_block(struct nvfuse_superblock *sb, struct nvfuse_buffer_head *bh, char *buf,
		    int offset);

int bp_merge_key(master_node_t *master, index_node_t *dp, bkey_t *key, bitem_t *value);
void bp_release_pair(master_node_t *master, key_pair_t *pair, int num);
int bp_merge_key2(key_pair_t *pair, bkey_t *key, bitem_t *value, int max);
int bp_split_data(master_node_t *master, index_node_t *p_ip, key_pair_t *child,
				  index_node_t *sibling_ip, int count, key_pair_t *median);
index_node_t *bp_split_index_node(master_node_t *master,
								  index_node_t *p_ip,
								  key_pair_t *child,
								  int count,
								  key_pair_t *median,
								  int is_leaf);
int bp_split_data_node(master_node_t *master,
		       index_node_t *ip,
		       index_node_t *dp,
		       bkey_t *key,
		       bitem_t *value,
		       key_pair_t *pair);
int bp_merge_key_tree(master_node_t *master, bkey_t *key, index_node_t *dp, int d_index);

int bp_redist_data_child(master_node_t *master, index_node_t *ip, index_node_t *child, int data_node);
index_node_t *bp_next_node(master_node_t *master, index_node_t *ip, bkey_t *key);

int bp_read_master(master_node_t *master);
int bp_find_key(master_node_t *master, bkey_t *key, bitem_t *value);
void bp_copy_node_to_raw(index_node_t *node, char *raw);
void bp_copy_raw_to_node(index_node_t *node, char *raw);

void bp_print_node(index_node_t *node);
void bubble_sort(master_node_t *master, key_pair_t *pair, int num, int(*compare)(void *src1,
		 void *src2));
int bp_alloc_master(struct nvfuse_superblock *sb, master_node_t *master);
void bp_deinit_master(master_node_t *master);
offset_t bp_alloc_bitmap(master_node_t *master, struct nvfuse_inode_ctx *ictx);

s32 bp_read_master_ctx(master_node_t *master, master_ctx_t *master_ctx, s32 master_id);
void bp_release_master_ctx(master_node_t *master, master_ctx_t *master_ctx, s32 dirty);
s32 bp_set_bitmap(master_node_t *master, u32 offset);
s32 bp_clear_bitmap(master_node_t *master, u32 offset);
s32 bp_test_bitmap(master_node_t *master, u32 offset);
s32 bp_inc_free_bitmap(master_node_t *master, u32 offset);
s32 _bp_scan_bitmap(char *bitmap, s32 offset, s32 length);
s32 bp_scan_bitmap(master_node_t *master);

int compare_str(void *src1, void *src2);

#define set_bit(b, i)	ext2fs_set_bit(i, b)
#define clear_bit(b, i) ext2fs_clear_bit(i, b)
#define test_bit(b, i)  ext2fs_test_bit(i, b)

#endif //__BP_TREE_H
