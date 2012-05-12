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

#include <string.h>
#include "ext2.h"

int ext2_dir_getnum(ext2_inode_t* inode,
			       unsigned number, char *name)
{
	ext2_dirent_t* entry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = ext2_sb_blocksize(inode->fs->sb), tpos=0, off=0;
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
		if (entry->inode == 0)
			continue;
		
		if (!(number)) {
			strncpy(name, (char *)entry->name, entry->name_len);
			return entry->inode;
		}
		number--;
		if (entry->record_len == 0)
			break;
	}
	return 0;
}

int ext2_dir_change_num(ext2_inode_t* inode, char *name,
			       unsigned new_number)
{
	ext2_dirent_t* entry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = ext2_sb_blocksize(inode->fs->sb), tpos=0, off=0;
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
		if (entry->inode == 0)
			continue;
		if (!strncmp((const char *)name, (const char *)entry->name, (unsigned)entry->name_len)
			&& (unsigned)entry->name_len == strlen((const char *)name)) {
			int old = entry->inode;
			entry->inode = new_number;
			ext2_inode_writedata(inode, off -  ext2_sb_blocksize(inode->fs->sb), ext2_sb_blocksize(inode->fs->sb), buf);
			return old;
		}
		if (entry->record_len == 0)
			break;
	}
	return 0;
}

int ext2_dir_change_type(ext2_inode_t* inode, char *name,
			       unsigned new_type)
{
	ext2_dirent_t* entry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = ext2_sb_blocksize(inode->fs->sb), tpos=0, off=0;
	unsigned len = strlen(name);
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
		if (entry->inode == 0)
			continue;
		
		if (!strncmp((const char *)name, (const char *)entry->name, entry->name_len) && entry->name_len == len) {
			int old = entry->type;
			entry->type = new_type;
			ext2_inode_writedata(inode, off -  ext2_sb_blocksize(inode->fs->sb), ext2_sb_blocksize(inode->fs->sb), buf);
			return old;
		}
		if (entry->record_len == 0)
			break;
	}
	return -1;
}

int ext2_dir_get(ext2_inode_t* inode, char *name, ext2_dirent_t *de)
{
	ext2_dirent_t* entry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = ext2_sb_blocksize(inode->fs->sb), tpos=0, off=0;
	unsigned len = strlen(name);
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
		if (!strncmp((const char *)name, (const char *)entry->name, len) && len == entry->name_len) {
			memcpy(de, entry, entry->record_len);
			return 1;
		}
	}
	return 0;
}

int ext2_dir_get_inode(ext2_inode_t* inode, char *name)
{
	ext2_dirent_t* entry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = ext2_sb_blocksize(inode->fs->sb), tpos=0, off=0;
	unsigned len = strlen(name);
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
		if (!strncmp((const char *)name, (const char *)entry->name, len) && len == entry->name_len) {
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

int ext2_dir_link(ext2_inode_t* dir, ext2_inode_t* inode, const char* name)
{
	ext2_dirent_t* entry;
	ext2_dirent_t* newentry;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	uint32_t pos = 0, tpos=0, off=0;
	size_t newentry_len = dirent_size(strlen(name));
	size_t entry_len = 0;
	ext2_inode_type_t type = 0;
	if (dir->fs->sb->revision && ((dir->fs->sb->features_req &
		EXT2_FEATURE_INCOMP_FILETYPE)))
	{
		type = ext2_inode_type(inode);
	}
	if (dir->size) {
		ext2_inode_readdata(dir, 0, ext2_sb_blocksize(inode->fs->sb), buf);
		while (pos < dir->size) {
			entry = (ext2_dirent_t*) &buf[tpos];
			entry_len = dirent_size(entry->name_len);
			
			if (entry->record_len == 0)
				return 0;
			if (entry->record_len - entry_len >= newentry_len) {
				newentry = (ext2_dirent_t*) &buf[tpos + entry_len];
				memset(newentry, 0, newentry_len);
				newentry->name_len = strlen(name);
				memcpy(newentry->name, name, newentry->name_len);
				newentry->inode = inode->number;
				newentry->record_len = entry->record_len - entry_len;
				newentry->type = type;
				entry->record_len = entry_len;
				
				ext2_inode_writedata(dir, off, ext2_sb_blocksize(inode->fs->sb), buf);
				inode->link_count++;
				ext2_inode_update(inode);
				return 1;
			} else
			pos += entry->record_len;
			tpos += entry->record_len;
			if(tpos >= ext2_sb_blocksize(inode->fs->sb) && pos < dir->size)
			{
				tpos=0;
				off+=ext2_sb_blocksize(inode->fs->sb);
				memset(buf, 0, ext2_sb_blocksize(inode->fs->sb));
				ext2_inode_readdata(dir, off, ext2_sb_blocksize(inode->fs->sb), buf);
			}
		}
	}
	newentry_len = ext2_sb_blocksize(inode->fs->sb);
	unsigned char newbuf[newentry_len];
	memset(newbuf, 0, newentry_len);
	newentry = (ext2_dirent_t*) newbuf;
	newentry->name_len = strlen(name);
	memcpy(newentry->name, name, newentry->name_len);
	newentry->inode = inode->number;
	newentry->record_len = newentry_len;
	newentry->type = type;
	int r = ext2_inode_writedata(dir, pos, newentry_len, newbuf);
	inode->link_count++;
	ext2_inode_update(inode);
	return 1;
}

int ext2_dir_unlink(ext2_inode_t* dir, const char* name, int dofree)
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
		if ((strlen(name) == entry->name_len) &&
			!strncmp((const char *)name, (const char *)entry->name, entry->name_len) && entry->inode)
		{
			if (!ext2_inode_read(dir->fs, entry->inode, &inode))
				return 0;
			--inode.link_count;
			if (inode.link_count == 0 || (EXT2_INODE_IS_DIR(&inode) && inode.link_count == 1)) {
				if (EXT2_INODE_IS_DIR(&inode)) {
					ext2_dir_unlink(&inode, "..", 1);
					inode.link_count--;
					bgnum = ext2_inode_to_internal(inode.fs, inode.number) /
					inode.fs->sb->inodes_per_group;
					ext2_bg_read(inode.fs, bgnum, &bg);
					bg.used_directories--;
					ext2_bg_update(inode.fs, bgnum, &bg);
					ext2_inode_read(dir->fs, dir->number, dir);
				}
				ext2_inode_free(&inode);
			}
			if (!ext2_inode_update(&inode))
				return 0;
			entry->inode = 0;
			ext2_inode_writedata(dir, off, ext2_sb_blocksize(dir->fs->sb), buf);
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

int ext2_dir_create(ext2_inode_t* parent, const char* name, ext2_inode_t* newi)
{
	ext2_blockgroup_t bg;
	uint32_t bgnum;
	
	if (!ext2_inode_alloc(parent->fs, newi))
		return 0;
	
	bgnum = ext2_inode_to_internal(parent->fs, newi->number) /
	newi->fs->sb->inodes_per_group;
	
	newi->deletion_time = 0;
	newi->mode = EXT2_INODE_MODE_DIR | 0777;
	
	if (!ext2_dir_link(parent, newi, name) ||
			!ext2_dir_link(newi, newi, ".") ||
			!ext2_dir_link(newi, parent, ".."))
		return 0;
	if (!ext2_inode_update(newi) || !ext2_inode_update(parent))
		return 0;
	ext2_bg_read(parent->fs, bgnum, &bg);
	bg.used_directories++;
	ext2_bg_update(parent->fs, bgnum, &bg);
	return 1;
}

