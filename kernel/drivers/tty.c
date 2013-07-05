/* tty access - Copyright (c) 2012 Daniel Bittman
 * Provides basic access to terminals (read, write, ioctl)
 */

#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <char.h>
#include <console.h>
#include <asm/system.h>
#include <termios.h>
#include <mod.h>
extern unsigned init_pid;
extern void update_cursor(int);
vterm_t consoles[MAX_CONSOLES];
unsigned *tty_calltable = 0;
extern console_driver_t crtc_drv;
/* Create a terminal if needed, and set as current */
int tty_open(int min)
{
	if((unsigned)min >= MAX_CONSOLES)
		return -ENOENT;
	if(!consoles[min].flag && min) {
		create_console(&consoles[min]);
		init_console(&consoles[min], &crtc_drv);
	}
	current_task->tty = min;
	return 0;
}

int tty_close(int min)
{
	return 0;
}

void tty_putch(vterm_t *con, int ch)
{
	if(!con->rend.putch)
		return;
	if(con->term.c_oflag & OPOST) {
		if((con->term.c_oflag & ONLCR || con->tty==0) && ch == '\n' 
				&& !(con->term.c_oflag & ONOCR))
			con->rend.putch(con, '\r');
		if((con->term.c_oflag & OCRNL && con->tty) && ch == '\r')
			ch = '\n';
		if(con->term.c_oflag & ONOCR && ch == '\r' && con->x==0)
			return;
	}
	con->rend.putch(con, ch);
}

void __tty_found_task_raise_action(task_t *t, int arg)
{
	t->sigd = arg;
	t->flags |= TF_SCHED;
	if(t->blocklist) task_unblock(t->blocklist, t);
}

int tty_raise_action(int min, int sig)
{
	if(!(consoles[min].term.c_lflag & ISIG))
		return 0;
	if((kernel_state_flags & KSF_SHUTDOWN))
		return 0;
	if(search_tqueue(primary_queue, TSEARCH_FINDALL | TSEARCH_TTY, min, __tty_found_task_raise_action, sig, 0))
		consoles[min].inpos=0;
	return 0;
}

int tty_read(int min, char *buf, size_t len)
{
	if((unsigned)min > MAX_CONSOLES)
		return -ENOENT;
	if(!buf) return -EINVAL;
	vterm_t *con=0;
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
		if(con->rend.update_cursor)
			con->rend.update_cursor(con);
		mutex_acquire(&con->inlock);
		while(!con->inpos) {
			mutex_release(&con->inlock);
			task_block(&con->input_block, (task_t *)current_task);
			if(got_signal(current_task))
				return -EINTR;
			mutex_acquire(&con->inlock);
		}
		t=con->input[0];
		con->inpos--;
		if(con->inpos)
			memmove(con->input, con->input+1, con->inpos+1);
		mutex_release(&con->inlock);
		if(con->rend.update_cursor)
			con->rend.update_cursor(con);
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
	vterm_t *con = &consoles[min];
	if(!con->flag)
		return -ENOENT;
	size_t i=0;
	mutex_acquire(&con->wlock);
	/* putch handles printable characters and control characters. 
	 * We handle escape codes */
	while(i<len) {
		if(got_signal(current_task)) {
			mutex_release(&con->wlock);
			return -EINTR;
		}
		if(*buf){
			if(*buf == 27)
			{
				/* Escape! */
				if(con->rend.clear_cursor)
					con->rend.clear_cursor(con);
				int l = read_escape_seq(con, buf);
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
	if(con->rend.update_cursor)
		con->rend.update_cursor(con);
	mutex_release(&con->wlock);
	return len;
}

int ttyx_ioctl(int min, int cmd, int arg)
{
	if((unsigned)min >= MAX_CONSOLES)
		return -ENOENT;
	vterm_t *con = &consoles[min];
	if(!con->flag)
		return -ENOENT;
	task_t *t=0;
	struct sgttyb *g=0;
	struct winsize *s;
	switch(cmd)
	{
		case 0:
			if(con->rend.clear) con->rend.clear(con);
			break;
		case 1:
			con->f = arg % 16;
			con->b = arg/16;
			break;
		case 2:
			current_task->tty = min;
			break;
		case 3:
			if(con->rend.clear_cursor)
				con->rend.clear_cursor(con);
			con->x = arg;
			if(con->rend.update_cursor)
				con->rend.update_cursor(con);
			break;
		case 4:
			if(con->rend.clear_cursor)
				con->rend.clear_cursor(con);
			con->y = arg;
			if(con->rend.update_cursor)
				con->rend.update_cursor(con);
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
			if(arg && arg != 1)
				*(int *)arg = min;
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
			if(arg)
				*(int *)arg = con->fw+con->es;
			return con->fh+con->es;
		case 11:
			if(arg)
				*(int *)arg = con->w/(con->fw+con->es);
			return con->h/(con->fh+con->es);
		case 12:
			break;
		case 13:
			break;
		case 14:
			con->inpos=0;
			break;
		case 15:
			return con->mode;
			break;
		case 16:
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
			task_unblock_all(&con->input_block);
			mutex_release(&con->inlock);
			if(!(con->term.c_lflag & ECHO) && arg != '\b' && arg != '\n')
				break;
			if(!(con->term.c_lflag & ECHOE) && arg == '\b')
				break;
			if(!(con->term.c_lflag & ECHONL) && arg == '\n')
				break;
			
			tty_putch(con, arg);
			if(con->rend.update_cursor)
				con->rend.update_cursor(con);
			
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
			create_console(con);
			break;
		case 23:
			if(!arg)
				con->term.c_oflag &= ~OCRNL;
			else
				con->term.c_oflag |= OCRNL;
			break;
		case 24:
			//if(!arg)
			//	con->term.c_oflag &= ~CBREAK;
			//else
			//	con->term.c_oflag |= CBREAK;
			break;
		case 25:
			break;
		case 26:
			if(con->rend.clear_cursor && arg)
				con->rend.clear_cursor(con);
			con->nocur=arg;
			break;
		case 27:
			switch_console(con);
			break;
		case 0x5413:
			s = (struct winsize *)arg;
			if(!s)
				return -EINVAL;
			s->ws_row=con->h-1;
			s->ws_col=con->w;
			return 0;
		case 0x5402: case 0x5403: case 0x5404:
			if(arg) 
				memcpy(&(con->term), (void *)(unsigned)arg, sizeof(struct termios));
			return 0;
		case 0x5401:
			if(arg) 
				memcpy((void *)(unsigned)arg, &(con->term), sizeof(struct termios));
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
				unsigned q = tty_calltable ? tty_calltable[cmd-128] : 0;
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

int tty_ioctl(int min, int cmd, int arg)
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

void tty_init(vterm_t **k)
{
	int i;
	assert(MAX_CONSOLES > 9);
	for(i=0;i<MAX_CONSOLES;i++)
	{
		printk(0, "%d\n", 7);
		//memset(&consoles[i], 0, sizeof(vterm_t));
		consoles[i].tty=i;
	}
	for(;;);
	*k = &consoles[0];
}

void console_init_stage2()
{
	create_console(&consoles[1]);
	create_console(&consoles[9]);
	init_console(&consoles[1], &crtc_drv);
	init_console(&consoles[9], &crtc_drv);
	memcpy(consoles[1].vmem, consoles[0].cur_mem, 80*25*2);
	consoles[1].x=consoles[0].x;
	consoles[1].y=consoles[0].y;
	switch_console(&consoles[1]);
	log_console = &consoles[9];
#if CONFIG_MODULES
	add_kernel_symbol(ttyx_ioctl);
	add_kernel_symbol(init_console);
	add_kernel_symbol(create_console);
	add_kernel_symbol(destroy_console);
	add_kernel_symbol(switch_console);
	_add_kernel_symbol((unsigned)(unsigned *)&curcons, "curcons");
#endif
}
