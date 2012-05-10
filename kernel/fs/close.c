/* Closes an open file descriptor. If its a pipe, we shutdown the pipe too */
#include <kernel.h>
#include <memory.h>
#include <task.h>
#include <fs.h>
#include <dev.h>
#include <sys/fcntl.h>

int sys_close(int fp)
{
	/* Make sure that we flush a mm file */
	check_mmf_and_flush((task_t *)current_task, fp);
	struct file *f = get_file_pointer((task_t *) current_task, fp);
	if(!f)
		return 0;
	mutex_on(&f->inode->lock);
	if(f->inode->pipe)
	{
		mutex_on(f->inode->pipe->lock);
		if(f->inode->pipe->count)
			f->inode->pipe->count--;
		if(f->flags & _FWRITE && f->inode->pipe->wrcount)
			f->inode->pipe->wrcount--;
		if(!f->inode->pipe->count)
			free_pipe(f->inode);
		else
			mutex_off(f->inode->pipe->lock);
	}
	if(S_ISCHR(f->inode->mode) && !fp)
		char_rw(CLOSE, f->inode->dev, 0, 0);
	if(f->inode->f_count > 0)
		f->inode->f_count--;
	sync_inode_tofs(f->inode);
	iput(f->inode);
	remove_file_pointer((task_t *)current_task, fp);
	return 0;
}
