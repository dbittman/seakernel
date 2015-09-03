/*print error messages when a subsystem is subscribed to the trace service*/

#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/trace.h>
#include <stdarg.h>
#include <sea/lib/hash-simple.h>
#include <sea/string.h>
#include <sea/tty/terminal.h>
#include <stdbool.h>
#include <sea/fs/kerfs.h>

static struct hash_table h_table;

/*initialize trace in main.c*/
int trace_init(){
	hash_table_create_default(&h_table, 100);
	return 1;
}

/*print out optional arguments formatted by msg if subsys is subscribed*/
int trace(char *subsys, char *msg, ...){
	bool value;
	bool *val=&value;
	if(!hash_table_get_entry(&h_table, subsys, 1, strlen(subsys)+1,(void **)&val)){
		if(value) {
			char printbuf[2065];
			int len = snprintf(printbuf, 64, "[%s]: ", subsys);
			va_list args;
			va_start(args, msg);
			vsnprintf(2000, printbuf+len-1, msg, args);
			printk(0, printbuf);
			va_end(args);
		}
	}
	return value;
}

/*subscribe subsys to tracing service*/
int trace_on(char *subsys)
{
	bool val=true;
	printk(0, "[trace]: enable %s\n", subsys);
	return hash_table_set_entry(&h_table, subsys, 1, strlen(subsys)+1, &val);
}

int trace_off(char *subsys)
{
	printk(0, "[trace]: disable %s\n", subsys);
	return hash_table_delete_entry(&h_table, subsys, 1, strlen(subsys)+1);
}

