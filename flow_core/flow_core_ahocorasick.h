#ifndef flow_core_ahocorasick_h
#define flow_core_ahocorasick_h

#include "flow_core_actypes.h"

#ifdef __cplusplus
extern "C" {
#endif

	/* Forward declaration */
	struct act_node;
	struct mpool;

	typedef struct ac_trie
	{
		struct act_node *root;  

		size_t patterns_count;   

		short trie_open; 

		struct mpool *mp;

		struct act_node *last_node; 
		size_t base_position; 

		AC_TEXT_t *text; 
		size_t position;  

		ACT_WORKING_MODE_t wm; 

	} AC_TRIE_t;


	AC_TRIE_t *ac_trie_create (void);
	AC_STATUS_t ac_trie_add (AC_TRIE_t *thiz, AC_PATTERN_t *patt, int copy);
	void ac_trie_finalize (AC_TRIE_t *thiz);
	void ac_trie_release (AC_TRIE_t *thiz);
	void ac_trie_display (AC_TRIE_t *thiz);

	int  ac_trie_search (AC_TRIE_t *thiz, AC_TEXT_t *text, int keep, 
			AC_MATCH_CALBACK_f callback, void *param);

	void ac_trie_settext (AC_TRIE_t *thiz, AC_TEXT_t *text, int keep);
	AC_MATCH_t ac_trie_findnext (AC_TRIE_t *thiz);


#ifdef __cplusplus
}
#endif

#endif
