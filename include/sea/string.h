#ifndef STRING_H
#define STRING_H
#include <sea/types.h>

void *memchr(const void *cs, int c, size_t count);
#define memcpy memmove
void *memmove(void *dest, const void *src, size_t n);
void *memset(void *s, int c, size_t count);
char *_strcat(char * dest, const char * src);
char *strchr(const char *s, int c);
int strcmp(const char *cs, const char *ct);
char *_strcpy(char * dest, const char * src);
size_t strlen(const char *s);
char *strncat(char * dest, const char * src, size_t count);
int strncmp(const char *cs, const char *ct, size_t count);
char *strncpy(char * dest, const char * src, size_t count);
size_t strnlen(const char *s, size_t count);
char *strrchr(const char *s, int c);
int strtoint(char *s);
int strtoint_oct(char *s);
int memcmp(const void * mem1, const void * mem2, const uint32_t count);

#endif
