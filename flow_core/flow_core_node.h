#ifndef flow_core_node_h 
#define flow_core_node_h

#include "flow_core_actypes.h"

#ifdef __cplusplus
extern "C" {
#endif

	struct act_edge;
	struct ac_trie;

	typedef struct act_node
	{
		int id;    

		int final;
		size_t depth; 
		struct act_node *failure_node; 

		struct act_edge *outgoing;  
		size_t outgoing_capacity;  
		size_t outgoing_size;     

		AC_PATTERN_t *matched;   
		size_t matched_capacity;
		size_t matched_size;    

		AC_PATTERN_t *to_be_replaced;  

		struct ac_trie *trie;  

	} ACT_NODE_t;

	struct act_edge
	{
		AC_ALPHABET_t alpha;
		ACT_NODE_t *next;  
	};

	ACT_NODE_t *node_create (struct ac_trie *trie);
	ACT_NODE_t *node_create_next (ACT_NODE_t *nod, AC_ALPHABET_t alpha);
	ACT_NODE_t *node_find_next (ACT_NODE_t *nod, AC_ALPHABET_t alpha);
	ACT_NODE_t *node_find_next_bs (ACT_NODE_t *nod, AC_ALPHABET_t alpha);

	void node_assign_id (ACT_NODE_t *nod);
	void node_add_edge (ACT_NODE_t *nod, ACT_NODE_t *next, AC_ALPHABET_t alpha);
	void node_sort_edges (ACT_NODE_t *nod);
	void node_accept_pattern (ACT_NODE_t *nod, AC_PATTERN_t *new_patt, int copy);
	void node_collect_matches (ACT_NODE_t *nod);
	void node_release_vectors (ACT_NODE_t *nod);
	int  node_book_replacement (ACT_NODE_t *nod);
	void node_display (ACT_NODE_t *nod);

#ifdef __cplusplus
}
#endif

#endif
