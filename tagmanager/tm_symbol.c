/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tm_symbol.h"
#include "tm_workspace.h"

static GMemChunk *sym_mem_chunk = NULL;

#define SYM_NEW(T) {\
	if (!sym_mem_chunk) \
		sym_mem_chunk = g_mem_chunk_new("TMSymbol MemChunk", sizeof(TMSymbol), 1024 \
		  , G_ALLOC_AND_FREE); \
	(T) = g_chunk_new0(TMSymbol, sym_mem_chunk);}

#define SYM_FREE(T) g_mem_chunk_free(sym_mem_chunk, (T))

void tm_symbol_print(TMSymbol *sym, guint level)
{
	guint i;

	g_return_if_fail (sym != NULL);
	for (i=0; i < level; ++i)
		fputc('\t', stderr);
	fprintf(stderr, "%s\n", (sym->tag)?sym->tag->name:"Root");
	if (sym->info.children)
	{
		if (sym->tag)
	    	{
			if (tm_tag_function_t == sym->tag->type ||
			    tm_tag_prototype_t == sym->tag->type)
			{
				tm_tag_print(sym->info.equiv, stderr);
			}
		}
		else
		{
			for (i=0; i < sym->info.children->len; ++i)
				tm_symbol_print(TM_SYMBOL(sym->info.children->pdata[i])
				  , level + 1);
		}
	}
}

#define SYM_ORDER(T) (((tm_tag_class_t == (T)->type) || (tm_tag_struct_t ==\
	(T)->type))?1:(((tm_tag_enum_t == (T)->type) || (tm_tag_interface_t ==\
	(T)->type))?2:3))

/* Comparison function for sorting symbols alphabetically */
int tm_symbol_compare(const void *p1, const void *p2)
{
	TMSymbol *s1, *s2;

	if (!p1 && !p2)
		return 0;
	else if (!p2)
		return 1;
	else if (!p1)
		return -1;
	s1 = *(TMSymbol **) p1;
	s2 = *(TMSymbol **) p2;
	if (!s1 && !s2)
		return 0;
	else if (!s2)
		return 1;
	else if (!s1)
		return -1;
	if (!s1->tag && !s2->tag)
		return 0;
	else if (!s2->tag)
		return 1;
	else if (!s1->tag)
		return -1;
	return strcmp(s1->tag->name, s2->tag->name);
}

/*
 * Compares function argument lists.
 * FIXME: Compare based on types, not an exact string match.
 */
int tm_arglist_compare(const TMTag* t1, const TMTag* t2)
{
	return strcmp(NVL(t1->atts.entry.arglist, "()"),
			NVL(t2->atts.entry.arglist, "()"));
}

/* Need this custom compare function to generate a symbol tree
in a simgle pass from tag list */
int tm_symbol_tag_compare(const TMTag **t1, const TMTag **t2)
{
	gint s1, s2;

	if ((!t1 && !t2) || (!*t1 && !*t2))
		return 0;
	else if (!t1 || !*t1)
		return -1;
	else if (!t2 || !*t2)
		return 1;
	if ((tm_tag_file_t == (*t1)->type) && (tm_tag_file_t == (*t2)->type))
		return 0;
	else if (tm_tag_file_t == (*t1)->type)
		return -1;
	else if (tm_tag_file_t == (*t2)->type)
		return 1;

	/* Compare on depth of scope - less depth gets higher priortity */
	s1 = tm_tag_scope_depth(*t1);
	s2 = tm_tag_scope_depth(*t2);
	if (s1 != s2)
		return (s1 - s2);

	/* Compare of tag type using a symbol ordering routine */
	s1 = SYM_ORDER(*t1);
	s2 = SYM_ORDER(*t2);
	if (s1 != s2)
		return (s1 - s2);

	/* Compare names alphabetically */
	s1 = strcmp((*t1)->name, (*t2)->name);
	if (s1 != 0)
		return (s1);

	/* Compare scope alphabetically */
	s1 = strcmp(NVL((*t1)->atts.entry.scope, ""),
	  NVL((*t2)->atts.entry.scope, ""));
	if (s1 != 0)
		return s1;

	/* If none of them are function/prototype, they are effectively equal */
	if ((tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_prototype_t != (*t1)->type)&&
	    (tm_tag_function_t != (*t2)->type) &&
	    (tm_tag_prototype_t != (*t2)->type))
		return 0;

	/* Whichever is not a function/prototype goes first */
	if ((tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_prototype_t != (*t1)->type))
		return -1;
	if ((tm_tag_function_t != (*t2)->type) &&
	    (tm_tag_prototype_t != (*t2)->type))
		return 1;

	/* Compare the argument list */
	s1 = tm_arglist_compare(*t1, *t2);
	if (s1 != 0)
		return s1;

	/* Functions go before prototypes */
	if ((tm_tag_function_t == (*t1)->type) &&
	    (tm_tag_function_t != (*t2)->type))
		return -1;
	if ((tm_tag_function_t != (*t1)->type) &&
	    (tm_tag_function_t == (*t2)->type))
		return 1;
	
	/* Give up */
	return 0;
}

static void
check_children_symbols(TMSymbol *sym, const char *name, gint level)
{
  if (level > 5) {
    g_warning ("Infinite recursion detected (symbol name = %s) !!", name);
    return;
  }
  if(sym && name)
  {
    GPtrArray *scope_tags;
    TMTag *tag;  
    TMSymbol *sym1 = sym->parent;
    
    while(sym1 && (tag = sym1->tag) && tag->name)
    {
      if(0 == strcmp(tag->name, name))
      {
        return;
      }
      sym1 = sym1->parent;
    }
    
    scope_tags = tm_tags_extract (tm_workspace_find_scope_members (NULL, name, FALSE, FALSE), tm_tag_max_t);
    if(scope_tags && scope_tags->len > 0)
    {
      unsigned int j;
      sym1 = NULL;
      
      tm_tags_custom_sort (scope_tags,
			   (TMTagCompareFunc) tm_symbol_tag_compare,
			   FALSE);
      sym->info.children = g_ptr_array_sized_new(scope_tags->len);
      for (j=0; j < scope_tags->len; ++j)
      {
        tag = TM_TAG(scope_tags->pdata[j]);
	if (tm_tag_prototype_t == tag->type)
	{
		/* Since the tags are sorted, we can now expect to find
		 * identical definitions/declarations in the previous element.
		 * Functions will come before their prototypes.
		 */
		if (sym1 && (tm_tag_function_t == sym1->tag->type) &&
		  (!sym1->info.equiv) &&
		  (0 == strcmp(NVL(tag->atts.entry.scope, ""),
			       NVL(sym1->tag->atts.entry.scope, ""))) &&
		  (0 == strcmp(tag->name, sym1->tag->name)) &&
		  (0 == tm_arglist_compare(tag, sym1->tag)))
		{
		  	sym1->info.equiv = tag;
		  	continue;
		}
	}
	if (strcmp(tag->name, sym->tag->name) == 0)
	{
            continue; /* Avoid recursive definition */
	}
        SYM_NEW(sym1);
        sym1->tag = tag;
        sym1->parent = sym;
        g_ptr_array_add(sym->info.children, sym1);
      }
      
      for (j=0; j < sym->info.children->len; ++j)
      {        
        sym1 = TM_SYMBOL(sym->info.children->pdata[j]);
        if ((tm_tag_member_t & sym1->tag->type) == tm_tag_member_t &&
            sym1->tag->atts.entry.pointerOrder == 0)
          check_children_symbols(sym1, sym1->tag->atts.entry.type_ref[1],
                                 level + 1);
      }
    }
    if (scope_tags)
      g_ptr_array_free (scope_tags, TRUE);
  }
  return;
}

static int
tm_symbol_get_root_index (TMSymbol * sym)
{
	int idx;
	char access;

	access = sym->tag->atts.entry.access;
	switch (sym->tag->type)
	{
	case tm_tag_class_t:
		idx = 0;
		break;
	case tm_tag_struct_t:
		idx = 1;
		break;
	case tm_tag_union_t:
		idx = 2;
		break;
	case tm_tag_function_t:
		switch (access)
		{
		case TAG_ACCESS_PRIVATE:
		case TAG_ACCESS_PROTECTED:
		case TAG_ACCESS_PUBLIC:
			idx = 8;
			break;
		default:
			idx = 3;
			break;
		}
		break;
	case tm_tag_prototype_t:
		if ((sym->info.equiv) && (TAG_ACCESS_UNKNOWN == access))
			access = sym->info.equiv->atts.entry.access;
		switch (access)
		{
		case TAG_ACCESS_PRIVATE:
		case TAG_ACCESS_PROTECTED:
		case TAG_ACCESS_PUBLIC:
			idx = 8;
			break;
		default:
			idx = 3;
			break;
		}
		break;
	case tm_tag_member_t:
		switch (access)
		{
		case TAG_ACCESS_PRIVATE:
		case TAG_ACCESS_PROTECTED:
		case TAG_ACCESS_PUBLIC:
			idx = 8;
			break;
		default:
			idx = 4;
			break;
		}
		break;
	case tm_tag_variable_t:
	case tm_tag_externvar_t:
		idx = 4;
		break;
	case tm_tag_macro_t:
	case tm_tag_macro_with_arg_t:
		idx = 5;
		break;
	case tm_tag_typedef_t:
		idx = 6;
		break;
	case tm_tag_enumerator_t:
		idx = 7;
		break;
	default:
		idx = 8;
		break;
	}
	return idx;
}

TMSymbol *tm_symbol_tree_new(GPtrArray *tags_array)
{
	GPtrArray *tags;
	TMSymbol *grand_root = NULL;
	static int subroot_types[] = {
		tm_tag_class_t, tm_tag_struct_t, tm_tag_union_t,
		tm_tag_prototype_t, tm_tag_variable_t, tm_tag_macro_t,
		tm_tag_typedef_t, tm_tag_enumerator_t, tm_tag_other_t,
		tm_tag_undef_t
	};
	static TMTag subroot_tags[sizeof(subroot_types)] = {
		{0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0}
	};
	static char* subroot_labels[sizeof(subroot_types)] = {
		"Classes", "Structs", "Unions", "Functions", "Variables",
		"Macros", "Typedefs", "Enumerators", "Others", NULL
	};
	TMSymbol *subroot[sizeof(subroot_types)] = {NULL, NULL, NULL,
		NULL, NULL, NULL, NULL, NULL, NULL, NULL};
	
#ifdef TM_DEBUG
	g_message("Building symbol tree..");
#endif
	
	if ((!tags_array) || (tags_array->len <= 0))
		return NULL;
	
#ifdef TM_DEBUG
	fprintf(stderr, "Dumping all tags..\n");
	tm_tags_array_print(tags_array, stderr);
#endif
	
	tags = tm_tags_extract(tags_array, tm_tag_max_t);
#ifdef TM_DEBUG
	fprintf(stderr, "Dumping unordered tags..\n");
	tm_tags_array_print(tags, stderr);
#endif
	
	if (tags && (tags->len > 0))
	{
		guint i;
				
		TMTag *tag;
		TMSymbol *sym = NULL;
		
		/* Creating top-level symbols */
		SYM_NEW(grand_root);
		if (!grand_root->info.children)
			grand_root->info.children = g_ptr_array_new();
		
		for (i = 0; subroot_types[i] != tm_tag_undef_t; i++)
		{
			SYM_NEW(sym);
			subroot_tags[i].name = subroot_labels[i];
			subroot_tags[i].type = subroot_types[i];
			sym->tag = &subroot_tags[i];
			sym->parent = grand_root;
			g_ptr_array_add(grand_root->info.children, sym);
			subroot[i] = sym;
		}
		
		tm_tags_custom_sort(tags, (TMTagCompareFunc) tm_symbol_tag_compare
		  , FALSE);
    
#ifdef TM_DEBUG
		fprintf(stderr, "Dumping ordered tags..");
		tm_tags_array_print(tags, stderr);
		fprintf(stderr, "Rebuilding symbol table..\n");
#endif
		sym = NULL;
		for (i=0; i < tags->len; ++i)
		{
			tag = TM_TAG(tags->pdata[i]);
			
			if (tm_tag_prototype_t == tag->type)
			{
				/* Since the tags are sorted, we can now expect to find
				 * identical definitions/declarations in the previous element.
				 * Functions will come before their prototypes.
				 */
				if (sym && (tm_tag_function_t == sym->tag->type) &&
				  (!sym->info.equiv) &&
				  (0 == strcmp(NVL(tag->atts.entry.scope, ""),
					       NVL(sym->tag->atts.entry.scope, ""))) &&
				  (0 == strcmp(tag->name, sym->tag->name)) &&
				  (0 == tm_arglist_compare(tag, sym->tag)))
				{
					sym->info.equiv = tag;
					continue;
				}
			}

			if(tag->atts.entry.scope)
			{
				if ((tm_tag_class_t | tm_tag_enum_t |
				     tm_tag_struct_t | tm_tag_union_t) & tag->type)
				{
					/* this is Hack an shold be fixed by adding this info in tag struct */
					if(NULL != strstr(tag->name, "_fake_"))
					{
						continue;
					}
				}
				else
				{
					continue;
				}
			}
			else
			{
				if(!(tag->type & tm_tag_enum_t)
				   && NULL != strstr(tag->name, "_fake_"))
				{
					/* This is Hack an should be fixed by adding this info in tag struct */
					continue;
				}
			}
			SYM_NEW(sym);
			sym->tag = tag;
			sym->parent = subroot[tm_symbol_get_root_index (sym)];
			if (!sym->parent->info.children)
				sym->parent->info.children = g_ptr_array_new();
			g_ptr_array_add(sym->parent->info.children, sym);
			
			if ((tm_tag_undef_t | tm_tag_function_t | tm_tag_prototype_t |
				 tm_tag_macro_t | tm_tag_macro_with_arg_t | tm_tag_file_t)
				& tag->type)
				continue;
			
			check_children_symbols(sym, tag->name, 0);
		}
#ifdef TM_DEBUG
		fprintf(stderr, "Done.Dumping symbol tree..");
		tm_symbol_print(grand_root, 0);
#endif
	}
	if (tags)
		g_ptr_array_free(tags, TRUE);
	
	return grand_root;
}

static void tm_symbol_free(TMSymbol *sym)
{
	if (!sym)
		return;
	if (sym->info.children)
	{
		guint i;
		for (i=0; i < sym->info.children->len; ++i)
			tm_symbol_free(TM_SYMBOL(sym->info.children->pdata[i]));
		g_ptr_array_free(sym->info.children, TRUE);
		sym->info.children = NULL;
	}
	SYM_FREE(sym);
}

void tm_symbol_tree_free(gpointer root)
{
	if (root)
		tm_symbol_free(TM_SYMBOL(root));
}

TMSymbol *tm_symbol_tree_update(TMSymbol *root, GPtrArray *tags)
{
	if (root)
		tm_symbol_free(root);
	if ((tags) && (tags->len > 0))
		return tm_symbol_tree_new(tags);
	else
		return NULL;
}