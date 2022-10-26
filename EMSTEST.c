/*
 * EMSTEST.c
 *
 *  Created on: 2022/10/26
 *      Author: pc98user
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dos.h>

#ifndef __far
#define __far volatile
#endif

typedef unsigned long ulong;
typedef unsigned int uint;
typedef unsigned char uchar;


#define EMSPUT 1
#define EMSGET 2

typedef struct move_source_dest {
    ulong region_length; /* 転送するバイト数 */
    uchar source_memory_type; /* 転送元のメモリ・タイプ（0:標準 1:拡張） */
    uint source_handle; /* 転送元のハンドル（0:標準メモリ） */
    uint source_initial_offset; /* 転送元のオフセット */
    uint source_initial_seg_page; /* 転送元のセグメントまたは論理ページ番号 */
    uchar dest_memory_type; /* 転送先のメモリ・タイプ（0:標準 1:拡張） */
    uint dest_handle; /* 転送先のハンドル（0:標準メモリ） */
    uint dest_initial_offset; /* 転送先のオフセット */
    uint dest_initial_seg_page; /* 転送先のセグメントまたは論理ページ番号 */
} __attribute__((packed)) EMS_move_source_dest;


typedef struct {
    uint handle; /* ハンドル */
    uint log_pages; /* 論理ページの数 */
} EMEM;

uint ems_frame; /* EMSページフレームセグメント */
uint free_pages; /* 空き論理ページの数 */
uint total_pages; /* 全論理ページの数 */

int EMSinit(void)
{
    union REGS regs;
    int fp;

    printf("%s start.\n", __func__);

    fp = open("EMMXXXX0", O_RDONLY);
    if (fp == -1) {
        printf("open failed.\n");
        return -1;
    }

    close(fp);

    regs.h.ah = 0x40; /* EMSステータスの取得 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0x00) {
        printf("int67h/0x40 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }

    regs.h.ah = 0x46; /* EMSバージョンの取得 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0x00) {
        printf("int67h/0x46 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }
    printf("EMS%d.%d\n", regs.h.al >> 4, regs.h.al & 0x0f);

    regs.h.ah = 0x41; /* ページフレームのセグメントアドレス取得 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0x00) {
        printf("int67h/0x41 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }
    ems_frame = regs.x.bx;
    printf("ems_frame = 0x%04X\n", ems_frame);

    regs.h.ah = 0x42; /* 未アロケートページ数の取得 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0x00) {
        printf("int67h/0x42 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }
    free_pages = regs.x.bx;
    total_pages = regs.x.dx;
    printf("free_pages = %u\n", free_pages);
    printf("total_pages = %u\n", total_pages);

    printf("%s end.\n", __func__);
    return 0;
}

int EMSalloc(ulong pages, EMEM *pmem)
{
    union REGS regs;

    printf("%s start.\n", __func__);

    if (pages > free_pages) {
        printf("pages(%lu) > free_pages(%u)\n", pages, free_pages);
        return -1;
    }

    regs.h.ah = 0x43; /* 論理ページの確保 */
    regs.x.bx = pages;
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0) {
        printf("int67h/0x40 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }

    pmem->handle = regs.x.dx; /* ハンドル */
    pmem->log_pages = pages; /* 確保された論理ページの数 */

    printf("handle = %u\n", pmem->handle);
    printf("log_pages = %u\n", pmem->log_pages);

    printf("%s end.\n", __func__);
    return 0;
}

void __far* EMSmap(EMEM *pmem, uchar phys, uint logi)
{
    union REGS regs;

//  printf("%s start.\n", __func__);

    regs.h.ah = 0x44; /* 論理ページの物理ページへの割当て */
    regs.h.al = phys; /* 使用する物理ページNo. */
    regs.x.bx = logi; /* 割当てる論理ページNo. */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0) {
        printf("int67h/0x44 failed. AH=%02x\n", regs.h.ah);
        return NULL;
    }

//  printf("%s end.\n", __func__);
    return MK_FP(ems_frame, (uint)0x4000 * phys);
}

uint EMSfree(uint handle)
{
    union REGS regs;

    printf("%s start.\n", __func__);

    regs.x.dx = handle;
    regs.h.ah = 0x45; /* EMSメモリ解放 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah) {
        printf("int67h/0x45 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }

    printf("%s end.\n", __func__);
    return 0;
}

ulong EMSMoveMemRegion(int dir, EMEM *pmem, int page, void *buf, uint offset, ulong size)
{
    union REGS regs;
    EMS_move_source_dest semm;

    if (dir == EMSPUT) { // EMS_MOVE_TO_EMS
        semm.region_length = size;
        semm.source_memory_type = 0;
        semm.source_handle = 0;
        semm.source_initial_offset = FP_OFF(buf);
        semm.source_initial_seg_page = FP_SEG(buf);
        semm.dest_memory_type = 1;
        semm.dest_handle = pmem->handle;
        semm.dest_initial_offset = offset;
        semm.dest_initial_seg_page = page;
    }
    else if (dir == EMSGET) {
        semm.region_length = size;
        semm.source_memory_type = 1;
        semm.source_handle = pmem->handle;
        semm.source_initial_offset = offset;
        semm.source_initial_seg_page = page;
        semm.dest_memory_type = 0;
        semm.dest_handle = 0;
        semm.dest_initial_offset = FP_OFF(buf);
        semm.dest_initial_seg_page = FP_SEG(buf);
    }
    else {
        return 0;
    }

    regs.x.si = FP_OFF(&semm);
    regs.x.ax = 0x5700; /* EMSメモリ転送 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0) {
        printf("int67h/0x5700 failed. AH=%02x\n", regs.h.ah);
        return -1L;
    }

    return size;
}

void __far* memcpy_far(void __far *dest, const void __far *src, size_t n)
{
    size_t i;

//  printf("src  = 0x%08lx\n", (ulong)src);
//  printf("dest = 0x%08lx\n", (ulong)dest);
//  printf("size = %ld\n", (long)n);

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
        EMSMoveMemRegion(EMSPUT, &pmem, testing_page, buf_s, 0, 0x4000);
        EMSMoveMemRegion(EMSGET, &pmem, testing_page, buf_d, 0, 0x4000);
    }
//  for (i = 0; i < size; i++) {
//      buf_e[i] = buf_s[i];
//  }
//  for (i = 0; i < size; i++) {
//      buf_d[i] = buf_e[i];
//  }

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

    for (page = 0; page < free_pages; page++) {
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

    ret = EMSinit();
    if (ret != 0) {
        return 1;
    }

    ret = EMSalloc(free_pages, &pmem);
    if (ret != 0) {
        return 1;
    }

    bank();

    EMSfree(pmem.handle);

    return 0;
}
