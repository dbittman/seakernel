#include <kernel.h>
#include <console.h>
int DEF_FG=15;
int DEF_BG=0;
int reset_terconal(vterm_t *con)
{
	return 0;
}

int un_save_c_and_a(vterm_t *con, int restore, int attr)
{
	return 0;
}

int tty_gotoxy(vterm_t *con, int x, int y)
{
	if(x >= 0) con->x = x * (con->fw + con->es);
	if(y >= 0) con->y = y * (con->fh + con->es);
	if(con->x > con->w) {
		con->x = con->y % con->w;
		con->y += con->x/con->w;
	}
	if(con->y > con->h-(con->fh+con->es))
	{
		con->y = con->h-(con->fh+con->es);
	}
	if(con->y < 0)
		con->y=0;
	if(con->x < 0)
		con->x=0;
	return 0;
}

int tty_movexy(vterm_t *con, int x, int y)
{
	con->x += x * (con->fw + con->es);
	con->y += y * (con->fh + con->es);
	if(con->x > con->w) {
		con->x = con->y % con->w;
		con->y += con->x/con->w;
	}
	if(con->y > con->h-(con->fh+con->es))
	{
		con->y = con->h-(con->fh+con->es);
	}
	if(con->y < 0)
		con->y=0;
	if(con->x < 0)
		con->x=0;
	return 0;
}

int scroll_display(vterm_t *con, int count)
{
	if(count < 0)
		while(con->rend.scroll_up && count++) con->rend.scroll_up(con);
	else
		while(con->rend.scroll && count--) con->rend.scroll(con);
	return 0;
}

/* 0: cur to EOL
 * 1: SOL to cur
 * 2: whole */
int tty_Kclear(vterm_t *con, int d)
{
	unsigned a = (unsigned)con->rend.scroll;
	con->rend.scroll=0;
	int t=0;
	con->no_wrap=1;
	if(d == 0){
		int x = con->x;
		int y = con->y;
		while(con->y==y && (t++ < con->w))
			con->rend.putch(con, ' ');
		con->x=x;
		con->y=y;
	}
	else if(d == 1) {
		int x = con->x;
		int y = con->y;
		volatile int *p = &con->x;
		con->x=0;
		while(*p < x && con->y==y && (t++ < con->w)) {
			con->rend.putch(con, ' ');
		}
		con->x=x;
		con->y=y;
	} else if(d == 2)
	{
		int x = con->x;
		int y = con->y;
		volatile int *p = &con->x;
		*p=0;
		while(con->y==y && (t++ < con->w))
			con->rend.putch(con, ' ');
		con->x=x;
		con->y=y;
	}
	con->no_wrap=0;
	con->rend.scroll = (void (*)(vterm_t *))a;
	return 0;
}

/* 0: cur to end
 * 1: start to cur
 * 2: All
 */
int tty_Jclear(vterm_t *con, int d)
{
	unsigned a = (unsigned)con->rend.scroll;
	int x = con->x;
	int y = con->y;
	if(d == 2 || (d == 0 && con->y==0))
		con->rend.clear(con);
	else if(d == 0) {
		con->rend.scroll=0;
		con->x=0;
		while(con->y < con->h)
			con->rend.putch(con, ' ');
		con->x=x;
		con->y=y;
		con->rend.scroll=(void*)a;
	} else if(d == 1) {
		con->rend.scroll=0;
		con->x=0;
		con->y=0;
		while(con->y <= y)
			con->rend.putch(con, ' ');
		con->x=x;
		con->y=y;
		con->rend.scroll=(void*)a;
	}
	return 0;
}

void csi_m(vterm_t *con, int a)
{
	if(a == 0) {
		con->f = 15;
		con->b=0;
	}
	if(a == 7) {
		int f = con->f;
		con->f = con->b;
		con->b = f;
	}
	if(a >= 30 && a <= 39)
	{
		if(a < 38)
			con->f = (a-30)+(a ? 8 : 0);
		else
			con->f = DEF_FG;
	}
	if(a >= 40 && a <= 49)
	{
		if(a < 48)
			con->b = a-40;
		else
			con->b = DEF_BG;
	}
}

int read_brak_esc(vterm_t *con, char *seq)
{
	char *info = seq + 2;
	if(*info == '?')
		info++;
	int i=0, d=0, t=0;
	int data[128];
	memset(data, 0, 128 * sizeof(int));
	char tmp[16];
	memset(tmp, 0, 16);
	int read=0;
	while(read++ < 16)
	{
		if(*(info + i) == ';' || !(*(info + i) >= '0' && *(info + i) <= '9'))
		{
			data[d++] = strtoint(tmp);
			memset(tmp, 0, 16);
			if(!(*(info + i) >= '0' && *(info + i) <= '9') && *(info + i) != ';') 
				break;
			if(!t) d--;
			t=0;
		} else
		{
			tmp[t++] = *(info + i);
		}
		i++;
	}
	if(!t) d--;
	char command = *(info + i);
	if(con->rend.clear_cursor)
		con->rend.clear_cursor(con);
	int a, b, len;
	//printk(0, ":: %c\n",command);
	switch(command)
	{
		case 'm':
			if(!d)
			{
				con->b = DEF_BG;
				con->f = DEF_FG;
			}
			while(d)
				csi_m(con, data[--d]);
			break;
		case 'r':
			printk(0, "[esc]: UNHANDLED: scroll: %d %d\n", data[0], data[1]);
			break;
		case 'A':if (!data[0]) data[0]++;
			tty_movexy(con, 0, 0-data[0]);
			break;
		case 'B':if (!data[0]) data[0]++;
			tty_movexy(con, 0, data[0]);
			break;
		case 'C':if (!data[0]) data[0]++;
			tty_movexy(con, data[0], 0);
			break;
		case 'D':if (!data[0]) data[0]++;
			tty_movexy(con, 0-data[0], 0);
			break;
		case 'E':if (!data[0]) data[0]++;
			tty_movexy(con, 0, data[0]);
			tty_gotoxy(con, 0, -1);
			break;
		case 'F':if (!data[0]) data[0]++;
			tty_movexy(con, 0, 0-data[0]);
			tty_gotoxy(con, 0, -1);
			break;
		case 'H': case 'f':
			if(data[0]) data[0]--;
			if(data[1]) data[1]--;
			tty_gotoxy(con, data[1], data[0]);
			break;
		case 'K':
			tty_Kclear(con, data[0]);
			break;
		case 'J':
			tty_Jclear(con, data[0]);
			break;
		case 'G': case '`': 
			if (data[0]) data[0]--;
			tty_gotoxy(con, data[0], -1);
			break;
		case 'M':
			a = con->scrollt;
			con->scrollt = con->y;
			scroll_display(con, -data[0]);
			con->scrollt=a;
			break;
		case 'P':
			if(!data[0]) data[0]++;
			len = con->w - con->x;
			memcpy(con->cur_mem + con->y*con->w*2 + con->x*2, 
					con->cur_mem + con->y*con->w*2 + con->x*2 + data[0]*2, len*2);
			break;
		case 'a':
			tty_movexy(con, data[0], 0);
			break;
		case 'd':
			if (data[0]) data[0]--;
			tty_gotoxy(con, -1, data[0]);
			break;
		case 's':
			con->ox = con->x;
			con->oy = con->y;
			break;
		case 'u':
			tty_gotoxy(con, con->ox, con->oy);
			break;
		default:
			break;
	}
	if(con->rend.update_cursor)
		con->rend.update_cursor(con);
	return 3 + i;
}

int read_par_esc(vterm_t *con, char *seq)
{
	printk(0, "[esc]: UNHANDLED: parethetical sequence\n");
	return 0;
}

int read_pure_esc(vterm_t *con, char *seq)
{
	//printk(0, "## %c\n", *(seq+1));
	switch(*(seq+1))
	{
		case 'c':
			reset_terconal(con);
			break;
		case 'M':
			scroll_display(con, -1);
			break;
		case '7':
			un_save_c_and_a(con, 0, 1);
			break;
		case '8':
			un_save_c_and_a(con, 1, 1);
			break;
		case 'A':
			tty_movexy(con, 0, -1);
			break;
		case 'B':
			tty_movexy(con, 0, 1);
			break;
		case 'C':
			tty_movexy(con, 1, 0);
			break;
		case 'D':
			tty_movexy(con, -1, 0);
			break;
		case 'H':
			tty_gotoxy(con, 0, 0);
			break;
		case 'K':
			tty_Kclear(con, 0);
			break;
		case 'J':
			tty_Jclear(con, 0);
			break;
	}
	return 2;
}

int read_escape_seq(vterm_t *con, char *seq)
{
	if(!seq || *seq != 27)
		return -1;
	if(*(seq+1) == '[') {
		int ret = read_brak_esc(con, seq);
		return ret;
	}
	else if(*(seq+1) == '(')
		return read_par_esc(con, seq);
	else
		return read_pure_esc(con, seq);
}
