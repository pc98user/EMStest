/*
 * emstest.c
 *
 *  Created on: 2022/10/26
 *      Author: pc98user
 *              pc98user@mydomain.mydns.jp
 */

#include <stdio.h>
#include <string.h>

#ifdef __DEBUG
#define __far
#endif


#include "emslib.h"

void __far* memcpy_far(void __far *dest, const void __far *src, size_t n);
int test();
int page_select();
int mode_select();
int pattern_select();
int frame_select();

typedef enum {
    SIMPLE_COPY = 0,
    FUNCTION_COPY,
    MODE_NUM
} TEST_MODE;

#define FRAME_NUM 4
#define PAGE_SIZE 0x4000


EMEM pmem;
TEST_MODE copy_mode;
int testing_pattern;
int testing_frame;
int testing_page;

char buf_s[PAGE_SIZE];
char buf_d[PAGE_SIZE];


void __far* memcpy_far(void __far *dest, const void __far *src, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        *((unsigned char __far*)dest + i) = *((unsigned char __far*)src + i);
    }

    return dest;
}

int test()
{
    char __far *buf_e;
    int ret;

    printf("frame=%d, pattern=%d, mode=%d, page=%d\n", testing_frame, testing_pattern, copy_mode, testing_page);
    fflush(stdout);

    buf_e = EMSmap(&pmem, testing_frame, testing_page);
    if (buf_e == NULL) {
        return 1;
    }

    memset(buf_d, 0x00, PAGE_SIZE);
    if (copy_mode == SIMPLE_COPY) {
        memcpy_far(buf_e, (void __far*) buf_s, PAGE_SIZE);
        memcpy_far((void __far*) buf_d, buf_e, PAGE_SIZE);
    }
    else if (copy_mode == FUNCTION_COPY) {
        EMSMoveMemRegion(&pmem, EMSPUT, testing_page, buf_s, 0, PAGE_SIZE);
        EMSMoveMemRegion(&pmem, EMSGET, testing_page, buf_d, 0, PAGE_SIZE);
    }
    else {
        return 0;
    }

    ret = memcmp(buf_s, buf_d, PAGE_SIZE);
    if (ret != 0) {
        printf("compare error.\n");
    }
    else {
//      printf("compare OK.\n");
    }

    EMSmap(&pmem, testing_frame, 0xffff);

    return 0;
}

int page_select()
{
    int page;
    int ret;

    for (page = 0; page < pmem.free_pages; page++) {
        testing_page = page;
        ret = test();
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

int mode_select()
{
    TEST_MODE mode;
    int ret;

    for (mode = 0; mode < MODE_NUM; mode++) {
        copy_mode = mode;
        ret = page_select();
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

int pattern_select()
{
    int pattern;
    int i;
    int ret;

    for (pattern = 0; pattern < 5; pattern++) {
        switch (pattern) {
        case 1:
            memset(buf_s, 0x00, PAGE_SIZE);
            break;
        case 2:
            memset(buf_s, 0xFF, PAGE_SIZE);
            break;
        case 3:
            memset(buf_s, 0x55, PAGE_SIZE);
            break;
        case 4:
            memset(buf_s, 0xAA, PAGE_SIZE);
            break;
        case 0:
            for (i = 0; i < PAGE_SIZE; i++) {
                buf_s[i] = (char)i;
            }
            break;
        }

        testing_pattern = pattern;
        ret = mode_select();
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

int frame_select()
{
    int frame;
    int ret;

    for (frame = 0; frame < FRAME_NUM; frame++) {
        testing_frame = frame;
        ret = pattern_select();
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

int main(void)
{
    int ret;


    ret = EMSinit(&pmem);
    if (ret != 0) {
        return 1;
    }

    ret = EMSalloc(&pmem, pmem.free_pages);
    if (ret != 0) {
        return 1;
    }

    frame_select();

    EMSfree(&pmem);

    return 0;
}
