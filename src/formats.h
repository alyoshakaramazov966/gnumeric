#ifndef GNUMERIC_FORMATS_H
#define GNUMERIC_FORMATS_H

typedef enum
{
    FMT_UNKNOWN = -1,

    FMT_GENERAL	 = 0,
    FMT_NUMBER	 = 1,
    FMT_CURRENCY = 2,
    FMT_ACCOUNT	 = 3,
    FMT_DATE	 = 4,
    FMT_TIME	 = 5,
    FMT_PERCENT	 = 6,
    FMT_FRACTION = 7,
    FMT_SCIENCE	 = 8,
    FMT_TEXT	 = 9,
    FMT_SPECIAL  = 10
} FormatFamily;

typedef struct
{
    char const * const symbol;
    char const * const description;
} CurrencySymbol;

typedef struct
{
	gboolean thousands_sep;
	gint	 num_decimals;	/* 0 - 30 */
	gint	 negative_fmt;	/* 0 - 3 */
	gint	 currency_symbol_index;
	gint	 list_element;
} FormatCharacteristics;

FormatFamily           cell_format_classify (char const * const fmt, FormatCharacteristics *info);

/* Indexed by FormatCharacteristics */
extern char const * const * const cell_formats [];

extern CurrencySymbol const currency_symbols [];

#endif