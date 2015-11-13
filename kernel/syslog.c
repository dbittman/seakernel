#include <sea/fs/kerfs.h>
#include <sea/lib/charbuffer.h>
#include <sea/lib/hash.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <sea/spinlock.h>
#include <sea/syslog.h>
#include <sea/errno.h>
static struct hash loggers;
static struct charbuffer logbuffer;
struct spinlock loglock;

static int kerfs_syslog(int direction, void *param,
		size_t size, size_t offset, size_t length, char *buf);

void syslog_init(void)
{
	hash_create(&loggers, 0, 64);
	spinlock_create(&loglock);
	charbuffer_create(&logbuffer, CHARBUFFER_LOCKLESS | CHARBUFFER_DROP, 4096);
	kerfs_register_report("/dev/syslog", kerfs_syslog);
}

static int kerfs_syslog(int direction, void *param,
		size_t size, size_t offset, size_t length, char *buf)
{
	if(direction != READ)
		return -EIO;
	return charbuffer_read(&logbuffer, buf, length);
}

static void __syslog(int level, char *buffer, size_t length)
{
	char *ident = "unknown";
	int facility = LOG_USER;
	struct syslogproc *log = hash_lookup(&loggers, &current_process->pid, sizeof(pid_t));
	if(log) {
		ident = log->ident;
		facility = log->facility;
	}
	char header[128];
	snprintf(header, 128, "<%d:%s>", LOG_MAKEPRI(facility, level), ident);
	bool need_newline = false;
	if(buffer[strlen(buffer)-1] != '\n')
		need_newline = true;
	spinlock_acquire(&loglock);
	charbuffer_write(&logbuffer, (unsigned char *)header, strlen(header));
	charbuffer_write(&logbuffer, (unsigned char *)buffer, length);
	if(need_newline)
		charbuffer_write(&logbuffer, &"\n", 1);
	spinlock_release(&loglock);
	if(need_newline)
		printk(0, "%s: %s\n", header, buffer);
	else
		printk(0, "%s: %s", header, buffer);
}

static void __openlog(char *ident, int option, int facility)
{
	struct syslogproc *log = kmalloc(sizeof(struct syslogproc));
	log->facility = facility;
	log->options = option;
	log->pid = current_process->pid;
	strncpy(log->ident, ident, SYSLOG_IDENT_MAX-1);
	hash_insert(&loggers, &log->pid, sizeof(log->pid), &log->elem, log);
}

static void __closelog(void)
{
	struct syslogproc *log = hash_lookup(&loggers, &current_process->pid, sizeof(pid_t));
	if(log) {
		hash_delete(&loggers, &current_process->pid, sizeof(pid_t));
		kfree(log);
	}
}

int sys_syslog(int level, char *buf, int len, int ctl)
{
	switch(ctl) {
		case 0:
			__syslog(level, buf, len);
			break;
		case 1:
			__openlog(buf, level, len);
			break;
		case 2:
			__closelog();
			break;
		default:
			return -EINVAL;
	}
	return 0;
}

