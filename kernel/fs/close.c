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
#include <sea/errno.h>
#include <sea/fs/socket.h>
int sys_close(int fp)
{
	struct file *f = fs_get_file_pointer((task_t *) current_task, fp);
	if(!f)
		return -EBADF;
	assert(f->inode);
	/* handle sockets calling close. We just translate it to a call to shutdown.
	 * be aware that shutdown does end up calling close! */
	if(f->socket) {
		fs_fput(current_task, fp, 0);
		sys_sockshutdown(fp, SHUT_RDWR);
		return 0;
	}
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
		dm_char_rw(CLOSE, f->inode->phys_dev, 0, 0);
	else if(S_ISBLK(f->inode->mode) && !fp)
		dm_block_device_rw(CLOSE, f->inode->phys_dev, 0, 0, 0);
	if(f->dirent)
		vfs_dirent_release(f->dirent);
	vfs_icache_put(f->inode);
	fs_fput((task_t *)current_task, fp, FPUT_CLOSE);
	return 0;
}

