#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <linux/fscrypt.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

/* This downstream mode is defined by the maintained kernel's UAPI. */
#ifndef FSCRYPT_MODE_PRIVATE
#define FSCRYPT_MODE_PRIVATE 127
#endif

enum { SHELL_UID = 2000, SKIP_EXIT = 77, TIME_LIMIT_SECONDS = 10 };

_Static_assert(FSCRYPT_POLICY_V2 == 2, "unexpected policy version");
_Static_assert(FSCRYPT_MODE_PRIVATE == 127, "unexpected PRIVATE mode");
_Static_assert(FSCRYPT_KEY_IDENTIFIER_SIZE == 16, "unexpected identifier size");
_Static_assert(sizeof(struct fscrypt_policy_v2) == 24, "unexpected v2 ABI");
_Static_assert(offsetof(struct fscrypt_policy_v2, master_key_identifier) == 8,
               "unexpected identifier offset");
_Static_assert(offsetof(struct fscrypt_get_policy_ex_arg, policy) == 8,
               "unexpected GET_POLICY_EX ABI");
_Static_assert(sizeof(struct fscrypt_get_policy_ex_arg) == 32,
               "unexpected GET_POLICY_EX argument size");
_Static_assert(FS_IOC_SET_ENCRYPTION_POLICY == 0x800c6613UL,
               "SET must retain its historical v1-sized encoding");
_Static_assert(FS_IOC_GET_ENCRYPTION_POLICY_EX == 0xc0096616UL,
               "GET_EX must retain its historical 9-byte encoding");

static int finish(const char *status, const char *reason, int code) {
    printf("result status=%s reason=%s exit=%d\n", status, reason, code);
    return code;
}

static void timed_out(int signal_number) {
    (void)signal_number;
    static const char message[] = "result status=FAIL reason=timeout exit=1\n";
    (void)write(STDOUT_FILENO, message, sizeof(message) - 1);
    _exit(1);
}

static int shell_fsuid(void) {
    char line[256];
    FILE *status = fopen("/proc/self/status", "re");
    if (status == NULL) {
        return 0;
    }
    for (int i = 0; i < 32 && fgets(line, sizeof(line), status) != NULL; ++i) {
        unsigned int real_uid, effective_uid, saved_uid, filesystem_uid;
        if (sscanf(line, "Uid: %u %u %u %u", &real_uid, &effective_uid,
                   &saved_uid, &filesystem_uid) == 4) {
            fclose(status);
            return effective_uid == SHELL_UID && filesystem_uid == SHELL_UID;
        }
    }
    fclose(status);
    return 0;
}

static int get_policy(int fd, struct fscrypt_get_policy_ex_arg *policy) {
    memset(policy, 0, sizeof(*policy));
    policy->policy_size = sizeof(policy->policy);
    return ioctl(fd, FS_IOC_GET_ENCRYPTION_POLICY_EX, policy);
}

static void print_policy(const char *stage,
                         const struct fscrypt_get_policy_ex_arg *policy) {
    printf("policy stage=%s version=%u", stage, policy->policy.version);
    if (policy->policy.version == FSCRYPT_POLICY_V2 &&
        policy->policy_size == sizeof(policy->policy.v2)) {
        printf(" contents_mode=%u filenames_mode=%u identifier=",
               policy->policy.v2.contents_encryption_mode,
               policy->policy.v2.filenames_encryption_mode);
        for (size_t i = 0; i < FSCRYPT_KEY_IDENTIFIER_SIZE; ++i) {
            printf("%02x", policy->policy.v2.master_key_identifier[i]);
        }
    } else if (policy->policy.version == FSCRYPT_POLICY_V1 &&
               policy->policy_size == sizeof(policy->policy.v1)) {
        printf(" contents_mode=%u filenames_mode=%u",
               policy->policy.v1.contents_encryption_mode,
               policy->policy.v1.filenames_encryption_mode);
    }
    putchar('\n');
}

static int verify_unchanged(int fd, const char *stage,
                            const struct fscrypt_policy_v2 *original) {
    struct fscrypt_get_policy_ex_arg after;
    errno = 0;
    int rc = get_policy(fd, &after);
    int saved_errno = rc < 0 ? errno : 0;
    printf("get_policy_ex stage=%s rc=%d errno=%d\n", stage, rc, saved_errno);
    if (rc != 0) {
        printf("unchanged stage=%s status=FAIL reason=policy-read-failed\n", stage);
        return 0;
    }
    print_policy(stage, &after);
    if (after.policy_size != sizeof(*original) ||
        after.policy.version != FSCRYPT_POLICY_V2 ||
        memcmp(original, &after.policy.v2, sizeof(*original)) != 0) {
        printf("unchanged stage=%s status=FAIL reason=policy-changed\n", stage);
        return 0;
    }
    printf("unchanged stage=%s status=PASS\n", stage);
    return 1;
}

static int unavailable_get_errno(int error) {
    return error == ENOTTY || error == EOPNOTSUPP || error == ENOSYS ||
           error == ENODATA || error == EACCES ||
           error == EPERM;
}

int main(int argc, char **argv) {
    (void)setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc == 2 && strcmp(argv[1], "--help") == 0) {
        puts("Usage: fscrypt-policy-identifier EXISTING_SHELL_OWNED_DIRECTORY");
        puts("Requires effective/filesystem UID2000, directory owner2000,");
        puts("and an existing fscrypt v2 policy with contents mode PRIVATE127.");
        puts("Exit0=runtime comparisons PASS; exit77=SKIP; other nonzero=FAIL.");
        return 0;
    }
    if (argc != 2) {
        return finish("FAIL", "expected-one-directory-argument", 2);
    }
    if (signal(SIGALRM, timed_out) == SIG_ERR) {
        return finish("FAIL", "timeout-handler-unavailable", 1);
    }
    alarm(TIME_LIMIT_SECONDS);
    if (geteuid() != SHELL_UID || !shell_fsuid()) {
        return finish("SKIP", "effective-and-filesystem-uid2000-required", SKIP_EXIT);
    }
    puts("caller status=PASS effective_uid=2000 filesystem_uid=2000");

    int fd = open(argv[1], O_RDONLY | O_DIRECTORY | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        int error = errno;
        int unavailable = error == ENOENT || error == ENOTDIR || error == ELOOP ||
                          error == EACCES || error == EPERM || error == ENOKEY ||
                          error == EOPNOTSUPP;
        printf("directory status=%s errno=%d\n", unavailable ? "SKIP" : "FAIL", error);
        return finish(unavailable ? "SKIP" : "FAIL",
                      "eligible-directory-unavailable", unavailable ? SKIP_EXIT : 1);
    }
    struct stat metadata;
    if (fstat(fd, &metadata) != 0) {
        printf("directory status=FAIL errno=%d\n", errno);
        close(fd);
        return finish("FAIL", "directory-stat-failed", 1);
    }
    if (!S_ISDIR(metadata.st_mode) || metadata.st_uid != SHELL_UID) {
        close(fd);
        return finish("SKIP", "shell-owned-directory-required", SKIP_EXIT);
    }
    puts("directory status=PASS owner_uid=2000");

    struct fscrypt_get_policy_ex_arg initial;
    errno = 0;
    int rc = get_policy(fd, &initial);
    int saved_errno = rc < 0 ? errno : 0;
    printf("get_policy_ex stage=initial rc=%d errno=%d\n", rc, saved_errno);
    if (rc != 0) {
        close(fd);
        if (unavailable_get_errno(saved_errno)) {
            return finish("SKIP", "encrypted-policy-or-get-policy-ex-unavailable",
                          SKIP_EXIT);
        }
        return finish("FAIL", "initial-policy-read-failed", 1);
    }
    print_policy("initial", &initial);
    if (initial.policy.version != FSCRYPT_POLICY_V2) {
        close(fd);
        return finish("SKIP", "encrypted-v2-policy-required", SKIP_EXIT);
    }
    if (initial.policy_size != sizeof(initial.policy.v2)) {
        close(fd);
        return finish("FAIL", "unexpected-v2-policy-size", 1);
    }
    if (initial.policy.v2.contents_encryption_mode != FSCRYPT_MODE_PRIVATE) {
        close(fd);
        return finish("SKIP", "private127-contents-mode-required", SKIP_EXIT);
    }

    const struct fscrypt_policy_v2 original = initial.policy.v2;
    errno = 0;
    rc = ioctl(fd, FS_IOC_SET_ENCRYPTION_POLICY, &original);
    saved_errno = rc < 0 ? errno : 0;
    printf("set_policy stage=exact rc=%d errno=%d status=%s\n", rc, saved_errno,
           rc == 0 ? "PASS" : "FAIL");
    int exact_ok = rc == 0;
    int exact_unchanged = verify_unchanged(fd, "after-exact", &original);
    if (!exact_ok || !exact_unchanged) {
        close(fd);
        return finish("FAIL", "exact-policy-comparison-failed", 1);
    }

    /* Change only the last identifier byte, outside the legacy v1 descriptor. */
    struct fscrypt_policy_v2 different = original;
    different.master_key_identifier[FSCRYPT_KEY_IDENTIFIER_SIZE - 1] ^= 1;
    errno = 0;
    rc = ioctl(fd, FS_IOC_SET_ENCRYPTION_POLICY, &different);
    saved_errno = rc < 0 ? errno : 0;
    int different_ok = rc == -1 && saved_errno == EEXIST;
    printf("set_policy stage=identifier-last-byte-xor1 rc=%d errno=%d status=%s\n",
           rc, saved_errno, different_ok ? "PASS" : "FAIL");
    int final_unchanged = verify_unchanged(fd, "after-different", &original);
    close(fd);
    if (!different_ok || !final_unchanged) {
        return finish("FAIL", "different-identifier-comparison-failed", 1);
    }
    return finish("PASS", "existing-v2-private-policy-identifier-compared", 0);
}
