#include <sea/config.h>
#include <sea/string.h>

int strtoint(char *s)
{
	int i;
	int dig = strlen(s);
	int mul=1;
	int total=0;
	char neg=0;
	if(*s == '-')
	{
		neg = 1;
		s++;
	}
	for(i=0;i<(dig-1);i++) mul *= 10;
	while(*s)
	{
		if(*s < 48 || *s > 57)
			return -1;
		total += ((*s)-48)*mul;
		mul = mul/10;
		s++;
	}
	if(neg) return (-total);
	return total;
}
