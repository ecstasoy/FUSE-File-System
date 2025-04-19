/*
 * file:        testing.c
 * description: libcheck test skeleton for file system project
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

/* test data for getattr */
struct {
    const char *path;
    uint16_t uid;
    uint16_t gid;
    uint32_t mode;
    uint32_t size;
    uint32_t ctime;
    uint32_t mtime;
} getattr_test[] = {
        {"/", 0, 0, 040777, 4096, 1565283152, 1565283167},
        {"/file.1k", 500, 500, 0100666, 1000, 1565283152, 1565283152},
        {"/file.10", 500, 500, 0100666, 10, 1565283152, 1565283167},
        {"/dir-with-long-name", 0, 0, 040777, 4096, 1565283152, 1565283167},
        {"/dir-with-long-name/file.12k+", 0, 500, 0100666, 12289, 1565283152, 1565283167},
        {"/dir2", 500, 500, 040777, 8192, 1565283152, 1565283167},
        {"/dir2/twenty-seven-byte-file-name", 500, 500, 0100666, 1000, 1565283152, 1565283167},
        {"/dir2/file.4k+", 500, 500, 0100777, 4098, 1565283152, 1565283167},
        {"/dir3", 0, 500, 040777, 4096, 1565283152, 1565283167},
        {"/dir3/subdir", 0, 500, 040777, 4096, 1565283152, 1565283167},
        {"/dir3/subdir/file.4k-", 500, 500, 0100666, 4095, 1565283152, 1565283167},
        {"/dir3/subdir/file.8k-", 500, 500, 0100666, 8190, 1565283152, 1565283167},
        {"/dir3/subdir/file.12k", 500, 500, 0100666, 12288, 1565283152, 1565283167},
        {"/dir3/file.12k-", 0, 500, 0100777, 12287, 1565283152, 1565283167},
        {"/file.8k+", 500, 500, 0100666, 8195, 1565283152, 1565283167},
        {NULL, 0, 0, 0, 0, 0, 0}

};

/* test that getattr works for all paths */
START_TEST(test_getattr_paths) {
    for (int i = 0; getattr_test[i].path != NULL; i++) {
        printf("getattr_test[%d]: %s\n", i, getattr_test[i].path);
        struct stat sb;
        int rv = fs_ops.getattr(getattr_test[i].path, &sb);
        ck_assert_int_eq(rv, 0);
        ck_assert_int_eq(sb.st_size, getattr_test[i].size);
        ck_assert_int_eq(sb.st_uid, getattr_test[i].uid);
        ck_assert_int_eq(sb.st_gid, getattr_test[i].gid);
        ck_assert_int_eq(sb.st_mode & S_IFMT, getattr_test[i].mode & S_IFMT);
        ck_assert_int_eq(sb.st_ctime, getattr_test[i].ctime);
        ck_assert_int_eq(sb.st_mtime, getattr_test[i].mtime);
        ck_assert_int_eq(sb.st_atime, getattr_test[i].mtime);
    }
}
END_TEST

/* test that getattr fails for invalid paths */
START_TEST(test_getattr_not_a_file) {
    struct stat sb;
    int rv = fs_ops.getattr("/not-a-file", &sb);
    printf("(test_getattr_not_a_file) rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT);
}
END_TEST

/* test getattr fails for paths that are not directories */
START_TEST(test_getattr_not_a_dir) {
    struct stat sb;
    int rv = fs_ops.getattr("/file.1k/file.0", &sb);
    printf("(test_getattr_not_a_dir) rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOTDIR);
}
END_TEST

/* test getattr fails for paths that are not directories */
START_TEST(test_getattr_middle_missing) {
    struct stat sb;
    int rv = fs_ops.getattr("/not-a-dir/file.0", &sb);
    printf("(test_getattr_middle_missing) rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT);
}
END_TEST

/* test getattr fails for paths that are not directories */
START_TEST(test_getattr_subdir_missing) {
    struct stat sb;
    int rv = fs_ops.getattr("/dir2/not-a-file", &sb);
    printf("(test_getattr_subdir_missing) rv: %d\n", rv);
    ck_assert_int_eq(rv, -ENOENT);
}
END_TEST

/* directory entry for tests */
struct {
    const char* name;
    int seen;
} dirent[16];

struct {
    const char *path;
    const char *entries[16];
} readdir_test[] = {
        {"/", {"dir2", "dir3", "dir-with-long-name", "file.10", "file.1k", "file.8k+", NULL}},
        {"/dir2", {"twenty-seven-byte-file-name", "file.4k+", NULL}},
        {"/dir3", {"subdir", "file.12k-", NULL}},
        {"/dir3/subdir", {"file.4k-", "file.8k-", "file.12k", NULL}},
        {"/dir-with-long-name", {"file.12k+", NULL}},
        {NULL}
};

void test_load(const char **names) {
    int i = 0;
    while (names[i] != NULL) {
        dirent[i].name = names[i];
        dirent[i].seen = 0;
        i++;
    }
    dirent[i].name = NULL;
}

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

/* test that readdir works for all paths */
START_TEST(test_readdir_all) {
    for (int i = 0; readdir_test[i].path != NULL; i++) {
        test_load(readdir_test[i].entries);
        int rv = fs_ops.readdir(readdir_test[i].path, NULL, test_filler, 0, NULL);
        printf("(test_readdir_all) rv: %d\n", rv);
        ck_assert_int_eq(rv, 0);
        for (int j = 0; dirent[j].name != NULL; j++) {
            printf("(test_readdir_all) dirent[%d]: %s, seen: %d\n",
                   j, dirent[j].name, dirent[j].seen);
            ck_assert_int_eq(dirent[j].seen, 1);
        }
    }
}
END_TEST

/* test different error cases for readdir */
START_TEST(test_readdir_errors){
    int rv1 = fs_ops.readdir("/file.1k", NULL, test_filler, 0, NULL);
    printf("(test_readdir_errors) rv: %d\n", rv1);
    ck_assert_msg(rv1 == -ENOTDIR, "Expected -ENOTDIR, got %d", rv1);

    int rv2 = fs_ops.readdir("/no/such/path", NULL, test_filler, 0, NULL);
    printf("(test_readdir_errors) rv: %d\n", rv2);
    ck_assert_msg(rv2 == -ENOENT, "Expected -ENOENT, got %d", rv2);
}
END_TEST

/* test data for read */
struct {
    const char *path;
    size_t size;
    unsigned cksum;
} read_test[] = {
        {"/file.1k", 1000, 1726121896},
        {"/file.10", 10, 3766980606},
        {"/dir-with-long-name/file.12k+", 12289, 2781093465},
        {"/dir2/twenty-seven-byte-file-name", 1000, 2902524398},
        {"/dir2/file.4k+", 4098, 1626046637},
        {"/dir3/subdir/file.4k-", 4095, 2991486384},
        {"/dir3/subdir/file.8k-", 8190, 724101859},
        {"/dir3/subdir/file.12k", 12288, 1483119748},
        {"/dir3/file.12k-", 12287, 1203178000},
        {"/file.8k+", 8195, 1217760297},
        {NULL}
};

/* test that read works for all paths */
START_TEST(test_read_full) {
    char *buffer = malloc(15000);
    if (buffer == NULL) {
        ck_abort_msg("Failed to allocate buffer");
    }
    for (int i = 0; read_test[i].path != NULL; i++) {
        memset(buffer, 0, 15000);

        int bytes_read = fs_ops.read(read_test[i].path, buffer, read_test[i].size, 0, NULL);
        printf("(test_read_full) read %s: %d bytes\n", read_test[i].path, bytes_read);
        ck_assert_msg(bytes_read == read_test[i].size, "Expected %zu bytes, got %d from %s",
                      read_test[i].size, bytes_read, read_test[i].path);

        unsigned cksum = crc32(0L, buffer, bytes_read);
        ck_assert_msg(cksum == read_test[i].cksum, "Expected cksum %u, got %u from %s",
                      read_test[i].cksum, cksum, read_test[i].path);
    }

    free(buffer);
}
END_TEST

/* test that read works for all paths with different chunk sizes */
START_TEST(test_read_chunks) {
    size_t chunks[] = {17, 100, 1000, 1024, 1970, 3000};
    for (int i = 0; read_test[i].path != NULL; i++) {
        size_t file_size = read_test[i].size;

        for (int j = 0; j < sizeof(chunks) / sizeof(chunks[0]); j++) {
            char *buffer = malloc(file_size + 1);
            if (buffer == NULL) {
                ck_abort_msg("Failed to allocate buffer");
            }
            memset(buffer, 0, file_size + 1);

            size_t offset = 0;
            while (offset < file_size) {
                size_t chunk_size = chunks[j];
                if (offset + chunk_size > file_size) {
                    chunk_size = file_size - offset;
                }

                int bytes_read = fs_ops.read(read_test[i].path, buffer + offset, chunk_size, offset, NULL);
                ck_assert_msg(bytes_read == chunk_size, "Expected %zu bytes, got %d from %s",
                              chunk_size, bytes_read, read_test[i].path);

                offset += bytes_read;
            }

            printf("(test_read_chunks) read %s: %zu bytes in chunks of %zu\n",
                   read_test[i].path, file_size, chunks[j]);

            free(buffer);
        }
    }
}
END_TEST

/* test that statfs works */
START_TEST(test_statfs) {
    struct statvfs sv;
    int rv = fs_ops.statfs("/", &sv);
    printf("(test_statfs) rv: %d\n", rv);
    ck_assert_int_eq(rv, 0);
    ck_assert_int_eq(sv.f_bsize, 4096);
    ck_assert_int_eq(sv.f_blocks, 398); // total blocks - 2
    ck_assert_int_eq(sv.f_bfree, 355);
    ck_assert_int_eq(sv.f_bavail, 355);
    ck_assert_int_eq(sv.f_namemax, 27);
}
END_TEST

/* test that chmod works */
START_TEST(test_chmod) {
    struct stat sb;

    // --- Test chmod on a file ---
    const char *file_path = "/file.1k";
    int rv = fs_ops.chmod(file_path, 0644);
    printf("(test_chmod) file_path: %s, rv: %d\n", file_path, rv);
    ck_assert_int_eq(rv, 0);

    rv = fs_ops.getattr(file_path, &sb);
    ck_assert_int_eq(rv, 0);
    ck_assert(S_ISREG(sb.st_mode));
    ck_assert_int_eq(sb.st_mode & 0777, 0644); // octal mask

    // --- Test chmod on a directory ---
    const char *dir_path = "/dir3";
    rv = fs_ops.chmod(dir_path, 0700);
    printf("(test_chmod) dir_path: %s, rv: %d\n", dir_path, rv);
    ck_assert_int_eq(rv, 0);

    rv = fs_ops.getattr(dir_path, &sb);
    ck_assert_int_eq(rv, 0);
    ck_assert(S_ISDIR(sb.st_mode));
    ck_assert_int_eq(sb.st_mode & 0777, 0700);
}
END_TEST

/* test rename works for files and directories */
START_TEST(test_rename_file) {
    const char *old_path = "/file.1k";
    const char *new_path = "/file.2k";

    int rv = fs_ops.rename(old_path, new_path);
    printf("(test_rename_file) rv: %d\n", rv);
    ck_assert_int_eq(rv, 0);

    char *buffer = malloc(1000);
    rv = fs_ops.read(new_path, buffer, 1000, 0, NULL);
    printf("(test_rename_file) read rv: %d\n", rv);
    ck_assert_int_eq(rv, 1000);

    unsigned cksum = crc32(0L, buffer, rv);
    ck_assert_int_eq(cksum, 1726121896);

    free(buffer);
}
END_TEST

/* test rename works for directories */
START_TEST(test_rename_directory) {
    const char *old_path = "/dir3";
    const char *new_path = "/dir3-renamed";

    int rv = fs_ops.rename(old_path, new_path);
    printf("(test_rename_directory) rv: %d\n", rv);
    ck_assert_int_eq(rv, 0);

    const char *file_path = "/dir3-renamed/subdir/file.4k-";
    char *buffer = malloc(4095);
    rv = fs_ops.read(file_path, buffer, 4095, 0, NULL);
    printf("(test_rename_directory) read rv: %d\n", rv);
    ck_assert_int_eq(rv, 4095);

    unsigned cksum = crc32(0L, buffer, rv);
    ck_assert_int_eq(cksum, 2991486384);

    free(buffer);
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
    system("python gen-disk.py -q disk1.in test.img");
    block_init("test.img");
    fs_ops.init(NULL);
    
    Suite *s = suite_create("fs5600");
    TCase *tc = tcase_create("read_mostly");

    printf("Running tests...\n");

    tcase_add_test(tc, test_getattr_paths);
    tcase_add_test(tc, test_getattr_not_a_file);
    tcase_add_test(tc, test_getattr_not_a_dir);
    tcase_add_test(tc, test_getattr_middle_missing);
    tcase_add_test(tc, test_getattr_subdir_missing);
    tcase_add_test(tc, test_readdir_all);
    tcase_add_test(tc, test_readdir_errors);
    tcase_add_test(tc, test_read_full);
    tcase_add_test(tc, test_read_chunks);
    tcase_add_test(tc, test_statfs);
    tcase_add_test(tc, test_chmod);
    tcase_add_test(tc, test_rename_file);
    tcase_add_test(tc, test_rename_directory);

    suite_add_tcase(s, tc);
    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    
    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);
    
    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
