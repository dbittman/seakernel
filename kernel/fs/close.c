/* Closes an open file descriptor. If its a pipe, we shutdown the pipe too */
#include <sea/kernel.h>
#include <sea/mm/vmm.h>
#include <sea/tm/process.h>
#include <sea/fs/inode.h>
#include <sea/dm/dev.h>
#include <sea/sys/fcntl.h>
#include <sea/dm/block.h>
#include <sea/dm/char.h>
#include <sea/rwlock.h>
#include <sea/cpu/atomic.h>
#include <sea/fs/file.h>
#include <sea/dm/pipe.h>
int sys_close(int fp)
{
	struct file *f = fs_get_file_pointer((task_t *) current_task, fp);
	if(!f)
		return -EBADF;
	assert(f->inode && f->inode->f_count);
	if(f->inode->pipe)
	{
		/* okay, its a pipe. We need to do some special things
		 * to close a pipe, like decrement the various counts.
		 * If the counts reach zero, we free it */
		mutex_acquire(f->inode->pipe->lock);
		if(f->inode->pipe->count)
			sub_atomic(&f->inode->pipe->count, 1);
		if(f->flags & _FWRITE && f->inode->pipe->wrcount 
				&& f->inode->pipe->type != PIPE_NAMED)
			sub_atomic(&f->inode->pipe->wrcount, 1);
		if(!f->inode->pipe->count && f->inode->pipe->type != PIPE_NAMED) {
			assert(!f->inode->pipe->read_blocked->num);
			assert(!f->inode->pipe->write_blocked->num);
			dm_free_pipe(f->inode);
			f->inode->pipe = 0;
		} else {
			tm_remove_all_from_blocklist(f->inode->pipe->read_blocked);
			tm_remove_all_from_blocklist(f->inode->pipe->write_blocked);
			mutex_release(f->inode->pipe->lock);
		}
	}
	/* close devices */
	if(S_ISCHR(f->inode->mode) && !fp)
		dm_char_rw(CLOSE, f->inode->dev, 0, 0);
	else if(S_ISBLK(f->inode->mode) && !fp)
		dm_block_device_rw(CLOSE, f->inode->dev, 0, 0, 0);
	if(!sub_atomic(&f->inode->f_count, 1) && f->inode->marked_for_deletion)
		vfs_do_unlink(f->inode);
	else
		vfs_iput(f->inode);
	fs_fput((task_t *)current_task, fp, FPUT_CLOSE);
	return 0;
}
