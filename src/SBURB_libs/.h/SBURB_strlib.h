#ifndef SBURB_STRLIB_H
#define SBURB_STRLIB_H

#include <stddef.h>
#include <stdlib.h>

char *itoa(int value, char *str, int base);
char *concat(char *dest, const char *str1, const char *str2);
char *strcpy(char *dest, const char *src);
char *indexof(const char *str, char c);
char *substr(const char *str, size_t start, size_t len);

#endif // SBURB_STRLIB_H