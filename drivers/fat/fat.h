#ifndef _SEA_FAT_H
#define _SEA_FAT_H

typedef struct {
	unsigned char drive_num, flags, sig;
	unsigned char serial[4];
	unsigned char label[11];
	unsigned char sysid[8];
} fat_16_extbpb;

typedef struct {
	unsigned int sec_per_fat;
	unsigned short flags;
	unsigned short version;
	unsigned int root_cluster;
	unsigned short fsinfo_cluster, backup_cluster;
	unsigned char res_1[12];
	fat_16_extbpb fat16;
} fat_32_extbpb;

typedef struct {
	char jmp_code[3];
	char oem[8];
	unsigned short bytes_per_sector;
	unsigned char sec_per_cluster;
	unsigned short res_sectors;
	unsigned char num_fats;
	unsigned short num_dirent;
	unsigned short total_sectors;
	unsigned char mdt;
	unsigned short sec_per_fat;
	unsigned short sec_per_track;
	unsigned short num_heads;
	unsigned int hidden_sec;
	unsigned int large_num_sec;
} bpb_t;

typedef struct {
	unsigned char name[8];
	unsigned char ext[3];
	unsigned char attr;
	unsigned char res1;
	unsigned char created;
	unsigned short create_time;
	unsigned short create_date;
	unsigned short acc_date;
	unsigned short start_high;
	unsigned short mod_time;
	unsigned short mod_date;
	unsigned short start_low;
	unsigned int size;
} fat_dirent;

#define FAT12 1
#define FAT16 2
#define FAT32 3

typedef struct {
	int vid;
	bpb_t *bpb;
	char fat_type;
	fat_32_extbpb *ext32;
	fat_16_extbpb *ext16;
	int dev, block;
	
	unsigned root_dir_sec, num_data_sec, first_data_sec;
	
} fat_volume;

#define MAX_VOLS 16

#define NUM_CLUSTERS(v) (v->num_data_sec / v->bpb->sec_per_cluster)

static inline char fat_type(int num) {
	if(num<4085)
	{
		return FAT12;
	} else
	{
		if(num < 65525)
			return FAT16;
		else
			return FAT32;
	}
	return 0;
}

#define TOTAL_SECTORS(v) (v->bpb->total_sectors ? v->bpb->total_sectors : v->bpb->large_num_sec)


#endif
