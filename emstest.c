/*
 * emstest.c
 *
 *  Created on: 2022/10/26
 *      Author: pc98user
 */

#include <stdio.h>
#include <string.h>

#ifdef __DEBUG
#define __far
#endif


#include "emslib.h"


void __far* memcpy_far(void __far *dest, const void __far *src, size_t n)
{
    size_t i;

    for (i = 0; i < n; i++) {
        *((unsigned char __far*)dest + i) = *((unsigned char __far*)src + i);
    }

    return dest;
}

char buf_s[0x4000];
char buf_d[0x4000];

EMEM pmem;
int copy_mode;
int testing_pattern;
int testing_bank;
int testing_page;

int test()
{
    char __far *buf_e;
    int ret;

    printf("bank=%d, pattern=%d, mode=%d, page=%d\n", testing_bank, testing_pattern, copy_mode, testing_page);
    fflush(stdout);

    buf_e = EMSmap(&pmem, testing_bank, testing_page);
    if (buf_e == NULL) {
        return 1;
    }

    memset(buf_d, 0x00, 0x4000);
    if (copy_mode == 0) {
        memcpy_far(buf_e, (void __far*) buf_s, 0x4000);
        memcpy_far((void __far*) buf_d, buf_e, 0x4000);
    }
    else {
        EMSMoveMemRegion(&pmem, EMSPUT, testing_page, buf_s, 0, 0x4000);
        EMSMoveMemRegion(&pmem, EMSGET, testing_page, buf_d, 0, 0x4000);
    }

    ret = memcmp(buf_s, buf_d, 0x4000);
    if (ret != 0) {
        printf("compare error.\n");
    }
    else {
//      printf("compare OK.\n");
    }

    EMSmap(&pmem, testing_bank, 0xffff);

    return 0;
}

int page()
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

int mode()
{
    int mode;
    int ret;

    for (mode = 0; mode < 2; mode++) {
        copy_mode = mode;
        ret = page();
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

int pattern()
{
    int pattern;
    int i;
    int ret;

    for (pattern = 0; pattern < 5; pattern++) {
        switch (pattern) {
        case 1:
            memset(buf_s, 0x00, 0x4000);
            break;
        case 2:
            memset(buf_s, 0xFF, 0x4000);
            break;
        case 3:
            memset(buf_s, 0x55, 0x4000);
            break;
        case 4:
            memset(buf_s, 0xAA, 0x4000);
            break;
        case 0:
            for (i = 0; i < 0x4000; i++) {
                buf_s[i] = (char)i;
            }
            break;
        }

        testing_pattern = pattern;
        ret = mode();
        if (ret != 0) {
            return -1;
        }
    }

    return 0;
}

int bank()
{
    int bank;
    int ret;

    for (bank = 2; bank < 4; bank++) {
        testing_bank = bank;
        ret = pattern();
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

    bank();

    EMSfree(&pmem);

    return 0;
}
