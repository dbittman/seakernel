#include <sea/config.h>
#include <sea/libgen.h>
#include <sea/types.h>
#include <sea/string.h>

char *dirname(char *path)
{
	if(path == 0 || path[0] == 0)
		return ".";
	int length = strlen(path);
	while((length>0) && path[length-1] == '/') {
		if(length == 1)
			return path;
		path[(--length)] = 0;
	}

	while(length>0 && path[length-1] != '/') length--;

	if(length == 1)
		return "/";
	else if(length == 0)
		return ".";
	else
		path[length-1] = 0;
	return path;
}

