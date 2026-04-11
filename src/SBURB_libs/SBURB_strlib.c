#include ".h/SBURB_strlib.h"
char *itoa(int value, char *str, int base)
{
    char *ptr = str, *ptr1 = str, tmp_char;
    int tmp_value;

    if (base < 2 || base > 36)
    {
        *str = '\0';
        return str;
    }

    do
    {
        tmp_value = value;
        value /= base;
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[tmp_value - value * base];
    } while (value);

    // Apply negative sign for base 10
    if (tmp_value < 0 && base == 10)
        *ptr++ = '-';

    *ptr-- = '\0';

    // Reverse the string
    while (ptr1 < ptr)
    {
        tmp_char = *ptr;
        *ptr-- = *ptr1;
        *ptr1++ = tmp_char;
    }
    return str;
}

char *concat(char *dest, const char *str1, const char *str2)
{
    char *ptr = dest;

    while (*str1)
        *ptr++ = *str1++;

    while (*str2)
        *ptr++ = *str2++;

    *ptr = '\0';
    return dest;
}

char *strcpy(char *dest, const char *src)
{
    char *ptr = dest;
    while (*src)
        *ptr++ = *src++;
    *ptr = '\0';
    return dest;
}

char *indexof(const char *str, char c)
{
    while (*str)
    {
        if (*str == c)
            return (char *)str;
        str++;
    }
    return NULL;
}
char *substr(const char *str, size_t start, size_t length)
{
    char *result = malloc(length + 1);
    if (!result)
        return NULL;

    for (size_t i = 0; i < length; i++)
    {
        result[i] = str[start + i];
    }
    result[length] = '\0';
    return result;
}