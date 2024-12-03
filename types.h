#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128    

/**
 * 错误处理
 */
#define NO_ERROR 0
#define IO_ERROR EIO
#define NO_SAPCE_ERROR ENOSPC
#define EXIST_ERROR EEXIST
#define MKDIR_ON_FILE_ERROR ENXIO
#define MKNODE_ON_FILE_ERROR ENXIO
#define NOT_FOUND_ERROR ENOENT
#define ACCESS_ERROR EACCES
#define OPERATE_DIR_ON_FILE_ERROR EISDIR
#define SEEK_ERROR ESPIPE
#define DELETE_ROOT_ERROR EINVAL

/**
 * 位置计算方法相关
 */
#define FIND_DOWN_EDGE(value, round)    ((value) % (round) == 0 ? (value) : ((value) / (round)) * (round))
#define FIND_UP_EDGE(value, round)      ((value) % (round) == 0 ? (value) : ((value) / (round) + 1) * (round))
#define INO_OFFSET(ino) (super.inode_part_offset + ino * BLKS_SZ(INODE_PER_FILE))
#define DATA_OFFSET(ino) (super.data_part_offset + ino * BLKS_SZ(DATA_PART_PER_FILE))

/**
 * 磁盘属性判断、获取方法相关
 */
#define IS_DIR(node)  (node->related_dentry->filetype == DIR)
#define IS_FILE(node)  (node->related_dentry->filetype == Normal_FILE)
#define IO_SZ()  (super.sz_io)
#define BLOCK_SIZE() (super.sz_block)
#define DISK_SIZE() (super.sz_disk)
#define BLKS_SZ(blks)  ((blks) * BLOCK_SIZE())
#define fd() (super.fd)

/**
 * 磁盘属性定义相关
 */
#define SUPER_OFFSET 0
#define DATA_PART_PER_FILE 6
#define INODE_PER_FILE 1
#define ROOT_INO 0
#define MAX_DENTRY_D_PER_BLOCK (BLOCK_SIZE() / sizeof(struct newfs_dentry_d))

/**
 * 其它
 */
#define TRUE 1
#define FALSE 0
#define boolean int
#define BITS_IN_A_BYTE 0x8 

struct custom_options {
	const char*        device;
};
typedef enum sfs_file_type {
    Normal_FILE,
    DIR,
}FILE_TYPE;


struct newfs_super;
struct newfs_inode;
struct newfs_dentry;

struct newfs_super {
    uint32_t magic;
    int      fd;
    
    int sz_io;
    int sz_disk;
    int sz_block;

    int ino_max;  // 最大支持inode数
    int file_max;  //  一个文件最多占用几个逻辑块

    uint8_t* map_inode;  // 索引位图的位置
    uint32_t map_inode_blks;  // 索引位图的占用字节数
    uint32_t map_inode_offset;  // 索引位图的偏移量

    uint8_t* map_data;  // 数据位图的位置
    uint32_t map_data_blks;  // 数据位图的占用字节数
    uint32_t map_data_offset;  // 数据位图的偏移量

    uint32_t inode_part_blks;  // 索引块的占用字节数
    uint32_t inode_part_offset;  // 索引块的偏移量
    
    uint32_t data_part_blks;  // 数据块的占用字节数
    uint32_t data_part_offset;  // 数据块的偏移量

    boolean is_mounted; // 是否被挂载
    int sz_usage; // 用了多少

    struct newfs_dentry* root_dentry;  // 根目录

};

struct newfs_inode {
    uint32_t ino;

    int size; // 文件占用空间
    int dir_cnt;  // 目录项个数
    struct newfs_dentry* related_dentry;  // 关联的文件
    struct newfs_dentry* first_child;  // 包含的所有目录项
    uint8_t* data_pointer;  // 指向数据块的指针
    int data_blks_cnt; // 用了几个数据块
};

struct newfs_dentry {
    char     fname[MAX_NAME_LEN];
    uint32_t ino;

    struct newfs_dentry* parent; 
    struct newfs_dentry* brother;
    struct newfs_inode* related_inode;
    FILE_TYPE filetype;
};

struct newfs_dentry* new_dentry(char* fname,FILE_TYPE ftype){
    struct newfs_dentry* dentry = (struct newfs_dentry*) malloc (sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    dentry->brother = NULL;
    dentry->parent = NULL;
    dentry->filetype = ftype;
    dentry->ino = -1;
    dentry->related_inode = NULL;
    memcpy(dentry->fname,fname,strlen(fname));
    return dentry;
}

struct newfs_super_d{
    uint32_t magic_num;
    uint32_t sz_usage;

    int ino_max;  
    int file_max;  

    uint32_t map_inode_blks;  
    uint32_t map_inode_offset;   
    uint32_t map_data_blks; 
    uint32_t map_data_offset;    
    uint32_t inode_part_blks;  
    uint32_t inode_part_offset;   
    uint32_t data_part_blks; 
    uint32_t data_part_offset;  
};

struct newfs_inode_d{
    uint32_t ino;
    int size;
    int dir_cnt;
    int data_blks_cnt;
    FILE_TYPE ftype;
};

struct newfs_dentry_d{
    char fname[MAX_NAME_LEN];
    FILE_TYPE ftype;
    uint32_t ino;
};

#endif /* _TYPES_H_ */
