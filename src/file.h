#ifndef GNUMERIC_FILE_H
#define GNUMERIC_FILE_H

#include "sheet.h"

typedef gboolean  (*FileFormatProbe)(const char *filename);
typedef gboolean  (*FileFormatOpen) (Workbook *wb, const char *filename);
typedef int       (*FileFormatSave) (Workbook *wb, const char *filename);

typedef struct {
	int             priority;
	char           *format_description;
	FileFormatProbe probe;
	FileFormatOpen  open;
} FileOpener;

/* A GList of FileOpener structures */
extern GList *gnumeric_file_openers;

typedef struct {
	char            *extension;
	char            *format_description;
	FileFormatSave  save;
} FileSaver;

/* A GList of FileSaver structures */
extern GList *gnumeric_file_savers;

void file_format_register_open   (int             priority,
				  const char     *format_description,
				  FileFormatProbe probe_fn,
				  FileFormatOpen  open_fn);
void file_format_unregister_open (FileFormatProbe probe, FileFormatOpen open);

void file_format_register_save   (char           *extension,
				  const char     *format_description,
				  FileFormatSave save_fn);
void file_format_unregister_save (FileFormatSave save);

Workbook *workbook_import        (Workbook *parent_dlg, const char *filename);
Workbook *workbook_import_with   (const char *filename, FileFormatOpen open_fn);
gboolean  workbook_load_from     (Workbook *wb, const char *filename);

#endif /* GNUMERIC_FILE_H */