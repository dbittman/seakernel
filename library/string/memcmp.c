#include <sea/string.h>

int memcmp(const void* m1,
           const void* m2,
           unsigned int n)
{
	unsigned char *s1 = (unsigned char *) m1;
	unsigned char *s2 = (unsigned char *) m2;
	while (n--)
	{
		if (*s1 != *s2)
		{
			return *s1 - *s2;
		}
		s1++;
		s2++;
	}
	return 0;
}
