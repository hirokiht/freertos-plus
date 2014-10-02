#include <string.h>
#include <FreeRTOS.h>
#include <semphr.h>
#include <unistd.h>
#include "fio.h"
#include "filesystem.h"
#include "romfs.h"
#include "clib.h"

struct romfs_meta_t{
	char magic[8];		//must be "-rom1fs-"
	size_t full_size;	//number of bytes accessible by this fs
	uint32_t checksum; 	//checksum of first 512bytes
	char volume_name[];	//padded to 16byte boundary, file headers after that
};


struct romfs_fds_t {
	const struct romfh * meta;
    const uint8_t * file;	//points to fileheaders
    uint32_t cursor;
    off_t size;
};

static struct romfs_fds_t  romfs_fds[MAX_FDS];

static uint32_t get_unaligned(const uint8_t * d) {
    return ((uint32_t) d[3]) | ((uint32_t) (d[2] << 8)) | ((uint32_t) (d[1] << 16)) | ((uint32_t) (d[0] << 24));
}

static ssize_t romfs_read(void * opaque, void * buf, size_t count) {
    struct romfs_fds_t * fd = (struct romfs_fds_t *) opaque;
    
    if ((fd->cursor + count) > fd->size)
        count = fd->size - fd->cursor;

    memcpy(buf, fd->file + fd->cursor, count);
    fd->cursor += count;

    return count;
}

static off_t romfs_seek(void * opaque, off_t offset, int whence) {
    struct romfs_fds_t * fd = (struct romfs_fds_t *) opaque;
    
    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        offset += fd->cursor;
        break;
    case SEEK_END:
        offset = fd->size;
        break;
    default:
        return -1;
    }
    if (offset < 0)
        return -1;

    fd->cursor = offset>fd->size? fd->size : offset;
    return (off_t) fd->cursor;
}

const struct romfh * romfs_get_file(const struct romfs_meta_t * romfs, const char * path, const uint8_t ** file, off_t * len){
	uint8_t * fh = (uint8_t *) romfs->volume_name;
	struct romfh ** fhp = (struct romfh **) &fh;
	for( ; *(fh+15) ; fh += 16);	//iterate to last of volume name
	fh += 16;		//iterate to fileheaders
	char *p = (char *) path, *tok = strchr(p,'/');
	for( ; p ; p = tok+1, tok = strchr(p,'/')){
		while((strlen((*fhp)->filename) != (tok? tok-p : strlen(p))) && strncmp(p,(*fhp)->filename,strlen((*fhp)->filename))){
//			fio_printf(1,"add: %x, filename: %s \r\n",fh-(uint8_t*)romfs, (*fhp)->filename);
			if((*fhp)->nextfh&0xF0FFFFFF)	//there's next node
				fh = (uint8_t *)romfs+(get_unaligned((uint8_t *)&(*fhp)->nextfh)&~0xF);
			else return NULL;	//last time
		}
//		fio_printf(1,"offset: %x, filename: %s, type: %d, size: %d \r\n",fh-(uint8_t*)romfs, (*fhp)->filename, ((*fhp)->nextfh&0x7000000) >> 24, get_unaligned((uint8_t *)&(*fhp)->size));
		if((((*fhp)->nextfh&0x7000000) >> 24) == ROMFH_REG)
			if(tok)	//partial match
				return NULL;
			else break;
		else if((((*fhp)->nextfh&0x7000000) >> 24) == ROMFH_DIR || ((*fhp)->nextfh&7) == ROMFH_HRD){
//			fio_printf(1,"enter dir: %s, add: %x \r\n", (*fhp)->filename, get_unaligned((uint8_t *)&(*fhp)->spec));
			fh = (uint8_t *) romfs+get_unaligned((uint8_t *)&(*fhp)->spec);
		}else if((((*fhp)->nextfh&0x7000000) >> 24) == ROMFH_LNK){	//untested code
//			size_t namelen = strlen(tok);
//			namelen += 16-(namelen&15);
//			fh = (struct romfh *) romfs_get_file(romfs, fh->filename+namelen, NULL);
//to be implemented
			return NULL;
		}else return NULL;
	}
	if(tok)
		return NULL;	//partial match on path
	*len = get_unaligned((uint8_t *)&(*fhp)->size);
//	fio_printf(1,"offset: %x, filename: %s, type: %d, size: %d \r\n",fh-(uint8_t*)romfs, (*fhp)->filename, ((*fhp)->nextfh&0x7000000) >> 24, get_unaligned((uint8_t *)&(*fhp)->size));
	size_t namelen = strlen(tok);
	namelen += 16-(namelen&15);
	*file = (const uint8_t *) (*fhp)->filename+namelen;
//	fio_printf(1,"content: %s \r\n",*file);
	return *fhp;
}

static int romfs_open(void * opaque, const char * path, int flags, int mode) {
    const struct romfs_meta_t * romfs = (const struct romfs_meta_t *) opaque;
    if(strcmp(romfs->magic,"-rom1fs-"))
    	return -1;	//not romfs
    const uint8_t * file;
    off_t size = -1;
    int r = -1;

    const struct romfh * fh = romfs_get_file(romfs, path, &file, &size);

    if (fh) {
        r = fio_open(romfs_read, NULL, romfs_seek, NULL, NULL);
        if (r > 0) {
        	romfs_fds[r].meta = fh;
            romfs_fds[r].file = file;
            romfs_fds[r].cursor = 0;
            romfs_fds[r].size = size;
            fio_set_opaque(r, &romfs_fds[r]);
        }
    }
    return r;
}

void register_romfs(const char * mountpoint, const uint8_t * romfs) {
//    DBGOUT("Registering romfs `%s' @ %p\r\n", mountpoint, romfs);
    register_fs(mountpoint, romfs_open, (void *) romfs);
}
