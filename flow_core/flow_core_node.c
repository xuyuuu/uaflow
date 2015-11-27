#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdint.h>

#include "flow_core_node.h"
#include "flow_core_mpool.h"
#include "flow_core_ahocorasick.h"

/* Privates */
static void node_init (ACT_NODE_t *thiz);
static int  node_edge_compare (const void *l, const void *r);
static int  node_has_pattern (ACT_NODE_t *thiz, AC_PATTERN_t *patt);
static void node_grow_outgoing_vector (ACT_NODE_t *thiz);
static void node_grow_matched_vector (ACT_NODE_t *thiz);
static void node_copy_pattern (ACT_NODE_t *thiz, 
		AC_PATTERN_t *to, AC_PATTERN_t *from);

struct act_node * node_create (struct ac_trie *trie)
{
	ACT_NODE_t *node;

	node = (ACT_NODE_t *) mpool_malloc (trie->mp, sizeof(ACT_NODE_t));
	node_init (node);
	node->trie = trie;

	return node;
}

static void node_init (ACT_NODE_t *thiz)
{
	node_assign_id (thiz);

	thiz->final = 0;
	thiz->failure_node = NULL;
	thiz->depth = 0;

	thiz->matched = NULL;
	thiz->matched_capacity = 0;
	thiz->matched_size = 0;

	thiz->outgoing = NULL;
	thiz->outgoing_capacity = 0;
	thiz->outgoing_size = 0;

	thiz->to_be_replaced = NULL;
}

void node_release_vectors(ACT_NODE_t *nod)
{
	free(nod->matched);
	free(nod->outgoing);
}

ACT_NODE_t * node_find_next(ACT_NODE_t *nod, AC_ALPHABET_t alpha)
{
	size_t i;

	for (i=0; i < nod->outgoing_size; i++)
	{
		if(nod->outgoing[i].alpha == alpha)
			return (nod->outgoing[i].next);
	}
	return NULL;
}

ACT_NODE_t *node_find_next_bs (ACT_NODE_t *nod, AC_ALPHABET_t alpha)
{
	size_t mid;
	int min, max;
	AC_ALPHABET_t amid;

	min = 0;
	max = nod->outgoing_size - 1;

	while (min <= max)
	{
		mid = (min + max) >> 1;
		amid = nod->outgoing[mid].alpha;
		if (alpha > amid)
			min = mid + 1;
		else if (alpha < amid)
			max = mid - 1;
		else
			return (nod->outgoing[mid].next);
	}
	return NULL;
}

static int node_has_pattern (ACT_NODE_t *thiz, AC_PATTERN_t *patt)
{
	size_t i, j;
	AC_TEXT_t *txt;
	AC_TEXT_t *new_txt = &patt->ptext;

	for (i = 0; i < thiz->matched_size; i++)
	{
		txt = &thiz->matched[i].ptext;

		if (txt->length != new_txt->length)
			continue;

		/* The following loop is futile! Because the input pattern always come 
		 * from a failure node, and if they have the same length, then they are
		 * equal. But for the sake of functional integrity we leave it here. */

		for (j = 0; j < txt->length; j++)
			if (txt->astring[j] != new_txt->astring[j])
				break;

		if (j == txt->length)
			return 1;
	}
	return 0;
}

ACT_NODE_t *node_create_next (ACT_NODE_t *nod, AC_ALPHABET_t alpha)
{
	ACT_NODE_t *next;

	if (node_find_next (nod, alpha) != NULL)
		/* The edge already exists */
		return NULL;

	next = node_create (nod->trie);
	node_add_edge (nod, next, alpha);

	return next;
}

void node_accept_pattern (ACT_NODE_t *nod, AC_PATTERN_t *new_patt, int copy)
{
	AC_PATTERN_t *patt;

	/* Check if the new pattern already exists in the node list */
	if (node_has_pattern(nod, new_patt))
		return;

	/* Manage memory */
	if (nod->matched_size == nod->matched_capacity)
		node_grow_matched_vector (nod);

	patt = &nod->matched[nod->matched_size++];

	if (copy)
	{
		/* Deep copy */
		node_copy_pattern (nod, patt, new_patt);
	}
	else
	{
		/* Shallow copy */
		*patt = *new_patt;
	}
}

	static void node_copy_pattern
(ACT_NODE_t *thiz, AC_PATTERN_t *to, AC_PATTERN_t *from)
{
	struct mpool *mp = thiz->trie->mp;

	to->ptext.astring = (AC_ALPHABET_t *) mpool_strndup (mp, 
			(const char *) from->ptext.astring, 
			from->ptext.length * sizeof(AC_ALPHABET_t));
	to->ptext.length = from->ptext.length;

	to->rtext.astring = (AC_ALPHABET_t *) mpool_strndup (mp, 
			(const char *) from->rtext.astring, 
			from->rtext.length * sizeof(AC_ALPHABET_t));
	to->rtext.length = from->rtext.length;

	if (from->id.type == AC_PATTID_TYPE_STRING)
		to->id.u.stringy = (const char *) mpool_strdup (mp, 
				(const char *) from->id.u.stringy);
	else
		to->id.u.number = from->id.u.number;

	to->id.type = from->id.type;

	/* private data */
	to->name_length = from->name_length;
	to->itemid = from->itemid;
	to->idx = from->idx;
	strncpy(to->name, from->name, from->name_length);
}

void node_add_edge (ACT_NODE_t *nod, ACT_NODE_t *next, AC_ALPHABET_t alpha)
{
	struct act_edge *oe; /* Outgoing edge */

	if(nod->outgoing_size == nod->outgoing_capacity)
		node_grow_outgoing_vector (nod);

	oe = &nod->outgoing[nod->outgoing_size];
	oe->alpha = alpha;
	oe->next = next;
	nod->outgoing_size++;
}

void node_assign_id (ACT_NODE_t *nod)
{
	static int unique_id = 1;
	nod->id = unique_id++;
}

static int node_edge_compare (const void *l, const void *r)
{
	/* 
	 * NOTE: Because edge alphabets are unique in every node we ignore
	 * equivalence case.
	 */
	if (((struct act_edge *)l)->alpha >= ((struct act_edge *)r)->alpha)
		return 1;
	else
		return -1;
}

void node_sort_edges (ACT_NODE_t *nod)
{
	qsort ((void *)nod->outgoing, nod->outgoing_size, 
			sizeof(struct act_edge), node_edge_compare);
}

int node_book_replacement (ACT_NODE_t *nod)
{
	size_t j;
	AC_PATTERN_t *pattern;
	AC_PATTERN_t *longest = NULL;

	if(!nod->final)
		return 0;

	for (j=0; j < nod->matched_size; j++)
	{
		pattern = &nod->matched[j];

		if (pattern->rtext.astring != NULL)
		{
			if (!longest)
				longest = pattern;
			else if (pattern->ptext.length > longest->ptext.length)
				longest = pattern;
		}
	}

	nod->to_be_replaced = longest;

	return longest ? 1 : 0;
}

static void node_grow_outgoing_vector (ACT_NODE_t *thiz)
{
	const size_t grow_factor = (8 / (thiz->depth + 1)) + 1;

	/* The outgoing edges of nodes grow with different pace in different
	 * depths; the shallower nodes the bigger outgoing number of nodes.
	 * So for efficiency (speed & memory usage), we apply a measure to 
	 * manage different growth rate.
	 */

	if (thiz->outgoing_capacity == 0)
	{
		thiz->outgoing_capacity = grow_factor;
		thiz->outgoing = (struct act_edge *) malloc 
			(thiz->outgoing_capacity * sizeof(struct act_edge));
	}
	else
	{
		thiz->outgoing_capacity += grow_factor;
		thiz->outgoing = (struct act_edge *) realloc (
				thiz->outgoing, 
				thiz->outgoing_capacity * sizeof(struct act_edge));
	}
}

static void node_grow_matched_vector (ACT_NODE_t *thiz)
{
	if (thiz->matched_capacity == 0)
	{
		thiz->matched_capacity = 1;
		thiz->matched = (AC_PATTERN_t *) malloc 
			(thiz->matched_capacity * sizeof(AC_PATTERN_t));
	}
	else
	{
		thiz->matched_capacity += 2;
		thiz->matched = (AC_PATTERN_t *) realloc (
				thiz->matched,
				thiz->matched_capacity * sizeof(AC_PATTERN_t));
	}
}

void node_collect_matches (ACT_NODE_t *nod)
{
	size_t i;
	ACT_NODE_t *n = nod;

	while ((n = n->failure_node))
	{
		for (i = 0; i < n->matched_size; i++)
			/* Always call with copy parameter 0 */
			node_accept_pattern (nod, &(n->matched[i]), 0);

		if (n->final)
			nod->final = 1;
	}

	node_sort_edges (nod);
	/* Sort matched patterns? Is that necessary? I don't think so. */
}

void node_display (ACT_NODE_t *nod)
{
	size_t j;
	struct act_edge *e;
	AC_PATTERN_t patt;

	printf("NODE(%3d)/....fail....> ", nod->id);
	if (nod->failure_node)
		printf("NODE(%3d)\n", nod->failure_node->id);
	else
		printf ("N.A.\n");

	for (j = 0; j < nod->outgoing_size; j++)
	{
		e = &nod->outgoing[j];
		printf("         |----(");
		if(isgraph(e->alpha))
			printf("%c)---", e->alpha);
		else
			printf("0x%x)", e->alpha);
		printf("--> NODE(%3d)\n", e->next->id);
	}

	if (nod->matched_size)
	{
		printf("Accepts: {");
		for (j = 0; j < nod->matched_size; j++)
		{
			patt = nod->matched[j];
			if(j) 
				printf(", ");
			switch (patt.id.type)
			{
				case AC_PATTID_TYPE_DEFAULT:
				case AC_PATTID_TYPE_NUMBER:
					printf("%ld", patt.id.u.number);
					break;
				case AC_PATTID_TYPE_STRING:
					printf("%s", patt.id.u.stringy);
					break;
			}
			printf(": %.*s", (int)patt.ptext.length, patt.ptext.astring);
		}
		printf("}\n");
	}
	printf("\n");
}
