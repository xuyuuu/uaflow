#ifndef flow_config_xml_h 
#define flow_config_xml_h 

extern xmlDocPtr config_new(const char *f1);
extern xmlDocPtr config_load(const char *filename);
extern int config_save(xmlDocPtr pdoc, const char *filename);

#define config_unlink_children1(pdoc,f1) config_unlink_children4(pdoc, f1, NULL, NULL, NULL)
#define config_unlink_children2(pdoc,f1,f2) config_unlink_children4(pdoc, f1, f2, NULL, NULL)
#define config_unlink_children3(pdoc,f1,f2,f3) config_unlink_children4(pdoc, f1, f2, f3, NULL)

extern xmlNodePtr config_unlink_children4(xmlDocPtr pdoc, const char *f1,
						const char *f2, const char *f3, const char *f4);

extern void config_free(xmlDocPtr pdoc);
extern void config_attr_free(const char *attr);
extern const char *config_search_attr_value(xmlNodePtr pnode, const char *v);
extern xmlAttrPtr config_set_attr_value(xmlNodePtr pnode, const char *attr, const char *v);

extern xmlNodePtr config_search_next(xmlNodePtr pnode, const char *v);
extern xmlNodePtr config_search_children(xmlNodePtr pnode, const char *v);

extern const char *config_search_children_value(xmlNodePtr pnode);

#define config_search1(pdoc,f1) config_search4(pdoc, f1, NULL, NULL, NULL)
#define config_search2(pdoc,f1,f2) config_search4(pdoc, f1, f2, NULL, NULL)
#define config_search3(pdoc,f1,f2,f3) config_search4(pdoc, f1, f2, f3, NULL)

extern xmlNodePtr config_search4(xmlDocPtr pdoc, const char *f1,
					const char *f2, const char *f3, const char *f4);

#define config_add_node1(pdoc,f1) config_add_node4(pdoc, f1, NULL, NULL, NULL)
#define config_add_node2(pdoc,f1,f2) config_add_node4(pdoc, f1, f2, NULL, NULL)
#define config_add_node3(pdoc,f1,f2,f3) config_add_node4(pdoc, f1, f2, f3, NULL)

extern xmlNodePtr config_add_node4(xmlDocPtr pdoc, const char *f1,
					const char *f2, const char *f3, const char *f4);

extern xmlNodePtr config_add_children_node(xmlNodePtr pnode, const char *v);

#endif
