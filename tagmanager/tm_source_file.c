/*
*
*   Copyright (c) 2001-2002, Biswapesh Chattopadhyay
*
*   This source code is released for free distribution under the terms of the
*   GNU General Public License.
*
*/

#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "general.h"
#include "entry.h"
#include "parse.h"
#include "read.h"
#define LIBCTAGS_DEFINED
#include "tm_work_object.h"

#include "tm_source_file.h"
#include "tm_tag.h"


guint source_file_class_id = 0;
static TMSourceFile *current_source_file = NULL;

gboolean tm_source_file_init(TMSourceFile *source_file, const char *file_name
  , gboolean update)
{
	if (0 == source_file_class_id)
		source_file_class_id = tm_work_object_register(tm_source_file_free
		  , tm_source_file_update, NULL);

#ifdef TM_DEBUG
	g_message("Source File init: %s", file_name);
#endif

	if (FALSE == tm_work_object_init(&(source_file->work_object),
		  source_file_class_id, file_name, FALSE))
		return FALSE;
	source_file->lang = LANG_AUTO;
	source_file->inactive = FALSE;
	if (update)
		tm_source_file_update(TM_WORK_OBJECT(source_file), FALSE, FALSE, FALSE);
	return TRUE;
}

TMWorkObject *tm_source_file_new(const char *file_name, gboolean update)
{
	TMSourceFile *source_file = g_new(TMSourceFile, 1);
	if (TRUE != tm_source_file_init(source_file, file_name, update))
	{
		g_free(source_file);
		return NULL;
	}
	return (TMWorkObject *) source_file;
}

void tm_source_file_destroy(TMSourceFile *source_file)
{
#ifdef TM_DEBUG
	g_message("Destroying source file: %s", source_file->work_object.file_name);
#endif

	if (NULL != TM_WORK_OBJECT(source_file)->tags_array)
	{
		tm_tags_array_free(TM_WORK_OBJECT(source_file)->tags_array, TRUE);
		TM_WORK_OBJECT(source_file)->tags_array = NULL;
	}
	tm_work_object_destroy(&(source_file->work_object));
}

void tm_source_file_free(gpointer source_file)
{
	if (NULL != source_file)
	{
		tm_source_file_destroy(source_file);
		g_free(source_file);
	}
}

gboolean tm_source_file_parse(TMSourceFile *source_file)
{
	const char *file_name;
	gboolean status = TRUE;

	source_file->lang = LANG_AUTO;

	source_file->lang = LANG_AUTO;

	if ((NULL == source_file) || (NULL == source_file->work_object.file_name))
	{
		g_warning("Attempt to parse NULL file");
		return FALSE;
	}

#ifdef TM_DEBUG
	g_message("Parsing source file %s", source_file->work_object.file_name);
#endif
	file_name = source_file->work_object.file_name;
	if (NULL == LanguageTable)
	{
		initializeParsing();
		installLanguageMapDefaults();
		if (NULL == TagEntryFunction)
			TagEntryFunction = tm_source_file_tags;
	}
	
	current_source_file = source_file;
	if (LANG_AUTO == source_file->lang) {
		source_file->lang = getFileLanguage (file_name);
#ifdef TM_DEBUG
		g_message("New language detected %d", source_file->lang);
#endif
		
	}
	
		
	if (source_file->lang == LANG_IGNORE)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (unknown language)\n", file_name);
#endif
	}
	else if (! LanguageTable [source_file->lang]->enabled)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (language disabled)\n", file_name);
#endif
	}
	else
	{
		int passCount = 1;
		while ((TRUE == status) && (passCount < 3))
		{
			if (source_file->work_object.tags_array)
				tm_tags_array_free(source_file->work_object.tags_array, FALSE);
#ifdef TM_DEBUG			
			printf("DEBUG: opening file: %s\n", file_name );
#endif
			if (fileOpen (file_name, source_file->lang))
			{
				if (LanguageTable [source_file->lang]->parser != NULL)
				 	LanguageTable [source_file->lang]->parser ();
				else if (LanguageTable [source_file->lang]->parser2 != NULL)
					status = LanguageTable [source_file->lang]->parser2 (passCount);
				fileClose ();
			}
			else
			{
				g_warning("Unable to open %s", file_name);
				return FALSE;
			}
			++ passCount;
		}
		return TRUE;
	}
	return status;
}

gboolean tm_source_file_buffer_parse(TMSourceFile *source_file, unsigned char* text_buf
		, int text_buf_size)
{
	const char *file_name;
	gboolean status = TRUE;

	if ((NULL == source_file) || (NULL == source_file->work_object.file_name))
	{
		g_warning("Attempt to parse NULL file");
		return FALSE;
	}

	if ((NULL == text_buf) || (0 == text_buf_size)) 
	{
		g_warning("Attempt to parse a NULL text buffer");
	}
	
	file_name = source_file->work_object.file_name;
	if (NULL == LanguageTable)
	{
		initializeParsing();
		installLanguageMapDefaults();
		if (NULL == TagEntryFunction)
			TagEntryFunction = tm_source_file_tags;
	}
	current_source_file = source_file;
	if (LANG_AUTO == source_file->lang)
		source_file->lang = getFileLanguage (file_name);
	if (source_file->lang == LANG_IGNORE)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (unknown language)\n", file_name);
#endif
	}
	else if (! LanguageTable [source_file->lang]->enabled)
	{
#ifdef TM_DEBUG
		g_warning("ignoring %s (language disabled)\n", file_name);
#endif
	}
	else
	{
		int passCount = 1;
		while ((TRUE == status) && (passCount < 3))
		{
			if (source_file->work_object.tags_array)
				tm_tags_array_free(source_file->work_object.tags_array, FALSE);
			if (bufferOpen (text_buf, text_buf_size, file_name, source_file->lang))
			{
				if (LanguageTable [source_file->lang]->parser != NULL)
					LanguageTable [source_file->lang]->parser ();
				else if (LanguageTable [source_file->lang]->parser2 != NULL)
					status = LanguageTable [source_file->lang]->parser2 (passCount);
				bufferClose ();
			}
			else
			{
				g_warning("Unable to open %s", file_name);
				return FALSE;
			}
			++ passCount;
		}
		return TRUE;
	}
	return status;
}

int tm_source_file_tags(const tagEntryInfo *tag)
{
	if (NULL == current_source_file)
		return 0;
	if (NULL == current_source_file->work_object.tags_array)
		current_source_file->work_object.tags_array = g_ptr_array_new();
	g_ptr_array_add(current_source_file->work_object.tags_array,
	  tm_tag_new(current_source_file, tag));
	return TRUE;
}

gboolean tm_source_file_update(TMWorkObject *source_file, gboolean force
  , gboolean __unused__ recurse, gboolean update_parent)
{
#ifdef TM_DEBUG
	g_message("Updating source file %s", source_file->file_name);
#endif
	if (force || (tm_work_object_is_changed(source_file)))
	{
		tm_source_file_parse(TM_SOURCE_FILE(source_file));
		tm_tags_sort(source_file->tags_array, NULL, FALSE);
		source_file->analyze_time = time(NULL);
		if ((source_file->parent) && update_parent)
		{
#ifdef TM_DEBUG
			g_message("Updating parent [project]..");
#endif
			tm_work_object_update(source_file->parent, TRUE, FALSE, TRUE);
		}
#ifdef TM_DEBUG
		else
			g_message("Skipping parent update because parent is %s and update_parent is %s"
			  , source_file->parent?"NOT NULL":"NULL", update_parent?"TRUE":"FALSE");
#endif
		return TRUE;
	}
	else {
#ifdef	TM_DEBUG
		g_message ("no parsing of %s has been done", source_file->file_name);
#endif
		return FALSE;
	}
}


gboolean tm_source_file_buffer_update(TMWorkObject *source_file, unsigned char* text_buf
	, int text_buf_size, gboolean update_parent)
{
#ifdef TM_DEBUG
	g_message("Buffer updating based on source file %s", source_file->file_name);
#endif

	tm_source_file_buffer_parse (TM_SOURCE_FILE(source_file), text_buf, text_buf_size);
	tm_tags_sort(source_file->tags_array, NULL, FALSE);
	source_file->analyze_time = time(NULL);
	if ((source_file->parent) && update_parent)
	{
#ifdef TM_DEBUG
		g_message("Updating parent [project] from buffer..");
#endif
		tm_work_object_update(source_file->parent, TRUE, FALSE, TRUE);
	}
#ifdef TM_DEBUG
	else
		g_message("Skipping parent update because parent is %s and update_parent is %s"
		  , source_file->parent?"NOT NULL":"NULL", update_parent?"TRUE":"FALSE");
		  
#endif
	return TRUE;
}


gboolean tm_source_file_write(TMWorkObject *source_file, FILE *fp, guint attrs)
{
	TMTag *tag;
	guint i;

	if (NULL != source_file)
	{
		if (NULL != (tag = tm_tag_new(TM_SOURCE_FILE(source_file), NULL)))
		{
			tm_tag_write(tag, fp, tm_tag_attr_max_t);
			tm_tag_free(tag);
			if (NULL != source_file->tags_array)
			{
				for (i=0; i < source_file->tags_array->len; ++i)
				{
					tag = TM_TAG(source_file->tags_array->pdata[i]);
					if (TRUE != tm_tag_write(tag, fp, attrs))
						return FALSE;
				}
			}
		}
	}
	return TRUE;
}