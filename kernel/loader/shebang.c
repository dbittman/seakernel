#include <sea/types.h>
#include <sea/fs/inode.h>
#include <sea/loader/exec.h>
#include <sea/string.h>
static void __load_first_line(struct file *file, unsigned char *buf, int len)
{
	int i;
	for(i=0;i<len;i++) {
		if(fs_file_pread(file, i, &buf[i], 1) != 1 || buf[i] == '\n')
			break;
	}
	buf[i]=0;
}

static char *chomp(char *buf)
{
	if(!buf)
		return 0;
	while(*buf == ' ')
		buf++;
	int end = strlen(buf)-1;
	if(*buf == 0)
		return 0;
	while(buf[end] == ' ')
		end--;
	buf[end+1] = 0;
	return buf;
}

static int parse_line_args(char *args, char **list)
{
	if(!args)
		return 0;
	char search = ' ';
	int count = 0;
	char *last = args;
	int okay=0;
	while(*args && count < 128) {
		if(*args == '"') {
			if(search == ' ')
				search = '"';
			else
				search = ' ';
		} else if(*args == search) {
			*args = 0;
			okay = 0;
			list[count++] = last;
			last = args+1;
		} else {
			okay = 1;
		}
		args++;
	}
	if(okay)
		list[count++] = last;
	return count;
}

/* takes a valid file descriptor, and will close it before
 * the function returns (if it does)
 */
int loader_do_shebang(struct file *file, char **argv, char **env)
{
	unsigned char buf[1024];
	__load_first_line(file, buf, 1024);
	file_put(file);
	char *interp = (char *)buf+2; /* skip the #! */

	interp = chomp(interp);
	char *interp_args = strchr(interp, ' ');
	if(interp_args)
		*(interp_args++) = 0;
	interp_args = chomp(interp_args);

	/* count args */
	int argc=0;
	while(argv[argc])
		argc++;

	char *list[128];
	int intargc = parse_line_args(interp_args, list);

	int i=0;
	char *allargs[intargc + argc + 2 /* one for null, one for command */];
	allargs[0] = interp;
	for(i=0;i<intargc + argc;i++)
		allargs[i+1] = (i < intargc) ? chomp(list[i]) : chomp(argv[i - intargc]);
	allargs[i+1] = 0;

	return do_exec(interp, allargs, env, 1);
}

