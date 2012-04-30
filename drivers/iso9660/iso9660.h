#ifndef ISO9660_H
#define ISO9660_h
#define MAX_ISO 16

typedef struct {
	unsigned char type;
	char id[5];
	char version;
	/* The rest is data */
} voldesc_t;

typedef struct
{
	voldesc_t desc;
	char data[2041];
} iso_pvd_t_blank;

struct Iso9660Timestamp
{
  uint8_t Year[4];
  uint8_t Month[2];
  uint8_t Day[2];
  uint8_t Hour[2];
  uint8_t Minute[2];
  uint8_t Second[2];
  uint8_t CentiSeconds[2];
  uint8_t Offset;
} __attribute__((packed));

struct Iso9660DirTimestamp
{
  uint8_t Year;
  uint8_t Month;
  uint8_t Day;
  uint8_t Hour;
  uint8_t Minute;
  uint8_t Second;
  uint8_t Offset;
} __attribute__((packed));

struct iso9660pvd
{
  voldesc_t Header;

  uint8_t   Unused1;

  uint8_t   SysIdent[32];
  uint8_t   VolIdent[32];

  uint8_t   Unused2[8];

  uint32_t  VolSpaceSize_LE; // little-endian
  uint32_t  VolSpaceSize_BE; // big-endian

  uint8_t   Unused3_EscSequences[32]; // Supplementary descriptors use this field

  uint16_t  VolSetSize_LE;
  uint16_t  VolSetSize_BE;

  uint16_t  VolSeqNum_LE;
  uint16_t  VolSeqNum_BE;

  uint16_t  LogicalBlockSize_LE;
  uint16_t  LogicalBlockSize_BE;

  uint32_t  PathTableSize_LE;
  uint32_t  PathTableSize_BE;

  uint32_t  TypeLPathTableOccurence;
  uint32_t  TypeLPathTableOptionOccurence;
  uint32_t  TypeMPathTableOccurence;
  uint32_t  TypeMPathTableOptionOccurence;

  uint8_t   RootDirRecord[34];

  uint8_t   VolSetIdent[128];
  uint8_t   PublisherIdent[128];
  uint8_t   DataPreparerIdent[128];
  uint8_t   ApplicationIdent[128];
  uint8_t   CopyrightFileIdent[37];
  uint8_t   AbstractFileIdent[37];
  uint8_t   BiblioFileIdent[37];

  struct Iso9660Timestamp VolumeCreationTime;
  struct Iso9660Timestamp VolumeModificationTime;
  struct Iso9660Timestamp VolumeExpiryTime;
  struct Iso9660Timestamp VolumeEffectiveTime;

  uint8_t FileStructVersion;

  uint8_t Rsvd1;

  uint8_t ApplicationUse[512];

  uint8_t Rsvd2[653];
} __attribute__((packed));

struct iso9660DirRecord
{
  unsigned char RecLen;

  uint8_t ExtAttrRecordLen;

  uint32_t ExtentLocation_LE;
  uint32_t ExtentLocation_BE;

  uint32_t DataLen_LE;
  uint32_t DataLen_BE;

  struct Iso9660DirTimestamp Time;

  uint8_t FileFlags;
  uint8_t FileUnitSize;
  uint8_t InterleaveGapSize;

  uint16_t VolSeqNum_LE;
  uint16_t VolSeqNum_BE;

  uint8_t FileIdentLen;
  uint8_t FileIdent[];
} __attribute__((packed));

typedef struct iso_vol_data {
	int flag;
	unsigned long long dev, block;
	char node[16];
	struct iso9660pvd *pvd;
	struct inode *root;
	struct iso_vol_data *next;
} iso_fs_t;

struct iso9660DirRecord *get_root_dir(iso_fs_t *fs);
int iso9660_read_file(iso_fs_t *fs, struct iso9660DirRecord *file, char *buffer, int offset, int length);
int search_dir_rec(iso_fs_t *fs, struct iso9660DirRecord *dir, char *name, struct iso9660DirRecord *ret);
int iso_read_block(iso_fs_t *fs, unsigned block, unsigned char *buf);
int iso_read_off(iso_fs_t *fs, unsigned off, unsigned char *buf, unsigned len);
int read_dir_rec(iso_fs_t *fs, struct iso9660DirRecord *dir, int n, struct iso9660DirRecord *ret, char *name);
#endif
