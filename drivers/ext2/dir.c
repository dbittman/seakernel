/*
 * Copyright (c) 2008 The tyndur Project. All rights reserved.
 *
 * This code is derived from software contributed to the tyndur Project
 * by Kevin Wolf.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *     This product includes software developed by the tyndur Project
 *     and its contributors.
 * 4. Neither the name of the tyndur Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sea/string.h>
#include <modules/ext2.h>
#include <sea/fs/inode.h>

#define DET_UNKNOWN 0
#define DET_REG 1
#define DET_DIR 2
#define DET_CHAR 3
#define DET_BLOCK 4
#define DET_FIFO 5
#define DET_SOCK 6
#define DET_SLINK 7

static int __map_type[8] = {
	[DET_UNKNOWN] = DT_UNKNOWN,
	[DET_REG] = DT_REG,
	[DET_DIR] = DT_DIR,
	[DET_CHAR] = DT_CHR,
	[DET_BLOCK] = DT_BLK,
	[DET_FIFO] = DT_FIFO,
	[DET_SOCK] = DT_SOCK,
	[DET_SLINK] = DT_LNK,
};

int ext2_dir_getdents(ext2_inode_t* inode, unsigned start, struct dirent_posix *dirs, unsigned count, unsigned *nextoff)
{
	ext2_dirent_t* entry;
	int bs = ext2_sb_blocksize(inode->fs->sb);
	unsigned char buf[bs];
	uint32_t tpos = start;
	uint32_t pos = start % bs;
	
	unsigned read = 0;

	ext2_inode_readdata(inode, tpos & ~(bs-1), ext2_sb_blocksize(inode->fs->sb), buf);
	while (tpos < inode->size && read < count) {
		if(pos >= ext2_sb_blocksize(inode->fs->sb))
		{
			pos = 0;
			ext2_inode_readdata(inode, tpos & ~(bs-1), ext2_sb_blocksize(inode->fs->sb), buf);
		}
		entry = (ext2_dirent_t*)(buf + pos);
		pos += entry->record_len;
		tpos += entry->record_len;
		if (entry->inode == 0)
			continue;

		//int reclen = ((entry->record_len + 1) & (~15)) + 16;
		int report_reclen = sizeof(struct dirent_posix) + entry->name_len + 1;
		report_reclen = (report_reclen & ~(15)) + 16;
		struct dirent_posix *rec = (void *)((addr_t)dirs + read);
		if(read + report_reclen > count)
			break;
		read += report_reclen;
		rec->d_off = tpos;
		*nextoff = tpos;
		rec->d_reclen = report_reclen;
		rec->d_type = __map_type[entry->type];
		rec->d_ino = entry->inode;
		memcpy(rec->d_name, entry->name, entry->name_len);
		rec->d_name[entry->name_len]=0;
		
		if (entry->record_len == 0)
			break;
	}
	return read;
}

int ext2_dir_get_inode(ext2_inode_t* inode, const char *name, int namelen)
{
	ext2_dirent_t* entry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = ext2_sb_blocksize(inode->fs->sb), tpos=0, off=0;
	unsigned len = namelen;
	while (tpos < inode->size) {
		if(pos >= ext2_sb_blocksize(inode->fs->sb))
		{
			pos = 0;
			ext2_inode_readdata(inode, off, ext2_sb_blocksize(inode->fs->sb), buf);
			off += ext2_sb_blocksize(inode->fs->sb);
		}
		entry = (ext2_dirent_t*)(buf + pos);
		pos += entry->record_len;
		tpos += entry->record_len;
		if (entry->record_len < 8)
			break;
		if (entry->inode == 0)
			continue;
		if (!strncmp((const char *)name, (const char *)entry->name, len) 
				&& len == entry->name_len) {
			return entry->inode;
		}
	}
	return 0;
}

static size_t dirent_size(size_t name_len)
{
	size_t size = name_len + sizeof(ext2_dirent_t);
	if ((size % 4) != 0) {
		size += 4 - (size % 4);
	}
	return size;
}

int ext2_dir_addent(ext2_inode_t* dir, uint32_t num, ext2_inode_type_t type, const char* name, int namelen)
{
	ext2_dirent_t* entry;
	ext2_dirent_t* newentry;
	unsigned char buf[ext2_sb_blocksize(dir->fs->sb)];
	uint32_t pos = 0, tpos=0, off=0;
	size_t newentry_len = dirent_size(strlen(name));
	size_t entry_len = 0;
	if (dir->size) {
		ext2_inode_readdata(dir, 0, ext2_sb_blocksize(dir->fs->sb), buf);
		while (pos < dir->size) {
			entry = (ext2_dirent_t*) &buf[tpos];
			entry_len = dirent_size(entry->name_len);
			
			if (entry->record_len == 0)
				return 0;
			if (entry->record_len - entry_len >= newentry_len) {
				newentry = (ext2_dirent_t*) &buf[tpos + entry_len];
				memset(newentry, 0, newentry_len);
				newentry->name_len = namelen;
				memcpy(newentry->name, name, newentry->name_len);
				newentry->inode = num;
				newentry->record_len = entry->record_len - entry_len;
				newentry->type = type;
				entry->record_len = entry_len;
				mutex_acquire(&dir->fs->fs_lock);
				ext2_inode_writedata(dir, off, ext2_sb_blocksize(dir->fs->sb), buf);
				mutex_release(&dir->fs->fs_lock);
				return 1;
			} else
			pos += entry->record_len;
			tpos += entry->record_len;
			if(tpos >= ext2_sb_blocksize(dir->fs->sb) && pos < dir->size)
			{
				tpos=0;
				off+=ext2_sb_blocksize(dir->fs->sb);
				memset(buf, 0, ext2_sb_blocksize(dir->fs->sb));
				ext2_inode_readdata(dir, off, ext2_sb_blocksize(dir->fs->sb), buf);
			}
		}
	}
	newentry_len = ext2_sb_blocksize(dir->fs->sb);
	unsigned char newbuf[newentry_len];
	memset(newbuf, 0, newentry_len);
	newentry = (ext2_dirent_t*) newbuf;
	newentry->name_len = namelen;
	memcpy(newentry->name, name, newentry->name_len);
	newentry->inode = num;
	newentry->record_len = newentry_len;
	newentry->type = type;
	mutex_acquire(&dir->fs->fs_lock);
	int r = ext2_inode_writedata(dir, pos, newentry_len, newbuf);
	mutex_release(&dir->fs->fs_lock);
	return 1;
}

int ext2_dir_delent(ext2_inode_t* dir, const char* name, int namelen, int dofree)
{
	ext2_dirent_t* prev_entry = NULL;
	ext2_dirent_t* entry;
	ext2_inode_t inode;
	unsigned char buf[ext2_sb_blocksize(dir->fs->sb)];
	uint32_t pos = 0, tpos=0, off=0;
	int ret = 0;
	ext2_blockgroup_t bg;
	uint32_t bgnum;
	ext2_inode_readdata(dir, 0, ext2_sb_blocksize(dir->fs->sb), buf);
	while (pos < dir->size) {
		entry = (ext2_dirent_t*) &buf[tpos];
		if ((namelen == entry->name_len) &&
			!strncmp((const char *)name, (const char *)entry->name, 
				entry->name_len) && entry->inode)
		{
			if (!ext2_inode_read(dir->fs, entry->inode, &inode))
				return 0;
			mutex_acquire(&dir->fs->fs_lock);
			--inode.link_count;
			if (inode.link_count == 0) {
				struct inode *target = vfs_icache_get(dir->fs->filesys, entry->inode);
				assert(target);
				if (S_ISDIR(target->mode)) {
					bgnum = ext2_inode_to_internal(inode.fs, inode.number) /
					inode.fs->sb->inodes_per_group;
					ext2_bg_read(inode.fs, bgnum, &bg);
					kprintf("subbing %d:%s (was %d)\n", entry->inode, name, bg.used_directories);
					bg.used_directories--;
					ext2_bg_update(inode.fs, bgnum, &bg);
					ext2_inode_read(dir->fs, dir->number, dir);
				}
				vfs_icache_put(target);
				ext2_inode_free(&inode);
			}
			if (!ext2_inode_update(&inode)) {
				mutex_release(&dir->fs->fs_lock);
				return 0;
			}
			
			entry->inode = 0;
			ext2_inode_writedata(dir, off, ext2_sb_blocksize(dir->fs->sb), buf);
			mutex_release(&dir->fs->fs_lock);
			return 1;
		}
		
		pos += entry->record_len;
		tpos += entry->record_len;
		if(!entry->record_len)
			break;
		if(tpos >= ext2_sb_blocksize(dir->fs->sb))
		{
			tpos=0;
			off += ext2_sb_blocksize(dir->fs->sb);
			ext2_inode_readdata(dir, off, ext2_sb_blocksize(dir->fs->sb), buf);
		}
	}
	return ret;
}

void ext2_dir_change_dir_count(ext2_inode_t *node, int minus)
{
	ext2_blockgroup_t bg;
	uint32_t bgnum = ext2_inode_to_internal(parent->fs, node->number)
		/ node->fs->sb->inodes_per_group;

	mutex_acquire(&node->fs->bg_lock);
	ext2_bg_read(node->fs, bgnum, &bg);
	if(minus)
		bg.used_directories--;
	else
		bg.used_directories++;
	ext2_bg_update(node->fs, bgnum, &bg);
	mutex_release(&node->fs->bg_lock);
}

