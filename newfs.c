#define _XOPEN_SOURCE 700

#include "newfs.h"

/******************************************************************************
* SECTION: 宏定义
*******************************************************************************/
#define OPTION(t, p)        { t, offsetof(struct custom_options, p), 1 }

/******************************************************************************
* SECTION: 全局变量
*******************************************************************************/
static const struct fuse_opt option_spec[] = {		/* 用于FUSE文件系统解析参数 */
	OPTION("--device=%s", device),
	FUSE_OPT_END
};

struct custom_options newfs_options;			 /* 全局选项 */
struct newfs_super super; 
/******************************************************************************
* SECTION: FUSE操作定义
*******************************************************************************/
static struct fuse_operations operations = {
	.init = newfs_init,						 /* mount文件系统 */		
	.destroy = newfs_destroy,				 /* umount文件系统 */
	.mkdir = newfs_mkdir,					 /* 建目录，mkdir */
	.getattr = newfs_getattr,				 /* 获取文件属性，类似stat，必须完成 */
	.readdir = newfs_readdir,				 /* 填充dentrys */
	.mknod = newfs_mknod,					 /* 创建文件，touch相关 */
	.write = newfs_write,								  	 /* 写入文件 */
	.read = newfs_read,								  	 /* 读文件 */
	.utimens = newfs_utimens,				 /* 修改时间，忽略，避免touch报错 */
	.truncate = newfs_truncate,						  		 /* 改变文件大小 */
	.unlink = newfs_unlink,							  		 /* 删除文件 */
	.rmdir	= newfs_rmdir,							  		 /* 删除目录， rm -r */
	.rename = newfs_rename,							  		 /* 重命名，mv */

	.open = newfs_open,							
	.opendir = newfs_opendir,
	.access = newfs_access
};
/******************************************************************************
* SECTION: 必做函数实现
*******************************************************************************/
/**
 * @brief 挂载（mount）文件系统
 * 
 * @param conn_info 可忽略，一些建立连接相关的信息 
 * @return void*
 */
void* newfs_init(struct fuse_conn_info * conn_info) {
	/* TODO: 在这里进行挂载 */
	struct newfs_super_d super_d;
	struct newfs_dentry* root_dentry;
	struct newfs_inode* root_inode;
	boolean is_init = FALSE;

	int inode_num;
	int data_num;
	int super_blks;


	/* 下面是一个控制设备的示例 */
	super.fd = ddriver_open(newfs_options.device);
	super.is_mounted = FALSE;
	
	ddriver_ioctl(super.fd, IOC_REQ_DEVICE_SIZE,  &super.sz_disk);
    ddriver_ioctl(super.fd, IOC_REQ_DEVICE_IO_SZ, &super.sz_io);
	super.sz_block = 2 * super.sz_io;
	root_dentry = new_dentry("/", DIR);

	if(read_from_disk(SUPER_OFFSET,(uint8_t* )(&super_d),sizeof(struct newfs_super_d)) != NO_ERROR){ 
		return NULL;    
	}

	if(super_d.magic_num != NEWFS_MAGIC){
		super_blks = 1;
		inode_num = DISK_SIZE() / ((DATA_PART_PER_FILE + INODE_PER_FILE) * BLOCK_SIZE());
		data_num = DISK_SIZE() / BLOCK_SIZE() - inode_num - 3;

		super_d.ino_max = inode_num;
		super_d.file_max = DATA_PART_PER_FILE;
		super_d.map_inode_blks = 1;
		super_d.map_inode_offset = SUPER_OFFSET + BLKS_SZ(super_blks);
		super_d.map_data_blks = 1;
		super_d.map_data_offset = super_d.map_inode_offset + BLKS_SZ(super_d.map_inode_blks);
		super_d.inode_part_blks = inode_num;
		super_d.inode_part_offset = super_d.map_data_offset + BLKS_SZ(super_d.map_data_blks);
		super_d.data_part_blks = data_num;
		super_d.data_part_offset = super_d.inode_part_offset + BLKS_SZ(super_d.inode_part_blks);

		super_d.sz_usage = 0;
		is_init = TRUE;
	}
	super.file_max = super_d.file_max;
	super.ino_max = super_d.ino_max;
	super.sz_usage = super_d.sz_usage;
	super.map_inode_blks = super_d.map_inode_blks;
	super.map_inode_offset = super_d.map_inode_offset;
	super.map_data_blks = super_d.map_data_blks;
	super.map_data_offset = super_d.map_data_offset;
	super.inode_part_blks = super_d.inode_part_blks;
	super.inode_part_offset = super_d.inode_part_offset;
	super.data_part_blks = super_d.data_part_blks;
	super.data_part_offset = super_d.data_part_offset;

	super.map_inode = (uint8_t*) malloc (BLKS_SZ(super.map_inode_blks));
	super.map_data = (uint8_t*) malloc (BLKS_SZ(super.map_data_blks));
	
	if(read_from_disk(super.map_inode_offset,(uint8_t*) super.map_inode,BLKS_SZ(super.map_inode_blks)) != NO_ERROR){
		return NULL;
	}
	if(read_from_disk(super.map_data_offset,(uint8_t*) super.map_data,BLKS_SZ(super.map_data_blks)) != NO_ERROR){
		return NULL;
	}

	if(is_init){
		root_inode = alloc_free_inode(root_dentry);
		write_inode_to_disk(root_inode);
	}

	root_inode = read_inode_from_disk(root_dentry,ROOT_INO);
	super.root_dentry = root_dentry;
	super.is_mounted = TRUE;

	return NULL;
}

/**
 * @brief 卸载（umount）文件系统
 * 
 * @param p 可忽略
 * @return void
 */
void newfs_destroy(void* p) {
	/* TODO: 在这里进行卸载 */
	struct newfs_super_d super_d;
	if(!super.is_mounted) {
		return;
	}

	write_inode_to_disk(super.root_dentry->related_inode);  // 数据写回磁盘（索引区和数据区）

	super_d.magic_num = NEWFS_MAGIC;
	super_d.file_max = super.file_max;
	super_d.ino_max = super.ino_max;
	super_d.map_inode_blks = super.map_inode_blks;
	super_d.map_inode_offset = super.map_inode_offset;
	super_d.map_data_blks = super.map_data_blks;
	super_d.map_data_offset = super.map_data_offset;
	super_d.inode_part_blks = super.inode_part_blks;
	super_d.inode_part_offset = super.inode_part_offset;
	super_d.data_part_blks = super.data_part_blks;
	super_d.data_part_offset = super.data_part_offset;
	super_d.sz_usage = super.sz_usage;

	if(write_to_disk(SUPER_OFFSET,(uint8_t*)&super_d,sizeof(struct newfs_super_d)) != NO_ERROR){
		return;
	}
	if(write_to_disk(super.map_inode_offset,(uint8_t*)super.map_inode,BLKS_SZ(super.map_inode_blks)) != NO_ERROR){
        return;
    }
    if(write_to_disk(super.map_data_offset,(uint8_t*)super.map_data,BLKS_SZ(super.map_data_blks)) != NO_ERROR){
        return;
    }

	free(super.map_inode);
	free(super.map_data);

	ddriver_close(super.fd);

	return;
}

/**
 * @brief 创建目录
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建模式（只读？只写？），可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_mkdir(const char* path, mode_t mode) {
	/* TODO: 解析路径，创建目录 */
	boolean is_find,is_root;
	struct newfs_dentry* father_dentry = lookup(path,&is_find,&is_root);
	struct newfs_dentry* dentry;
	struct newfs_inode* inode;

	if(is_find){
		return -EXIST_ERROR;
	}
	if(IS_FILE(father_dentry->related_inode)){
		return -MKDIR_ON_FILE_ERROR;
	}

	char* fname = get_fname(path);
	dentry = new_dentry(fname,DIR);
	inode = alloc_free_inode(dentry);
	insert_dentry_to_inode(father_dentry->related_inode,dentry);

	return NO_ERROR;
}

/**
 * @brief 获取文件或目录的属性，该函数非常重要
 * 
 * @param path 相对于挂载点的路径
 * @param newfs_stat 返回状态
 * @return int 0成功，否则返回对应错误号
 */
int newfs_getattr(const char* path, struct stat * newfs_stat) {
	/* TODO: 解析路径，获取Inode，填充newfs_stat，可参考/fs/simplefs/sfs.c的sfs_getattr()函数实现 */
	boolean is_find,is_root;
	struct newfs_dentry* dentry = lookup(path,&is_find,&is_root);

	if(is_find == FALSE){
		return -NOT_FOUND_ERROR;
	}
	if(IS_DIR(dentry->related_inode)){
		newfs_stat->st_mode = S_IFDIR | NEWFS_DEFAULT_PERM;
	}
	else if(IS_FILE(dentry->related_inode)){
		newfs_stat->st_mode = S_IFREG | NEWFS_DEFAULT_PERM;
	}

	newfs_stat->st_size = dentry->related_inode->size;
	newfs_stat->st_nlink = 1;
	newfs_stat->st_uid = getuid();
	newfs_stat->st_gid = getgid();
	newfs_stat->st_atime = time(NULL);
	newfs_stat->st_mtime = time(NULL);
	newfs_stat->st_blksize = BLOCK_SIZE();

	if(is_root){
		newfs_stat->st_size = super.sz_usage;
		newfs_stat->st_blocks = DISK_SIZE() / BLOCK_SIZE();
		newfs_stat->st_nlink = 2;
	}
	return NO_ERROR;
}

/**
 * @brief 遍历目录项，填充至buf，并交给FUSE输出
 * 
 * @param path 相对于挂载点的路径
 * @param buf 输出buffer
 * @param filler 参数讲解:
 * 
 * typedef int (*fuse_fill_dir_t) (void *buf, const char *name,
 *				const struct stat *stbuf, off_t off)
 * buf: name会被复制到buf中
 * name: dentry名字
 * stbuf: 文件状态，可忽略
 * off: 下一次offset从哪里开始，这里可以理解为第几个dentry
 * 
 * @param offset 第几个目录项？
 * @param fi 可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_readdir(const char * path, void * buf, fuse_fill_dir_t filler, off_t offset,
			    		 struct fuse_file_info * fi) {
    /* TODO: 解析路径，获取目录的Inode，并读取目录项，利用filler填充到buf，可参考/fs/simplefs/sfs.c的sfs_readdir()函数实现 */
	boolean is_find,is_root;
	int cur_dir = offset;

	struct newfs_dentry* dentry = lookup(path,&is_find,&is_root);
	struct newfs_dentry* dentry_cur;
	struct newfs_inode* inode;
	if(is_find){
		inode = dentry->related_inode;
		dentry_cur = get_ith_son_dentry(inode,cur_dir);
		if(dentry_cur){
			filler(buf,dentry_cur->fname,NULL,++offset);
		}
		return NO_ERROR;
	}
    return NOT_FOUND_ERROR;
}

/**
 * @brief 创建文件
 * 
 * @param path 相对于挂载点的路径
 * @param mode 创建文件的模式，可忽略
 * @param dev 设备类型，可忽略
 * @return int 0成功，否则返回对应错误号
 */
int newfs_mknod(const char* path, mode_t mode, dev_t dev) {
	/* TODO: 解析路径，并创建相应的文件 */
	boolean is_find,is_root;
	struct newfs_dentry* father_dentry = lookup(path,&is_find,&is_root);
	struct newfs_dentry* dentry;
	struct newfs_inode* inode;

	if(is_find){
		return -EXIST_ERROR;
	}
	if(IS_FILE(father_dentry->related_inode)){
		return -MKNODE_ON_FILE_ERROR;
	}

	char* fname = get_fname(path);
	dentry = new_dentry(fname,Normal_FILE);
	inode = alloc_free_inode(dentry);
	insert_dentry_to_inode(father_dentry->related_inode,dentry);
	return NO_ERROR;
}

/**
 * @brief 修改时间，为了不让touch报错 
 * 
 * @param path 相对于挂载点的路径
 * @param tv 实践
 * @return int 0成功，否则返回对应错误号
 */
int newfs_utimens(const char* path, const struct timespec tv[2]) {
	(void)path;
	return 0;
}
/******************************************************************************
* SECTION: 选做函数实现
*******************************************************************************/
/**
 * @brief 写入文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 写入的内容
 * @param size 写入的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 写入大小
 */
int newfs_write(const char* path, const char* buf, size_t size, off_t offset,
		        struct fuse_file_info* fi) {
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode* inode;
	int size_ret = size;

	if(is_find == FALSE){
		return -NOT_FOUND_ERROR;
	}

	inode = dentry->related_inode;
	if(IS_DIR(inode)){
		return -OPERATE_DIR_ON_FILE_ERROR;
	}
	if(inode->size < offset){   // 不允许不连续的书写
		return -SEEK_ERROR;
	}
	if(offset + size > BLKS_SZ(super.file_max)){
		size_ret = BLKS_SZ(super.file_max) - offset;
	}

	memcpy(inode->data_pointer + offset,buf,size_ret);
	inode->size = offset + size_ret > inode->size ? offset + size_ret : inode->size;
	modify_data_map(inode);
	return size_ret;
}

/**
 * @brief 读取文件
 * 
 * @param path 相对于挂载点的路径
 * @param buf 读取的内容
 * @param size 读取的字节数
 * @param offset 相对文件的偏移
 * @param fi 可忽略
 * @return int 读取大小
 */
int newfs_read(const char* path, char* buf, size_t size, off_t offset,
		       struct fuse_file_info* fi) {
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode* inode;
	int size_ret = size;

	if(is_find == FALSE){
		return -NOT_FOUND_ERROR;
	}

	inode = dentry->related_inode;
	if(IS_DIR(inode)){
		return -OPERATE_DIR_ON_FILE_ERROR;
	}
	if(inode->size < offset){   // 不允许读超出文件使用大小的部分
		return -SEEK_ERROR;
	}
	if(offset + size > BLKS_SZ(super.file_max)){
		size_ret = BLKS_SZ(super.file_max) - offset;
	}

	memcpy(buf,inode->data_pointer+offset,size_ret);
	return size_ret;			   
}

/**
 * @brief 删除文件
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_unlink(const char* path) {
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode* inode;

	if(is_find == FALSE){
		return -NOT_FOUND_ERROR;
	}

	inode = dentry->related_inode;
	drop_inode(inode);
	drop_dentry(dentry->parent->related_inode,dentry);
	return NO_ERROR;
}

/**
 * @brief 删除目录
 * 
 * 一个可能的删除目录操作如下：
 * rm ./tests/mnt/j/ -r
 *  1) Step 1. rm ./tests/mnt/j/j
 *  2) Step 2. rm ./tests/mnt/j
 * 即，先删除最深层的文件，再删除目录文件本身
 * 
 * @param path 相对于挂载点的路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_rmdir(const char* path) {
	return newfs_unlink(path);
}

/**
 * @brief 重命名文件 
 * 
 * @param from 源文件路径
 * @param to 目标文件路径
 * @return int 0成功，否则返回对应错误号
 */
int newfs_rename(const char* from, const char* to) {
	int ret = NO_ERROR;
	boolean	is_find, is_root;
	struct newfs_dentry* from_dentry = lookup(from, &is_find, &is_root);
	struct newfs_inode* from_inode;
	struct newfs_dentry* to_dentry;
	mode_t mode = 0;

	if(is_find == FALSE){
		return -NOT_FOUND_ERROR;
	}
	if(strcmp(from,to) == 0){
		return NO_ERROR;
	}

	from_inode = from_dentry->related_inode;
	if (IS_DIR(from_inode)) {
		mode = S_IFDIR;
	}
	else if (IS_FILE(from_inode)) {
		mode = S_IFREG;
	}

    ret = newfs_mknod(to,mode,NULL);
	if(ret != NO_ERROR){
		return ret;
	}

    // 为to新建文件，删除to原有的inode，并把to的inode改成from的inode，最后删除from
	to_dentry = lookup(to,&is_find,&is_root);
	drop_inode(to_dentry->related_inode);
	to_dentry->ino = from_dentry->ino;
	to_dentry->related_inode = from_inode;
	drop_dentry(from_dentry->parent->related_inode,from_dentry);

	return ret;
}

/**
 * @brief 打开文件，可以在这里维护fi的信息，例如，fi->fh可以理解为一个64位指针，可以把自己想保存的数据结构
 * 保存在fh中
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int newfs_open(const char* path, struct fuse_file_info* fi) {
	return NO_ERROR;
}

/**
 * @brief 打开目录文件
 * 
 * @param path 相对于挂载点的路径
 * @param fi 文件信息
 * @return int 0成功，否则返回对应错误号
 */
int newfs_opendir(const char* path, struct fuse_file_info* fi) {
	return NO_ERROR;
}

/**
 * @brief 改变文件大小
 * 
 * @param path 相对于挂载点的路径
 * @param offset 改变后文件大小
 * @return int 0成功，否则返回对应错误号
 */
int newfs_truncate(const char* path, off_t offset) {
	boolean	is_find, is_root;
	struct newfs_dentry* dentry = lookup(path, &is_find, &is_root);
	struct newfs_inode*  inode;

	if (is_find == FALSE) {
		return -NOT_FOUND_ERROR;
	}
	inode = dentry->related_inode;
	if (IS_DIR(inode)) {
		return OPERATE_DIR_ON_FILE_ERROR;
	}

	inode->size = offset;  // 改变文件的大小
	modify_data_map(inode);
	return NO_ERROR;
}

/**
 * @brief 访问文件，因为读写文件时需要查看权限
 * 
 * @param path 相对于挂载点的路径
 * @param type 访问类别
 * R_OK: Test for read permission. 
 * W_OK: Test for write permission.
 * X_OK: Test for execute permission.
 * F_OK: Test for existence. 
 * 
 * @return int 0成功，否则返回对应错误号
 */
int newfs_access(const char* path, int type) {
	// 即根据文件的存在性和权限判断
	boolean is_find,is_root;
	struct sfs_dentry* dentry = lookup(path, &is_find, &is_root);
	boolean is_access_ok = FALSE;
	switch (type)
	{
	case R_OK:
		is_access_ok = TRUE;
		break;
	case F_OK:
		if (is_find) {
			is_access_ok = TRUE;
		}
		break;
	case W_OK:
		is_access_ok = TRUE;
		break;
	case X_OK:
		is_access_ok = TRUE;
		break;
	default:
		break;
	}
	return is_access_ok ? NO_ERROR : -ACCESS_ERROR;
}	
/******************************************************************************
* SECTION: FUSE入口
*******************************************************************************/
int main(int argc, char **argv)
{
    int ret;
	struct fuse_args args = FUSE_ARGS_INIT(argc, argv);

	newfs_options.device = strdup("TODO: 这里填写你的ddriver设备路径");

	if (fuse_opt_parse(&args, &newfs_options, option_spec, NULL) == -1)
		return -1;
	
	ret = fuse_main(args.argc, args.argv, &operations, NULL);
	fuse_opt_free_args(&args);
	return ret;
}

/******************************************************************************
* SECTION: 辅助函数
*******************************************************************************/
/**
 * 驱动读
 */
int read_from_disk(int offset, uint8_t *out_content, int size){  // 先从磁盘里读取数据填充temp，再从temp中截取需要的数据返回
	int down = FIND_DOWN_EDGE(offset, IO_SZ());  //最近的IO块下界
    int bias = offset - down;
    int IO_size = FIND_UP_EDGE((size + bias), IO_SZ()); // 组合起来的IO上界
    uint8_t* temp_content = (uint8_t*)malloc(IO_size);
    uint8_t* cur = temp_content;
    ddriver_seek(fd(), down, SEEK_SET);
    while (IO_size != 0)
    {
        ddriver_read(fd(), cur, IO_SZ());
        cur += IO_SZ();
        IO_size -= IO_SZ();   
    }
    memcpy(out_content,temp_content + bias,size);
    free(temp_content);
    return NO_ERROR;
}

/**
 * 驱动写
 */
int write_to_disk(int offset, uint8_t *in_content, int size){   // 先从磁盘中读出temp，拷贝要写的内容到temp里，再把temp写回磁盘
	int down = FIND_DOWN_EDGE(offset, IO_SZ());  
    int bias = offset - down;
    int IO_size = FIND_UP_EDGE((size + bias), IO_SZ()); 
    uint8_t* temp_content = (uint8_t*)malloc(IO_size);
    uint8_t* cur = temp_content;
    read_from_disk(down,temp_content,IO_size);
	memcpy(temp_content + bias,in_content,size);
	ddriver_seek(fd(),down,SEEK_SET);
    while (IO_size != 0)
    {
        ddriver_write(fd(), cur, IO_SZ());
        cur += IO_SZ();
        IO_size -= IO_SZ();   
    }
    free(temp_content);
    return NO_ERROR;
}

/**
 * 函数作用：根据ino从磁盘中获取并构建inode
 * 根据ino从磁盘中读出inode_d，根据inode_d构建inode，把inode和dentry关联上。然后根据inode的类型：
 * 如果是目录项，则从数据区读出所有的dentry_d，根据dentry_d构建dentry_cur，并把dentry_cur作为inode的子dentry与inode关联起来
 */
struct newfs_inode* read_inode_from_disk(struct newfs_dentry* dentry,int ino){
	struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
	struct newfs_inode_d inode_d;
	struct newfs_dentry* dentry_cur;
	struct newfs_dentry_d dentry_d;
	int dir_cnt = 0,i;
	if(read_from_disk(INO_OFFSET(ino),(uint8_t*)&inode_d,sizeof(struct newfs_inode_d)) != NO_ERROR){
		return NULL;
	}
	inode->dir_cnt = 0;  // 后面才修改
	inode->data_blks_cnt = inode_d.data_blks_cnt;
	inode->ino = inode_d.ino;
	inode->size = inode_d.size;
	inode->related_dentry = dentry;
    dentry->related_inode = inode;
	inode->first_child = NULL;
	if(IS_DIR(inode)){
		dir_cnt = inode_d.dir_cnt;
		for(i = 0;i < dir_cnt;i++){
			if(read_from_disk(DATA_OFFSET(ino) + i * sizeof(struct newfs_dentry_d),(uint8_t*)&dentry_d,sizeof(struct newfs_dentry_d)) != NO_ERROR){
                return NULL;
			}
			dentry_cur = new_dentry(dentry_d.fname,dentry_d.ftype);
			dentry_cur->ino = dentry_d.ino;
			insert_dentry_to_inode(inode,dentry_cur);
		}
	}
	else if(IS_FILE(inode)){
		inode->data_pointer = (uint8_t *)malloc(BLKS_SZ(DATA_PART_PER_FILE));
		if(read_from_disk(DATA_OFFSET(ino),(uint8_t*)inode->data_pointer,BLKS_SZ(DATA_PART_PER_FILE)) != NO_ERROR){
		    return NULL;
		}
	}
	return inode;
}

/**
 * 将dentry作为inode的子目录之一与inode进行关联,头插
 */
int insert_dentry_to_inode(struct newfs_inode* inode, struct newfs_dentry* dentry){
	dentry->parent = inode->related_dentry;
	if(inode->first_child == NULL){
		inode->first_child = dentry;
	}
	else{
		dentry->brother = inode->first_child;
		inode->first_child = dentry;
	}
	inode->dir_cnt++;
	inode->size += sizeof(struct newfs_dentry_d);
	return NO_ERROR;
}

/**
 * 把inode写回磁盘。根据inode构建inode_d，将inode_d写回磁盘。
 * 如果是目录，则递归把子目录项对应的inode也写回磁盘
 * 如果是文件，则把数据写回磁盘
 */
int write_inode_to_disk(struct newfs_inode* inode){
	struct newfs_inode_d inode_d;
	struct newfs_dentry* dentry_cur;
	struct newfs_dentry_d dentry_d;
	int ino = inode->ino;
	inode_d.ino = ino;
	inode_d.size = inode->size;
	inode_d.ftype = inode->related_dentry->filetype;
	inode_d.dir_cnt = inode->dir_cnt;
	inode_d.data_blks_cnt = inode->data_blks_cnt;
	modify_data_map(inode);  // 写回时才构建数据位图
	if(write_to_disk(INO_OFFSET(ino),(uint8_t*)&inode_d,sizeof(struct newfs_inode_d)) != NO_ERROR){
		return IO_ERROR;
	}
	int offset = DATA_OFFSET(ino);
	if(IS_DIR(inode)){
		dentry_cur = inode->first_child;
		while(dentry_cur != NULL){
			memcpy(dentry_d.fname,dentry_cur->fname,MAX_NAME_LEN);
			dentry_d.ftype = dentry_cur->filetype;
			dentry_d.ino = dentry_cur->ino;
			if(write_to_disk(offset,(uint8_t*)&dentry_d,sizeof(struct newfs_dentry_d)) != NO_ERROR){
                return -IO_ERROR;
			}
			if(dentry_cur->related_inode != NULL){
				write_inode_to_disk(dentry_cur->related_inode);
			}
			dentry_cur = dentry_cur->brother;
			offset += sizeof(struct newfs_dentry_d);
		}
	}
	else if(IS_FILE(inode)){
		if(write_to_disk(offset,inode->data_pointer,BLKS_SZ(DATA_PART_PER_FILE)) != NO_ERROR){
			return -IO_ERROR;
		}
	}
	return NO_ERROR;
}

/**
 * 从位图中找一个没用过的inode，初始化相关数据并与dentry关联上。
 */
struct newfs_inode* alloc_free_inode(struct newfs_dentry* dentry){
	struct newfs_inode* inode;
	int byte_cur = 0;
	int bite_cur = 0;
	int ino_cur = 0;
	boolean find_free_inode = FALSE;
	for(byte_cur = 0;byte_cur < BLKS_SZ(super.map_inode_blks);byte_cur++){
		for(bite_cur = 0;bite_cur < BITS_IN_A_BYTE;bite_cur++,ino_cur++){
			if((super.map_inode[byte_cur] & (0x1 << bite_cur)) == 0){
				super.map_inode[byte_cur] |= (0x1 << bite_cur);  // 位图相关位置置1
				find_free_inode = TRUE;
				break;
			}
		}
		if(find_free_inode) break;
	}
	if(!find_free_inode || ino_cur >= super.ino_max){
		return NULL;
	} 
	inode = (struct newfs_inode*) malloc (sizeof(struct newfs_inode));
	inode->ino = ino_cur;
	inode->size = 0;
	inode->dir_cnt = 0;
	inode->data_blks_cnt = 0;
	inode->related_dentry = dentry;
	inode->data_pointer = NULL;
	dentry->related_inode = inode;
	dentry->ino = ino_cur;
	if(inode->related_dentry->filetype == Normal_FILE){
		inode->data_pointer = (uint8_t*) malloc (BLKS_SZ(DATA_PART_PER_FILE));
	}
	return inode;
}

/**
 * 计算路径的层级（按‘/’划分，只有根目录的话层级为0）
 */
int cal_path_level(const char* path){
	char* str_cur = (char *)malloc(strlen(path));
    strcpy(str_cur, path);
	int level = 0;
	if(strcmp(path,"/") == 0){
		return level;
	}
	while(*str_cur != NULL){
		if(*str_cur == '/'){
			level++;
		}
		str_cur++;
	}
	return level;
}

/**
 * 根据路径寻找相应的dentry，如果找到返回dentry，找不到父目录的dentry（上一个有效路径）
 * 从根目录找起
 */
struct newfs_dentry* lookup(const char * path, boolean* is_find, boolean* is_root){
	struct newfs_dentry* dentry_cur = super.root_dentry;
	struct newfs_dentry* dentry_ret = NULL;
	struct newfs_inode* inode;
	int total_level = cal_path_level(path); 
	int level = 0;
	boolean is_hit;
	char* fname = NULL;
	char* path_cpy = (char*) malloc (sizeof(path));
	*is_root = FALSE;
	strcpy(path_cpy,path);
	if(total_level == 0){  // 如果只找根目录
		*is_find = TRUE;
		*is_root = TRUE;
		dentry_ret = super.root_dentry;
	}
	fname = strtok(path_cpy,"/");  // 按'/'分割
	while(fname){	
		level++;
		if(dentry_cur->related_inode == NULL){  // 第一次读取dentry除根节点外的related_inode都是空的
			read_inode_from_disk(dentry_cur,dentry_cur->ino);
		}
		inode = dentry_cur->related_inode;
		if(IS_FILE(inode) && level < total_level){  // 是文件且层级不对的话，说明找的是错的，直接返回
		    (*is_find) = FALSE;
			dentry_ret = inode->related_dentry;
			break;
		}
		if(IS_DIR(inode)){
			dentry_cur = inode->first_child;
			is_hit = FALSE;
			while(dentry_cur){
				if(memcmp(dentry_cur->fname,fname,strlen(fname)) == 0){
					is_hit = TRUE;
					break;
				}
				dentry_cur = dentry_cur->brother;
			}
			if(!is_hit){   // 没命中返回父目录
				*is_find = FALSE;
				dentry_ret = inode->related_dentry;
				break;
			}
			if(is_hit && level == total_level){  // 命中返回当前目录项
				*is_find = TRUE;
				dentry_ret = dentry_cur;
				break;
			}
		}
		fname = strtok(NULL,"/");
	}
	if(dentry_ret->related_inode == NULL){
		dentry_ret->related_inode = read_inode_from_disk(dentry_ret,dentry_ret->ino);
	}
	return dentry_ret;
}

/**
 * 获取文件名(从路径的末位获取（最后一个‘/’后面）)
 */
char* get_fname(const char* path){
	char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * 找到并返回inode的第dir个子dentry
 */
struct newfs_dentry* get_ith_son_dentry(struct newfs_inode* inode,int dir){
	struct newfs_dentry* dentry_cur = inode->first_child;
	int cnt = 0;
	while(dentry_cur){
		if(dir == cnt){
			return dentry_cur;
		}
		cnt ++;
		dentry_cur = dentry_cur->brother;
	}
	return NULL;
}

/**
 * 修改数据位图
 */
void modify_data_map(struct newfs_inode* inode){
	int data_ino = inode->ino * DATA_PART_PER_FILE;
	if(IS_DIR(inode)){
		data_ino = data_ino + inode->data_blks_cnt - 1;
		while(inode->dir_cnt > inode->data_blks_cnt * MAX_DENTRY_D_PER_BLOCK){
			inode->data_blks_cnt++;
			data_ino++;
			super.map_data[data_ino / BITS_IN_A_BYTE] |= (0x1 << (data_ino % BITS_IN_A_BYTE));
		}
	}
	else if(IS_FILE(inode)){
		inode->data_blks_cnt = FIND_UP_EDGE(inode->size,BLOCK_SIZE()) / BLOCK_SIZE();
		for(int i=0;i<super.file_max;i++,data_ino++){
			if(i < inode->data_blks_cnt){
				super.map_data[data_ino / BITS_IN_A_BYTE] |= (0x1 << (data_ino % BITS_IN_A_BYTE));
			}
			else{
				super.map_data[data_ino / BITS_IN_A_BYTE] &= (uint8_t)(~(0x1 << (data_ino % BITS_IN_A_BYTE)));
			}
		}
	}
	return; 
}

/**
 * 根据ino清空数据位图
 */
void clear_data_map(struct newfs_inode* inode){
	int to_clear_ino = inode->ino * DATA_PART_PER_FILE;
	for(int i=0;i<BITS_IN_A_BYTE;i++,to_clear_ino++){
		super.map_data[to_clear_ino / BITS_IN_A_BYTE] &= (uint8_t)(~(0x1 << (to_clear_ino % BITS_IN_A_BYTE)));
	}
}

/**
 * 删除inode，若是目录项就迭代删除子目录项，若是文件就清空数据
 * 同时修改数据位图和索引位图
 * 最后用free删除
 */
int drop_inode(struct newfs_inode* inode){
	struct newfs_dentry* dentry_cur;
	struct newfs_inode* inode_cur;
	int byte_cur = 0;
	int bit_cur = 0;
	int ino_cur = 0;
	boolean is_find = FALSE;
	if(inode == super.root_dentry->related_inode){
		return -DELETE_ROOT_ERROR;
	}
	if(IS_DIR(inode)){
		dentry_cur = inode->first_child;
		clear_data_map(inode);
		while(dentry_cur){
			inode_cur = dentry_cur->related_inode;
			drop_inode(inode_cur);
			drop_dentry(inode,dentry_cur);
			dentry_cur = dentry_cur->brother;
		}
		for(byte_cur = 0;byte_cur < BLKS_SZ(super.map_inode_blks);byte_cur++){
			for(bit_cur = 0;bit_cur < BITS_IN_A_BYTE;bit_cur++){
				if(ino_cur == inode->ino){
					super.map_inode[byte_cur] &= (uint8_t)(~(0x1 << bit_cur));
					is_find = TRUE;
					break;
				}
				ino_cur++;
			}
			if(is_find == TRUE){
				break;
			}
		}
		free(inode);
	}
	else if(IS_FILE(inode)){
		clear_data_map(inode);
		for(byte_cur = 0;byte_cur < BLKS_SZ(super.map_inode_blks);byte_cur++){
			for(bit_cur = 0;bit_cur < BITS_IN_A_BYTE;bit_cur++){
				if(ino_cur == inode->ino){
					super.map_inode[byte_cur] &= (uint8_t)(~(0x1 << bit_cur));
					is_find = TRUE;
					break;
				}
				ino_cur++;
			}
			if(is_find == TRUE){
				break;
			}
		}
		if(inode->data_pointer){
			free(inode->data_pointer);
		}
		free(inode);
	}
	return NO_ERROR;
} 

/**
 * 删除关联，切断inode与子dentry之间的联系。主要是修改inode(其它子dentry和一些属性)
 */
int drop_dentry(struct newfs_inode* inode,struct newfs_dentry* dentry){
	boolean is_find = FALSE;
	struct newfs_dentry* dentry_cur;
	dentry_cur = inode->first_child;
	if(dentry_cur == dentry){
		inode->first_child = dentry->brother;
		is_find = TRUE;
	}
	else{
		while(dentry_cur){
			if(dentry_cur->brother == dentry){
				dentry_cur->brother = dentry->brother;
				is_find = TRUE;
				break;
			}
			dentry_cur = dentry_cur->brother;
		}
	}
	if(!is_find){
		return -NOT_FOUND_ERROR;
	}
	inode->size -= sizeof(struct newfs_dentry_d);
	inode->dir_cnt --;
	return inode->dir_cnt;
}
