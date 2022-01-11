//#include "md5.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/md5.h>

// Print the MD5 sum as hex-digits.
void print_md5_sum(unsigned char *md) {
    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        printf("%02x", md[i]);
    }
    printf("\n");
}

// Get the size of the file by its file descriptor
unsigned long get_size_by_fd(int fd) {
    struct stat statbuf;
    if (fstat(fd, &statbuf) < 0) exit(-1);
    return statbuf.st_size;
}

unsigned char *md5_for_file(char *filename) {
    int file_descript;
    unsigned long file_size;
    char *file_buffer;
    unsigned char *result = malloc(sizeof(*result) * MD5_DIGEST_LENGTH);
    if (NULL == result) {
        printf("malloc failed\n");
        goto END;
    }

    printf("using file:\t%s\n", filename);

    file_descript = open(filename, O_RDONLY);
    if (file_descript < 0) exit(-1);

    file_size = get_size_by_fd(file_descript);
    printf("file size:\t%lu\n", file_size);

    file_buffer = mmap(0, file_size, PROT_READ, MAP_SHARED, file_descript, 0);
    MD5((unsigned char *) file_buffer, file_size, result);
    munmap(file_buffer, file_size);

    print_md5_sum(result);
    END:
    return result;
}

int md5_is_match(unsigned char *md5_1, unsigned char *md5_2) {
    if (!md5_1 || !md5_2) {
        return 0;
    }

    int i;
    for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (md5_1[i] != md5_2[i]) {
            return 0;
        }
    }

    return 1;
}

int md5_is_match_str(unsigned char *md5, char *md5_str) {
    if (!md5 || !md5_str) { return 0; }

    /** Make byte arrary from md5_str */
    unsigned char md5_arr[MD5_DIGEST_LENGTH] = {0};

    const char *pos = md5_str;
    size_t count = 0;

    /* WARNING: no sanitization or error-checking whatsoever */
    for (count = 0; count < sizeof(md5_arr) / sizeof(md5_arr[0]); count++) {
        sscanf(pos, "%2hhx", &md5_arr[count]);
        pos += 2;
    }

    for (count = 0; count < sizeof(md5_arr) / sizeof(md5_arr[0]); count++) {
        printf("%02x", md5_arr[count]);
    }
    printf("\n");

    /** actual comparison */
    if (memcmp(md5, md5_arr, MD5_DIGEST_LENGTH)) {
        return 0;
    }

    return 1;
}
