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
/* This code is adapted from the CDI project */

#include <sea/types.h>
#include <sea/string.h>
#include <sea/lib/cache.h>
#include <sea/dm/block.h>
#include <modules/ext2.h>
static uint32_t block_free(ext2_fs_t* fs, uint32_t num);

static int get_bg_block(ext2_fs_t* fs, int group_nr)
{
	uint32_t num;
	unsigned bs = ext2_sb_blocksize(fs->sb);
	num = fs->sb->first_data_block + 1 + 
		(group_nr * sizeof(ext2_blockgroup_t)) / bs; 
	return num;
}

int ext2_bg_read(ext2_fs_t* fs, int group_nr, ext2_blockgroup_t* bg)
{
	int bg_n = get_bg_block(fs, group_nr);
	ext2_read_off(fs, bg_n * ext2_sb_blocksize(fs->sb) + 
		((group_nr * sizeof(ext2_blockgroup_t)) % ext2_sb_blocksize(fs->sb)), 
		(unsigned char *)bg, sizeof(ext2_blockgroup_t));
	return 1;
}

int ext2_bg_update(ext2_fs_t* fs, int group_nr, ext2_blockgroup_t* bg)
{
	int num = get_bg_block(fs, group_nr);
	ext2_write_off(fs, num * ext2_sb_blocksize(fs->sb) + 
		(group_nr * sizeof(ext2_blockgroup_t)) % ext2_sb_blocksize(fs->sb), 
		(unsigned char *)bg, sizeof(ext2_blockgroup_t));
	return 1;
}

/**
* Pointer auf den Cache-Block in dem sich der Inode befindet holen.
*
* @param fs        Dateisystem zu dem der Inode gehoert
* @param inode_nr  Inodenummer
* @param offset    Wenn != 0 wird an dieser Speicherstelle der Offset vom
*                  Anfang dieses Blocks aus zum Inode abgelegt.
*
* @return Pointer auf das Handle fuer diesen Block
*/
static int inode_get_block(
ext2_fs_t* fs, uint32_t inode_nr, size_t* offset)
{
	uint32_t    inode_offset;
	uint32_t    inode_internal = ext2_inode_to_internal(fs, inode_nr);
	size_t      inode_size = ext2_sb_inodesize(fs->sb);
	size_t      bs = ext2_sb_blocksize(fs->sb);
	ext2_blockgroup_t *bg, bg__;bg = &bg__;
	ext2_bg_read(fs, inode_internal / (fs->sb->inodes_per_group), bg);
	// Offset des Inodes zum Anfang der Tabelle
	inode_offset = inode_size * (inode_internal % (fs->sb->inodes_per_group));
	// Offset zum Anfang des Blocks
	if (offset) {
		*offset = inode_offset % bs;
	}
	return bg->inode_table +
	inode_offset / bs;
}

int ext2_inode_read(ext2_fs_t* fs, uint32_t inode_nr, ext2_inode_t* inode)
{
	struct ce_t *c = cache_find_element(fs->cache, inode_nr, 1);
	if(c) {
		memcpy(inode, c->data, sizeof (ext2_inode_t));
		return 1;
	}
	size_t offset;
	int num;
	if (!(num = inode_get_block(fs, inode_nr, &offset))) {
		return 0;
	}
	ext2_read_off(fs, num * ext2_sb_blocksize(fs->sb) + offset, 
		(unsigned char *)inode, ext2_sb_inodesize(fs->sb));
	inode->fs = fs;
	inode->number = inode_nr;
	cache_object_clean(fs->cache, inode_nr, 1, sizeof(ext2_inode_t), (void *)inode);
	return 1;
}

int ext2_inode_update(ext2_inode_t* inode)
{
	struct ce_t *c = cache_find_element(inode->fs->cache, inode->number, 1);
	if(c)
		memcpy(c->data, inode, sizeof (ext2_inode_t));
	size_t offset;
	int num;
	if (!(num = inode_get_block(inode->fs, inode->number, &offset))) {
		return 0;
	}
	ext2_write_off(inode->fs, num * ext2_sb_blocksize(inode->fs->sb) + offset, 
		(unsigned char *)inode, ext2_sb_inodesize(inode->fs->sb));
	return 1;
}

/**
* Block mit der Inode-Bitmap holen
*
* @param fs    Das Dateisystem
* @param bg    Blockgruppendeskriptor
*
* @return Blockhandle
*/
static inline int ibitmap_get_block(
ext2_fs_t* fs, ext2_blockgroup_t* bg)
{
	return bg->inode_bitmap;
}

/**
* Neuen Inode allozieren
*
* @return Interne Inodenummer
*/

static uint32_t inode_alloc(ext2_fs_t* fs, uint32_t bgnum)
{
	uint32_t* bitmap;
	uint32_t i, j;
	ext2_blockgroup_t bg;
	ext2_bg_read(fs, bgnum, &bg);
	ext2_inode_t ino;
	// Bitmap laden
	int blk = ibitmap_get_block(fs, &bg);
	unsigned char b[ext2_sb_blocksize(fs->sb)];
	ext2_read_block(fs, blk, b);
	bitmap = (uint32_t *)b;
	
	// Freies Bit suchen
	for (i = 0; i < ((fs->sb->inodes_per_group) + 31) / 32; i++) {
		if (bitmap[i] != ~0U) {
			for (j = 0; j < 32; j++) {
				if (((bitmap[i] & (1 << j)) == 0)
					&& (i * 32 + j <= (fs->sb->inodes_per_group)))
				{
					goto found;
				}
			}
		}
	}
	return 0;
	
	found:
	
	ext2_inode_read(fs, (bgnum * fs->sb->inodes_per_group) + 
		(i * 32) + j + 1, &ino);
	// Als besetzt markieren
	bitmap[i] |= (1 << j);
	ext2_write_block(fs, blk, b);
	
	fs->sb->free_inodes--;
	ext2_sb_update(fs, fs->sb);
	
	bg.free_inodes--;
	ext2_bg_update(fs, bgnum, &bg);
	return (bgnum * fs->sb->inodes_per_group) + (i * 32) + j;
}

int ext2_inode_alloc(ext2_fs_t* fs, ext2_inode_t* inode)
{
	uint32_t number;
	uint32_t bgnum;
	mutex_acquire(fs->m_node);
	for (bgnum = 0; bgnum < ext2_sb_bgcount(fs->sb); bgnum++) {
		number = inode_alloc(fs, bgnum);
		if (number) {
			goto found;
		}
	}
	mutex_release(fs->m_node);
	return 0;
	
	found:
	mutex_release(fs->m_node);
	if (!number) {
		return 0;
	}
	if (!ext2_inode_read(fs, ext2_inode_from_internal(fs, number), inode))
		return 0;
	memset(inode, 0, ext2_sb_inodesize(fs->sb));
	inode->number = ext2_inode_from_internal(fs, number);
	inode->fs = fs;
	ext2_inode_update(inode);
	return 1;
}

int ext2_inode_free(ext2_inode_t* inode)
{
	ext2_fs_t* fs = inode->fs;
	uint32_t inode_int = ext2_inode_to_internal(fs, inode->number);
	int i;
	mutex_acquire(fs->m_node);
	// dtime muss fuer geloeschte Inodes != 0 sein
	inode->deletion_time = arch_time_get_epoch();
	// Inodebitmap anpassen
	uint32_t bgnum = inode_int / fs->sb->inodes_per_group;
	uint32_t num = inode_int % fs->sb->inodes_per_group;
	uint32_t* bitmap;
	size_t   block_size = ext2_sb_blocksize(fs->sb);
	ext2_blockgroup_t bg;
	ext2_bg_read(fs, bgnum, &bg);
	
	int blk = ibitmap_get_block(fs, &bg);
	unsigned char b[ext2_sb_blocksize(inode->fs->sb)];
	ext2_read_block(fs, blk, b);
	bitmap = (uint32_t *)b;
	
	bitmap[num / 32] &= ~(1 << (num % 32));
	ext2_write_block(fs, blk, b);
	
	fs->sb->free_inodes++;
	ext2_sb_update(fs, fs->sb);
	
	bg.free_inodes++;
	ext2_bg_update(fs, bgnum, &bg);
	mutex_release(fs->m_node);
	void free_indirect_blocks(uint32_t table_block, int level)
	{
		uint32_t i_;
		uint32_t* table;
		
		int blck = table_block;
		unsigned char bf[ext2_sb_blocksize(inode->fs->sb)];
		ext2_read_block(fs, blck, bf);
		table = (uint32_t *)bf;
		
		for (i_ = 0; i_ < block_size / 4; i_++) {
			if (table[i_]) {
				if (level) {
					free_indirect_blocks(table[i_], level - 1);
				}
				block_free(inode->fs, table[i_]);
			}
		}
		
	}
	
	// Abbrechen bei fast-Symlinks
	if (!inode->sector_count) {
		return 1;
	}
	
	// Blocks freigeben
	for (i = 0; i < 15; i++) {
		if (inode->blocks[i]) {
			if (i >= 12) {
				free_indirect_blocks(inode->blocks[i], i - 12);
			}
			block_free(inode->fs, inode->blocks[i]);
		}
	}
	return 1;
}

static int block_alloc_bg(ext2_fs_t* fs, ext2_blockgroup_t* bg)
{
	uint32_t i;
	
	i = fs->block_prev_alloc /fs->sb->blocks_per_group;
	if (i < ext2_sb_bgcount(fs->sb)) {
		ext2_bg_read(fs, i, bg);
		if (bg->free_blocks) {
			return i;
		}
	}
	
	for (i = 0; i < ext2_sb_bgcount(fs->sb); i++) {
		ext2_bg_read(fs, i, bg);
		if (bg->free_blocks) {
			return i;
		}
	}
	
	return -1;
}

/**
* Block mit der Block-Bitmap holen
*
* @param fs    Das Dateisystem
* @param bg    Blockgruppendeskriptor
*
* @return Blockhandle
*/
static inline int bbitmap_get_block(
ext2_fs_t* fs, ext2_blockgroup_t* bg)
{
	return bg->block_bitmap;
}


static uint32_t block_alloc(ext2_fs_t* fs, int set_zero)
{
	uint32_t* bitmap;
	uint32_t block_num;
	uint32_t i, j;
	size_t   block_size = ext2_sb_blocksize(fs->sb);
	int block;
	uint32_t bgnum;
	ext2_blockgroup_t bg;
	mutex_acquire(fs->m_block);
	bgnum = block_alloc_bg(fs, &bg);
	if (bgnum == (uint32_t) -1) {
		mutex_release(fs->m_block);
		return 0;
	}
	
	// Bitmap laden
	block = bbitmap_get_block(fs, &bg);
	unsigned char b[ext2_sb_blocksize(fs->sb)];
	ext2_read_block(fs, block, b);
	bitmap = (uint32_t *)b;
	
	// Zuerst Blocks nach dem vorherigen alloziierten pruefen
	if (fs->block_prev_alloc &&
		(fs->block_prev_alloc /fs->sb->blocks_per_group == bgnum))
	{
		uint32_t first_j = fs->block_prev_alloc % 32;
		uint32_t first_i = fs->block_prev_alloc / 32;
		for (i = first_i;i < fs->sb->blocks_per_group / 32; i++) {
			if (bitmap[i] != ~0U) {
				j = (i == first_i) ? first_j : 0;
				for (; j < 32; j++) {
					if ((bitmap[i] & (1 << j)) == 0) {
						goto found;
					}
				}
			}
		}
	}
	
	// Freies Bit suchen
	for (i = 0; i < fs->sb->blocks_per_group / 32; i++) {
		if (bitmap[i] != ~0U) {
			for (j = 0; j < 32; j++) {
				if ((bitmap[i] & (1 << j)) == 0) {
					goto found;
				}
			}
		}
	}
	mutex_release(fs->m_block);
	return 0;
	
	found:
	block_num = fs->sb->blocks_per_group * bgnum + (i * 32 + j) +
	fs->sb->first_data_block;
	
	// Mit Nullen initialisieren, falls gewuenscht
	if (set_zero) {
		unsigned char tmp[block_size];
		memset(tmp, 0, block_size);
		ext2_write_block(fs, block_num, tmp);
	}
	
	// Als besetzt markieren
	bitmap[i] |= (1 << j);
	ext2_write_block(fs, block, b);
	
	fs->sb->free_blocks--;
	ext2_sb_update(fs, fs->sb);
	
	bg.free_blocks--;
	ext2_bg_update(fs, bgnum, &bg);
	
	fs->block_prev_alloc = block_num;
	mutex_release(fs->m_block);
	return block_num;
}

static uint32_t block_free(ext2_fs_t* fs, uint32_t num)
{
	uint32_t* bitmap;
	uint32_t bgnum;
	int block;
	if(!num) return 1;
	mutex_acquire(fs->m_block);
	num -= fs->sb->first_data_block;
	bgnum = num / fs->sb->blocks_per_group;
	num = num % fs->sb->blocks_per_group;
	
	ext2_blockgroup_t bg;
	ext2_bg_read(fs, bgnum, &bg);
	
	// Bitmap laden
	block = bbitmap_get_block(fs, &bg);
	unsigned char b[ext2_sb_blocksize(fs->sb)];
	ext2_read_block(fs, block, b);
	bitmap = (uint32_t *)b;
	
	// Als frei markieren
	bitmap[num / 32] &= ~(1 << (num % 32));
	ext2_write_block(fs, block, b);
	
	fs->sb->free_blocks++;
	ext2_sb_update(fs, fs->sb);
	
	bg.free_blocks++;
	ext2_bg_update(fs, bgnum, &bg);
	mutex_release(fs->m_block);
	return 1;
}

/**
* Berechnet wie oft indirekt dieser Block ist im Inode.
*
* @param inode          Betroffener Inode
* @param block          Blocknummer
* @param direct_block   Pointer auf die Variable fuer den Index in der
*                       Blocktabelle des Inodes.
* @param indirect_block Pointer auf die Variable fuer den Index in den
*                       indirekten Blocktabellen. Wird auf 0 gesetzt fuer
*                       direkte Blocks.
*
* @return Ein Wert von 0-3 je nach dem wie oft der Block indirekt ist
*/
static inline int get_indirect_block_level(ext2_inode_t* inode, uint32_t block, 
	uint32_t* direct_block, uint32_t* indirect_block)
{
	ext2_fs_t* fs = inode->fs;
	size_t block_size = ext2_sb_blocksize(fs->sb);
	
	if (block < 12) {
		*direct_block = block;
		*indirect_block = 0;
		return 0;
	} else if (block < ((block_size / 4) + 12)) {
		*direct_block = 12;
		*indirect_block = block - 12;
		return 1;
	} else if (block < ((block_size / 4) * ((block_size / 4) + 1) + 12)) {
		*direct_block = 13;
		*indirect_block = block - 12 - (block_size / 4);
		return 2;
	} else {
		*direct_block = 14;
		*indirect_block = block - 12 - (block_size / 4) *
		((block_size / 4) + 1);
		return 3;
	}
}

static uint32_t get_block_offset(ext2_inode_t* inode, uint32_t block, int alloc)
{
	size_t    block_size;
	uint32_t  block_nr;
	uint32_t  indirect_block;
	uint32_t  direct_block;
	int       level;
	int       i;
	uint32_t  j;
	uint32_t* table;
	int b;
	// Wird zum freigeben von Blocks benoetigt
	uint32_t path[4][2];
	memset(path, 0, sizeof(path));
	
	ext2_fs_t* fs = inode->fs;
	block_size = ext2_sb_blocksize(fs->sb);
	
	
	// Herausfinden wie oft indirekt der gesuchte Block ist
	level = get_indirect_block_level(inode, block, &direct_block, &indirect_block);
	// Direkte Blocknummer oder Nummer des ersten Tabellenblocks aus dem Inode
	// lesen
	block_nr = inode->blocks[direct_block];
	if (!block_nr && (alloc == 1)) {
		block_nr = inode->blocks[direct_block] = block_alloc(fs, level);
		inode->sector_count += block_size / 512;
	}
	path[0][0] = block_nr;
	unsigned char buf[ext2_sb_blocksize(inode->fs->sb)];
	// Indirekte Blocks heraussuchen und ggf. allozieren
	for (i = 0;  (i < level) && block_nr; i++) {
		int table_modified;
		uint32_t offset;
		uint32_t pow;
		
		b = block_nr;
		ext2_read_block(fs, b, buf);
		table = (uint32_t *)buf;
		
		// pow = (block_size / 4) ** (level - i - 1)
		pow = (level - i == 3 ? ((block_size / 4) * (block_size / 4)) :
		(level - i == 2 ? (block_size / 4) : 1));
		offset = indirect_block / pow;
		indirect_block %= pow;
		
		
		table_modified = 0;
		block_nr = table[offset];
		// Pfad fuers Freigeben der Blocks speichern
		path[i][1] = offset;
		path[i + 1][0] = block_nr;
		
		// Block allozieren wenn noetig und gewuenscht
		if (!block_nr && (alloc == 1)) {
			block_nr = table[offset] = block_alloc(fs, (level - i) > 1);
			inode->sector_count += block_size / 512;
			table_modified = 1;
		}
		if(table_modified)
			ext2_write_block(fs, b, buf);
	}
	
	// Wenn der Block nicht freigegeben werden soll, sind wir jetzt fertig
	if (alloc != 2) {
		return block_size * block_nr;
	}
	
	// Blocks freigeben
	int empty = 1;
	for (i = level; (i > 0) && empty; i--) {
		int was_empty = empty;
		
		b = path[i - 1][0];
		//char buf[ext2_sb_blocksize(inode->fs->sb)];
		ext2_read_block(fs, b, buf);
		table = (uint32_t *)buf;
		
		// Wenn es sich um einen Tabellenblock handelt, muss er leer sein
		// (Hier wird eigentlich der naechste gepfrueft... das funktioniert aber
		// so, da wir nur in die Schleife kommen wenn es sich um einen
		// indirekten Block handelt, und da der oberste Block bei i = level
		// nicht geprueft werden muss, da er nur ein Datenblock ist.
		for (j = 0; (j < block_size / 4) && empty; j++) {
			if (table[j]) {
				empty = 0;
			}
		}
		
		if (was_empty && path[i][0]) {
			table[path[i - 1][1]] = 0;
			block_free(fs, path[i][0]);
			inode->sector_count -= block_size / 512;
		}
		
		if(was_empty)
			ext2_write_block(fs, b, buf);
	}
	
	// Der Letzte muss manuell freigegeben werden, da er nicht in einer
	// Blocktabelle eingetragen ist.
	if (!path[0][1] && path[0][0] && empty) {
		inode->blocks[direct_block] = 0;
		
		inode->sector_count -= block_size / 512;
		block_free(fs, path[0][0]);
	}
	return block_size * block_nr;
}
int ext2_inode_writeblk(ext2_inode_t* inode, uint32_t block, void* buf);
int ext2_inode_readblk(ext2_inode_t* inode, uint32_t block, void* buf, size_t count)
{
	ext2_fs_t* fs = inode->fs;
	int b;
	size_t block_size = ext2_sb_blocksize(fs->sb);
	uint32_t offset;
	size_t i;
	
	for (i = 0; i < count; i++) {
		offset = get_block_offset(inode, block + i, 0);
		
		// Ein paar Nullen fuer Sparse Files
		if (offset == 0) {
			memset((unsigned char *)buf + block_size * i, 0, block_size);
			continue;
		}
		
		b = offset / block_size;
		ext2_read_block(fs, b, (unsigned char *)buf + block_size * i);
	}
	return count;
}

void force_sync(ext2_fs_t *fs, unsigned b)
{
	int off = b*2 + fs->block;
#if CONFIG_BLOCK_CACHE
	dm_write_block_cache(fs->dev, off);
	dm_write_block_cache(fs->dev, off+1);
#endif
}

static int writeblk(ext2_inode_t* inode, uint32_t block, const void* buf)
{
	ext2_fs_t* fs = inode->fs;
	unsigned b;
	uint32_t block_offset = get_block_offset(inode, block, 1);
	size_t   block_size = ext2_sb_blocksize(fs->sb);
	
	if (block_offset == 0) {
		return 0;
	}
	b = block_offset / block_size;
	ext2_write_block(fs, b, (unsigned char *)buf);
	return 1;
}

int ext2_inode_writeblk(ext2_inode_t* inode, uint32_t block, void* buf)
{
	size_t block_size = ext2_sb_blocksize(inode->fs->sb);
	int ret = writeblk(inode, block, buf);
	
	if ((block + 1) * block_size > inode->size) {
		inode->size = (block + 1) * block_size;
	}
	return ret;
}

int ext2_inode_writelink(ext2_inode_t *inode, unsigned int start, size_t len, 
	char *buf)
{
	memcpy(((char *)inode->blocks) + start, buf, len);
	if (start + len > inode->size) {
		inode->size = start + len;
	}
	ext2_inode_update(inode);
	return len;
}

int ext2_inode_readlink(ext2_inode_t *inode, unsigned int start, size_t len, 
	unsigned char *buf)
{
	unsigned end = start + len;
	unsigned tmp[15];
	unsigned s = start;
	int i=0;
	while(s < end)
	{
		tmp[i] = inode->blocks[s/4];
		s+=4;
		i++;
	}
	memcpy(buf, ((char *)tmp) + start % 4, len);
	return len;
}

int ext2_inode_readdata(ext2_inode_t* inode, uint32_t start, 
	size_t len, unsigned char* buf)
{
        size_t block_size = ext2_sb_blocksize(inode->fs->sb);
        uint32_t start_block = start / block_size;
        uint32_t end_block = (start + len - 1) / block_size;
        size_t block_count = end_block - start_block + 1;
        unsigned char localbuf[block_size];
        uint32_t i;
        int ret;
        int counter=0;
        if (len == 0) {
                return 0;
        }
        if(S_ISLNK(inode->mode) && inode->size < 60)
                return ext2_inode_readlink(inode, start, len, buf);
        // Wenn der erste Block nicht ganz gelesen werden soll, wird er zuerst in
        // einen Lokalen Puffer gelesen.
        if (start % block_size) {
                size_t bytes;
                size_t offset = start % block_size;
                if (!ext2_inode_readblk(inode, start_block, localbuf, 1)) {
                        return 0;
                }
                
                bytes = block_size - offset;
                if (len < bytes) {
                        bytes = len;
                }
                memcpy(buf, localbuf + offset, bytes);
                
                if (--block_count == 0) {
                        return bytes;
                }
                len -= bytes;
                buf += bytes;
                start_block++;
                counter+=bytes;
        }
        // Wenn der letzte Block nicht mehr ganz gelesen werden soll, muss er
        // separat eingelesen werden.
        if (len % block_size) {
                size_t bytes = len % block_size;
                //kprintf("emd\n");
                if (!ext2_inode_readblk(inode, end_block, localbuf, 1)) {
                        return counter;
                }
                memcpy(buf + len - bytes, localbuf, bytes);
                counter+=bytes;
                if (--block_count == 0) {
                        return counter;
                }
                len -= bytes;
                end_block--;
        }
        
        for (i = 0; i < block_count; i++) {
                ret = ext2_inode_readblk(inode, start_block + i, buf + i * block_size,
                                         1);
                if (!ret) {
                        return counter;
                }
                
                // Wenn mehrere Blocks aneinander gelesen wurden, muessen die jetzt
                // uebersprungen werden.
                i += ret - 1;
                counter+=block_size;
        }
        
        return counter;
}

int ext2_inode_writedata(ext2_inode_t* inode, uint32_t start, size_t len, 
	const unsigned char* buf)
{
	if(!len) return 0;
	size_t block_size = ext2_sb_blocksize(inode->fs->sb);
	uint32_t start_block = start / block_size;
	uint32_t end_block = (start + len - 1) / block_size;
	size_t block_count = end_block - start_block + 1;
	unsigned char localbuf[block_size*2];
	uint32_t i;
	unsigned int init_sect_count = inode->sector_count;
	int ret;
	size_t req_len = len;
	int counter=0;
	if(S_ISLNK(inode->mode))
	{
		if(inode->size < 60)
		{
			if(start + len > 60)
			{
				/* Expand */
				memset((char *)inode->blocks, 0, 4*15);
			}
			else
				return ext2_inode_writelink(inode, start, len, (char *)buf);
		}
	}
	if (start % block_size) {
		size_t bytes;
		size_t offset = start % block_size;
		if (!ext2_inode_readblk(inode, start_block, localbuf, 1)) {
			goto out;
		}
		bytes = block_size - offset;
		if (len < bytes) {
			bytes = len;
		}
		memcpy(localbuf + offset, buf, bytes);
		if (!writeblk(inode, start_block, localbuf)) {
			goto out;
		}
		counter+=bytes;
		if (--block_count == 0) {
			goto out;
		}
		len -= bytes;
		buf += bytes;
		start_block++;
	}
	if (len % block_size) {
		size_t bytes = len % block_size;
		if (!ext2_inode_readblk(inode, end_block, localbuf, 1)) {
			goto out;
		}
		memcpy(localbuf, buf + len - bytes, bytes);
		if (!writeblk(inode, end_block, localbuf)) {
			goto out;
		}
		counter+=bytes;
		if (--block_count == 0) {
			goto out;
		}
		len -= bytes;
		end_block--;
	}
	
	for (i = 0; i < block_count; i++) {
		ret = writeblk(inode, start_block + i, buf + i * block_size);
		if (!ret) {
			goto out;
		}
		counter+=block_size;
	}
	
	out:
	if (start + counter > inode->size || inode->sector_count != init_sect_count) {
		if(start + counter > inode->size) 
			inode->size = start + counter;
		ext2_inode_update(inode);
	}
	
	return counter;
}

int ext2_inode_truncate(ext2_inode_t* inode, uint32_t size, int noupdate)
{
	size_t block_size = ext2_sb_blocksize(inode->fs->sb);
	uint32_t first_to_free = (size / block_size) + 1;
	uint32_t last_to_free =
	((inode->size + block_size - 1) / block_size);
	uint32_t i;
	if(size > inode->size) return 0;
	unsigned char tmp[size+1];
	if(S_ISLNK(inode->mode))
	{
		if(inode->size >= 60 && size < 60) {
			ext2_inode_readdata(inode, 0, size, tmp);
			first_to_free = 0;
		}
	}
	
	for (i = first_to_free; i <= last_to_free; i++) {
		get_block_offset(inode, i, 2);
	}
	int old_s = inode->size;
	inode->size = size;
	if (!noupdate && !ext2_inode_update(inode)) {
		return noupdate;
	}
	if(S_ISLNK(inode->mode) && old_s >= 60 && size < 60)
		ext2_inode_writedata(inode, 0, size, tmp);
	return 1;
}
