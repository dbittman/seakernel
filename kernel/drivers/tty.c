/* tty access - Copyright (c) 2012 Daniel Bittman
 * Provides basic access to terminals (read, write, ioctl)
 */

#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/dm/char.h>
#include <sea/tty/terminal.h>
#include <sea/asm/system.h>
#include <sea/tty/termios.h>
#include <sea/loader/symbol.h>
#include <sea/tm/tqueue.h>
#include <sea/cpu/atomic.h>

struct vterm consoles[MAX_CONSOLES];
static unsigned *tty_calltable = 0;
extern struct console_driver crtc_drv;
/* Create a terminal if needed, and set as current */
int tty_open(int min)
{
	if((unsigned)min >= MAX_CONSOLES)
		return -ENOENT;
	if(!consoles[min].flag && min) {
		console_create(&consoles[min]);
		/* copy the driver pointer from the kernel's console. This
		 * will probably need to be changed later */
		console_initialize_vterm(&consoles[min], consoles[0].driver);
	}
	current_task->tty = min;
	return 0;
}

int tty_close(int min)
{
	return 0;
}

void tty_putch(struct vterm *con, int ch)
{
	if(!con->rend->putch)
		return;
	
	if(con->term.c_oflag & OPOST) {
		if(((con->term.c_oflag & ONLCR) || con->tty==0) && ch == '\n' 
				&& !(con->term.c_oflag & ONOCR)) {
			con->rend->putch(con, '\r');
		}
		if(((con->term.c_oflag & OCRNL) && con->tty) && ch == '\r')
			ch = '\n';
		if(((con->term.c_oflag & ONOCR) && ch == '\r') || (con->x==0 && ch == '\r'))
			return;
	}
	con->rend->putch(con, ch);
}

void __tty_found_task_raise_action(task_t *t, int arg)
{
	if(t->flags & TF_BGROUND) return;
	t->sigd = arg;
	tm_process_raise_flag(t, TF_SCHED);
	if(t->blocklist)
		tm_remove_from_blocklist(t->blocklist, t);
}

int tty_raise_action(int min, int sig)
{
	if(!(consoles[min].term.c_lflag & ISIG))
		return 0;
	if((kernel_state_flags & KSF_SHUTDOWN))
		return 0;
	if(tm_search_tqueue(primary_queue, TSEARCH_FINDALL | TSEARCH_TTY, min, __tty_found_task_raise_action, sig, 0))
		consoles[min].inpos=0;
	return 0;
}

int tty_read(int min, char *buf, size_t len)
{
	if((unsigned)min > MAX_CONSOLES)
		return -ENOENT;
	if(!buf) return -EINVAL;
	struct vterm *con=0;
	again:
	con = &consoles[min];
	if(!con->flag)
		return -ENOENT;
	volatile int rem=len;
	volatile char t=0;
	volatile size_t count=0;
	volatile int cb = !(con->term.c_lflag & ICANON);
	int x = con->x;
	while(1) {
		if(con->rend->update_cursor)
			con->rend->update_cursor(con);
		mutex_acquire(&con->inlock);
		while(!con->inpos) {
			mutex_release(&con->inlock);
			tm_add_to_blocklist_and_block(&con->input_block, (task_t *)current_task);
			if(tm_process_got_signal(current_task))
				return -EINTR;
			mutex_acquire(&con->inlock);
		}
		t=con->input[0];
		con->inpos--;
		if(con->inpos)
			memmove(con->input, con->input+1, con->inpos+1);
		mutex_release(&con->inlock);
		if(con->rend->update_cursor)
			con->rend->update_cursor(con);
		if(t == '\b' && !cb && count)
		{
			buf[--count]=0;
			rem++;
			if(con->x > x) {
				tty_putch(con, '\b');
				tty_putch(con, ' ');
				tty_putch(con, '\b');
			}
		} else if(t == 4 && !count && !cb)
			 return 0;
		else if(rem && (t != '\b' || cb)) {
			if(count < len) {
				buf[count++] = t;
				rem--;
			}
		}
		if(t == '\n' || t == '\r' || (cb && !rem))
			return len-rem;
	}
	return len-rem;
}

/* Write to screen */
int tty_write(int min, char *buf, size_t len)
{
	if((unsigned)min > MAX_CONSOLES)
		return -ENOENT;
	if(!buf)
		return -EINVAL;
	struct vterm *con = &consoles[min];
	if(!con->flag)
		return -ENOENT;
	size_t i=0;
	mutex_acquire(&con->wlock);
	/* putch handles printable characters and control characters. 
	 * We handle escape codes */
	while(i<len) {
		if(tm_process_got_signal(current_task)) {
			mutex_release(&con->wlock);
			return -EINTR;
		}
		if(*buf){
			if(*buf == 27)
			{
				/* Escape! */
				if(con->rend->clear_cursor)
					con->rend->clear_cursor(con);
				int l = tty_read_escape_seq(con, buf);
				if(l == -1)
					goto out;
				else if(l == 0)
					l++;
				i += l;
				buf += l;
				continue;
			}
			tty_putch(con, *buf);
		}
		buf++;
		i++;
	}
	out:
	if(con->rend->update_cursor)
		con->rend->update_cursor(con);
	mutex_release(&con->wlock);
	return len;
}

int ttyx_ioctl(int min, int cmd, long arg)
{
	if((unsigned)min >= MAX_CONSOLES)
		return -ENOENT;
	struct vterm *con = &consoles[min];
	if(!con->flag)
		return -ENOENT;
	task_t *t=0;
	struct sgttyb *g=0;
	struct winsize *s;
	switch(cmd)
	{
		case 0:
			if(con->rend->clear) con->rend->clear(con);
			break;
		case 1:
			con->f = arg % 16;
			con->b = arg/16;
			break;
		case 2:
			current_task->tty = min;
			break;
		case 3:
			if(con->rend->clear_cursor)
				con->rend->clear_cursor(con);
			con->x = arg;
			if(con->rend->update_cursor)
				con->rend->update_cursor(con);
			break;
		case 4:
			if(con->rend->clear_cursor)
				con->rend->clear_cursor(con);
			con->y = arg;
			if(con->rend->update_cursor)
				con->rend->update_cursor(con);
			break;
		case 5:
			return con->inpos;
		case 6:
			if(arg) *(int *)arg = con->x;
			return con->x;
		case 7:
			if(arg) *(int *)arg = con->y;
			return con->y;
		case 8:
			if(arg && arg != 1) {
				if(!mm_is_valid_user_pointer(SYS_IOCTL, (void *)arg, 0))
					 return -EINVAL;
				*(int *)arg = min;
			}
			if(arg == 1)
			{
				char tmp[3];
				sprintf(tmp, "%d", min);
				tty_write(min, tmp, strlen(tmp));
			}
			return min;
		case 9:
			break;
		case 10:
			if(arg) {
				if(!mm_is_valid_user_pointer(SYS_IOCTL, (void *)arg, 0))
					 return -EINVAL;
				*(int *)arg = con->fw+con->es;
			}
			return con->fh+con->es;
		case 11:
			if(arg) {
				if(!mm_is_valid_user_pointer(SYS_IOCTL, (void *)arg, 0))
					 return -EINVAL;
				*(int *)arg = con->w/(con->fw+con->es);
			}
			return con->h/(con->fh+con->es);
		case 14:
			con->inpos=0;
			break;
		case 17:
			if(con->term.c_iflag & ICRNL && arg == '\r')
				arg = '\n';
			if(con->term.c_iflag & IGNCR && arg == '\r')
				break;
			mutex_acquire(&con->inlock);
			if(con->inpos < 256) {
				con->input[con->inpos] = (char)arg;
				con->inpos++;
			}
			tm_remove_all_from_blocklist(&con->input_block);
			mutex_release(&con->inlock);
			if(!(con->term.c_lflag & ECHO) && arg != '\b' && arg != '\n')
				break;
			if(!(con->term.c_lflag & ECHOE) && arg == '\b')
				break;
			if(!(con->term.c_lflag & ECHONL) && arg == '\n')
				break;
			mutex_acquire(&con->wlock);
			tty_putch(con, arg);
			if(con->rend->update_cursor)
				con->rend->update_cursor(con);
			mutex_release(&con->wlock);
			break;
		case 18:
			return con->term.c_oflag;
		case 19:
			tty_raise_action(min, arg);
			break;
		case 20:
			if(!arg)
				con->term.c_lflag &= ~ECHO;
			else
				con->term.c_lflag |= ECHO;
			break;
		case 21:
			if(arg)
				*(unsigned *)arg = con->scrollb;
			return con->scrollt;
		case 22:
			console_create(con);
			break;
		case 23:
			if(!arg)
				con->term.c_oflag &= ~OCRNL;
			else
				con->term.c_oflag |= OCRNL;
			break;
		case 27:
			console_switch(con);
			break;
		case 0x5413:
			s = (struct winsize *)arg;
			if(!s)
				return -EINVAL;
			if(!mm_is_valid_user_pointer(SYS_IOCTL, (void *)arg, 0))
					 return -EINVAL;
			s->ws_row=con->h-1;
			s->ws_col=con->w;
			return 0;
		case 0x5402: case 0x5403: case 0x5404:
			if(arg) 
				memcpy(&(con->term), (void *)(addr_t)arg, sizeof(struct termios));
			return 0;
		case 0x5401:
			if(!mm_is_valid_user_pointer(SYS_IOCTL, (void *)arg, 0))
				return -EINVAL;
			if(arg)
				memcpy((void *)(addr_t)arg, &(con->term), sizeof(struct termios));
			return 0;
		case 0x540F:
			return 0;
			break;
		case 0x5414:
			/* set winsz */
			return 0;
			break;
		default:
			if(cmd >= 128 && cmd < 256 && ((current_task->system == SYS_LMOD)
					|| (current_task->pid < init_pid)))
			{
				addr_t q = tty_calltable ? tty_calltable[cmd-128] : 0;
				if(q)
				{
					int (*call)(int,int,int) = (int (*)(int,int,int))q;
					return call(min, cmd, arg);
				} else {
					if(!tty_calltable)
						tty_calltable = (unsigned *)kmalloc(128 * sizeof(unsigned));
					tty_calltable[cmd-128] = arg;
					return cmd;
				}
			} else
				printk(1, "[tty]: %d: invalid ioctl %d\n", min, cmd);
	}
	return 0;
}

int tty_ioctl(int min, int cmd, long arg)
{
	return ttyx_ioctl(current_task->tty, cmd, arg);
}

int ttyx_rw(int rw, int min, char *buf, size_t count)
{
	switch(rw) {
		case OPEN:
			return tty_open(min);
			break;
		case CLOSE:
			return tty_close(min);
			break;
		case READ:
			return tty_read(min, buf, count);
			break;
		case WRITE:
			return tty_write(min, buf, count);
			break;
	}
	return -EINVAL;
}

int tty_rw(int rw, int m, char *buf, size_t c)
{
 	if(!current_task) 
		return -ESRCH;
	return ttyx_rw(rw, m=current_task->tty, buf, c);
}

int ttyx_select(int min, int rw)
{
	if(!consoles[min].flag) return 1;
	if(rw == READ && !consoles[min].inpos)
		return 0;
	return 1;
}

int tty_select(int min, int rw)
{
	return ttyx_select(min=current_task->tty, rw);
}

void tty_init(struct vterm **k)
{
	int i;
	assert(MAX_CONSOLES > 9);
	for(i=0;i<MAX_CONSOLES;i++)
	{
		memset(&consoles[i], 0, sizeof(struct vterm));
		consoles[i].tty=i;
	}
	*k = &consoles[0];
}

void console_init_stage2()
{
	console_create(&consoles[1]);
	console_create(&consoles[9]);
	console_initialize_vterm(&consoles[1], consoles[0].driver);
	console_initialize_vterm(&consoles[9], consoles[0].driver);
	memcpy(consoles[1].vmem, consoles[0].cur_mem, 80*25*2);
	consoles[1].x=consoles[0].x;
	consoles[1].y=consoles[0].y;
	console_switch(&consoles[1]);
	log_console = &consoles[9];
#if CONFIG_MODULES
	loader_add_kernel_symbol(ttyx_ioctl);
	loader_add_kernel_symbol(console_initialize_vterm);
	loader_add_kernel_symbol(console_create);
	loader_add_kernel_symbol(console_destroy);
	loader_add_kernel_symbol(console_switch);
	loader_do_add_kernel_symbol((addr_t)(unsigned *)&current_console, "current_console");
#endif
}
