#ifndef KERNEL_LIB_STRING_H
#define KERNEL_LIB_STRING_H

#include <kernel/types.h>

void  *k_memcpy(void *dst, const void *src, size_t n);
void  *k_memset(void *dst, int val, size_t n);
void  *k_memmove(void *dst, const void *src, size_t n);
size_t k_strlen(const char *s);
int    k_strcmp(const char *a, const char *b);
int    k_strncmp(const char *a, const char *b, size_t n);
char  *k_strcpy(char *dst, const char *src);
char  *k_strncpy(char *dst, const char *src, size_t n);

#endif
