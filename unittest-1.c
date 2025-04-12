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

/* change test name and make it do something useful */
START_TEST(test_getattr_paths)
{
    for (int i = 0; getattr_test[i].path != NULL; i++) {
        printf("Testing: %s\n", getattr_test[i].path);
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

START_TEST(test_getattr_not_a_file)
{
    struct stat sb;
    int rv = fs_ops.getattr("/not-a-file", &sb);
    ck_assert_int_eq(rv, -ENOENT);
}
END_TEST

START_TEST(test_getattr_not_a_dir)
{
    struct stat sb;
    int rv = fs_ops.getattr("/file.1k/file.0", &sb);
    ck_assert_int_eq(rv, -ENOTDIR);
}
END_TEST

START_TEST(test_getattr_middle_missing)
{
    struct stat sb;
    int rv = fs_ops.getattr("/not-a-dir/file.0", &sb);
    ck_assert_int_eq(rv, -ENOENT);
}
END_TEST

START_TEST(test_getattr_subdir_missing)
{
    struct stat sb;
    int rv = fs_ops.getattr("/dir2/not-a-file", &sb);
    ck_assert_int_eq(rv, -ENOENT);
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

    suite_add_tcase(s, tc);
    SRunner *sr = srunner_create(s);
    srunner_set_fork_status(sr, CK_NOFORK);
    
    srunner_run_all(sr, CK_VERBOSE);
    int n_failed = srunner_ntests_failed(sr);
    printf("%d tests failed\n", n_failed);
    
    srunner_free(sr);
    return (n_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
