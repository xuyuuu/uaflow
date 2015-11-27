#ifndef flow_core_mpool_h 
#define flow_core_mpool_h	

#ifdef	__cplusplus
extern "C" {
#endif

struct mpool;


struct mpool *mpool_create (size_t size);
void mpool_free (struct mpool *pool);

void *mpool_malloc (struct mpool *pool, size_t size);
void *mpool_strdup (struct mpool *pool, const char *str);
void *mpool_strndup (struct mpool *pool, const char *str, size_t n);


#ifdef	__cplusplus
}
#endif

#endif	/* _MPOOL_H_ */
