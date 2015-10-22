#ifndef __SEA_CPU_TRACE_H
#define __SEA_CPU_TRACE_H

#if CONFIG_TRACE
#define TRACE_MSG(x,y,...)	(trace(x,y, ##__VA_ARGS__))
#else
#define TRACE_MSG(x,y,...)
#endif

void trace(char *, char *, ...);

int trace_on(char *);
int trace_off(char *);
void trace_init();

#endif

