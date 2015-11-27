#ifndef _flow_core_list_sort_h
#define _flow_core_list_sort_h


#ifndef likely
#define likely(x)  __builtin_expect((x),1)
#endif

#ifndef unlikely
#define unlikely(x)  __builtin_expect((x),0)
#endif

#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

struct list_head;

void list_sort(void *priv, struct list_head *head,
	       int (*cmp)(void *priv, struct list_head *a,
			  struct list_head *b));
#endif
