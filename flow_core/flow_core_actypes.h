#ifndef flow_core_actypes_h
#define flow_core_actypes_h 

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

	typedef char AC_ALPHABET_t;

	typedef struct ac_text
	{
		const AC_ALPHABET_t *astring;   /**< String of alphabets */
		size_t length;                  /**< String length */
	} AC_TEXT_t;

	enum ac_pattid_type
	{
		AC_PATTID_TYPE_DEFAULT = 0,
		AC_PATTID_TYPE_NUMBER,
		AC_PATTID_TYPE_STRING

	};

	typedef struct ac_pattid
	{
		union
		{
			const char *stringy;    /**< Null-terminated string */
			long number;            /**< Item indicator */
		} u;

		enum ac_pattid_type type;   /**< Shows the type of id */

	} AC_PATTID_t;

	typedef struct ac_pattern
	{
		AC_TEXT_t ptext;    /**< The search string */
		AC_TEXT_t rtext;    /**< The replace string */
		AC_PATTID_t id;   /**< Pattern identifier */

		/*private data*/
		uint32_t itemid;	
		uint8_t idx;
		char name[256];
		int name_length;

	} AC_PATTERN_t;

	typedef struct ac_match
	{
		AC_PATTERN_t *patterns;     /**< Array of matched pattern(s) */
		size_t size;                /**< Number of matched pattern(s) */

		size_t position;    /**< The end position of the matching pattern(s) in 
				     * the input text */
	} AC_MATCH_t;

	typedef enum ac_status
	{
		ACERR_SUCCESS = 0,          /**< No error occurred */
		ACERR_DUPLICATE_PATTERN,    /**< Duplicate patterns */
		ACERR_LONG_PATTERN,         /**< Pattern length is too long */
		ACERR_ZERO_PATTERN,         /**< Empty pattern (zero length) */
		ACERR_TRIE_CLOSED       /**< Trie is closed. */
	} AC_STATUS_t;

	typedef int (*AC_MATCH_CALBACK_f)(AC_MATCH_t *, void *);

	typedef void (*MF_REPLACE_CALBACK_f)(AC_TEXT_t *, void *);

#define AC_PATTRN_MAX_LENGTH 1024

#define MF_REPLACEMENT_BUFFER_SIZE 2048

#if (MF_REPLACEMENT_BUFFER_SIZE <= AC_PATTRN_MAX_LENGTH)
#error "REPLACEMENT_BUFFER_SIZE must be bigger than AC_PATTRN_MAX_LENGTH"
#endif

	typedef enum act_working_mode
	{
		AC_WORKING_MODE_SEARCH = 0, /* Default */
		AC_WORKING_MODE_FINDNEXT,
		AC_WORKING_MODE_REPLACE     /* Not used */
	} ACT_WORKING_MODE_t;


#ifdef __cplusplus
}
#endif

#endif
