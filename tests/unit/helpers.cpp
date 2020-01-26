#include <cstring>

#include <criterion/criterion.h>

#include "helpers.hpp"

char * cr_strdup(const char *str)
{
	char *ptr = (char *) cr_malloc(strlen(str) + 1);
	if (ptr)
		strcpy(ptr, str);
	return ptr;
}
