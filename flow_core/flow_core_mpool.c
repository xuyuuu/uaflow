#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "flow_core_mpool.h"


#define MPOOL_BLOCK_SIZE (24*1024)

#if (MPOOL_BLOCK_SIZE % 16 > 0)
#error "MPOOL_BLOCK_SIZE must be multiple 16"
#endif

struct mpool_block
{
    size_t size;
    unsigned char *bp;      /* Block pointer */
    unsigned char *free;    /* Free area; End of allocated section */
    
    struct mpool_block *next; /* Next block */
};

struct mpool 
{
    struct mpool_block *block;
};

static struct mpool_block *mpool_new_block (size_t size) 
{
    struct mpool_block *block;
    
    if (!size) 
        size = MPOOL_BLOCK_SIZE;
    
    block = (struct mpool_block *) malloc (sizeof(struct mpool_block));
    
    block->bp = block->free = malloc(size);
    block->size = size;
    block->next = NULL;
    
    return block;
}

struct mpool *mpool_create (size_t size) 
{
    struct mpool *ret;
    
    ret = malloc (sizeof(struct mpool));
    ret->block = mpool_new_block(size);
    
    return ret;
}

void mpool_free (struct mpool *pool) 
{
    struct mpool_block *p, *p_next;
    
    if (!pool)
        return;
    
    if (!pool->block) {
        free(pool);
	return;
    }
    
    p = pool->block;
    
    while (p) {
	p_next = p->next;
	free(p->bp);
	free(p);
	p = p_next;
    }
    
    free(pool);
}

void *mpool_malloc (struct mpool *pool, size_t size) 
{
    void *ret = NULL;
    struct mpool_block *block, *new_block;
    size_t remain, block_size;
    
    if(!pool || !pool->block || !size)
	return NULL;
    
    size = (size + 15) & ~0xF; /* This is to align memory allocation on 
                                * multiple 16 boundary */
    
    block = pool->block;
    remain = block->size - ((size_t)block->free - (size_t)block->bp);
    
    if (remain < size) 
    {
        /* Allocate a new block */
        block_size = ((size > block->size) ? size : block->size);
	new_block = mpool_new_block (block_size);
	new_block->next = block;
	block = pool->block = new_block;
    }
    
    ret = block->free;
    
    block->free = block->bp + (block->free - block->bp + size);
    
    return ret;
}

void *mpool_strndup (struct mpool *pool, const char *str, size_t n) 
{
    void *ret;
    
    if (!str)
        return NULL;
    
    if ((ret = mpool_malloc(pool, n+1)))
    {
        strncpy((char *)ret, str, n);
        ((char *)ret)[n] = '\0';
    }
    
    return ret;
}

void *mpool_strdup (struct mpool *pool, const char *str) 
{
    size_t len;
    
    if (!str) 
        return NULL;
    len = strlen(str);
    
    return mpool_strndup (pool, str, len);
}
