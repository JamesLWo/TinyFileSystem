/*
 *  Copyright (C) 2021 CS416 Rutgers CS
 *	Tiny File System
 *	File:	tfs.c
 *
 */

#define FUSE_USE_VERSION 26

#include <fuse.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>
#include <sys/time.h>
#include <libgen.h>
#include <limits.h>

#include "block.h"
#include "tfs.h"

char diskfile_path[PATH_MAX];

// Declare your in-memory data structures here

char* inode_bitmap = NULL;
char* data_region_bitmap = NULL;
struct superblock* superblock;
struct inode_node* inode_table;
struct data_node* data_list;
struct inode* inode_table;
void* data_region;




/* 
 * Get available inode number from bitmap
 */
int get_avail_ino() {

	// Step 1: Read inode bitmap from disk
	
	// Step 2: Traverse inode bitmap to find an available slot
	int avail = -1;
	int i;
	for(i = 0; i < MAX_INUM; i++){
		if(get_bitmap(inode_bitmap, i) != 1){
			avail = i;
			break;
		}
	}
	
	if(avail == -1)
		return -1;
	// Step 3: Update inode bitmap and write to disk 
	set_bitmap(inode_bitmap, avail);
	bio_write(superblock->i_bitmap_blk, inode_bitmap);
	return 0;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	int avail = -1;
	// Step 1: Read data block bitmap from disk
	
	// Step 2: Traverse data block bitmap to find an available slot
	int i;
	for (i = 0; i < MAX_DNUM; i++){
		if (get_bitmap(data_region_bitmap, i) != 1) {
			avail = i;
			break;
		} 
	}
	if (avail == -1)
		return avail;
	// Step 3: Update data block bitmap and write to disk 
	set_bitmap(data_region_bitmap, avail);
	bio_write(superblock->d_bitmap_blk, data_region_bitmap);
	return avail;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
	

  // Step 1: Get the inode's on-disk block number
  int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
  if(BLOCK_SIZE % sizeof(struct inode) != 0){
	  inodes_per_block++;
  }
  int inode_block_index = superblock->i_start_blk + ino / inodes_per_block;
  
  

  // Step 2: Get offset of the inode in the inode on-disk block
  int offset = ino % inodes_per_block;
  void* buffer = malloc(BLOCK_SIZE);
  // Step 3: Read the block from disk and then copy into inode structure
  bio_read(inode_block_index, buffer);
  memcpy(inode, buffer+offset, sizeof(struct inode));
	return 0;
}

int writei(uint16_t ino, struct inode *inode) {

	// Step 1: Get the block number where this inode resides on disk
	int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
	if(BLOCK_SIZE % sizeof(struct inode) != 0){
		inodes_per_block++;
	}
	int inode_block_index = superblock->i_start_blk + ino / inodes_per_block;
	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = ino % inodes_per_block;

	void* buffer = malloc(BLOCK_SIZE);
	bio_read(inode_block_index, buffer);

	//update buffer at offset with our inode
	memcpy(buffer+offset, inode, sizeof(struct inode));
	

	// Step 3: Write inode to disk 
	bio_write()

	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {

  // Step 1: Call readi() to get the inode using ino (inode number of current directory)

  // Step 2: Get data block of current directory from inode

  // Step 3: Read directory's data block and check each directory entry.
  //If the name matches, then copy directory entry to dirent structure

	return 0;
}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	int i;
	int duplicateFound = -1;
	for(i = 0; i < 16; i++){
		int current_data_block_index = dir_inode.direct_ptr[i];
		if(current_data_block_index == -1){
			break;
		}

		void* address_of_data_block = data_region + current_data_block_index * BLOCK_SIZE;
		int j = 0;
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			void* address_of_dir_entry = address_of_data_block + j;
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			if(strcmp(fname, current_entry.name) == 0){
				duplicateFound = 1;
				break;
			}

			j = j + sizeof(struct dirent);
			

		}
		//this logic is kind of messy but we can change later
		if(duplicateFound){
			break;
		}
	}
	
	// Step 2: Check if fname (directory name) is already used in other entries
	if(duplicateFound){
		printf("Directory with same name exists]\n");
		return -1;

	}

	// Step 3: Add directory entry in dir_inode's data block and write to disk
	struct dirent *new_directory_entry = malloc(sizeof(*new_directory_entry));
	new_directory_entry->ino = f_ino;
	new_directory_entry->valid = 1; //not sure
	new_directory_entry->name = fname;
	new_directory_entry->len = name_len;

	// Allocate a new data block for this directory if it does not exist


	// Update directory inode

	// Write directory entry

	return 0;
}

int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode
	
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk

	return 0;
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	char* truncatedPath = path+1;
	int i=0;
	int index = -1;
	for (i = 0; i < strlen(truncatedPath); i++){
		if (truncatedPath[i] == '/'){
			index = i;
			break;
		}
	}	
	if (index == -1) //end of filepath, e.g. a.txt is passed
		index = strlen(truncatedPath);


	char* directory_name = malloc(index);
	memcpy(directory_name, truncatedPath, index);
	struct inode current_inode = inode_table[ino];
	int next_ino = -1;
	int i = 0;
	for (i = 0; i < 16; i++){
		int current_data_block_index = current_inode.direct_ptr[i];
		if(current_data_block_index == -1){
			break;
		}
		//for this current data block, check if the foo dirent is in there
		//get block from current_data_block_index on disk using bio_read
		void* address_of_data_block = data_region + current_data_block_index * BLOCK_SIZE;
		int j = 0;
		while(j+sizeof(struct dirent) < BLOCK_SIZE){
			//go through each dirent in the current block
			void* address_of_dir_entry = address_of_data_block + j;
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			//checking if we found it and that we're done
			if(strcmp(directory_name, current_entry.name) == 0 && strstr(truncatedPath, "/") == NULL){
				//dirent is found, and we're at the end of filepath
				memcpy(inode,inode_table[current_entry.ino], sizeof(struct inode));
				return 1;
			} 
			//found it, have another directory to go into
			else if(strcmp(directory_name, current_entry.name) == 0 && inode_table[current_entry.ino].type == 0){
				//dirent is found
				next_ino = current_entry.ino;
				break;
				
			}
			
			j = j + sizeof(struct dirent);
		}

		if(next_ino != -1){
			break;
		}
	}
	if (next_ino == -1){
		return -1; //not found
	}
	//at this point, current_ino is set to be the inode number of foo
	//but we also need to check if this current inode is another directory or a file
	//if it is a file, we should have reached end of path (/a.txt)
	//if it is a directory, we should have not reached end of path yet (/foo/a.txt)



	int retval;
	char* substring = strstr(truncatedPath, "/");
	retval = get_node_by_path(substring, next_ino, inode);
	if (!retval) {
		return -1; //file or dir not found
	}
	printf("Successfully found\n");
	return 0; //found the elusive inode, stored inside *inode
}


/* 
 * Make file system
 */
int tfs_mkfs() {
	// Call dev_init() to initialize (Create) Diskfile
	dev_init(diskfile_path);
	// write superblock information
	superblock = malloc(sizeof(*superblock));
	superblock->magic_num = MAGIC_NUM;

	superblock->max_inum = MAX_INUM;
	superblock->max_dnum = MAX_DNUM;

	superblock->i_bitmap_blk = 1;
	superblock->d_bitmap_blk = superblock->i_bitmap_blk + MAX_INUM / 8;
	superblock->i_start_blk = superblock->d_bitmap_blk + MAX_DNUM / 8;
	int num_bytes_inode = MAX_INUM * sizeof(struct inode);
	int blocks_needed = 0;
	
	if (num_bytes_inode % BLOCKSIZE != 0) {
		blocks_needed = num_bytes_inode / BLOCKSIZE + 1;
	} else{
		blocks_needed = num_bytes_inode / BLOCKSIZE;
	}
	
	superblock->d_start_blk = superblock->i_start_blk + blocks_needed; 
	
	bio_write(0, superblock);


	// initialize inode bitmap

	int number_of_inodes = MAX_INUM;
	int number_of_elements = MAX_INUM / 8;
	inode_bitmap = malloc(number_of_elements);


	// initialize data block bitmap
	int number_of_datablocks = MAX_DNUM;
	number_of_elements = MAX_DNUM / 8;
	data_region_bitmap = malloc(number_of_elements);

	// initializing inode table
	inode_table = malloc(sizeof(struct inode) * MAX_INUM);
	// initialize data region
	data_region = malloc(BLOCK_SIZE * MAX_DNUM);

	// update bitmap information for root directory
	// allocating 0-th inode for root
	set_bitmap(inode_bitmap, 0);

	// update inode for root directory
	struct inode root_inode;
	root_inode.ino = 0; //0 as 'well-known' ino
	root_inode.type = 0; //0 for directory, 0 for file
	//do other initializations..

	//set this inode into our in-memory table
	inode_table[0] = root_inode;
	
	//write to disk
	bio_write(3, inode_table);

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
	

	// Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path) == -1){
		tfs_mkfs();
	} else {// Step 1b: If disk file is found, just initialize in-memory data structures and read superblock from disk
		// initialize inode bitmap
		int number_of_inodes = MAX_INUM;
		int number_of_elements = MAX_INUM / 8;
		inode_bitmap = malloc(number_of_elements);
		// initialize data block bitmap
		int number_of_datablocks = MAX_DNUM;
		number_of_elements = MAX_DNUM / 8;
		data_region_bitmap = malloc(number_of_elements);
		
		struct superblock* superblock = malloc(sizeof(*superblock));
		struct inode_node* inode_table = malloc(sizeof(*inode_table));
		
		bio_read(0, superblock);
		
	}

  

	return NULL;
}

static void free_inode_table(struct inode_node* head){
	struct *inode_node tmp;
	while (head != NULL){
		tmp = head;
		head = head->next;
		free(tmp);
	}
}

static void free_data_list(struct data_node* head){
	struct *data_node tmp;
	while (head != NULL){
		tmp = head;
		head = head->next;
		free(tmp);
	}
}

static void tfs_destroy(void *userdata) {

	// Step 1: De-allocate in-memory data structures
	free(inode_bitmap); 
	free(data_region_bitmap);
	free(superblock);
	free_inode_table(inode_table);
	free_data_list(data_list);
	// Step 2: Close diskfile
	dev_close();

}

static int tfs_getattr(const char *path, struct stat *stbuf) {

	// Step 1: call get_node_by_path() to get inode from path

	// Step 2: fill attribute of file into stbuf from inode

		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
		time(&stbuf->st_mtime);

	return 0;
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

    return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: Read directory entries from its data blocks, and copy them to filler

	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory

	// Step 5: Update inode for target directory

	// Step 6: Call writei() to write inode to disk
	

	return 0;
}

static int tfs_rmdir(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name

	// Step 2: Call get_node_by_path() to get inode of target directory

	// Step 3: Clear data block bitmap of target directory

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target directory in its parent directory

	return 0;
}

static int tfs_releasedir(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of parent directory

	// Step 3: Call get_avail_ino() to get an available inode number

	// Step 4: Call dir_add() to add directory entry of target file to parent directory

	// Step 5: Update inode for target file

	// Step 6: Call writei() to write inode to disk

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {

	// Step 1: Call get_node_by_path() to get inode from path

	// Step 2: If not find, return -1

	return 0;
}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return 0;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk

	// Note: this function should return the amount of bytes you write to disk
	return size;
}

static int tfs_unlink(const char *path) {

	// Step 1: Use dirname() and basename() to separate parent directory path and target file name

	// Step 2: Call get_node_by_path() to get inode of target file

	// Step 3: Clear data block bitmap of target file

	// Step 4: Clear inode bitmap and its data block

	// Step 5: Call get_node_by_path() to get inode of parent directory

	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory

	return 0;
}

static int tfs_truncate(const char *path, off_t size) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_release(const char *path, struct fuse_file_info *fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
	return 0;
}

static int tfs_flush(const char * path, struct fuse_file_info * fi) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}

static int tfs_utimens(const char *path, const struct timespec tv[2]) {
	// For this project, you don't need to fill this function
	// But DO NOT DELETE IT!
    return 0;
}


static struct fuse_operations tfs_ope = {
	.init		= tfs_init,
	.destroy	= tfs_destroy,

	.getattr	= tfs_getattr,
	.readdir	= tfs_readdir,
	.opendir	= tfs_opendir,
	.releasedir	= tfs_releasedir,
	.mkdir		= tfs_mkdir,
	.rmdir		= tfs_rmdir,

	.create		= tfs_create,
	.open		= tfs_open,
	.read 		= tfs_read,
	.write		= tfs_write,
	.unlink		= tfs_unlink,

	.truncate   = tfs_truncate,
	.flush      = tfs_flush,
	.utimens    = tfs_utimens,
	.release	= tfs_release
};


int main(int argc, char *argv[]) {
	int fuse_stat;

	getcwd(diskfile_path, PATH_MAX);
	strcat(diskfile_path, "/DISKFILE");

	fuse_stat = fuse_main(argc, argv, &tfs_ope, NULL);

	return fuse_stat;
}

