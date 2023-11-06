#ifndef _IMGSTORE_H_
#define _IMGSTORE_H_

#include <stdbool.h>
#include <stdint.h>

bool imgStoreInit(void);

bool imgStoreGetUpdateInfo(uint16_t hwType, uint64_t *revP, uint32_t *lenP, uint32_t *handleP);

bool imgStoreGetImgInfo(const uint8_t *mac, uint64_t *revP, uint32_t *lenP, uint32_t *handleP);

void imgStoreGetFileData(void* dst, uint32_t handle, uint32_t ofst, uint32_t len);

void imgStoreProcess(void);

#endif
