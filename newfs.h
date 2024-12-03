#ifndef _NEWFS_H_
#define _NEWFS_H_

#define FUSE_USE_VERSION 26
#include "stdio.h"
#include "stdlib.h"
#include <unistd.h>
#include "fcntl.h"
#include "string.h"
#include "fuse.h"
#include <stddef.h>
#include "ddriver.h"
#include "errno.h"
#include "types.h"
#include "stdint.h"

#define NEWFS_MAGIC       0x12345678     
#define NEWFS_DEFAULT_PERM    0777   /* 全权限打开 */

/******************************************************************************
* SECTION: newfs.c
*******************************************************************************/
void* 			   newfs_init(struct fuse_conn_info *);
void  			   newfs_destroy(void *);
int   			   newfs_mkdir(const char *, mode_t);
int   			   newfs_getattr(const char *, struct stat *);
int   			   newfs_readdir(const char *, void *, fuse_fill_dir_t, off_t,
						                struct fuse_file_info *);
int   			   newfs_mknod(const char *, mode_t, dev_t);
int   			   newfs_write(const char *, const char *, size_t, off_t,
					                  struct fuse_file_info *);
int   			   newfs_read(const char *, char *, size_t, off_t,
					                 struct fuse_file_info *);
int   			   newfs_access(const char *, int);
int   			   newfs_unlink(const char *);
int   			   newfs_rmdir(const char *);
int   			   newfs_rename(const char *, const char *);
int   			   newfs_utimens(const char *, const struct timespec tv[2]);
int   			   newfs_truncate(const char *, off_t);
			
int   			   newfs_open(const char *, struct fuse_file_info *);
int   			   newfs_opendir(const char *, struct fuse_file_info *);

/******************************************************************************
* SECTION: 工具函数
*******************************************************************************/
int                   write_to_disk(int, uint8_t *, int);
int                   read_from_disk(int, uint8_t *, int);
struct newfs_inode*   read_inode_from_disk(struct newfs_dentry* ,int);
int                   insert_dentry_to_inode(struct newfs_inode* , struct newfs_dentry* );
int                   write_inode_to_disk(struct newfs_inode* );
struct newfs_inode*   alloc_free_inode(struct newfs_dentry* );
int                   cal_path_level(const char* );
struct newfs_dentry*  lookup(const char* , boolean* , boolean* );
char*                 get_fname(const char* );
struct newfs_dentry*  get_ith_son_dentry(struct newfs_inode* ,int );
void                  modify_data_map(struct newfs_inode* );
void                  clear_data_map(struct newfs_inode* );
int                   drop_inode(struct newfs_inode* );
int                   drop_dentry(struct newfs_inode* ,struct newfs_dentry* );

#endif  /* _newfs_H_ */
