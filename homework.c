/*
 * file: homework.c
 * description: skeleton file for CS 5600 system
 *
 * CS 5600, Computer Systems, Northeastern
 */

#define FUSE_USE_VERSION 27
#define _FILE_OFFSET_BITS 64
#define MAX_BLOCKS 32768 //4096 bytes × 8 bits
#define BLOCK_SIZE 4096

#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <fuse.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdbool.h>

#include "fs5600.h"

/* if you don't understand why you can't use these system calls here, 
 * you need to read the assignment description another time
 */
#define stat(a, b) error do not use stat()
#define open(a, b) error do not use open()
#define read(a, b, c) error do not use read()
#define write(a, b, c) error do not use write()

unsigned char bitmap[BLOCK_SIZE]; // block 1, global for use in allocation later

/* disk access. All access is in terms of 4KB blocks; read and
 * write functions return 0 (success) or -EIO.
 */
extern int block_read(void *buf, int lba, int nblks);

extern int block_write(void *buf, int lba, int nblks);

/* bitmap functions
 */
void bit_set(unsigned char *map, int i) {
    map[i / 8] |= (1 << (i % 8));
}

void bit_clear(unsigned char *map, int i) {
    map[i / 8] &= ~(1 << (i % 8));
}

int bit_test(unsigned char *map, int i) {
    return map[i / 8] & (1 << (i % 8));
}

#define MAX_PATH_LEN 10
#define MAX_NAME_LEN 27

/*
 * parse - split a path into components. The path is split on '/'.
 * The components are stored in the argv array, and the number of
 * components is returned.
 */
int parse(char *path, char **argv) {
    int i;
    for (i = 0; i < MAX_PATH_LEN; i++) {
        if ((argv[i] = strtok(path, "/")) == NULL)
            break;
        if (strlen(argv[i]) > MAX_NAME_LEN)
            argv[i][MAX_NAME_LEN] = 0;
        path = NULL;
    }
    return i;
}

/*
 * translate - translate a path into an inode number. The path is
 * assumed to be absolute, and the first component is the root
 * directory.
 */
int translate(int pathc, char **pathv) {
    int inum = 2; // root inode
    struct fs_inode inode;

    for (int i = 0; i < pathc; i++) {
        if (block_read(&inode, inum, 1) < 0) {
            fprintf(stderr, "Error reading inode %d\n", inum);
            return -EIO;
        } // read the inode
        if (!S_ISDIR(inode.mode)) {
            fprintf(stderr, "Not a directory: %s\n", pathv[i]);
            return -ENOTDIR;
        } //

        struct fs_dirent dirent[128];
        if (block_read(dirent, inode.ptrs[0], 1) < 0) {
            fprintf(stderr, "Failed to read directory block %u for inode %d\n", inode.ptrs[0], inum);
            return -EIO;
        } // read the directory entries

        bool found = false;
        for (int j = 0; j < 128; j++) {
            if (dirent[j].valid && strcmp(pathv[i], dirent[j].name) == 0) {
                inum = dirent[j].inode;
                found = true;
                break;
            } // check if the name matches between the directory entry and the path
        } // loop through the directory entries

        if (!found) {
            fprintf(stderr, "File not found: %s\n", pathv[i]);
            return -ENOENT;
        }
    }

    return inum;
}

// factored out inode-to-struct stat conversion
int inode_to_stat(int inum, struct stat *sb) {
    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error reading inode %d\n", inum);
        return -EIO;
    }

    memset(sb, 0, sizeof(struct stat));
    sb->st_mode = inode.mode;
    sb->st_nlink = 1;
    sb->st_uid = inode.uid;
    sb->st_gid = inode.gid;
    sb->st_size = inode.size;
    sb->st_mtime = inode.mtime;
    sb->st_ctime = inode.ctime;
    sb->st_atime = inode.mtime;
    sb->st_blocks = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    return 0;
}

/* init - this is called once by the FUSE framework at startup. Ignore
 * the 'conn' argument.
 * recommended actions:
 *   - read superblock
 *   - allocate memory, read bitmaps and inodes
 */
void *fs_init(struct fuse_conn_info *conn) {
    if (block_read(bitmap, 1, 1) < 0) {
        fprintf(stderr, "Error reading block bitmap\n");
        return NULL;
    }
    return NULL;
}

/* Note on path translation errors:
 * In addition to the method-specific errors listed below, almost
 * every method can return one of the following errors if it fails to
 * locate a file or directory corresponding to a specified path.
 *
 * ENOENT - a component of the path doesn't exist.
 * ENOTDIR - an intermediate component of the path (e.g. 'b' in
 *           /a/b/c) is not a directory
 */

/* note on splitting the 'path' variable:
 * the value passed in by the FUSE framework is declared as 'const',
 * which means you can't modify it. The standard mechanisms for
 * splitting strings in C (strtok, strsep) modify the string in place,
 * so you have to copy the string and then free the copy when you're
 * done. One way of doing this:
 *
 *    char *_path = strdup(path);
 *    int inum = translate(_path);
 *    free(_path);
 */

/* getattr - get file or directory attributes. For a description of
 *  the fields in 'struct stat', see 'man lstat'.
 *
 * Note - for several fields in 'struct stat' there is no corresponding
 *  information in our file system:
 *    st_nlink - always set it to 1
 *    st_atime, st_ctime - set to same value as st_mtime
 *
 * success - return 0
 * errors - path translation, ENOENT
 * hint - factor out inode-to-struct stat conversion - you'll use it
 *        again in readdir
 */
int fs_getattr(const char *c_path, struct stat *sb) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    int inum = translate(pathc, pathv);
    free(path);

    if (inum < 0) {
        fprintf(stderr, "Error translating path: %s\n", c_path);
        return inum;
    }

    return inode_to_stat(inum, sb);
}

/* readdir - get directory contents.
 *
 * call the 'filler' function once for each valid entry in the 
 * directory, as follows:
 *     filler(buf, <name>, <statbuf>, 0)
 * where <statbuf> is a pointer to a struct stat
 * success - return 0
 * errors - path resolution, ENOTDIR, ENOENT
 * 
 * hint - check the testing instructions if you don't understand how
 *        to call the filler function
 */
int fs_readdir(const char *c_path, void *ptr, fuse_fill_dir_t filler,
               off_t offset, struct fuse_file_info *fi) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    int inum = translate(pathc, pathv);
    free(path);

    if (inum < 0) {
        fprintf(stderr, "Error translating path: %s\n", c_path);
        return inum;
    }

    struct fs_inode inode; // inode of the directory
    if (block_read(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error reading inode %d\n", inum);
        return -EIO;
    } // read the inode of the directory

    if (!S_ISDIR(inode.mode)) {
        fprintf(stderr, "Not a directory: %s\n", c_path);
        return -ENOTDIR;
    } // check if the inode is a directory

    struct fs_dirent dirent[128]; // directory entries
    if (block_read(dirent, inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        return -EIO;
    } // read the directory entries

    // loop through the directory entries and call the filler function
    for (int i = 0; i < 128; i++) {
        if (!dirent[i].valid) {
            continue;
        }

        struct stat sb;
        if (inode_to_stat(dirent[i].inode, &sb) < 0) {
            fprintf(stderr, "Error converting inode to stat\n");
            return -EIO;
        }

        filler(ptr, dirent[i].name, &sb, 0); // call the filler function
    }
}

/* create - create a new file with specified permissions
 *
 * success - return 0
 * errors - path resolution, EEXIST
 *          in particular, for create("/a/b/c") to succeed,
 *          "/a/b" must exist, and "/a/b/c" must not.
 *
 * Note that 'mode' will already have the S_IFREG bit set, so you can
 * just use it directly. Ignore the third parameter.
 *
 * If a file or directory of this name already exists, return -EEXIST.
 * If there are already 128 entries in the directory (i.e. it's filled an
 * entire block), you are free to return -ENOSPC instead of expanding it.
 */
int fs_create(const char *c_path, mode_t mode, struct fuse_file_info *fi) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);

    if (pathc < 1) {
        fprintf(stderr, "Invalid path: %s\n", c_path);
        free(path);
        return -ENOENT;
    }

    int parent_inum = translate(pathc - 1, pathv);
    if (parent_inum < 0) {
        fprintf(stderr, "Error translating path: %s\n", c_path);
        free(path);
        return parent_inum;
    }

    if (translate(pathc, pathv) >= 0) {
        fprintf(stderr, "File already exists: %s\n", c_path);
        free(path);
        return -EEXIST;
    }

    int block_num = -1;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!bit_test(bitmap, i)) {
            block_num = i;
            bit_set(bitmap, block_num);
            break;
        }
    }

    if (block_num < 0) {
        fprintf(stderr, "No free blocks available\n");
        free(path);
        return -ENOSPC;
    }

    struct fs_inode new_inode;
    memset(&new_inode, 0, sizeof(struct fs_inode));
    new_inode.uid = getuid();
    new_inode.gid = getgid();
    new_inode.mode = mode;
    new_inode.size = 0;
    new_inode.mtime = time(NULL);
    new_inode.ctime = new_inode.mtime;
    new_inode.ptrs[0] = block_num;

    if (block_write(&new_inode, block_num, 1) < 0) {
        fprintf(stderr, "Error writing new inode\n");
        free(path);
        return -EIO;
    }

    if (block_write(bitmap, block_num, 1) < 0) {
        fprintf(stderr, "Error writing bitmap\n");
        free(path);
        return -EIO;
    } // looks like bitmap has to be written into disk from memory

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, parent_inum, 1) < 0) {
        fprintf(stderr, "Error reading parent inode %d\n", parent_inum);
        free(path);
        return -EIO;
    }

    struct fs_dirent dirent[128];
    if (block_read(dirent, parent_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        free(path);
        return -EIO;
    }

    bool found = false;
    for (int i = 0; i < 128; i++) {
        if (!dirent[i].valid) {
            dirent[i].valid = true;
            dirent[i].inode = block_num;
            strncpy(dirent[i].name, pathv[pathc - 1], sizeof(dirent[i].name) - 1);
            dirent[i].name[sizeof(dirent[i].name) - 1] = '\0'; // use sizeof(dirent[i].name) instead of MAX_NAME_LEN
            found = true;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Directory is full\n");
        free(path);
        return -ENOSPC;
    }

    if (block_write(dirent, parent_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error writing directory entries\n");
        free(path);
        return -EIO;
    }

    free(path);
    return 0;
}

/* mkdir - create a directory with the given mode.
 *
 * WARNING: unlike fs_create, @mode only has the permission bits. You
 * have to OR it with S_IFDIR before setting the inode 'mode' field.
 *
 * success - return 0
 * Errors - path resolution, EEXIST
 * Conditions for EEXIST are the same as for create. 
 */
int fs_mkdir(const char *c_path, mode_t mode) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);

    if (pathc < 1) {
        fprintf(stderr, "Invalid path: %s\n", c_path);
        free(path);
        return -ENOENT;
    }

    int parent_inum = translate(pathc - 1, pathv);
    if (parent_inum < 0) {
        fprintf(stderr, "Error translating path: %s\n", c_path);
        free(path);
        return parent_inum;
    }

    if (translate(pathc, pathv) >= 0) {
        fprintf(stderr, "File already exists: %s\n", c_path);
        free(path);
        return -EEXIST;
    }

    int block_num = -1;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!bit_test(bitmap, i)) {
            block_num = i;
            bit_set(bitmap, block_num);
            break;
        }
    }

    if (block_num < 0) {
        fprintf(stderr, "No free blocks available\n");
        free(path);
        return -ENOSPC;
    }

    struct fs_inode new_dir_inode;
    memset(&new_dir_inode, 0, sizeof(struct fs_inode));
    new_dir_inode.uid = getuid();
    new_dir_inode.gid = getgid();
    new_dir_inode.mode = mode | S_IFDIR;
    new_dir_inode.ctime = time(NULL);
    new_dir_inode.mtime = new_dir_inode.ctime;
    new_dir_inode.size = BLOCK_SIZE;

    int dir_block_num = -1;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (!bit_test(bitmap, i)) {
            dir_block_num = i;
            bit_set(bitmap, dir_block_num);
            break;
        }
    }

    if (dir_block_num < 0) {
        fprintf(stderr, "No free blocks available for directory\n");
        free(path);
        return -ENOSPC;
    }

    new_dir_inode.ptrs[0] = dir_block_num;

    if (block_write(&new_dir_inode, block_num, 1) < 0) {
        fprintf(stderr, "Error writing new directory inode\n");
        free(path);
        return -EIO;
    }

    if (block_write(bitmap, 1, 1) < 0) {
        fprintf(stderr, "Error writing bitmap\n");
        free(path);
        return -EIO;
    }

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, parent_inum, 1) < 0) {
        fprintf(stderr, "Error reading parent inode %d\n", parent_inum);
        free(path);
        return -EIO;
    }

    struct fs_dirent dirent[128];
    if (block_read(dirent, parent_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        free(path);
        return -EIO;
    }

    bool found = false;
    for (int i = 0; i < 128; i++) {
        if (!dirent[i].valid) {
            dirent[i].valid = true;
            dirent[i].inode = block_num;
            strncpy(dirent[i].name, pathv[pathc - 1], sizeof(dirent[i].name) - 1);
            dirent[i].name[sizeof(dirent[i].name) - 1] = '\0';
            found = true;
            break;
        }
    }

    if (!found) {
        fprintf(stderr, "Directory is full\n");
        free(path);
        return -ENOSPC;
    }

    if (block_write(dirent, parent_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error writing directory entries\n");
        free(path);
        return -EIO;
    }

    free(path);
    return 0;
}


/* unlink - delete a file
 *  success - return 0
 *  errors - path resolution, ENOENT, EISDIR
 */
int fs_unlink(const char *c_path) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);

    if (pathc < 1) {
        fprintf(stderr, "Invalid path: %s\n", c_path);
        free(path);
        return -ENOENT;
    }

    int parent_inum = translate(pathc - 1, pathv);
    if (parent_inum < 0) {
        fprintf(stderr, "Error translating path: %s\n", c_path);
        free(path);
        return parent_inum;
    }

    int file_inum = translate(pathc, pathv);
    if (file_inum >= 0) {
        fprintf(stderr, "File already exists: %s\n", c_path);
        free(path);
        return -EEXIST;
    }

    struct fs_inode file_inode;
    if (block_read(&file_inode, file_inum, 1) < 0) {
        fprintf(stderr, "Error reading file inode %d\n", file_inum);
        free(path);
        return -EIO;
    }

    if (S_ISDIR(file_inode.mode)) {
        fprintf(stderr, "Not a file: %s\n", c_path);
        free(path);
        return -EISDIR;
    }

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, parent_inum, 1) < 0) {
        free(path);
        return -EIO;
    }

    struct fs_dirent dirent[128];
    if (block_read(dirent, parent_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        free(path);
        return -EIO;
    }

    bool removed = false;
    for (int i = 0; i < 128; i++) {
        if (dirent[i].valid && dirent[i].inode == file_inum) {
            dirent[i].valid = 0;
            removed = true;
            break;
        }
    }

    if (!removed) {
        fprintf(stderr, "File not found: %s\n", c_path);
        free(path);
        return -ENOENT;
    }

    if (block_write(dirent, parent_inode.ptrs[0], 1) < 0) {
        free(path);
        return -EIO;
    }

    int block_num = (file_inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE; // calculate the number of blocks used by the file
    for (int i = 0; i < block_num; i++) {
        bit_clear(bitmap, file_inode.ptrs[i]); // clear the bitmap for each block used by the file
    }

    bit_clear(bitmap, file_inum); // clear the bitmap for the inode itself

    if (block_write(bitmap, 1, 1) < 0) {
        fprintf(stderr, "Error writing bitmap\n");
        free(path);
        return -EIO;
    }

    free(path);
    return 0;
}

/* rmdir - remove a directory
 *  success - return 0
 *  Errors - path resolution, ENOENT, ENOTDIR, ENOTEMPTY
 */
int fs_rmdir(const char *c_path) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);

    if (pathc < 1) {
        fprintf(stderr, "Invalid path: %s\n", c_path);
        free(path);
        return -ENOENT;
    }

    int parent_inum = translate(pathc - 1, pathv);
    if (parent_inum < 0) {
        free(path);
        return parent_inum;
    }

    int dir_inum = translate(pathc, pathv);
    if (dir_inum < 0) {
        fprintf(stderr, "Error translating path: %s\n", c_path);
        free(path);
        return dir_inum;
    }

    struct fs_inode dir_inode;
    if (block_read(&dir_inode, dir_inum, 1) < 0) {
        fprintf(stderr, "Error reading directory inode %d\n", dir_inum);
        free(path);
        return -EIO;
    }

    if (!S_ISDIR(dir_inode.mode)) {
        fprintf(stderr, "Not a directory: %s\n", c_path);
        free(path);
        return -ENOTDIR;
    }

    struct fs_dirent dirent[128];
    if (block_read(dirent, dir_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error reading directory entries\n");
        free(path);
        return -EIO;
    }

    bool empty = true;
    for (int i = 0; i < 128; i++) {
        if (dirent[i].valid) {
            empty = false;
            break;
        }
    }

    if (!empty) {
        fprintf(stderr, "Directory not empty: %s\n", c_path);
        free(path);
        return -ENOTEMPTY;
    }

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, parent_inum, 1) < 0) {
        free(path);
        return -EIO;
    }

    struct fs_dirent parent_dirent[128];
    if (block_read(parent_dirent, parent_inode.ptrs[0], 1) < 0) {
        free(path);
        return -EIO;
    }

    bool removed = false;
    for (int i = 0; i < 128; i++) {
        if (parent_dirent[i].valid &&
            strcmp(parent_dirent[i].name, pathv[pathc - 1]) == 0) {
            parent_dirent[i].valid = 0;
            removed = true;
            break;
        }
    }

    if (!removed) {
        fprintf(stderr, "Directory not found: %s\n", c_path);
        free(path);
        return -ENOENT;
    }

    if (block_write(parent_dirent, parent_inode.ptrs[0], 1) < 0) {
        fprintf(stderr, "Error writing parent directory entries\n");
        free(path);
        return -EIO;
    }

    bit_clear(bitmap, dir_inode.ptrs[0]); // clear the bitmap for the directory block

    bit_clear(bitmap, dir_inum); // clear the bitmap for the inode itself

    if (block_write(bitmap, 1, 1) < 0) {
        fprintf(stderr, "Error writing bitmap\n");
        free(path);
        return -EIO;
    }

    free(path);
    return 0;
}

/* rename - rename a file or directory
 * success - return 0
 * Errors - path resolution, ENOENT, EINVAL, EEXIST
 *
 * ENOENT - source does not exist
 * EEXIST - destination already exists
 * EINVAL - source and destination are not in the same directory
 *
 * Note that this is a simplified version of the UNIX rename
 * functionality - see 'man 2 rename' for full semantics. In
 * particular, the full version can move across directories, replace a
 * destination file, and replace an empty directory with a full one.
 */
int fs_rename(const char *src_path, const char *dst_path) {
    char *src = strdup(src_path);
    char *dst = strdup(dst_path);

    char *srcv[MAX_PATH_LEN], *dstv[MAX_PATH_LEN];
    int src_pathc = parse(src, srcv);
    int dst_pathc = parse(dst, dstv);

    for (int i = 0; i < src_pathc - 1; i++) {
        if (strcmp(srcv[i], dstv[i]) != 0) {
            free(src);
            free(dst);
            fprintf(stderr, "Source and destination paths do not match\n");
            return -EINVAL;
        } // enforce renaming within the same directory
    }

    int parent_inum = translate(src_pathc - 1, srcv);
    if (parent_inum < 0) {
        free(src);
        free(dst);
        fprintf(stderr, "Error translating source path: %s\n", src_path);
        return parent_inum;
    }

    struct fs_inode parent_inode;
    if (block_read(&parent_inode, parent_inum, 1) < 0) {
        free(src);
        free(dst);
        fprintf(stderr, "Error reading parent inode %d\n", parent_inum);
        return -EIO;
    }

    struct fs_dirent dirent[128];
    if (block_read(&dirent, parent_inode.ptrs[0], 1) < 0) {
        free(src);
        free(dst);
        fprintf(stderr, "Error reading directory entries\n");
        return -EIO;
    }

    bool found_src = false;
    bool found_dst = false;
    int src_idx = -1;

    for (int i = 0; i < 128; i++) {
        if (dirent[i].valid && strcmp(srcv[src_pathc - 1], dirent[i].name) == 0) {
            found_src = true;
            src_idx = i;
        }
        if (dirent[i].valid && strcmp(dstv[dst_pathc - 1], dirent[i].name) == 0) {
            found_dst = true;
        }
    }

    if (!found_src) {
        free(src);
        free(dst);
        fprintf(stderr, "Source file not found: %s\n", srcv[src_pathc - 1]);
        return -ENOENT;
    }
    if (found_dst) {
        free(src);
        free(dst);
        fprintf(stderr, "Destination file already exists: %s\n", dstv[dst_pathc - 1]);
        return -EEXIST;
    }

    strncpy(dirent[src_idx].name, dstv[dst_pathc - 1], MAX_NAME_LEN);
    dirent[src_idx].name[MAX_NAME_LEN] = '\0';

    if (block_write(&dirent, parent_inode.ptrs[0], 1) < 0) {
        free(src);
        free(dst);
        fprintf(stderr, "Error writing directory entries\n");
        return -EIO;
    }

    free(src);
    free(dst);
    return 0;
}

/* chmod - change file permissions
 * utime - change access and modification times
 *         (for definition of 'struct utimebuf', see 'man utime')
 *
 * success - return 0
 * Errors - path resolution, ENOENT.
 */
int fs_chmod(const char *c_path, mode_t mode) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    int inum = translate(pathc, pathv);
    free(path);

    if (inum < 0) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error reading inode %d\n", inum);
        return -EIO;
    }

    inode.mode = (inode.mode & S_IFMT) | (mode & ~S_IFMT);

    if (block_write(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error writing inode %d\n", inum);
        return -EIO;
    }

    return 0;
}

int fs_utime(const char *path, struct utimbuf *ut) {
    /* your code here */
    return -EOPNOTSUPP;
}

/* truncate - truncate file to exactly 'len' bytes
 * success - return 0
 * Errors - path resolution, ENOENT, EISDIR, EINVAL
 *    return EINVAL if len > 0.
 */
int fs_truncate(const char *c_path, off_t len) {
    /* you can cheat by only implementing this for the case of len==0,
     * and an error otherwise.
     */
    if (len != 0)
        return -EINVAL;        /* invalid argument */

    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    int inum = translate(pathc, pathv);
    free(path);

    if (inum < 0) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error reading inode %d\n", inum);
        return -EIO;
    }

    if (S_ISDIR(inode.mode)) {
        fprintf(stderr, "Not a file: %s\n", c_path);
        return -EISDIR;
    }

    int block_num = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int i = 0; i < block_num; i++) {
        bit_clear(bitmap, inode.ptrs[i]);
    }

    if (block_write(bitmap, 1, 1) < 0) {
        fprintf(stderr, "Error writing bitmap\n");
        return -EIO;
    }

    inode.size = 0;
    memset(inode.ptrs, 0, sizeof(inode.ptrs));

    if (block_write(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error writing inode %d\n", inum);
        return -EIO;
    }

    return 0;
}


/* read - read data from an open file.
 * success: should return exactly the number of bytes requested, except:
 *   - if offset >= file len, return 0
 *   - if offset+len > file len, return #bytes from offset to end
 *   - on error, return <0
 * Errors - path resolution, ENOENT, EISDIR
 */
int fs_read(const char *c_path, char *buf, size_t len, off_t offset,
            struct fuse_file_info *fi) {
    char *path = strdup(c_path);
    char *pathv[MAX_PATH_LEN];
    int pathc = parse(path, pathv);
    int inum = translate(pathc, pathv);
    free(path);

    if (inum < 0) {
        return inum;
    }

    struct fs_inode inode;
    if (block_read(&inode, inum, 1) < 0) {
        fprintf(stderr, "Error reading inode %d\n", inum);
        return -EIO;
    }

    if (S_ISDIR(inode.mode)) {
        fprintf(stderr, "Not a file: %s\n", c_path);
        return -EISDIR;
    }

    if (offset >= inode.size) {
        return 0;
    }

    size_t bytes_to_read = len;
    if (offset + len > inode.size) {
        bytes_to_read = inode.size - offset;
    }

    char *file_buf = malloc(BLOCK_SIZE);
    if (file_buf == NULL) {
        fprintf(stderr, "Error allocating memory\n");
        return -ENOMEM;
    }

    int block_num = offset / BLOCK_SIZE;
    int block_offset = offset % BLOCK_SIZE;
    size_t bytes_read = 0;

    while (bytes_read < bytes_to_read) {

        if (block_read(file_buf, inode.ptrs[block_num], 1) < 0) {
            fprintf(stderr, "Error reading block %d\n", inode.ptrs[block_num]);
            free(file_buf);
            return -EIO;
        }

        size_t bytes_to_copy = BLOCK_SIZE - block_offset;
        if (bytes_read + bytes_to_copy > bytes_to_read) {
            bytes_to_copy = bytes_to_read - bytes_read;
        } // adjust for partial read

        memcpy(buf + bytes_read, file_buf + block_offset, bytes_to_copy);
        bytes_read += bytes_to_copy;
        block_num++;
        block_offset = 0;
    }

    free(file_buf);
    return bytes_read;
}

/* write - write data to a file
 * success - return number of bytes written. (this will be the same as
 *           the number requested, or else it's an error)
 * Errors - path resolution, ENOENT, EISDIR
 *  return EINVAL if 'offset' is greater than current file length.
 *  (POSIX semantics support the creation of files with "holes" in them, 
 *   but we don't)
 */
int fs_write(const char *path, const char *buf, size_t len,
             off_t offset, struct fuse_file_info *fi) {
    /* your code here */
    return -EOPNOTSUPP;
}

/* statfs - get file system statistics
 * see 'man 2 statfs' for description of 'struct statvfs'.
 * Errors - none. Needs to work.
 */
int fs_statfs(const char *path, struct statvfs *st) {
    /* needs to return the following fields (set others to zero):
     *   f_bsize = BLOCK_SIZE
     *   f_blocks = total image - (superblock + block map)
     *   f_bfree = f_blocks - blocks used
     *   f_bavail = f_bfree
     *   f_namemax = <whatever your max namelength is>
     *
     * it's OK to calculate this dynamically on the rare occasions
     * when this function is called.
     */

    memset(st, 0, sizeof(struct statvfs));
    struct fs_super sb;
    if (block_read(&sb, 0, 1) < 0) {
        fprintf(stderr, "Error reading superblock\n");
        return -EIO;
    }
    int disk_size = sb.disk_size;
    st->f_bsize = BLOCK_SIZE;
    st->f_blocks = disk_size - 2;

    int used = 0;
    for (int i = 0; i < MAX_BLOCKS; i++) {
        if (bit_test(bitmap, i)) {
            used++;
        }
    }
    printf("Used blocks: %d\n", used);

    st->f_bfree = st->f_blocks - (used - 2);
    st->f_bavail = st->f_bfree;
    st->f_namemax = MAX_NAME_LEN;

    return 0;
}

/* operations vector. Please don't rename it, or else you'll break things
 */
struct fuse_operations fs_ops = {
        .init = fs_init,            /* read-mostly operations */
        .getattr = fs_getattr,
        .readdir = fs_readdir,
        .rename = fs_rename,
        .chmod = fs_chmod,
        .read = fs_read,
        .statfs = fs_statfs,

        .create = fs_create,        /* write operations */
        .mkdir = fs_mkdir,
        .unlink = fs_unlink,
        .rmdir = fs_rmdir,
        .utime = fs_utime,
        .truncate = fs_truncate,
        .write = fs_write,
};

