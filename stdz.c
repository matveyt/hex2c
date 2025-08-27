#include "stdz.h"
#include <errno.h>

/*noreturn*/
void z_die(const char* where)
{
    if (errno)
        perror(where);
    else
        fprintf(stderr, "%s: Unspecified error\n", where);
    exit(EXIT_FAILURE);
}

FILE* z_fopen(const char* fname, const char* mode)
{
    FILE* f = fopen(fname, mode);
    if (f == NULL)
        z_die("fopen");
    return f;
}

void* z_malloc(size_t size)
{
    void* ptr = malloc(size);
    if (ptr == NULL)
        z_die("malloc");
    return ptr;
}

void* z_realloc(void* ptr, size_t size)
{
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    ptr = realloc(ptr, size);
    if (ptr == NULL)
        z_die("realloc");
    return ptr;
}

char* z_strdup(const char* str)
{
    return strcpy(z_malloc(strlen(str) + 1), str);
}
