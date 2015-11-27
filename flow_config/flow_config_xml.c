
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <string.h>
#include <libxml/xmlreader.h>

#include "flow_config_xml.h"

xmlDocPtr config_new(const char *f1)
{
	xmlDocPtr pdoc;
	xmlNodePtr proot;

	/** Create Doc **/
	pdoc = xmlNewDoc(BAD_CAST "1.0");
	proot = xmlNewNode(NULL, BAD_CAST f1);
	xmlNewProp(proot, BAD_CAST "Version", BAD_CAST "1.0");
	xmlDocSetRootElement(pdoc, proot);

	return pdoc;
}

int config_save(xmlDocPtr pdoc, const char *filename)
{
	if (access(filename, F_OK) == 0)
		unlink(filename);

	return xmlSaveFormatFileEnc(filename, pdoc, "UTF-8", 1); 
}

const char *config_search_attr_value(xmlNodePtr pnode, const char *v) 
{
	return (char *)xmlGetProp(pnode, BAD_CAST v); 
}

xmlAttrPtr config_set_attr_value(xmlNodePtr pnode, const char *attr, const char *v) 
{
	return xmlSetProp(pnode, BAD_CAST attr, BAD_CAST v); 
}

xmlNodePtr config_search_list(xmlNodePtr pnode, const char *v) 
{
	while (pnode != NULL)
	{
		if (!xmlStrcmp(pnode->name, BAD_CAST v))
		{
			break;
		}
		pnode = pnode->next;
	}
	return pnode;
}

xmlNodePtr config_search_next(xmlNodePtr pnode, const char *v) 
{
	return config_search_list(pnode->next, v); 
}

xmlNodePtr config_search_children(xmlNodePtr pnode, const char *v) 
{
	return config_search_list(pnode->children, v); 
}

const char *config_search_children_value(xmlNodePtr pnode)
{
	return (char *)pnode->children->content;
}

xmlNodePtr config_search4(xmlDocPtr pdoc, const char *f1,
				const char *f2, const char *f3, const char *f4)
{
	xmlNodePtr proot, pnode;

	/** Get Root Node **/
	if ((proot = xmlDocGetRootElement(pdoc)) == NULL)
	{
		return NULL;
	}

	/** Check Root Node **/
	if (xmlStrcmp(proot->name, BAD_CAST f1) != 0)
	{
		return NULL;
	}

	/** check level 1 **/
	pnode = config_search_children(proot, f2);

	/** check level 2 **/
	if (!pnode || !f3)
	{
		return pnode;
	}
	pnode = config_search_children(pnode, f3);

	/** check level 3 **/
	if (!pnode || !f4)
	{
		return pnode;
	}
	pnode = config_search_children(pnode, f4);
	return pnode;
}


xmlNodePtr config_add_node4(xmlDocPtr pdoc, const char *f1,
				const char *f2, const char *f3, const char *f4)
{
	xmlNodePtr proot, pnode = NULL, ptmp;

	/** Get Root Node **/
	proot = xmlDocGetRootElement(pdoc);

	/** Check Root Node **/
	if (xmlStrcmp(proot->name, BAD_CAST f1) != 0)
	{
		return NULL;
	}

	/** check level 1 **/
	if ((ptmp = config_search_children(proot, f2)) == NULL)
	{
		ptmp = config_add_children_node(proot, f2);
	}
	pnode = ptmp;

	/** check level 2 **/
	if (!f3)
	{
		return pnode;
	}

	if ((ptmp = config_search_children(pnode, f3)) == NULL) 
	{
		ptmp = config_add_children_node(pnode, f3);
	}
	pnode = ptmp;

	/** check level 3 **/
	if (!f4)
	{
		return pnode;
	}
	if ((ptmp = config_search_children(pnode, f4)) == NULL)
	{
		ptmp = config_add_children_node(pnode, f4);
	}
	pnode = ptmp;
	return pnode;
}

xmlNodePtr config_add_children_node(xmlNodePtr pnode, const char *v)
{
	return xmlNewChild(pnode, NULL, BAD_CAST v, NULL);
}


xmlDocPtr config_load(const char *filename)
{
	xmlDocPtr pdoc = NULL;
	xmlNodePtr proot = NULL;

	if (access(filename, F_OK) == -1) 
	{
		return NULL;
	}
	else
	{
		pdoc = xmlReadFile(filename, "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
		if (pdoc)
		{
			proot = xmlDocGetRootElement(pdoc);
			if (proot)
				return pdoc;

			config_free(pdoc);
		}

		pdoc = xmlReadFile(filename, "GB2312", XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
		if (pdoc)
		{
			if (proot)
				return pdoc;

			config_free(pdoc);
		}

		pdoc = xmlReadFile(filename, "UNICODE", XML_PARSE_RECOVER | XML_PARSE_NOBLANKS);
		if (pdoc)
		{
			if (proot)
				return pdoc;

			config_free(pdoc);
		}
	}

	return NULL;
}

void config_free(xmlDocPtr pdoc)
{
	xmlFreeDoc(pdoc);
	xmlCleanupParser();
	xmlMemoryDump();
}

void config_attr_free(const char *attr)
{
	if (attr != NULL)
		xmlFree((xmlChar *)attr);
}

xmlNodePtr config_unlink_children4(xmlDocPtr pdoc, const char *f1,
					const char *f2, const char *f3, const char *f4)
{
	xmlNodePtr pnode;

	if ((pnode = config_search4(pdoc, f1, f2, f3, f4)) == NULL)
	{
		pnode = config_add_node4(pdoc, f1, f2, f3, f4);
	}
	else if (pnode->children)
	{
		xmlFreeNodeList(pnode->children);
		pnode->children = NULL;
	}
	return pnode;
}
