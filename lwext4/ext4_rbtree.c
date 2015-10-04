/*
  Red Black Trees
  (C) 1999  Andrea Arcangeli <andrea@suse.de>
  (C) 2002  David Woodhouse <dwmw2@infradead.org>

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

  linux/lib/rbtree.c
*/
#include "ext4_rbtree.h"

static void __ext4_rb_rotate_left(struct ext4_rb_node *node, struct ext4_rb_root *root)
{
	struct ext4_rb_node *right = node->ext4_rb_right;
	struct ext4_rb_node *parent = ext4_rb_parent(node);

	if ((node->ext4_rb_right = right->ext4_rb_left))
		ext4_rb_set_parent(right->ext4_rb_left, node);
	right->ext4_rb_left = node;

	ext4_rb_set_parent(right, parent);

	if (parent) {
		if (node == parent->ext4_rb_left)
			parent->ext4_rb_left = right;
		else
			parent->ext4_rb_right = right;
	} else
		root->ext4_rb_node = right;
	ext4_rb_set_parent(node, right);
}

static void __ext4_rb_rotate_right(struct ext4_rb_node *node, struct ext4_rb_root *root)
{
	struct ext4_rb_node *left = node->ext4_rb_left;
	struct ext4_rb_node *parent = ext4_rb_parent(node);

	if ((node->ext4_rb_left = left->ext4_rb_right))
		ext4_rb_set_parent(left->ext4_rb_right, node);
	left->ext4_rb_right = node;

	ext4_rb_set_parent(left, parent);

	if (parent) {
		if (node == parent->ext4_rb_right)
			parent->ext4_rb_right = left;
		else
			parent->ext4_rb_left = left;
	} else
		root->ext4_rb_node = left;
	ext4_rb_set_parent(node, left);
}

void ext4_rb_insert_color(struct ext4_rb_node *node, struct ext4_rb_root *root)
{
	struct ext4_rb_node *parent, *gparent;

	while ((parent = ext4_rb_parent(node)) && ext4_rb_is_red(parent)) {
		gparent = ext4_rb_parent(parent);

		if (parent == gparent->ext4_rb_left) {
			{
				register struct ext4_rb_node *uncle =
				    gparent->ext4_rb_right;
				if (uncle && ext4_rb_is_red(uncle)) {
					ext4_rb_set_black(uncle);
					ext4_rb_set_black(parent);
					ext4_rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->ext4_rb_right == node) {
				register struct ext4_rb_node *tmp;
				__ext4_rb_rotate_left(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			ext4_rb_set_black(parent);
			ext4_rb_set_red(gparent);
			__ext4_rb_rotate_right(gparent, root);
		} else {
			{
				register struct ext4_rb_node *uncle =
				    gparent->ext4_rb_left;
				if (uncle && ext4_rb_is_red(uncle)) {
					ext4_rb_set_black(uncle);
					ext4_rb_set_black(parent);
					ext4_rb_set_red(gparent);
					node = gparent;
					continue;
				}
			}

			if (parent->ext4_rb_left == node) {
				register struct ext4_rb_node *tmp;
				__ext4_rb_rotate_right(parent, root);
				tmp = parent;
				parent = node;
				node = tmp;
			}

			ext4_rb_set_black(parent);
			ext4_rb_set_red(gparent);
			__ext4_rb_rotate_left(gparent, root);
		}
	}

	ext4_rb_set_black(root->ext4_rb_node);
}

static void __ext4_rb_erase_color(struct ext4_rb_node *node, struct ext4_rb_node *parent,
			       struct ext4_rb_root *root)
{
	struct ext4_rb_node *other;

	while ((!node || ext4_rb_is_black(node)) && node != root->ext4_rb_node) {
		if (parent->ext4_rb_left == node) {
			other = parent->ext4_rb_right;
			if (ext4_rb_is_red(other)) {
				ext4_rb_set_black(other);
				ext4_rb_set_red(parent);
				__ext4_rb_rotate_left(parent, root);
				other = parent->ext4_rb_right;
			}
			if ((!other->ext4_rb_left ||
			     ext4_rb_is_black(other->ext4_rb_left)) &&
			    (!other->ext4_rb_right ||
			     ext4_rb_is_black(other->ext4_rb_right))) {
				ext4_rb_set_red(other);
				node = parent;
				parent = ext4_rb_parent(node);
			} else {
				if (!other->ext4_rb_right ||
				    ext4_rb_is_black(other->ext4_rb_right)) {
					ext4_rb_set_black(other->ext4_rb_left);
					ext4_rb_set_red(other);
					__ext4_rb_rotate_right(other, root);
					other = parent->ext4_rb_right;
				}
				ext4_rb_set_color(other, ext4_rb_color(parent));
				ext4_rb_set_black(parent);
				ext4_rb_set_black(other->ext4_rb_right);
				__ext4_rb_rotate_left(parent, root);
				node = root->ext4_rb_node;
				break;
			}
		} else {
			other = parent->ext4_rb_left;
			if (ext4_rb_is_red(other)) {
				ext4_rb_set_black(other);
				ext4_rb_set_red(parent);
				__ext4_rb_rotate_right(parent, root);
				other = parent->ext4_rb_left;
			}
			if ((!other->ext4_rb_left ||
			     ext4_rb_is_black(other->ext4_rb_left)) &&
			    (!other->ext4_rb_right ||
			     ext4_rb_is_black(other->ext4_rb_right))) {
				ext4_rb_set_red(other);
				node = parent;
				parent = ext4_rb_parent(node);
			} else {
				if (!other->ext4_rb_left ||
				    ext4_rb_is_black(other->ext4_rb_left)) {
					ext4_rb_set_black(other->ext4_rb_right);
					ext4_rb_set_red(other);
					__ext4_rb_rotate_left(other, root);
					other = parent->ext4_rb_left;
				}
				ext4_rb_set_color(other, ext4_rb_color(parent));
				ext4_rb_set_black(parent);
				ext4_rb_set_black(other->ext4_rb_left);
				__ext4_rb_rotate_right(parent, root);
				node = root->ext4_rb_node;
				break;
			}
		}
	}
	if (node)
		ext4_rb_set_black(node);
}

void __ext4_rb_erase(struct ext4_rb_node *node, struct ext4_rb_root *root)
{
	struct ext4_rb_node *child, *parent;
	int color;

	if (!node->ext4_rb_left)
		child = node->ext4_rb_right;
	else if (!node->ext4_rb_right)
		child = node->ext4_rb_left;
	else {
		struct ext4_rb_node *old = node, *left;

		node = node->ext4_rb_right;
		while ((left = node->ext4_rb_left) != NULL)
			node = left;

		if (ext4_rb_parent(old)) {
			if (ext4_rb_parent(old)->ext4_rb_left == old)
				ext4_rb_parent(old)->ext4_rb_left = node;
			else
				ext4_rb_parent(old)->ext4_rb_right = node;
		} else
			root->ext4_rb_node = node;

		child = node->ext4_rb_right;
		parent = ext4_rb_parent(node);
		color = ext4_rb_color(node);

		if (parent == old) {
			parent = node;
		} else {
			if (child)
				ext4_rb_set_parent(child, parent);
			parent->ext4_rb_left = child;

			node->ext4_rb_right = old->ext4_rb_right;
			ext4_rb_set_parent(old->ext4_rb_right, node);
		}

		node->ext4_rb_parent_color = old->ext4_rb_parent_color;
		node->ext4_rb_left = old->ext4_rb_left;
		ext4_rb_set_parent(old->ext4_rb_left, node);

		goto color;
	}

	parent = ext4_rb_parent(node);
	color = ext4_rb_color(node);

	if (child)
		ext4_rb_set_parent(child, parent);
	if (parent) {
		if (parent->ext4_rb_left == node)
			parent->ext4_rb_left = child;
		else
			parent->ext4_rb_right = child;
	} else
		root->ext4_rb_node = child;

color:
	if (color == EXT4_RB_BLACK)
		__ext4_rb_erase_color(child, parent, root);
}

static void ext4_rb_augment_path(struct ext4_rb_node *node, ext4_rb_augment_f func,
			      void *data)
{
	struct ext4_rb_node *parent;

up:
	func(node, data);
	parent = ext4_rb_parent(node);
	if (!parent)
		return;

	if (node == parent->ext4_rb_left && parent->ext4_rb_right)
		func(parent->ext4_rb_right, data);
	else if (parent->ext4_rb_left)
		func(parent->ext4_rb_left, data);

	node = parent;
	goto up;
}

/*
 * after inserting @node into the tree, update the tree to account for
 * both the new entry and any damage done by rebalance
 */
void ext4_rb_augment_insert(struct ext4_rb_node *node, ext4_rb_augment_f func,
			 void *data)
{
	if (node->ext4_rb_left)
		node = node->ext4_rb_left;
	else if (node->ext4_rb_right)
		node = node->ext4_rb_right;

	ext4_rb_augment_path(node, func, data);
}

/*
 * before removing the node, find the deepest node on the rebalance path
 * that will still be there after @node gets removed
 */
struct ext4_rb_node *ext4_rb_augment_erase_begin(struct ext4_rb_node *node)
{
	struct ext4_rb_node *deepest;

	if (!node->ext4_rb_right && !node->ext4_rb_left)
		deepest = ext4_rb_parent(node);
	else if (!node->ext4_rb_right)
		deepest = node->ext4_rb_left;
	else if (!node->ext4_rb_left)
		deepest = node->ext4_rb_right;
	else {
		deepest = ext4_rb_next(node);
		if (deepest->ext4_rb_right)
			deepest = deepest->ext4_rb_right;
		else if (ext4_rb_parent(deepest) != node)
			deepest = ext4_rb_parent(deepest);
	}

	return deepest;
}

/*
 * after removal, update the tree to account for the removed entry
 * and any rebalance damage.
 */
void ext4_rb_augment_erase_end(struct ext4_rb_node *node, ext4_rb_augment_f func,
			    void *data)
{
	if (node)
		ext4_rb_augment_path(node, func, data);
}

/*
 * This function returns the first node (in sort order) of the tree.
 */
struct ext4_rb_node *ext4_rb_first(const struct ext4_rb_root *root)
{
	struct ext4_rb_node *n;

	n = root->ext4_rb_node;
	if (!n)
		return NULL;
	while (n->ext4_rb_left)
		n = n->ext4_rb_left;
	return n;
}

struct ext4_rb_node *ext4_rb_last(const struct ext4_rb_root *root)
{
	struct ext4_rb_node *n;

	n = root->ext4_rb_node;
	if (!n)
		return NULL;
	while (n->ext4_rb_right)
		n = n->ext4_rb_right;
	return n;
}

struct ext4_rb_node *ext4_rb_next(const struct ext4_rb_node *node)
{
	struct ext4_rb_node *parent;

	if (ext4_rb_parent(node) == node)
		return NULL;

	/* If we have a right-hand child, go down and then left as far
		as we can. */
	if (node->ext4_rb_right) {
		node = node->ext4_rb_right;
		while (node->ext4_rb_left)
			node = node->ext4_rb_left;
		return (struct ext4_rb_node *)node;
	}

	/* No right-hand children.  Everything down and left is
	   smaller than us, so any 'next' node must be in the general
	   direction of our parent. Go up the tree; any time the
	   ancestor is a right-hand child of its parent, keep going
	   up. First time it's a left-hand child of its parent, said
	   parent is our 'next' node. */
	while ((parent = ext4_rb_parent(node)) && node == parent->ext4_rb_right)
		node = parent;

	return parent;
}

struct ext4_rb_node *ext4_rb_prev(const struct ext4_rb_node *node)
{
	struct ext4_rb_node *parent;

	if (ext4_rb_parent(node) == node)
		return NULL;

	/* If we have a left-hand child, go down and then right as far
	   as we can. */
	if (node->ext4_rb_left) {
		node = node->ext4_rb_left;
		while (node->ext4_rb_right)
			node = node->ext4_rb_right;
		return (struct ext4_rb_node *)node;
	}

	/* No left-hand children. Go up till we find an ancestor which
	   is a right-hand child of its parent */
	while ((parent = ext4_rb_parent(node)) && node == parent->ext4_rb_left)
		node = parent;

	return parent;
}

void ext4_rb_replace_node(struct ext4_rb_node *victim, struct ext4_rb_node *new,
		       struct ext4_rb_root *root)
{
	struct ext4_rb_node *parent = ext4_rb_parent(victim);

	/* Set the surrounding nodes to point to the replacement */
	if (parent) {
		if (victim == parent->ext4_rb_left)
			parent->ext4_rb_left = new;
		else
			parent->ext4_rb_right = new;
	} else {
		root->ext4_rb_node = new;
	}
	if (victim->ext4_rb_left)
		ext4_rb_set_parent(victim->ext4_rb_left, new);
	if (victim->ext4_rb_right)
		ext4_rb_set_parent(victim->ext4_rb_right, new);

	/* Copy the pointers/colour from the victim to the replacement */
	*new = *victim;
}

void ext4_rb_erase(struct ext4_rb_node *node, struct ext4_rb_root *root)
{
	__ext4_rb_erase(node, root);
}

void ext4_rb_insert(struct ext4_rb_root *root, struct ext4_rb_node *node,
		 int (*cmp)(struct ext4_rb_node *, struct ext4_rb_node *))
{
	struct ext4_rb_node **new = &(root->ext4_rb_node), *parent = NULL;

	/* Figure out where to put new node */
	while (*new) {
		int result = cmp(node, *new);

		parent = *new;
		if (result < 0) {
			new = &((*new)->ext4_rb_left);
		} else if (result > 0) {
			new = &((*new)->ext4_rb_right);
		} else
			return;
	}

	/* Add new node and rebalance tree. */
	ext4_rb_link_node(node, parent, new);
	ext4_rb_insert_color(node, root);
}
