/*print error messages when a subsystem is subscribed to the trace service*/

#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/trace.h>
#include <stdarg.h>
#include <sea/lib/hash.h>
#include <sea/string.h>
#include <stdbool.h>
#include <sea/fs/kerfs.h>

static struct hash h_table;

struct trace {
	char *name;
	struct hashelem hash_elem;
};

/*initialize trace in main.c*/
void trace_init()
{
	hash_create(&h_table, 0, 100);
}

/*print out optional arguments formatted by msg if subsys is subscribed*/
void trace(char *subsys, char *msg, ...)
{
	struct trace *trace;
	if((trace = hash_lookup(&h_table, subsys, strlen(subsys))) != NULL) {
		char printbuf[2065];
		int len = snprintf(printbuf, 64, "[%s]: ", subsys);
		va_list args;
		va_start(args, msg);
		vsnprintf(2000, printbuf+len-1, msg, args);
		printk(0, printbuf);
		va_end(args);
	}
}

/*subscribe subsys to tracing service*/
int trace_on(char *subsys)
{
	printk(0, "[trace]: enable %s\n", subsys);
	struct trace *trace = kmalloc(sizeof(struct trace));
	trace->name = kmalloc((strlen(subsys) + 1) * sizeof(char));
	strncpy(trace->name, subsys, strlen(subsys));
	return hash_insert(&h_table, trace->name, strlen(subsys), &trace->hash_elem, trace);
}

int trace_off(char *subsys)
{
	printk(0, "[trace]: disable %s\n", subsys);
	struct trace *trace;
	if((trace = hash_lookup(&h_table, subsys, strlen(subsys))) != NULL) {
		hash_delete(&h_table, subsys, strlen(subsys));
		kfree(trace->name);
		kfree(trace);
	}
	return 0;
}

