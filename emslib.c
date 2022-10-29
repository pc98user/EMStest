/*
 * emslib.c
 *
 *  Created on: 2022/10/27
 *      Author: pc98user
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <dos.h>

#ifdef __DEBUG
#define __far
#endif

#include "emslib.h"

typedef struct move_source_dest {
    uint32_t region_length;           /* 転送するバイト数 */
    uint8_t  source_memory_type;      /* 転送元のメモリ・タイプ（0:標準 1:拡張） */
    uint16_t source_handle;           /* 転送元のハンドル（0:標準メモリ） */
    uint16_t source_initial_offset;   /* 転送元のオフセット */
    uint16_t source_initial_seg_page; /* 転送元のセグメントまたは論理ページ番号 */
    uint8_t  dest_memory_type;        /* 転送先のメモリ・タイプ（0:標準 1:拡張） */
    uint16_t dest_handle;             /* 転送先のハンドル（0:標準メモリ） */
    uint16_t dest_initial_offset;     /* 転送先のオフセット */
    uint16_t dest_initial_seg_page;   /* 転送先のセグメントまたは論理ページ番号 */
} __attribute__((packed)) EMS_move_source_dest;


int EMSinit(EMEM *pmem)
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
    pmem->ems_frame = regs.x.bx;
    printf("ems_frame = 0x%04X\n", pmem->ems_frame);

    regs.h.ah = 0x42; /* 未アロケートページ数の取得 */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0x00) {
        printf("int67h/0x42 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }
    pmem->free_pages = regs.x.bx;
    pmem->total_pages = regs.x.dx;
    printf("free_pages = %u\n", pmem->free_pages);
    printf("total_pages = %u\n", pmem->total_pages);

    printf("%s end.\n", __func__);
    return 0;
}

int EMSalloc(EMEM *pmem, uint16_t pages)
{
    union REGS regs;

    printf("%s start.\n", __func__);

    if (pages > pmem->free_pages) {
        printf("pages(%u) > free_pages(%u)\n", pages, pmem->free_pages);
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

void __far* EMSmap(EMEM *pmem, uint8_t phys, uint16_t logi)
{
    union REGS regs;

//  printf("%s start.\n", __func__);

    regs.h.ah = 0x44; /* 論理ページの物理ページへの割当て */
    regs.x.dx = pmem->handle;
    regs.h.al = phys; /* 使用する物理ページNo. */
    regs.x.bx = logi; /* 割当てる論理ページNo. */
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0) {
        printf("int67h/0x44 failed. AH=%02x\n", regs.h.ah);
        return NULL;
    }

//  printf("%s end.\n", __func__);
    return MK_FP(pmem->ems_frame, (uint16_t)0x4000 * phys);
}

uint16_t EMSfree(EMEM *pmem)
{
    union REGS regs;

    printf("%s start.\n", __func__);

    regs.h.ah = 0x45; /* EMSメモリ解放 */
    regs.x.dx = pmem->handle;
    int86(0x67, &regs, &regs);
    if (regs.h.ah) {
        printf("int67h/0x45 failed. AH=%02x\n", regs.h.ah);
        return -1;
    }

    printf("%s end.\n", __func__);
    return 0;
}

uint32_t EMSMoveMemRegion(EMEM *pmem, int dir, int page, void *buf, uint16_t offset, uint32_t size)
{
    union REGS regs;
    EMS_move_source_dest semm;

    if (dir == EMSPUT) {
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

    regs.x.ax = 0x5700; /* EMSメモリ転送 */
    regs.x.dx = pmem->handle;
    regs.x.si = FP_OFF(&semm);
    int86(0x67, &regs, &regs);
    if (regs.h.ah != 0) {
        printf("int67h/0x5700 failed. AH=%02x\n", regs.h.ah);
        return -1L;
    }

    return size;
}
