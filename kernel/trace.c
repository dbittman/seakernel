/*print error messages when a subsystem is subscribed to the trace service*/

#include <sea/vsprintf.h>
#include <sea/string.h>
#include <sea/trace.h>
#include <stdarg.h>
#include <sea/lib/hash-simple.h>
#include <sea/string.h>
#include <sea/tty/terminal.h>

static struct hash_table h_table;

/*initialize trace in main.c*/
int trace_init(){
	hash_table_create_default(&h_table, 100);
	return 1;
}

/*print out optional arguments formatted by msg if subsys is subscribed*/
int trace(char *subsys, char *msg, ...){
	int value=0;
	int *val=&value;
	if(!hash_table_get_entry(&h_table, subsys, 1, strlen(subsys)+1,(void **)&val)){
		char printbuf[2024];
		va_list args;
		va_start(args, msg);
		int len=strlen(subsys)+1;
		printbuf[0]='[';
		strncpy(printbuf+1, subsys, len);
		printbuf[len+1]=']';
		printbuf[len+2]=':';
		vsnprintf(2024, printbuf+len+3, msg, args);
		printk(0, printbuf);
		va_end(args);
	}
	return value;
}

/*subscribe subsys to tracing service*/
int trace_on(char *subsys){	
	int val=1;
	return hash_table_set_entry(&h_table, subsys, 1, strlen(subsys)+1, &val);
}

int trace_off(char *subsys){	
	return hash_table_delete_entry(&h_table, subsys, 1, strlen(subsys)+1);
}
