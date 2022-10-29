/*
 * emslib.h
 *
 *  Created on: 2022/10/27
 *      Author: pc98user
 */

#ifndef EMSLIB_H_
#define EMSLIB_H_

typedef struct {
    uint16_t ems_frame;   /* EMSページフレームセグメント */
    uint16_t free_pages;  /* 空き論理ページの数 */
    uint16_t total_pages; /* 全論理ページの数 */

    uint16_t handle;      /* ハンドル */
    uint16_t log_pages;   /* 論理ページの数 */
} EMEM;

#define EMSPUT 1
#define EMSGET 2

int EMSinit(EMEM *pmem);
int EMSalloc(EMEM *pmem, uint16_t pages);
void __far* EMSmap(EMEM *pmem, uint8_t phys, uint16_t logi);
uint16_t EMSfree(EMEM *pmem);
uint32_t EMSMoveMemRegion(EMEM *pmem, int dir, int page, void *buf, uint16_t offset, uint32_t size);

#endif /* EMSLIB_H_ */
