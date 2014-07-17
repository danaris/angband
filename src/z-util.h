#ifndef INCLUDED_Z_UTIL_H
#define INCLUDED_Z_UTIL_H

#include "h-basic.h"


/**** Available variables ****/

/**
 * The name of the program.
 */
extern char *argv0;


/* Aux functions */
size_t (*text_mbcs_hook)(wchar_t *dest, const char *src, int n);
extern void (*plog_aux)(const char *);
extern void (*quit_aux)(const char *);


/**** Available Functions ****/

/*
 * Return "s" (or not) depending on whether n is singular.
 */
#define PLURAL(n)		((n) == 1 ? "" : "s")

/**
 * Return the verb form matching the given count
 */
#define VERB_AGREEMENT(count, singular, plural)    (((count) == 1) ? (singular) : (plural))


/**
 * Case insensitive comparison between two strings
 */
extern int my_stricmp(const char *s1, const char *s2);

/**
 * Case insensitive comparison between two strings, up to n characters long.
 */
extern int my_strnicmp(const char *a, const char *b, int n);

/**
 * Case-insensitive strstr
 */
extern char *my_stristr(const char *string, const char *pattern);

/**
 * Copy up to 'bufsize'-1 characters from 'src' to 'buf' and NULL-terminate
 * the result.  The 'buf' and 'src' strings may not overlap.
 *
 * Returns: strlen(src).  This makes checking for truncation
 * easy.  Example:
 *   if (my_strcpy(buf, src, sizeof(buf)) >= sizeof(buf)) ...;
 *
 * This function should be equivalent to the strlcpy() function in BSD.
 */
extern size_t my_strcpy(char *buf, const char *src, size_t bufsize);

/**
 * Try to append a string to an existing NULL-terminated string, never writing
 * more characters into the buffer than indicated by 'bufsize', and
 * NULL-terminating the buffer.  The 'buf' and 'src' strings may not overlap.
 *
 * my_strcat() returns strlen(buf) + strlen(src).  This makes checking for
 * truncation easy.  Example:
 *   if (my_strcat(buf, src, sizeof(buf)) >= sizeof(buf)) ...;
 *
 * This function should be equivalent to the strlcat() function in BSD.
 */
extern size_t my_strcat(char *buf, const char *src, size_t bufsize);

/* Capitalise string 'buf' */
void my_strcap(char *buf);

/* Test equality, prefix, suffix */
extern bool streq(const char *s, const char *t);
extern bool prefix(const char *s, const char *t);
extern bool prefix_i(const char *s, const char *t);
extern bool suffix(const char *s, const char *t);

#define streq(s, t)		(!strcmp(s, t))

/* skip occurrences of a characters */
extern void strskip(char *s, const char c);
extern void strescape(char *s, const char c);

/* determines if a string is "empty" */
bool contains_only_spaces(const char* s);

/* Check if a char is a vowel */
bool is_a_vowel(int ch);


/*
 * Allow override of the multi-byte to wide char conversion
 */
size_t text_mbstowcs(wchar_t *dest, const char *src, int n);

/* Print an error message */
extern void plog(const char *str);

/* Exit, with optional message */
extern void quit(const char *str);


/* Sorting functions */
extern void sort(void *array, size_t nmemb, size_t smemb,
		 int (*comp)(const void *a, const void *b));

/* Mathematical functions */
int mean(int *nums, int size);
int variance(int *nums, int size);

#endif /* INCLUDED_Z_UTIL_H */
