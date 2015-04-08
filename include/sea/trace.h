#ifndef __SEA_CPU_TRACE_H
#define __SEA_CPU_TRACE_H

#if CONFIG_TRACE
#define TRACE_MSG(x,y,...)	(trace(x,y, __VA_ARGS__))
#else
#define TRACE_MSG(x,y,...)
#endif

/*returns 1 if subsys was subscribed, 0 otherwise. Print optional arguments formatted by msg if subsys is subscribed to tracing service*/

int trace(char *, char *, ...);

/*return 1 if subscribe successful, 0 otherwise.*/
int trace_on(char *);

/*return 1 if subsystem is successfully unsubscribed, 0 otherwise*/
int trace_off(char *);

/*return 1 if initialization of tracing is successful, 0 otherwise. Initializes hash table containing entries for subscribed subsystems.*/
int trace_init();

#endif

