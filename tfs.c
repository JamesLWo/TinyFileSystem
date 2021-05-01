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

unsigned char* inode_bitmap = NULL;
unsigned char* data_region_bitmap = NULL;
struct superblock* superblock;



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
	if(avail == -1){
		return -1;
	}

	// Step 3: Update inode bitmap and write to disk 
	set_bitmap(inode_bitmap, avail);
	bio_write(superblock->i_bitmap_blk, inode_bitmap);
	return avail;
}

/* 
 * Get available data block number from bitmap
 */
int get_avail_blkno() {
	// Step 1: Read data block bitmap from disk
	// Step 2: Traverse data block bitmap to find an available slot

	int avail = -1;
	int i;
	for (i = 0; i < MAX_DNUM; i++){
		if (get_bitmap(data_region_bitmap, i) != 1) {
			avail = i;
			break;
		} 
	}
	if (avail == -1){
		return avail;
	}
	
	// Step 3: Update data block bitmap and write to disk 
	set_bitmap(data_region_bitmap, avail);
	bio_write(superblock->d_bitmap_blk, data_region_bitmap);
	return avail;
}

/* 
 * inode operations
 */
int readi(uint16_t ino, struct inode *inode) {
	//printf("----------------------------\n");
	//printf("entered readi for ino: %d\n", ino);
  // Step 1: Get the inode's on-disk block number
  int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
  //printf("number of inodes per block: %d\n", inodes_per_block);
  int inode_block_index = superblock->i_start_blk + ino / inodes_per_block;
	//printf("block number: %d\n", inode_block_index);

  
  // Step 2: Get offset of the inode in the inode on-disk block
  int offset = ino % inodes_per_block;
  //printf("offset = %d from %d mod %d\n", offset, ino, inodes_per_block);
  void* buffer = malloc(BLOCK_SIZE);
  // Step 3: Read the block from disk and then copy into inode structure
  bio_read(inode_block_index, buffer);
  memcpy(inode, buffer+(offset*sizeof(struct inode)), sizeof(struct inode));
  //printf("readi finished\n");
  //printf("-------------\n");
  return 0;
}

int writei(uint16_t ino, struct inode *inode) {
	//printf("----------------------------\n");
	//printf("entered writei\n");
	// Step 1: Get the block number where this inode resides on disk
	int inodes_per_block = BLOCK_SIZE / sizeof(struct inode);
	//printf("number of inodes per block: %d\n", inodes_per_block);
	int inode_block_index = superblock->i_start_blk + ino / inodes_per_block;
	//printf("block number: %d\n", inode_block_index);

	// Step 2: Get the offset in the block where this inode resides on disk
	int offset = ino % inodes_per_block;
	//printf("offset = %d from %d mod %d\n", offset, ino, inodes_per_block);

	void* buffer = malloc(BLOCK_SIZE);
	bio_read(inode_block_index, buffer);

	struct inode *before = buffer + (offset*sizeof(struct inode));
	//printf("block buffer before memcpy: %d\n", before->ino);

	//update buffer at offset with our inode
	memcpy(buffer + (offset*sizeof(struct inode)), inode, sizeof(struct inode));
	
	struct inode *after = buffer + (offset*sizeof(struct inode));
	//printf("block buffer after memcpy: %d\n", after->ino);

	// Step 3: Write inode to disk 
	bio_write(inode_block_index, buffer);
	//printf("finished writei\n");
	//printf("-------------------------\n");
	return 0;
}


/* 
 * directory operations
 */
int dir_find(uint16_t ino, const char *fname, size_t name_len, struct dirent *dirent) {
	printf("----------------------\n");
	printf("entered dir_find\n");
	printf("ino: %d\n fname: %s\n name_len: %d\n", ino, fname, name_len);
    // Step 1: Call readi() to get the inode using ino (inode number of current directory)

	printf("reading directory inode...\n");
	struct inode dir_inode;
	readi(ino, &dir_inode);
	printf("read directory inode: %d\n", dir_inode.ino);
	//Step 2: iterate over every dirent and find if matches
	int i;
	int foundDir = -1;

	//allocate space for current data block being read
	void* current_data_block = malloc(BLOCK_SIZE);
	for(i = 0; i < 16; i++){
		int current_data_block_index = superblock->d_start_blk + dir_inode.direct_ptr[i];
		printf("current datablock index at dir_inode.direct_ptr[%d]: %d\n", i, current_data_block_index);
		if(current_data_block_index == -1){
			printf("reached end of valid data blocks\n");
			break;
		}
		
		bio_read(current_data_block_index, current_data_block);

		int j = 0;
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			//void* address_of_dir_entry = block->data + j;
			void* address_of_dir_entry = current_data_block + j;
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			if(strcmp(fname, current_entry.name) == 0){
				if(dirent != NULL){ //if we're calling dir_find for dir_remove/dir_add, don't copy anything
					memcpy(dirent, address_of_dir_entry, sizeof(struct dirent));
				}
				//if name matches, we found the target directory entry
				foundDir = 0;
				break;
			}

			j = j + sizeof(struct dirent);
			

		}
		//this logic is kind of messy but we can change later
		if(foundDir){
			break;
		}
	}
	free(current_data_block);
	if(foundDir == 0){
		//we found the dirent
		return 0;
	}
	//could not find dirent
	return -1;

	
  	// Step 2: Get data block of current directory from inode

  	// Step 3: Read directory's data block and check each directory entry.
  	//If the name matches, then copy directory entry to dirent structure

}

int dir_add(struct inode dir_inode, uint16_t f_ino, const char *fname, size_t name_len) {
	printf("-------------------\n");
	printf("entered dir_add\n");
	//if the dirent already exists, return -1
	
	if (dir_find(dir_inode.ino, fname, strlen(fname), NULL) == 0){ 
		printf("Cannot dir_add, duplicate detected\n");
		return -EEXIST;
	}
	printf("dirent does not exist, okay to add\n");
	
	// Step 1: Read dir_inode's data block and check each directory entry of dir_inode
	

	// Step 3: Add directory entry in dir_inode's data block and write to disk
	

	int found_data_block_number =-1;
	void* found_block = NULL;

	//reserve space to read in the data blocks 
	void* current_data_block = malloc(BLOCK_SIZE);
	int z = 0;
	for (z = 0; z < 16; z++){
		printf("checking directptr[%d]]\n", z);
		//check if directptr[j] has enough space for our directory. if so, add using offset
		//we also know directptr[j] refers to a data block that only has dirents, we just want a dirent that has valid = 0

		//as soon as we see a -1, we know that we've already considered all available data blocks
		if(dir_inode.direct_ptr[z] == -1){
			printf("no blocks available\n");
			break;
		}
		bio_read(dir_inode.direct_ptr[z] + superblock->d_start_blk, current_data_block);
		int j = 0;
	
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			
			void* address_of_dir_entry = current_data_block + j;
			printf("current address of dir entry: %d\n", address_of_dir_entry);
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			if(current_entry.valid == -1){
				//not valid, we found an unoccupied one
				current_entry.valid = 0;
				current_entry.ino = f_ino;
				current_entry.len = name_len;
				int i = 0;
				while(i < name_len){
					current_entry.name[i] = *(fname + i);
					i++;
				}
				current_entry.name[i] = '\0';				
				
				//save info about the found data block
				found_data_block_number = dir_inode.direct_ptr[z] + superblock->d_start_blk;
				found_block = current_data_block;
				break;
			}

			j = j + sizeof(struct dirent);
			

		}
		if(found_data_block_number){
			break;
		}
	}

	void* new_data_block = malloc(BLOCK_SIZE);
	int new_data_block_number= -1;
	//if we ended up not finding a invalid dirent in the available data blocks, find a new data block

	if (found_data_block_number == -1){ 
		printf("did not find available data block for new directory entry\n");
		//new data block created
		int j = 0;
		printf("getting available block num...\n");
		new_data_block_number = get_avail_blkno();
		printf("new block number: %d\n", new_data_block_number);
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			printf("populating block with a dirent...\n");
			//fill new data block with invalid dirents 
			struct dirent new_dirent;
			new_dirent.valid = -1;
			new_dirent.ino = -1;
			new_dirent.name[0] = '\0';
			new_dirent.len = -1;
			printf("new_dirent.valid = %d\nnew_dirent.ino=%d\nnew_dirent.name=%s\nnew_dirent.len=%d\n", new_dirent.valid, new_dirent.ino, new_dirent.name, new_dirent.len);
			memcpy(new_data_block + j, &new_dirent, sizeof(struct dirent));
			

			struct dirent assert;
			memcpy(&assert, new_data_block + j, sizeof(struct dirent));
			//printf("Checking dirent at address %d to add\n", new_data_block + j);
			//printf("assert.valid = %d\nassert.ino=%d\nassert.name=%s\nassert.len=%d\n", assert.valid, assert.ino, assert.name, assert.len);
			j = j + sizeof(struct dirent);
		}
		//set first dirent to valid
		struct dirent* first_dirent = (struct dirent*) new_data_block;
		first_dirent->valid = 0;
		first_dirent->ino = f_ino;
		first_dirent->len = name_len;
		int i = 0;
		while(i < name_len){
			first_dirent->name[i] = *(fname + i);
			i++;
		}
		first_dirent->name[i] = '\0';
		//add the new block index to the directptr array
		int a;
		for(a = 0; a < 16; a++){
			if(dir_inode.direct_ptr[a] == -1){
				printf("adding new block number %d to direct_ptr index %d\n", new_data_block_number, a);
				dir_inode.direct_ptr[a] = new_data_block_number;
				printf("dir_inode.direct_ptr[%d] = %d\n", a, dir_inode.direct_ptr[a]);
				break;
			}
		}
	}

	// Update directory inode
	dir_inode.size += sizeof(struct dirent);
	//update link here
	dir_inode.link += 1;
	//update stat here
	
	printf("writing changes for directory inode to disk\n");
	writei(dir_inode.ino, &dir_inode);

	// Write directory entry
	if(found_data_block_number > 0){
		printf("writing to existing data block %d...\n", found_data_block_number);
		bio_write(found_data_block_number, found_block);
	}
	else{

		int j = 0;
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			struct dirent* copied_dirent = (struct dirent *)(new_data_block + j);
			printf("copied dirent's validity: %d\n", copied_dirent->valid);
			j = j + sizeof(struct dirent);
		}
		printf("writing to a new data block %d...\n", new_data_block_number);
		bio_write(new_data_block_number + superblock->d_start_blk, new_data_block);
		
	}
	
	//we also have to write the updated inode table 

	free(current_data_block);


	return 0;
}



int dir_remove(struct inode dir_inode, const char *fname, size_t name_len) {

	// Step 1: Read dir_inode's data block and checks each directory entry of dir_inode

	//allocate space for current data block being read
	void* current_data_block = malloc(BLOCK_SIZE);
	int i;
	for(i = 0; i < 16; i++){
		int current_data_block_index = superblock->d_start_blk + dir_inode.direct_ptr[i];
		if(current_data_block_index == -1){
			break;
		}
		
		bio_read(current_data_block_index, current_data_block);

		int j = 0;
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			//void* address_of_dir_entry = block->data + j;
			void* address_of_dir_entry = current_data_block + j;
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			if(strcmp(fname, current_entry.name) == 0){// found the dirent we want to remove
				current_entry.valid = -1;
				//clear data blocks 
				//update data bitmap
				//make the inode invalid and update inode bitmap
				struct inode inode;
				readi(current_entry.ino, &inode);
				inode.valid = -1;
				unset_bitmap(inode_bitmap, inode.ino);

				//write bitmaps to disk
				bio_write(superblock->i_bitmap_blk, inode_bitmap);
				bio_write(superblock->d_bitmap_blk, data_region_bitmap);

				//write inode block to disk
				writei(current_entry.ino, &inode);
				dir_inode.link--;
				writei(dir_inode.ino, &dir_inode);
			
				
				memcpy(current_data_block + j, &current_entry, sizeof(struct dirent));
				//write cleared datablock to disk
				bio_write(current_data_block_index, current_data_block);
				
				return 0; 
			}

			j = j + sizeof(struct dirent);
			

		}
		
	}
	//we cannot find the dirent
	return -1;
	// Step 2: Check if fname exist

	// Step 3: If exist, then remove it from dir_inode's data block and write to disk
}

/* 
 * namei operation
 */
int get_node_by_path(const char *path, uint16_t ino, struct inode *inode) {
	printf("---------------------------------------\n");
	printf("get node by path called\n");
	//ignore the first character, which is '/'
	const char* truncatedPath = path+1;
	int i;
	int index = -1;
	//find the index of the first occurrence of '/'
	for (i = 0; i < strlen(truncatedPath); i++){
		if (truncatedPath[i] == '/'){
			index = i;
			break;
		}
	}
	//if we could not find one, end of filepath
	if (index == -1){
		index = strlen(truncatedPath);
	}

	//get name of directory 
	char* directory_name = malloc(index);
	memcpy(directory_name, truncatedPath, index+1);

	printf("path passed in: %s\n", path);
	printf("name of directory: %s\n", directory_name);

	//get the inode of the directory 
	printf("reading inode for ino %d...\n", ino);
	struct inode current_inode;
	readi(ino, &current_inode);
	printf("current inode number: %d\n", current_inode.ino);
	int next_ino = -1;
	void* current_data_block = malloc(BLOCK_SIZE);
	for (i = 0; i < 16; i++){
		int current_data_block_index = superblock->d_start_blk + current_inode.direct_ptr[i];
		printf("looking at data block %d...\n", current_data_block_index);
		if(current_inode.direct_ptr[i] == -1){
			break;
		}
		
		bio_read(current_data_block_index, current_data_block);
		int j = 0;
		struct inode* inode_of_current_entry = malloc(sizeof(*inode_of_current_entry));
		while(j+sizeof(struct dirent) < BLOCK_SIZE){
			printf("current offset: %d\n", j);
			//go through each dirent in the current block
			//void* address_of_dir_entry = block->data + j;
			void* address_of_dir_entry = current_data_block + j;
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			
			readi(current_entry.ino, inode_of_current_entry);
			printf("current dirent ino: %d\ncurrent dirent validity: %d\ncurrent dirent name: %s\n ", current_entry.ino, current_entry.valid, current_entry.name);

			//checking if we found it and that we're done
			if(strcmp(directory_name, current_entry.name) == 0 && strstr(truncatedPath, "/") == NULL){
				//dirent is found, and we're at the end of filepath
				memcpy(inode, inode_of_current_entry, sizeof(struct inode));
				free(inode_of_current_entry);
				printf("Found target inode! Get node by path returning...\n");
				return 0;
			} 
			//found it, have another directory to go into
			else if(strcmp(directory_name, current_entry.name) == 0 && inode_of_current_entry->type == 0){
				//dirent is found
				next_ino = current_entry.ino;
				break;
				
			}
			
			
			j = j + sizeof(struct dirent);
		}
		free(inode_of_current_entry);
		

		if(next_ino != -1){
			break;
		}
	}
	
	free(current_data_block);
	if (next_ino == -1){
		return -1; //not found
	}
	//at this point, current_ino is set to be the inode number of foo
	//but we also need to check if this current inode is another directory or a file
	//if it is a file, we should have reached end of path (/a.txt)
	//if it is a directory, we should have not reached end of path yet (/foo/a.txt)



	int retval;
	//get next file path "/foo/bar/a.txt" -> "/bar/a.txt"
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
	printf("---------------------------------------\n");
	printf("tfs_mkfs called\n");
	// Call dev_init() to initialize (Create) Diskfile
	
	printf("calling dev_init...\n");
	dev_init(diskfile_path);
	printf("dev_init succeeded\n");
	// write superblock information

	printf("mallocing memory for superblock and initializing...\n");
	superblock = malloc(sizeof(*superblock));
	superblock->magic_num = MAGIC_NUM;

	superblock->max_inum = MAX_INUM;
	superblock->max_dnum = MAX_DNUM;

	superblock->i_bitmap_blk = 1;
	superblock->d_bitmap_blk = 2;
	superblock->i_start_blk = 3;


	printf("calculating number blocks needed for inode table...\n");
	int num_bytes_inode = MAX_INUM * sizeof(struct inode);
	int blocks_needed = num_bytes_inode / BLOCK_SIZE;
	if (num_bytes_inode % BLOCK_SIZE != 0) {
		blocks_needed++;
	} 
	printf("number of blocks needed for inode table: %d\n", blocks_needed);
	

	printf("data block region starts at block number %d\n",superblock->i_start_blk + blocks_needed);
	superblock->d_start_blk = superblock->i_start_blk + blocks_needed; 
	
	
	printf("calling bio_write to write superblock struct into block 0...\n");
	bio_write(0, superblock);
	printf("bio_write succeeded\n");


	// initialize inode bitmap

	printf("calculating number of elements in inode bitmap...\n");
	int number_of_elements = MAX_INUM / 8;
	printf("mallocing %d bytes for inode bitmap: \n", number_of_elements);
	inode_bitmap = malloc(number_of_elements);

	printf("setting all bits in bitmap to 0\n");
	memset(inode_bitmap, 0, number_of_elements);
	printf("writing inode bitmap to disk\n");
	bio_write(1, inode_bitmap);
	printf("successfully wrote inode bitmap to disk\n");

	printf("calculating number of elements in datablock bitmap...\n");
	// initialize data block bitmap
	number_of_elements = MAX_DNUM / 8;
	printf("mallocing %d bytes for datanode bitmap \n", number_of_elements);
	data_region_bitmap = malloc(number_of_elements);
	printf("setting datablock bitmap bits to 0\n");
	memset(data_region_bitmap, 0, number_of_elements);
	printf("writing datablock bitmap to disk\n");
	bio_write(2, data_region_bitmap);
	printf("successfully wrote data bitmap to disk\n");

	
	

	// update bitmap information for root directory
	// allocating 0-th inode for root
	printf("allocating first inode for the root in inode bitmap\n");
	set_bitmap(inode_bitmap, 0);
	printf("checking inode bitmap value at index 0: %d\n", get_bitmap(inode_bitmap, 0));

	// update inode for root directory
	printf("updating inode for root directory\n");
	struct inode root_inode;
	root_inode.ino = 0; //0 as 'well-known' ino
	root_inode.type = 0; //0 for directory, 0 for file
	root_inode.vstat.st_mode = S_IFDIR | 0755;
	memset(root_inode.direct_ptr, -1, sizeof(int) * 16);
	printf("Verifying root_inode.ino: %d should be 0, root_inode.type: %d should be 0\n", root_inode.ino, root_inode.type);

	//write to disk

	printf("writing root_inode to block...\n");
	void *tempbuffer = malloc(BLOCK_SIZE);
	bio_read(3, tempbuffer);
	struct inode* before = tempbuffer;
	printf("buffer before writei: %d\n", before->ino);

	writei(root_inode.ino, &root_inode);
	
	bio_read(3, tempbuffer);
	struct inode* after = tempbuffer;
	printf("buffer after writei: %d\n", after->ino);
	printf("write successful\n");
	
	printf("---------------------------------------\n");

	return 0;
}


/* 
 * FUSE file operations
 */
static void *tfs_init(struct fuse_conn_info *conn) {
	printf("---------------------------------------\n");
	printf("TFS INIT CALLED\n");
	

	// Step 1a: If disk file is not found, call mkfs
	if(dev_open(diskfile_path) == -1){
		printf("Diskfile not found... calling tfs_mkfs()\n");
		tfs_mkfs();
	} else {
		printf("Diskfile found... initializing in-memory data structures\n");
		// Step 1b: If disk file is found, just initialize in-memory data structures and read superblock from disk
		// initialize inode bitmap
		int number_of_elements = MAX_INUM / 8;
		printf("mallocing %d elements for inode bitmap\n", number_of_elements);
		inode_bitmap = malloc(number_of_elements);
		// initialize data block bitmap
		number_of_elements = MAX_DNUM / 8;
		printf("mallocing %d elements for data bitmap\n", number_of_elements);
		data_region_bitmap = malloc(number_of_elements);
		printf("mallocing superblock\n");
		struct superblock* superblock = malloc(sizeof(*superblock));
		//bioread for the bitmaps
		printf("Reading superblock from disk...\n");
		bio_read(0, superblock);
		printf("Successfully read contents into superblock from disk!\n");
		
		
	}

	printf("TFS INIT COMPLETED\n");
	printf("---------------------------------------\n");

	return NULL;
}

static void tfs_destroy(void *userdata) {
	printf("---------------------------------------\n");
	printf("entered tfs_destroy. freeing in-memory DS\n");
	// Step 1: De-allocate in-memory data structures
	free(inode_bitmap); 
	free(data_region_bitmap);
	free(superblock);
	// Step 2: Close diskfile
	printf("closing diskfile...\n");
	dev_close();
	printf("---------------------------------------\n");

}

static int tfs_getattr(const char *path, struct stat *stbuf) {
	printf("---------------------------------------\n");
	printf("entered tfs_getattr\n");
	// Step 1: call get_node_by_path() to get inode from path

	struct inode target_inode;
	printf("getting inode for path %s\n", path);
	int ret_val = get_node_by_path(path, 0, &target_inode);
	if(ret_val < 0){
		printf("file not found\n");
		return -ENOENT;
	}
	printf("ino: %d\n", target_inode.ino);
	printf("size of inode: %d\n", target_inode.size);
	printf("size of inode from vstat: %d\n", target_inode.vstat.st_size);

	// Step 2: fill attribute of file into stbuf from inode

	if (target_inode.type == 0){ //dir
		stbuf->st_mode   = S_IFDIR | 0755;
		stbuf->st_nlink  = 2;
	}
	else{ //file
		stbuf->st_mode   = S_IFREG | 0644;
		stbuf->st_nlink  = 1;
	}
	
	time(&stbuf->st_mtime);

	//more attributes to fill in to stbuf
	stbuf->st_blksize = BLOCK_SIZE;
	stbuf->st_size = target_inode.vstat.st_size;

	printf("inode attributes filled in\n");
	printf("---------------------------------------\n");
	return 0;
	
}

static int tfs_opendir(const char *path, struct fuse_file_info *fi) {
	printf("---------------------------------------\n");
	printf("entered tfs_opendir\n");
	struct inode* inode = malloc(sizeof(*inode));
	printf("getting node at path %s\n", path);
	return get_node_by_path(path, 0, inode);

	// Step 1: Call get_node_by_path() to get inode from path
	

	// Step 2: If not find, return -1

    //return 0;
}

static int tfs_readdir(const char *path, void *buffer, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
	printf("---------------------------------------\n");
	printf("entered tfs_readdir\n");
	// Step 1: Call get_node_by_path() to get inode from path
	struct inode* inode = malloc(sizeof(*inode));
	printf("calling get node by path for path %s\n", path);
	int retval = get_node_by_path(path, 0, inode);
	printf("get node by path returned %d\n", retval);
	if (retval < 0){
		return -ENOENT;
	}
	int i;

	void* current_data_block = malloc(BLOCK_SIZE);
	for(i = 0; i < 16; i++){
		int current_data_block_index = inode->direct_ptr[i] + superblock->d_start_blk;
		if(current_data_block_index == -1){
			break;
		}
		bio_read(current_data_block_index, current_data_block);

		int j = 0;
		while(j + sizeof(struct dirent) < BLOCK_SIZE){
			//void* address_of_dir_entry = block->data + j;
			void* address_of_dir_entry = current_data_block + j;
			struct dirent current_entry;
			memcpy(&current_entry, address_of_dir_entry, sizeof(struct dirent));

			
			//filler function here with name of dirent as the second arg
			//filler(buffer, name_of_dirent, NULL, 0);
			filler(buffer, current_entry.name, NULL ,offset);

			j = j + sizeof(struct dirent);
			

		}
	}

	// Step 2: Read directory entries from its data blocks, and copy them to filler
	//rn, inode directptrs have all dirents

	//iterate over every directptr block to find all dirents
	//for every dirent found, call the filler function


	return 0;
}


static int tfs_mkdir(const char *path, mode_t mode) {
	printf("-----------------------------\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	printf("entered tfs_mkdir\n");
	printf("splitting path...\n");
	
	char* basename = strrchr(path, '/');
	struct inode parent_inode;
	char* dirname;
	
	//case where path starts from root dir, i.e. path = /file
	if(basename == path){
		dirname = "/";
		//get inode of parent directory which is root 
		readi(0, &parent_inode);
		printf("dirname: %s, truncated basename: %s\n", dirname, basename);
	}
	//file is not directly under root
	else {
		int length_of_parent_directory_name = basename - path;
		
		char* dirname = malloc(length_of_parent_directory_name + 1);
		memcpy(dirname, path, length_of_parent_directory_name);
		dirname[length_of_parent_directory_name+1] = '\0';
		printf("dirname: %s, truncated basename: %s\n", dirname, basename);
		// Step 2: Call get_node_by_path() to get inode of parent directory
		int retval = get_node_by_path(dirname, 0, &parent_inode);
		if (retval < 0) {
			printf("dir not found\n");
			return -ENOENT;
		}
	}
	basename += 1;
	
	// Step 3: Call get_avail_ino() to get an available inode number
	int new_inode_number = get_avail_ino();
	printf("found available inode %d\n", new_inode_number);

	// Step 4: Call dir_add() to add directory entry of target directory to parent directory
	printf("calling dir_add to add this dirent to the parent directory \n");
	dir_add(parent_inode, new_inode_number, basename, strlen(basename));

	//make new inode for directory
	printf("making new directory inode\n");
	struct inode new_inode;
	new_inode.ino = new_inode_number;
	new_inode.link = 2;
	new_inode.type = 0; //directory
	new_inode.size = 0;
	new_inode.valid = 1;
	memset(new_inode.direct_ptr, -1, sizeof(int)*16);

	// Step 5: Update inode for target directory
	//printf("updating parent inode\n");
	//parent_inode.link++;
	//parent_inode.size += new_inode.size;

	// Step 6: Call writei() to write inode to disk
	printf("writing inode...\n");
	writei(new_inode.ino, &new_inode);	
	printf("write success\n");
	printf("-------------------------\n");
	return 0;
}

static int tfs_rmdir(const char *path) {
	printf("-----------------------------\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	printf("entered tfs_rmdir\n");
	printf("splitting path...\n");
	printf("path passed in: %s\n", path);
	char* basename = strrchr(path, '/');
	char* dirname;
	//case where path starts from root dir, i.e. path = /file
	if(basename == path){
		dirname = "/";
	}
	//file is not directly under root
	else {
		int length_of_parent_directory_name = basename - path;
		char* dirname = malloc(length_of_parent_directory_name + 1);
		memcpy(dirname, path, length_of_parent_directory_name);
		dirname[length_of_parent_directory_name+1] = '\0';
	}
	//truncate basename by 1
	basename += 1;
	//copy /foo/bar into dirname
	printf("dirname: %s, truncated basename: %s\n", dirname, basename);
	// if absolute path is /foo/bar/tmp, basename will be "/tmp" after strrchr


	//get target directory inode 
	struct inode target_directory_inode;
	int retval = get_node_by_path(path, 0, &target_directory_inode);
	if(retval < 0){
		printf("target directory not found \n");
		return -ENOENT;
	}

	//clear data block bitmap of target directory
	int i;
	for(i = 0; i < 16; i++){
		int data_block_to_clear = target_directory_inode.direct_ptr[i];
		if(data_block_to_clear == -1){
			break;
		}
		unset_bitmap(data_region_bitmap, data_block_to_clear);
	}
	bio_write(2, data_region_bitmap);

	//clear inode bitmap for target directory inode 
	unset_bitmap(inode_bitmap, target_directory_inode.ino);
	bio_write(1, inode_bitmap);

	//clear inodes data block
	target_directory_inode.valid = -1;
	writei(target_directory_inode.ino, &target_directory_inode);


	//remove the directory entry corresponding to the target directory inside the parent directory
	struct inode parent_directory_inode;
	//Call get_node_by_path() to get inode of parent directory
	retval = get_node_by_path(dirname, 0, &parent_directory_inode);
	if (retval < 0) {
		printf("dir not found\n");
		return -ENOENT;
	}
	dir_remove(parent_directory_inode, basename, strlen(basename));
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
	printf("-----------------------------\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	printf("entered tfs_create\n");
	printf("splitting path...\n");
	printf("path passed in: %s\n", path);
	char* basename = strrchr(path, '/');
	struct inode parent_inode;
	char* dirname;
	//case where path starts from root dir, i.e. path = /file
	if(basename == path){
		dirname = "/";
		//get inode of parent directory which is root 
		readi(0, &parent_inode);
	}
	//file is not directly under root
	else {
		int length_of_parent_directory_name = basename - path;
		
		char* dirname = malloc(length_of_parent_directory_name + 1);
		memcpy(dirname, path, length_of_parent_directory_name);
		dirname[length_of_parent_directory_name+1] = '\0';
		// Step 2: Call get_node_by_path() to get inode of parent directory
		int retval = get_node_by_path(dirname, 0, &parent_inode);
		if (retval < 0) {
			printf("dir not found\n");
			return -ENOENT;
		}
	}

	
	//truncate basename by 1
	basename += 1;
	//copy /foo/bar into dirname
	printf("dirname: %s, truncated basename: %s\n", dirname, basename);
	// if absolute path is /foo/bar/tmp, basename will be "/tmp" after strrchr
	
	
	
	
	
	// Step 3: Call get_avail_ino() to get an available inode number
	int new_inode_number = get_avail_ino();
	printf("found available inode %d\n", new_inode_number);

	// Step 4: Call dir_add() to add directory entry of target file to parent directory
	dir_add(parent_inode, new_inode_number, basename, strlen(basename));

	// Step 5: Update inode for target file
	//make new inode for file
	printf("making new directory inode\n");
	struct inode new_inode;
	new_inode.ino = new_inode_number;
	new_inode.link = 0;
	new_inode.type = 1; //file
	new_inode.size = 0;
	new_inode.vstat.st_size = 0;
	new_inode.valid = 1;
	memset(new_inode.direct_ptr, -1, sizeof(int)*16);

	printf("new inode info\nino: %d\n link: %d\n type: %d\n size: %d\n valid: %d\n", new_inode.ino, new_inode.link, new_inode.type, new_inode.size, new_inode.valid);

	//printf("updating parent inode\n");
	//parent_inode.link++;
	//parent_inode.size += new_inode.size;

	// Step 6: Call writei() to write inode to disk
	printf("writing inode...\n");
	writei(new_inode.ino, &new_inode);	
	printf("write success\n");
	printf("writing inode bitmap to disk...\n");
	bio_write(1, inode_bitmap);
	printf("tfs_create finished\n");
	printf("---------------------------------------\n");

	return 0;
}

static int tfs_open(const char *path, struct fuse_file_info *fi) {
	printf("---------------------------------------\n");
	printf("entered tfs_open\n");

	// Step 1: Call get_node_by_path() to get inode from path
	printf("getting node from path %s\n", path);
	struct inode inode;
	return get_node_by_path(path, 0, &inode);
	// Step 2: If not find, return -1

}

static int tfs_read(const char *path, char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {

	// Step 1: You could call get_node_by_path() to get inode from path
	struct inode target_file_inode;
	get_node_by_path(path, 0, &target_file_inode);
	
	// Step 2: Based on size and offset, read its data blocks from disk
	int start_block_index = offset / BLOCK_SIZE;
	int end_block_index = (offset+size) / BLOCK_SIZE;

	int bytes_written = 0;

	int beginning = start_block_index;

	void* current_block = malloc(BLOCK_SIZE);
	if(offset % BLOCK_SIZE != 0){
		//calculate offset for middle of page
		int block_offset = offset % BLOCK_SIZE;

		int remaining_block_room = BLOCK_SIZE - block_offset;
		bio_read(target_file_inode.direct_ptr[beginning] + superblock->d_start_blk, current_block);

		if(size <= remaining_block_room){
			
			memcpy(buffer, current_block + block_offset, size);
			bytes_written += size;
		}
		else{
			memcpy(buffer, current_block + block_offset, remaining_block_room);
			bytes_written +=remaining_block_room;
		}

		beginning++;

	}
	int i;
	for(i = beginning; i <= end_block_index; i++){
		int remaining_bytes = size - bytes_written;
		bio_read(target_file_inode.direct_ptr[i]+superblock->d_start_blk, current_block);

		if(remaining_bytes <= BLOCK_SIZE){
			memcpy(buffer+bytes_written, current_block, remaining_bytes);
			bytes_written += remaining_bytes;
		}
		else{
			memcpy(buffer+bytes_written, current_block, BLOCK_SIZE);
			bytes_written += BLOCK_SIZE;
		}                        

	}

	// Step 3: copy the correct amount of data from offset to buffer

	// Note: this function should return the amount of bytes you copied to buffer
	return bytes_written;
}

static int tfs_write(const char *path, const char *buffer, size_t size, off_t offset, struct fuse_file_info *fi) {
	// Step 1: You could call get_node_by_path() to get inode from path
	printf("-------------------------\n");
	printf("entered tfs write\n");
	printf("path: %s\n", path);
	printf("buffer: %s\n", buffer);
	printf("size: %d\n", size);
	printf("offset: %d\n", offset);
	struct inode target_file_inode;


	printf("calling get node to see if file exists\n");
	int ret_val = get_node_by_path(path, 0, &target_file_inode);
	if(ret_val < 0){
		printf("inode does not exist\n");
		return -ENOENT;
	}

	printf("file found!\n");

	int start_block_index = offset / BLOCK_SIZE;
	int end_block_index = (offset+size) / BLOCK_SIZE;

	int bytes_written = 0;

	int beginning = start_block_index;
	printf("first block: %d\n", start_block_index);
	printf("last block: %d\n", end_block_index);
	void* current_block = malloc(BLOCK_SIZE);
	if(offset % BLOCK_SIZE != 0){
		//calculate offset for middle of page
		int block_offset = offset % BLOCK_SIZE;

		int remaining_block_room = BLOCK_SIZE - block_offset;

		int block_number = target_file_inode.direct_ptr[beginning]+superblock->d_start_blk;
		//if this block has not been made yet, allocate a new block
		if(block_number == -1){
			int new_block_number = get_avail_blkno();
			target_file_inode.direct_ptr[beginning] = new_block_number;
			set_bitmap(data_region_bitmap, new_block_number);
			block_number = new_block_number;


		}
		//if block already exists 
		else{
			bio_read(block_number, current_block);
		}

		if(size <= remaining_block_room){
			
			memcpy(current_block+ block_offset, buffer, size);
			bytes_written += size;
		}
		else{
			memcpy(current_block + block_offset, buffer, remaining_block_room);
			bytes_written +=remaining_block_room;
		}

		bio_write(block_number, current_block);
		beginning++;

	}
	int i;
	for(i = beginning; i <= end_block_index; i++){
		int remaining_bytes = size - bytes_written;
		int block_number = target_file_inode.direct_ptr[i]+superblock->d_start_blk;
		if(block_number == -1){
			int new_block_number = get_avail_blkno();
			target_file_inode.direct_ptr[i] = new_block_number;
			set_bitmap(data_region_bitmap, new_block_number);
			block_number = new_block_number;


		}
		//if block already exists 
		else{
			bio_read(block_number, current_block);
		}


		if(remaining_bytes <= BLOCK_SIZE){
			memcpy(current_block,buffer+bytes_written,  remaining_bytes);
			bytes_written += remaining_bytes;
		}
		else{
			memcpy(current_block,buffer+bytes_written,  BLOCK_SIZE);
			bytes_written += BLOCK_SIZE;
		}

		bio_write(block_number, current_block);               



	}

	// Step 2: Based on size and offset, read its data blocks from disk

	// Step 3: Write the correct amount of data from offset to disk

	// Step 4: Update the inode info and write it to disk
	target_file_inode.size += bytes_written;
	target_file_inode.vstat.st_size += bytes_written;
	printf("updated size of file: %d\n", target_file_inode.size);
	printf("updated size of file from vstat : %d\n", target_file_inode.vstat.st_size);


	writei(target_file_inode.ino, &target_file_inode);
	readi(target_file_inode.ino, &target_file_inode);
	printf("updated size of file in disk: %d\n", target_file_inode.size);
	printf("updated size of file from vstat indisk : %d\n", target_file_inode.vstat.st_size);




	// Note: this function should return the amount of bytes you write to disk
	return bytes_written;
}

static int tfs_unlink(const char *path) {
	printf("-----------------------------\n");
	// Step 1: Use dirname() and basename() to separate parent directory path and target directory name
	printf("entered tfs_unlink\n");
	printf("splitting path...\n");
	printf("path passed in: %s\n", path);
	struct inode target_inode;
	char* basename = strrchr(path, '/');
	char* dirname;
	//case where path starts from root dir, i.e. path = /file
	if(basename == path){
		dirname = "/";
	}
	//file is not directly under root
	else {
		int length_of_parent_directory_name = basename - path;
		char* dirname = malloc(length_of_parent_directory_name + 1);
		memcpy(dirname, path, length_of_parent_directory_name);
		dirname[length_of_parent_directory_name+1] = '\0';
	}
	//truncate basename by 1
	basename += 1;
	//copy /foo/bar into dirname
	printf("dirname: %s, truncated basename: %s\n", dirname, basename);
	// if absolute path is /foo/bar/tmp, basename will be "/tmp" after strrchr
	
	// Step 2: Call get_node_by_path() to get inode of target file
	int retval = get_node_by_path(path, 0, &target_inode);
	if (retval < 0) {
		printf("target_file inode not found\n");
		return -ENOENT;
	}

	// Step 3: Clear data block bitmap of target file
	int i;
	for(i = 0; i < 16; i++){
		int current_data_block_number = target_inode.direct_ptr[i];
		if(current_data_block_number == -1){
			break;
		}
		unset_bitmap(data_region_bitmap, current_data_block_number);
	}
	bio_write(2, data_region_bitmap);

	// Step 4: Clear inode bitmap and its data block
	unset_bitmap(inode_bitmap, target_inode.ino);
	target_inode.valid = -1;
	writei(target_inode.ino, &target_inode);
	bio_write(1, inode_bitmap);
	
	// Step 5: Call get_node_by_path() to get inode of parent directory
	struct inode parent_inode;
	retval = get_node_by_path(dirname, 0, &parent_inode);
	if (retval < 0) {
		printf("parent inode not found\n");
		return -ENOENT;
	}
	
	
	// Step 6: Call dir_remove() to remove directory entry of target file in its parent directory
	dir_remove(parent_inode, basename, strlen(basename));

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

