/*
 * file:        unittest-2.c
 * description: libcheck test skeleton, part 2
 */

#define _FILE_OFFSET_BITS 64
#define FUSE_USE_VERSION 26

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <check.h>
#include <zlib.h>
#include <fuse.h>
#include <stdlib.h>
#include <errno.h>

extern struct fuse_operations fs_ops;
extern void block_init(char *file);

/* mockup for fuse_get_context. you can change ctx.uid, ctx.gid in
 * tests if you want to test setting UIDs in mknod/mkdir
 */
struct fuse_context ctx = { .uid = 500, .gid = 500};
struct fuse_context *fuse_get_context(void)
{
    return &ctx;
}

/* directory entry used for tests */
struct {
    const char* name;
    int seen;
} dirent[16];

/* test_filler is a callback function for readdir. it checks
 * that the directory entries are seen in the correct order.
 * it is used to check that the directories are created and
 * removed correctly.
 */
int test_filler(void *ptr, const char *name, const struct stat *st, off_t off) {
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
        return 0;

    for (int i = 0; dirent[i].name != NULL; i++) {
        if (strcmp(name, dirent[i].name) == 0) {
            ck_assert_int_eq(dirent[i].seen, 0);
            dirent[i].seen = 1;
            return 0;
        }
    }

    return 0;
}

int print_filler(void *ptr, const char *name, const struct stat *st, off_t off) {
    if (strcmp(name, ".") != 0 && strcmp(name, "..") != 0)
        printf("  -> %s\n", name);
    return 0;
}

/* create a directory and check that it is visible in readdir
 * then remove it and check that it is no longer visible
 */
START_TEST(test_mkdir_rmdir_flat) {
    const char *dir_path[] = {"/A", "/B", "/C", "/D", NULL};
    for (int i = 0; dir_path[i] != NULL; i++) {
        int rv = fs_ops.mkdir(dir_path[i], 0777);
        printf("(test_mkdir_rmdir_flat) mkdir %s rv: %d\n", dir_path[i], rv);
        ck_assert_int_eq(rv, 0);
    }

    memset(dirent, 0, sizeof(dirent));
    dirent[0].name = "A";
    dirent[0].seen = 0;
    dirent[1].name = "B";
    dirent[1].seen = 0;
    dirent[2].name = "C";
    dirent[2].seen = 0;
    dirent[3].name = "D";
    dirent[3].seen = 0;
    dirent[4].name = NULL;
    dirent[4].seen = 0;

    printf("Root directory before test:\n");
    fs_ops.readdir("/", NULL, print_filler, 0, NULL);

    int rv = fs_ops.readdir("/", NULL, test_filler, 0, NULL);
    printf("(test_mkdir_rmdir_flat) readdir rv: %d\n", rv);
    ck_assert_int_eq(rv, 0);

    for (int j = 0; dirent[j].name != NULL; j++) {
        ck_assert_msg(dirent[j].seen == 1,
                      "Directory %s not found in readdir", dirent[j].name);
    }

    for (int i = 0; dir_path[i] != NULL; i++) {
        rv = fs_ops.rmdir(dir_path[i]);
        printf("(test_mkdir_rmdir_flat) rmdir %s rv: %d\n", dir_path[i], rv);
        fs_ops.readdir("/", NULL, print_filler, 0, NULL);
        ck_assert_int_eq(rv, 0);
    }

    // Confirm the directories are removed using getattr
    struct stat sb;
    for (int i = 0; dir_path[i] != NULL; i++) {
        rv = fs_ops.getattr(dir_path[i], &sb);
        printf("(test_mkdir_rmdir_flat) getattr %s rv: %d\n", dir_path[i], rv);
        ck_assert_msg(rv == -ENOENT, "Expected -ENOENT after rmdir %s, got %d", dir_path[i], rv);
    }

    // Check that readdir no longer sees the directories
    memset(dirent, 0, sizeof(dirent));
    rv = fs_ops.readdir("/", NULL, test_filler, 0, NULL);
    ck_assert_int_eq(rv, 0);
    for (int i = 0; i < 4; i++) {
        ck_assert_msg(dirent[i].seen == 0, "Directory %s still visible in readdir after rmdir", dirent[i].name);
    }
}
END_TEST

/* create a directory with a nested structure and check that
 * the directories are visible in readdir. then remove them
 * and check that they are no longer visible.
 */
START_TEST(test_mkdir_rmdir_nested) {
    ck_assert_int_eq(fs_ops.mkdir("/X", 0777), 0);
    ck_assert_int_eq(fs_ops.mkdir("/X/Y", 0777), 0);
    ck_assert_int_eq(fs_ops.mkdir("/X/Y/Z", 0777), 0);

    memset(dirent, 0, sizeof(dirent));
    dirent[0].name = "Y";
    dirent[0].seen = 0;
    dirent[1].name = NULL;
    dirent[1].seen = 0;

    ck_assert_int_eq(fs_ops.readdir("/X", NULL, test_filler, 0, NULL), 0);
    for (int i = 0; dirent[i].name != NULL; i++) {
        printf("(test_mkdir_rmdir_nested) readdir /X: %s, seen: %d\n",
               dirent[i].name, dirent[i].seen);
        ck_assert_msg(dirent[i].seen == 1,
                      "Directory %s not found in readdir /X", dirent[i].name);
    }

    memset(dirent, 0, sizeof(dirent));
    dirent[0].name = "Z";
    dirent[0].seen = 0;
    dirent[1].name = NULL;
    dirent[1].seen = 0;

    ck_assert_int_eq(fs_ops.readdir("/X/Y", NULL, test_filler, 0, NULL), 0);
    for (int i = 0; dirent[i].name != NULL; i++) {
        printf("(test_mkdir_rmdir_nested) readdir /X/Y: %s, seen: %d\n",
               dirent[i].name, dirent[i].seen);
        ck_assert_msg(dirent[i].seen == 1,
                      "Directory %s not found in readdir /X/Y", dirent[i].name);
    }

    ck_assert_int_eq(fs_ops.rmdir("/X/Y/Z"), 0);
    ck_assert_int_eq(fs_ops.rmdir("/X/Y"), 0);
    ck_assert_int_eq(fs_ops.rmdir("/X"), 0);

    // Check that the directories are removed
    struct stat sb;
    int rv = fs_ops.getattr("/X/Y/Z", &sb);
    printf("(test_mkdir_rmdir_nested) getattr /X/Y/Z rv: %d\n", rv);
    ck_assert_msg(rv == -ENOENT, "Expected -ENOENT after rmdir /X/Y/Z, got %d", rv);
    rv = fs_ops.getattr("/X/Y", &sb);
    printf("(test_mkdir_rmdir_nested) getattr /X/Y rv: %d\n", rv);
    ck_assert_msg(rv == -ENOENT, "Expected -ENOENT after rmdir /X/Y, got %d", rv);
    rv = fs_ops.getattr("/X", &sb);
    printf("(test_mkdir_rmdir_nested) getattr /X rv: %d\n", rv);
    ck_assert_msg(rv == -ENOENT, "Expected -ENOENT after rmdir /X, got %d", rv);

    // Check that readdir no longer sees the directories
    memset(dirent, 0, sizeof(dirent));
    rv = fs_ops.readdir("/", NULL, test_filler, 0, NULL);
    ck_assert_int_eq(rv, 0);
    for (int i = 0; i < 2; i++) {
        ck_assert_msg(dirent[i].seen == 0, "Directory %s still visible in readdir after rmdir", dirent[i].name);
    }
}
END_TEST

/* create a flat structure of files and check that
 * the files are visible in readdir. then remove them
 * and check that they are no longer visible.
 */
START_TEST(test_create_unlink_flat) {
    const char *file_path[] = {"/file1", "/file2", "/file3", "/file4", NULL};
    for (int i = 0; file_path[i] != NULL; i++) {
        int rv = fs_ops.create(file_path[i], S_IFREG | 0777, NULL);
        printf("(test_create_unlink_flat) create %s rv: %d\n", file_path[i], rv);
        ck_assert_int_eq(rv, 0);
    }
    memset(dirent, 0, sizeof(dirent));
    dirent[0].name = "file1";
    dirent[0].seen = 0;
    dirent[1].name = "file2";
    dirent[1].seen = 0;
    dirent[2].name = "file3";
    dirent[2].seen = 0;
    dirent[3].name = "file4";
    dirent[3].seen = 0;
    dirent[4].name = NULL;
    dirent[4].seen = 0;

    printf("(test_create_unlink_flat) Root directory before test:\n");
    fs_ops.readdir("/", NULL, print_filler, 0, NULL);

    int rv = fs_ops.readdir("/", NULL, test_filler, 0, NULL);
    printf("(test_create_unlink_flat) readdir rv: %d\n", rv);
    ck_assert_int_eq(rv, 0);
    for (int j = 0; dirent[j].name != NULL; j++) {
        ck_assert_msg(dirent[j].seen == 1,
                      "File %s not found in readdir", dirent[j].name);
    }

    for (int i = 0; file_path[i] != NULL; i++) {
        rv = fs_ops.unlink(file_path[i]);
        printf("(test_create_unlink_flat) unlink %s rv: %d\n", file_path[i], rv);
        fs_ops.readdir("/", NULL, print_filler, 0, NULL);
        ck_assert_int_eq(rv, 0);
    }

    // Confirm the files are unlinked using getattr
    struct stat sb;
    for (int i = 0; file_path[i] != NULL; i++) {
        rv = fs_ops.getattr(file_path[i], &sb);
        printf("(test_create_unlink_flat) getattr %s rv: %d\n", file_path[i], rv);
        ck_assert_msg(rv == -ENOENT, "Expected -ENOENT after unlinking %s, got %d", file_path[i], rv);
    }

    // Check that readdir no longer sees the files
    memset(dirent, 0, sizeof(dirent));
    rv = fs_ops.readdir("/", NULL, test_filler, 0, NULL);
    printf("(test_create_unlink_flat) readdir after unlink rv: %d\n", rv);
    ck_assert_int_eq(rv, 0);
    for (int i = 0; i < 4; i++) {
        ck_assert_msg(dirent[i].seen == 0, "File %s still visible in readdir after unlink", dirent[i].name);
    }
}
END_TEST

/* create a directory with a nested structure and check that
 * the files are visible in readdir. then remove them
 * and check that they are no longer visible.
 */
START_TEST(test_create_unlink_nested) {
    const char *dirA = "/dirA";
    const char *dirB = "/dirA/dirB";
    const char *file_path[] = {"/dirA/dirB/file1", "/dirA/dirB/file2", "/dirA/dirB/file3", "/dirA/dirB/file4", NULL};

    ck_assert_int_eq(fs_ops.mkdir(dirA, 0777), 0);
    ck_assert_int_eq(fs_ops.mkdir(dirB, 0777), 0);

    for (int i = 0; file_path[i] != NULL; i++) {
        int rv = fs_ops.create(file_path[i], S_IFREG | 0777, NULL);
        printf("(test_create_unlink_nested) create %s rv: %d\n", file_path[i], rv);
        ck_assert_int_eq(rv, 0);
    }

    memset(dirent, 0, sizeof(dirent));
    dirent[0].name = "file1";
    dirent[0].seen = 0;
    dirent[1].name = "file2";
    dirent[1].seen = 0;
    dirent[2].name = "file3";
    dirent[2].seen = 0;
    dirent[3].name = "file4";
    dirent[3].seen = 0;
    dirent[4].name = NULL;
    dirent[4].seen = 0;

    int rv = fs_ops.readdir("/dirA/dirB", NULL, test_filler, 0, NULL);
    ck_assert_int_eq(rv, 0);

    for (int i = 0; dirent[i].name != NULL; i++) {
        printf("(test_create_unlink_nested) readdir /dirA/dirB: %s, seen: %d\n",
               dirent[i].name, dirent[i].seen);
        ck_assert_msg(dirent[i].seen == 1,
                      "File %s not found in readdir /dirA/dirB", dirent[i].name);
    }

    for (int i = 0; file_path[i] != NULL; i++) {
        int rv = fs_ops.unlink(file_path[i]);
        printf("(test_create_unlink_nested) unlink %s rv: %d\n", file_path[i], rv);
        ck_assert_int_eq(rv, 0);
    }

    // Confirm the files are unlinked using getattr
    struct stat sb;
    for (int i = 0; file_path[i] != NULL; i++) {
        rv = fs_ops.getattr(file_path[i], &sb);
        printf("(test_create_unlink_nested) getattr %s rv: %d\n", file_path[i], rv);
        ck_assert_msg(rv == -ENOENT, "Expected -ENOENT after unlinking %s, got %d", file_path[i], rv);
    }

    // Check that readdir no longer sees the files
    memset(dirent, 0, sizeof(dirent));
    rv = fs_ops.readdir("/dirA/dirB", NULL, test_filler, 0, NULL);
    ck_assert_int_eq(rv, 0);

    for (int i = 0; i < 4; i++) {
        ck_assert_msg(dirent[i].seen == 0, "File %s still visible in readdir after unlink", dirent[i].name);
    }


}
END_TEST

/* test multiple cases of errors with fs_create
 * 1. create a file in a non-existent directory
 * 2. create a file instead of a directory
 * 3. create a file that already exists
 * 4. create a file with the same name as a directory
 * 5. check for long filename error
 */
START_TEST(test_create_error) {
    system("python gen-disk.py -q disk2.in test2.img");
    block_init("test2.img");
    fs_ops.init(NULL);
    int rv;

    rv = fs_ops.create("/a/b/c", S_IFREG | 0777, NULL);
    printf("(test_create_error) create /a/b/c with missing /a/b => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to create a file in a non-existent directory

    ck_assert_int_eq(fs_ops.create("/notadirectory", S_IFREG | 0777, NULL), 0); // create a file instead of a directory
    rv = fs_ops.create("/notadirectory/file", 0777, NULL); // try to create a file in a non-directory
    printf("(test_create_error) create /notadirectory/file => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTDIR);

    ck_assert_int_eq(fs_ops.create("/file", S_IFREG | 0777, NULL), 0);
    rv = fs_ops.create("/file", 0777, NULL);
    printf("(test_create_error) create /file => rv: %d\n", rv);
    ck_assert_int_eq(rv, -EEXIST); // try to create a file that already exists

    ck_assert_int_eq(fs_ops.mkdir("/dir", 0777), 0);
    rv = fs_ops.create("/dir", 0777, NULL);
    printf("(test_create_error) create /dir => rv: %d\n", rv);
    ck_assert_int_eq(rv, -EEXIST); // try to create a file with the same name as a directory

    rv = fs_ops.create("/looooooooooo000000000000gname", S_IFREG | 0777, NULL);
    printf("(test_create_error) create with long name /looooooooooo000000000000gname => rv: %d\n", rv);
    ck_assert(rv == -EINVAL);  // check for long filename error
}
END_TEST

/* test multiple cases of errors with fs_unlink
 * 1. unlink a file in a non-existent directory
 * 2. unlink a file in a non-directory
 * 3. unlink a file not in a directory
 * 4. unlink a directory
 */
START_TEST(test_unlink_error) {
    system("python gen-disk.py -q disk2.in test2.img");
    block_init("test2.img");
    fs_ops.init(NULL);
    int rv;
    ck_assert_int_eq(fs_ops.create("/notadir", S_IFREG | 0777, NULL), 0);

    rv = fs_ops.unlink("/no/such/file");
    printf("(test_unlink_error) unlink /no/such/file => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to unlink a non-existent file

    rv = fs_ops.unlink("/notadir/file");
    printf("(test_unlink_error) unlink /notadir/file => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTDIR); // try to unlink a file in a non-directory

    ck_assert_int_eq(fs_ops.mkdir("/dir", 0777), 0);
    rv = fs_ops.unlink("/dir/nosuchfile");
    printf("(test_unlink_error) unlink /dir/nosuchfile => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to unlink a file not in a directory

    ck_assert_int_eq(fs_ops.mkdir("/dir/dir2", 0777), 0);
    rv = fs_ops.unlink("/dir/dir2");
    printf("(test_unlink_error) unlink /dir/dir2 => rv: %d\n", rv);
    ck_assert_int_eq(rv, -EISDIR); // try to unlink a directory
}
END_TEST

/* test multiple cases of errors with fs_mkdir
 * 1. mkdir a directory in a non-existent path
 * 2. mkdir a file instead of a directory
 * 3. mkdir a directory that already exists
 * 4. mkdir a directory with the same name as an existing file
 * 5. mkdir a directory with the same name as an existing directory
 * 6. check for long directory name error
 */
START_TEST(test_mkdir_error) {
    system("python gen-disk.py -q disk2.in test2.img");
    block_init("test2.img");
    fs_ops.init(NULL);
    int rv;

    rv = fs_ops.mkdir("/a/b/c", 0777);
    printf("(test_mkdir_error) mkdir /a/b/c with missing /a/b => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to create a directory in a non-existent path

    ck_assert_int_eq(fs_ops.create("/notadir", S_IFREG | 0777, NULL), 0);
    rv = fs_ops.mkdir("/notadir/file", 0777);
    printf("(test_mkdir_error) mkdir /notadir/file => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTDIR); // try to create a directory with the same name as a file

    ck_assert_int_eq(fs_ops.create("/existingfile", S_IFREG | 0777, NULL), 0);
    rv = fs_ops.mkdir("/existingfile", 0777);
    printf("(test_mkdir_error) mkdir /existingfile => rv: %d\n", rv);
    ck_assert_int_eq(rv, -EEXIST); // try to create a directory with the same name as an existing file

    ck_assert_int_eq(fs_ops.mkdir("/existingdir", 0777), 0);
    rv = fs_ops.mkdir("/existingdir", 0777);
    printf("(test_mkdir_error) mkdir /existingdir => rv: %d\n", rv);
    ck_assert_int_eq(rv, -EEXIST); // try to create a directory with the same name as an existing directory

    rv = fs_ops.mkdir("/looooooooooo000000000000gname", 0777);
    printf("(test_mkdir_error) mkdir with long name /looooooooooo000000000000gname => rv: %d\n", rv);
    ck_assert(rv == -EINVAL);  // check for long directory name error
}
END_TEST

/* test multiple cases of errors with fs_rmdir
 * 1. rmdir a directory in a non-existent path
 * 2. rmdir a file instead of a directory
 * 3. rmdir a directory that does not exist
 * 4. rmdir a file instead of a directory
 * 5. rmdir a non-empty directory
 */
START_TEST(test_rmdir_error) {
    system("python gen-disk.py -q disk2.in test2.img");
    block_init("test2.img");
    fs_ops.init(NULL);
    int rv;

    rv = fs_ops.rmdir("/a/b/c");
    printf("(test_rmdir_error) rmdir /a/b/c where b is missing => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to remove a directory in a non-existent path

    ck_assert_int_eq(fs_ops.create("/notadir", S_IFREG | 0777, NULL), 0);
    rv = fs_ops.rmdir("/notadir/file");
    printf("(test_rmdir_error) rmdir /notadir/file where parent is not a directory => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTDIR); // try to remove a file in a non-directory

    ck_assert_int_eq(fs_ops.mkdir("/dir", 0777), 0);
    rv = fs_ops.rmdir("/dir/nosuch");
    printf("(test_rmdir_error) rmdir /dir/nosuch which does not exist => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to remove a non-existent directory

    ck_assert_int_eq(fs_ops.create("/dir/file", S_IFREG | 0777, NULL), 0);
    rv = fs_ops.rmdir("/dir/file");
    printf("(test_rmdir_error) rmdir /dir/file which is not a dir => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTDIR); // try to remove a file instead of a directory

    ck_assert_int_eq(fs_ops.mkdir("/nonempty", 0777), 0);
    ck_assert_int_eq(fs_ops.create("/nonempty/file", S_IFREG | 0777, NULL), 0);
    rv = fs_ops.rmdir("/nonempty");
    printf("(test_rmdir_error) rmdir /nonempty which is not empty => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTEMPTY); // try to remove a non-empty directory
}
END_TEST

/* generate a buffer of numbers starting from 'start' and
 * of length 'len'. used to generate test data for write tests
 */
char *test_generate(int start, int len) {
    char *buf = malloc(len + 10);
    char *ptr = buf;
    int i = start;
    while (ptr - buf < len) {
        ptr += sprintf(ptr, "%d ", i++);
    }
    return buf;
}

/* actual testing function of writing and reading a file. the file is created
 * with the given path and size. the contents are generated
 * using test_generate. the file is then read back and
 * compared to the original contents.
 */
void test_write_and_read(const char *path, int size, int seed) {
    ck_assert_int_eq(fs_ops.create(path, S_IFREG | 0777, NULL), 0);

    char *buf = test_generate(seed, size);
    printf("(test_write_and_read) write %s size: %d\n", path, size);
    ck_assert_int_eq(fs_ops.write(path, buf, size, 0, NULL), size);

    char *read_buf = malloc(size);
    printf("(test_write_and_read) read %s size: %d\n", path, size);
    ck_assert_int_eq(fs_ops.read(path, read_buf, size, 0, NULL), size);
    printf("(test_write_and_read) compare buf and read_buf\n");
    ck_assert_int_eq(memcmp(buf, read_buf, size), 0);

    free(buf);
    free(read_buf);
}

/* test writing and reading a file with different sizes.
 * call test_write_and_read with different sizes
 */
START_TEST(test_write_read) {
    test_write_and_read("/file1k", 1000, 1);
    test_write_and_read("/file4k", 4096, 2);
    test_write_and_read("/file6k", 6000, 3);
    test_write_and_read("/file8k", 8192, 4);
    test_write_and_read("/file12k", 12000, 5);
    test_write_and_read("/file12k_exact", 12288, 6);
}
END_TEST

/* test writing and overwriting a file. the file is created
 * with the given path and size. the contents are generated
 * using test_generate. the file is then written to and
 * read back and compared to the original contents.
 */
void test_write_and_overwrite(const char *path, int size, int seed1, int seed2) {
    ck_assert_int_eq(fs_ops.create(path, S_IFREG | 0777, NULL), 0);

    char *buf1 = test_generate(seed1, size);
    char *buf2 = test_generate(seed2, size);

    ck_assert_int_eq(fs_ops.write(path, buf1, size, 0, NULL), size);
    ck_assert_int_eq(fs_ops.write(path, buf2, size, 0, NULL), size);

    char *read_buf = malloc(size);
    int rv = fs_ops.read(path, read_buf, size, 0, NULL);
    printf("(overwrite check) read %s rv: %d\n", path, rv);
    ck_assert_int_eq(rv, size);
    printf("(overwrite check) compare buf2 and read_buf\n");
    ck_assert_int_eq(memcmp(buf2, read_buf, size), 0);

    free(buf1);
    free(buf2);
    free(read_buf);
}

/* test writing and overwriting a file with different sizes.
 * call test_write_and_overwrite with different sizes
 */
START_TEST(test_write_overwrite) {
    test_write_and_overwrite("/overwrite1k", 1000, 1, 11);
    test_write_and_overwrite("/overwrite4k", 4096, 2, 12);
    test_write_and_overwrite("/overwrite6k", 6000, 3, 13);
    test_write_and_overwrite("/overwrite8k", 8192, 4, 14);
    test_write_and_overwrite("/overwrite12k", 12000, 5, 15);
    test_write_and_overwrite("/overwrite12k_exact", 12288, 6, 16);
}
END_TEST

/* test writing and unlinking a file. the file is created
 * with the given path and size. the contents are generated
 * using test_generate. the file is then unlinked and
 * the statfs is checked before and after to see if the
 * free space is the same.
 */
void test_write_and_unlink_block(const char *path, int size) {
    struct statvfs sv_before, sv_after;
    ck_assert_int_eq(fs_ops.statfs("/", &sv_before), 0);
    printf("(test_write_and_unlink_block) statfs before: %lu\n", sv_before.f_bfree);

    char *buf = test_generate(0, size);
    ck_assert_int_eq(fs_ops.create(path, S_IFREG | 0777, NULL), 0);
    ck_assert_int_eq(fs_ops.write(path, buf, size, 0, NULL), size);

    ck_assert_int_eq(fs_ops.unlink(path), 0);
    ck_assert_int_eq(fs_ops.statfs("/", &sv_after), 0);
    printf("(test_write_and_unlink_block) statfs after: %lu\n", sv_after.f_bfree);

    ck_assert_int_eq(sv_before.f_bfree, sv_after.f_bfree);
    free(buf);
}

/* test writing and unlinking a file with different sizes.
 * call test_write_and_unlink_block with different sizes
 */
START_TEST(test_write_unlink_block) {
    test_write_and_unlink_block("/block1k", 1000);
    test_write_and_unlink_block("/block4k", 4096);
    test_write_and_unlink_block("/block6k", 6000);
    test_write_and_unlink_block("/block8k", 8192);
    test_write_and_unlink_block("/block12k", 12000);
    test_write_and_unlink_block("/block12k_exact", 12288);
}

/* test different cases of errors with fs_write
 */
START_TEST(test_write_error) {
    char *buf = test_generate(0, 1000);
    int rv = fs_ops.write("/nosuchfile", buf, 1000, 0, NULL);
    printf("(test_write_error) write /nosuchfile => rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT); // try to write to a non-existent file

    ck_assert_int_eq(fs_ops.mkdir("/notafile", 0777), 0);
    rv = fs_ops.write("/notafile", buf, 1000, 0, NULL);
    printf("(test_write_error) write /notafile => rv: %d\n", rv);
    ck_assert_int_eq(rv, -EISDIR); // try to write to a directory instead of a file

    free(buf);
}
END_TEST

/* test fs_truncate by creating files of different sizes
 * and truncating them to zero. check that the
 * space has been freed and that the file size is zero
 */
START_TEST(test_truncate) {
    system("python gen-disk.py -q disk2.in test2.img");
    block_init("test2.img");
    fs_ops.init(NULL);
    const char *paths[] = {"/file1k", "/file4k", "/file6k", "/file8k", "/file12k", "/file12k_exact", NULL};
    int sizes[] = {1000, 4096, 6000, 8192, 12000, 12288};

    for (int i = 0; paths[i] != NULL; i++) {
        struct statvfs sv_before;
        printf("(test_truncate) statfs before: %s\n", paths[i]);
        ck_assert_int_eq(fs_ops.statfs("/", &sv_before), 0);
        printf("(test_truncate) before free blocks: %d\n", sv_before.f_bfree);
        int before_free = sv_before.f_bfree;

        ck_assert_int_eq(fs_ops.create(paths[i], S_IFREG | 0777, NULL), 0);
        printf("(test_truncate) write %s size: %d\n", paths[i], sizes[i]);
        char *buf = test_generate(0, sizes[i]);
        ck_assert_int_eq(fs_ops.write(paths[i], buf, sizes[i], 0, NULL), sizes[i]);

        ck_assert_int_eq(fs_ops.truncate(paths[i], 0), 0); // truncate to zero

        struct stat sb;
        printf("(test_truncate) getattr %s\n", paths[i]);
        ck_assert_int_eq(fs_ops.getattr(paths[i], &sb), 0);
        ck_assert_int_eq(sb.st_size, 0);

        char *read_buf = malloc(sizes[i]);
        int rv = fs_ops.read(paths[i], read_buf, sizes[i], 0, NULL);
        printf("(test_truncate) read %s rv: %d\n", paths[i], rv);
        ck_assert_int_eq(rv, -EINVAL); // read should fail after truncation

        struct statvfs sv_after;
        printf("(test_truncate) statfs after: %s\n", paths[i]);
        ck_assert_int_eq(fs_ops.statfs("/", &sv_after), 0);
        printf("(test_truncate) after free blocks: %d\n", sv_after.f_bfree);
        int after_free = sv_after.f_bfree;

        // check that the free space has increased
        ck_assert_msg(after_free >= before_free, "Expected more free blocks after truncation");

        free(buf);
        free(read_buf);
    }
}
END_TEST

/* this is an example of a callback function for readdir
 */
int empty_filler(void *ptr, const char *name, const struct stat *stbuf,
                 off_t off)
{
    /* FUSE passes you the entry name and a pointer to a 'struct stat' 
     * with the attributes. Ignore the 'ptr' and 'off' arguments 
     * 
     */
    return 0;
}

/* note that your tests will call:
 *  fs_ops.getattr(path, struct stat *sb)
 *  fs_ops.readdir(path, NULL, filler_function, 0, NULL)
 *  fs_ops.read(path, buf, len, offset, NULL);
 *  fs_ops.statfs(path, struct statvfs *sv);
 */

int main(int argc, char **argv)
{
    system("python gen-disk.py -q disk2.in test2.img");
    block_init("test2.img");
    fs_ops.init(NULL);
    
    Suite *s = suite_create("fs5600");
    TCase *tc = tcase_create("write_mostly");

    tcase_add_test(tc, test_mkdir_rmdir_flat);
    tcase_add_test(tc, test_mkdir_rmdir_nested);
    tcase_add_test(tc, test_create_unlink_flat);
    tcase_add_test(tc, test_create_unlink_nested);
    tcase_add_test(tc, test_create_error);
    tcase_add_test(tc, test_unlink_error);
    tcase_add_test(tc, test_mkdir_error);
    tcase_add_test(tc, test_rmdir_error);
    tcase_add_test(tc, test_write_read);
    tcase_add_test(tc, test_write_overwrite);
    tcase_add_test(tc, test_write_unlink_block);
    tcase_add_test(tc, test_write_error);
    tcase_add_test(tc, test_truncate);

    suite_add_tcase(s, tc);
    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    
    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);
    
    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

