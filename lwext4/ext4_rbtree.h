/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

  linux/include/linux/rbtree.h

  To use rbtrees you'll have to implement your own insert and search cores.
  This will avoid us to use callbacks and to drop drammatically performances.
  I know it's not the cleaner way,  but in C (not in C++) to get
  performances and genericity...

  Some example of insert and search follows here. The search is a plain
  normal search over an ordered tree. The insert instead must be implemented
  in two steps: First, the code must insert the element in order as a red leaf
  in the tree, and then the support library function ext4_rb_insert_color() must
  be called. Such function will do the not trivial work to rebalance the
  rbtree, if necessary.

-----------------------------------------------------------------------
static inline struct page * ext4_rb_search_page_cache(struct inode * inode,
						 unsigned long offset)
{
	struct ext4_rb_node * n = inode->i_ext4_rb_page_cache.ext4_rb_node;
	struct page * page;

	while (n)
	{
		page = ext4_rb_entry(n, struct page, ext4_rb_page_cache);

		if (offset < page->offset)
			n = n->ext4_rb_left;
		else if (offset > page->offset)
			n = n->ext4_rb_right;
		else
			return page;
	}
	return NULL;
}

static inline struct page * __ext4_rb_insert_page_cache(struct inode * inode,
						   unsigned long offset,
						   struct ext4_rb_node * node)
{
	struct ext4_rb_node ** p = &inode->i_ext4_rb_page_cache.ext4_rb_node;
	struct ext4_rb_node * parent = NULL;
	struct page * page;

	while (*p)
	{
		parent = *p;
		page = ext4_rb_entry(parent, struct page, ext4_rb_page_cache);

		if (offset < page->offset)
			p = &(*p)->ext4_rb_left;
		else if (offset > page->offset)
			p = &(*p)->ext4_rb_right;
		else
			return page;
	}

	ext4_rb_link_node(node, parent, p);

	return NULL;
}

static inline struct page * ext4_rb_insert_page_cache(struct inode * inode,
						 unsigned long offset,
						 struct ext4_rb_node * node)
{
	struct page * ret;
	if ((ret = __ext4_rb_insert_page_cache(inode, offset, node)))
		goto out;
	ext4_rb_insert_color(node, &inode->i_ext4_rb_page_cache);
 out:
	return ret;
}
-----------------------------------------------------------------------
*/

#ifndef EXT4_RBTREE_H_
#define EXT4_RBTREE_H_

#include <stddef.h>
#include <stdbool.h>

#ifndef offsetof
#define offsetof(type, member) ((size_t) & ((type *)0)->member)
#endif
#ifndef container_of
#define container_of(ptr, type, member)                                        \
	((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))
#endif

struct ext4_rb_node
{
	unsigned long ext4_rb_parent_color;
#define EXT4_RB_RED 0
#define EXT4_RB_BLACK 1
	struct ext4_rb_node *ext4_rb_right;
	struct ext4_rb_node *ext4_rb_left;
};
/* The alignment might seem pointless, but allegedly CRIS needs it */

struct ext4_rb_root
{
	struct ext4_rb_node *ext4_rb_node;
};

#define ext4_rb_parent(r) ((struct ext4_rb_node *)((r)->ext4_rb_parent_color & ~3))
#define ext4_rb_color(r) ((r)->ext4_rb_parent_color & 1)
#define ext4_rb_is_red(r) (!ext4_rb_color(r))
#define ext4_rb_is_black(r) ext4_rb_color(r)
#define ext4_rb_set_red(r)                                                        \
	do {                                                                   \
		(r)->ext4_rb_parent_color &= ~1;                                  \
	} while (0)
#define ext4_rb_set_black(r)                                                      \
	do {                                                                   \
		(r)->ext4_rb_parent_color |= 1;                                   \
	} while (0)

static inline void ext4_rb_set_parent(struct ext4_rb_node *rb, struct ext4_rb_node *p)
{
	rb->ext4_rb_parent_color =
	    (rb->ext4_rb_parent_color & 3) | (unsigned long)p;
}
static inline void ext4_rb_set_color(struct ext4_rb_node *rb, int color)
{
	rb->ext4_rb_parent_color = (rb->ext4_rb_parent_color & ~1) | color;
}

#define EXT4_RB_ROOT                                                          \
	(struct ext4_rb_root)                                                     \
	{                                                                      \
		NULL,                                                          \
	}
#define ext4_rb_entry(ptr, type, member) container_of(ptr, type, member)

#define EXT4_RB_EMPTY_ROOT(root) ((root)->ext4_rb_node == NULL)
#define EXT4_RB_EMPTY_NODE(node) (ext4_rb_parent(node) == node)
#define EXT4_RB_CLEAR_NODE(node) (ext4_rb_set_parent(node, node))

static inline void ext4_rb_init_node(struct ext4_rb_node *rb)
{
	rb->ext4_rb_parent_color = 0;
	rb->ext4_rb_right = NULL;
	rb->ext4_rb_left = NULL;
	EXT4_RB_CLEAR_NODE(rb);
}

extern void ext4_rb_insert_color(struct ext4_rb_node *, struct ext4_rb_root *);
extern void ext4_rb_erase(struct ext4_rb_node *, struct ext4_rb_root *);

typedef void (*ext4_rb_augment_f)(struct ext4_rb_node *node, void *data);

extern void ext4_rb_augment_insert(struct ext4_rb_node *node, ext4_rb_augment_f func,
				void *data);
extern struct ext4_rb_node *ext4_rb_augment_erase_begin(struct ext4_rb_node *node);
extern void ext4_rb_augment_erase_end(struct ext4_rb_node *node, ext4_rb_augment_f func,
				   void *data);

/* Find logical next and previous nodes in a tree */
extern struct ext4_rb_node *ext4_rb_next(const struct ext4_rb_node *);
extern struct ext4_rb_node *ext4_rb_prev(const struct ext4_rb_node *);
extern struct ext4_rb_node *ext4_rb_first(const struct ext4_rb_root *);
extern struct ext4_rb_node *ext4_rb_last(const struct ext4_rb_root *);

/* Fast replacement of a single node without remove/rebalance/add/rebalance */
extern void ext4_rb_replace_node(struct ext4_rb_node *victim, struct ext4_rb_node *new,
			      struct ext4_rb_root *root);

static inline void ext4_rb_link_node(struct ext4_rb_node *node,
				  struct ext4_rb_node *parent,
				  struct ext4_rb_node **ext4_rb_link)
{
	node->ext4_rb_parent_color = (unsigned long)parent;
	node->ext4_rb_left = node->ext4_rb_right = NULL;

	*ext4_rb_link = node;
}

extern void ext4_rb_erase(struct ext4_rb_node *, struct ext4_rb_root *);

extern void ext4_rb_insert(struct ext4_rb_root *, struct ext4_rb_node *,
			int (*)(struct ext4_rb_node *, struct ext4_rb_node *));

#endif /* EXT4_RBTREE_H_ */
