#include <stdio.h>
#include <string.h>

#include "flow_core_ahocorasick.h"

AC_ALPHABET_t *sample_patterns[] = {
	"experience",
	"新闻",
	"whatever",
};
#define PATTERN_COUNT (sizeof(sample_patterns)/sizeof(AC_ALPHABET_t *))

AC_ALPHABET_t *chunkstr = "experience ease and simplicity of multifast";

void print_match (AC_MATCH_t *m);

int main (int argc, char **argv)
{
	unsigned int i;
	AC_TRIE_t *trie;
	AC_PATTERN_t patt;
	AC_TEXT_t chunk;
	AC_MATCH_t match;

	trie = ac_trie_create ();
	for (i = 0; i < PATTERN_COUNT; i++)
	{
		patt.ptext.astring = sample_patterns[i];
		patt.ptext.length = strlen(patt.ptext.astring);
		patt.rtext.astring = NULL;
		patt.rtext.length = 0;
		patt.id.u.number = i + 1;
		patt.id.type = AC_PATTID_TYPE_NUMBER;

		ac_trie_add (trie, &patt, 0);
	}
	ac_trie_finalize (trie);

	printf ("Searching: \"%s\"\n", chunkstr);
	chunk.astring = chunkstr;
	chunk.length = strlen (chunk.astring);
	ac_trie_settext (trie, &chunk, 0);

	while ((match = ac_trie_findnext(trie)).size)
	{
		print_match (&match);
	}

	/* You may release the automata after you have done with it. */
	ac_trie_release (trie);

	return 0;
}

void print_match (AC_MATCH_t *m)
{
	unsigned int j;
	AC_PATTERN_t *pp;

	printf ("@%2lu found: ", m->position);

	for (j = 0; j < m->size; j++)
	{
		pp = &m->patterns[j];

		printf("#%ld \"%.*s\", ", pp->id.u.number,
				(int)pp->ptext.length, pp->ptext.astring);

		/* CAUTION: the AC_PATTERN_t::ptext.astring pointers, point to the 
		 * sample patters in our program, since we added patterns with copy 
		 * option disabled.
		 */        
	}

	printf ("\n");
}
