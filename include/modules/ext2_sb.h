#ifndef _MODULE_EXT2_SB_H
#define _MODULE_EXT2_SB_H
#include <sea/config.h>
#if CONFIG_MODULE_EXT2

#include <sea/types.h>
typedef struct ext2_superblock_t {
	/// Anzahl der Inodes
	uint32_t inode_count;
	
	/// Anzahl der Bloecke
	uint32_t block_count;
	
	/// Anzahl der reservierten Bloecke
	uint32_t reserved_blocks;
	
	/// Anzahl der freien Bloecke
	uint32_t free_blocks;
	
	/// Anzahl der freien Inodes
	uint32_t free_inodes;
	
	/// Erster Datenblock
	uint32_t first_data_block;
	
	/// Blockgroesse (Achtung dieser Wert ist log2(Blockgroesse / 1024) und
	/// nicht die effektive blockgroesse)
	uint32_t block_size;
	
	/// Fragmentgroesse (???) Hier gilt das selbe wie oben
	int32_t fragment_size;
	
	/// Anzahl der Bloecke pro Gruppe
	uint32_t blocks_per_group;
	
	/// Anzahl der Fragmente pro Gruppe
	uint32_t fragments_per_group;
	
	/// Anzahl der Inodes pro Gruppe
	uint32_t inodes_per_group;
	
	/// Zeit an der das Dateisystem gemountet wurde
	uint32_t mount_time;
	
	/// Zeit an der auf das Dateisystem geschrieben wurde
	uint32_t write_time;
	
	/// Anzahl der Mounts
	uint16_t mount_count;
	
	/// Anzahl der maximalen Mounts
	int16_t max_mount_count;
	
	/// Magic zum Erkennen
	uint16_t magic;
	
	/// Status des Dateisystems
	uint16_t state;
	
	/// Behandlung von Fehlern
	uint16_t errors;
	
	/// Minor Revision Level
	uint16_t minor_revision;
	
	/// Zeit des letzten Checks
	uint32_t last_check_time;
	
	/// Maximaler abstand zwischen Checks
	uint32_t check_interval;
	
	/// Betriebsystem das es erstellt hat
	uint32_t creator_os;
	
	/// Revision des Dateisystems
	uint32_t revision;
	
	/// Standard UID fuer reservierte Bloecke
	uint16_t default_res_uid;
	
	/// Standard GID fuer reservierte Bloecke
	uint16_t default_res_gid;
	
	
	// Diese Felder sind ab Revision 1 vorhanden
	/// Erster benutzbarer Inode
	uint32_t first_inode;
	
	/// Inode groesse
	uint16_t inode_size;
	
	/// Blockgruppennummer in der dieser Superblock liegt
	uint16_t blockgroup_num;
	
	/// Features mit denen die volle kompatibilitaet auch vorhanden ist, wenn
	/// sie nicht unterstutzt werden
	uint32_t features_opt;
	
	/// Features die dafuer sorgen, dass das Dateisytem werder gelesen noch
	/// beschrieben werden kann.
	uint32_t features_req;
	
	/// Features mit denen nur Lesen moeglich ist wenn sie nicht unterstuetzt
	/// werden
	uint32_t features_ro;
	
	/// Volume-Id
	uint32_t volume_id[4];
	
	/// Volume-Name (Nullterminiert(
	char volume_name[16];
	
	/// Pfad an den das Dateisystem zuletzt gemountet wurde. (Nullterminiert)
	char last_mounted[64];
	
	/// Wird bei komprimierten Datesystem benutzt, um den
	/// Kompressionsalorithmus festzustellen
	uint32_t algo_bitmap;
	
	// Reserviert
	uint32_t reserved[205];
} __attribute__((packed)) ext2_superblock_t;

// Magic-Number eines EXT2-Dateisystems
#define EXT2_SB_MAGIC 0xEF53

/// Sparse-Super Feature
#define EXT2_FEATURE_ROCOMP_SPARSE_SUPER 0x1

/// Typeeintrag in Verzeichniseintraegen
#define EXT2_FEATURE_INCOMP_FILETYPE 0x2

/**
 * Errechnet die Blockgroesse auf einem Dateisystem
 *
 * @param sb Pointer auf den Superblock
 *
 * @return Blockgroesse in Bytes
 */
static inline size_t ext2_sb_blocksize(ext2_superblock_t* sb) {
	return 1024 << sb->block_size;
}

/**
 * Errechnet die Anzahl der Blockgruppen auf einem Dateisystem
 *
 * @param sb Pointer auf den Superblock
 *
 * @return Blockgroesse in Bytes
 */
static inline size_t ext2_sb_bgcount(ext2_superblock_t* sb) {
	return (sb->block_count - sb->first_data_block +
	(sb->blocks_per_group - 1)) / sb->blocks_per_group;
}

/**
 * Errechnet die Blockgroesse auf einem Dateisystem
 *
 * @param sb Pointer auf den Superblock
 *
 * @return Blockgroesse in Bytes
 */
static inline size_t ext2_sb_inodesize(ext2_superblock_t* sb) {
	// Das Inode-Size Feld ist nur gueltig, wenn die Revision 1 ist
	if (sb->revision == 0) {
		return 128;
	} else {
		return sb->inode_size;
	}
}
#endif
#endif
