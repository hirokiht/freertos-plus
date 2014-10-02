#ifndef __ROMFS_H__
#define __ROMFS_H__

#include <stdint.h>

#define ROMFS_MAXFN 128
#define ROMFH_HRD 0
#define ROMFH_DIR 1
#define ROMFH_REG 2
#define ROMFH_LNK 3
#define ROMFH_BLK 4
#define ROMFH_CHR 5
#define ROMFH_SCK 6
#define ROMFH_FIF 7
#define ROMFH_EXEC 8

struct romfh {
	uint32_t nextfh;	//bit[0:2] stores mode, bit[3] marks executable, use &0xFFFFFFF0 to get real nextfh
	uint32_t spec;
	off_t size;
	uint32_t checksum;
	char filename[];	//padded to 16byte boundary, data after that
};
//reference: https://www.kernel.org/doc/Documentation/filesystems/romfs.txt

void register_romfs(const char * mountpoint, const uint8_t * romfs);
//const struct romfh * romfs_get_file(const struct romfs_meta_t * romfs,  const char * path, const uint8_t ** file, off_t * len);

#endif

