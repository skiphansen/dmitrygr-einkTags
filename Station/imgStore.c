#include "../firmware/common/proto.h"
#include "nrf52840_bitfields.h"
#include "timebase.h"
#include "imgStore.h"
#include "nrf52840.h"
#include <limits.h>
#include <string.h>
#include <stdio.h>
#include "printf.h"
#include "led.h"
#include "msc.h"



#define SECTOR_SIZE                 (512)

#define FLASH_DATA_START_ADDR       (0x05000)
#define FLASH_DATA_END_ADDR            (0x100000)
#define FLASH_PAGE_SIZE             (0x1000)

#define SECTORS_PER_FLASH_PAGE         (FLASH_PAGE_SIZE / SECTOR_SIZE)

#define SECTOR_NUM_MBR              (0)
#define SECTOR_NUM_PBR              (1)
#define SECTORS_PER_CLUSTER            (SECTORS_PER_FLASH_PAGE)

#define NUM_ROOT_DIR_SECTORS        (4)
#define USED_SECTORS_BEFORE_DATA    (SECTORS_PER_FAT * NUMBER_OF_FATS + NUM_ROOT_DIR_SECTORS)
//line up clusters to match flash pages
#define EXTRA_RESERVED_SECTORS         ((USED_SECTORS_BEFORE_DATA % SECTORS_PER_FLASH_PAGE) ? SECTORS_PER_FLASH_PAGE - (USED_SECTORS_BEFORE_DATA % SECTORS_PER_FLASH_PAGE) : 0)

#define NUM_RESERVED_SECTORS        (1 + EXTRA_RESERVED_SECTORS)     //includes PBR and thus >= 1. calculated such that data starts on a flash page boundary


#define NUMBER_OF_FATS              (1)
#define PARTITION_NUM_CLUSTERS         ((FLASH_DATA_END_ADDR - FLASH_DATA_START_ADDR) / (SECTORS_PER_CLUSTER * SECTOR_SIZE) - 1 /* approx how many cluster-sized spaces fat/root sec need */)


//derived values
#define BYTES_PER_FAT               (((PARTITION_NUM_CLUSTERS + 2) * 3 + 1) / 2)
#define SECTORS_PER_FAT             ((BYTES_PER_FAT + SECTOR_SIZE - 1) / SECTOR_SIZE)
#define SECTOR_NUM_FAT              (SECTOR_NUM_PBR + NUM_RESERVED_SECTORS)
#define SECTOR_NUM_ROOT_DIR            (SECTOR_NUM_FAT + SECTORS_PER_FAT * NUMBER_OF_FATS)
#define SECTOR_NUM_DATA_START       (SECTOR_NUM_ROOT_DIR + NUM_ROOT_DIR_SECTORS)
#define NUM_ROOT_DIR_ENTRIES        (NUM_ROOT_DIR_SECTORS * 16)
#define PARTITION_NUM_SECTORS       (SECTOR_NUM_DATA_START + PARTITION_NUM_CLUSTERS * SECTORS_PER_CLUSTER)
#define DISK_NUM_SECTORS            (PARTITION_NUM_SECTORS + SECTOR_NUM_PBR)


struct FatDirEntry {
   uint8_t name[11];
   uint8_t attrs;
   uint8_t unused;
   uint8_t crTimeDecisecs;
   uint16_t createTime;
   uint16_t createDate;
   uint16_t accessDate;
   uint16_t clusterHi;
   uint16_t modifiedTime;
   uint16_t modifiedDate;
   uint16_t clusterLo;
   uint32_t size;
} __attribute__((packed));



static uint8_t mCurSecCache[FLASH_PAGE_SIZE], mReadBuf[SECTOR_SIZE];
static uint64_t mCacheLastAction = 0;
static uint32_t mCurSecAddr = 0;


static void imgStorePrvFlashPageErase(uint32_t addr)
{
   uint32_t i;
   
   for (i = 0; i < 85; i++) { //as per docs
      
      NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Een;
      asm volatile(
         "cpsid i    \n\t"
         "str %0, [%1]  \n\t"
         "nop        \n\t"
         "cpsie i    \n\t"
         ::"r"(addr), "r"(&NRF_NVMC->ERASEPAGEPARTIAL)
         :"memory"
      );
      NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
   }
}

static void imgStorePrvFlashPageWrite(uint32_t addr, const void* data)
{
   const uint32_t *src = (const uint32_t*)data;
   uint32_t i;
   
   for (i = 0; i < FLASH_PAGE_SIZE; i += sizeof(uint32_t)) {
      
      NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Wen;
      asm volatile(
         "cpsid i    \n\t"
         "str %0, [%1]  \n\t"
         "nop        \n\t"
         "cpsie i    \n\t"
         ::"r"(*src++), "r"(addr + i)
         :"memory"
      );
      NRF_NVMC->CONFIG = NVMC_CONFIG_WEN_Ren;
   }
}

static void imgStorePrvFlashCacheFlush(void)
{
   uint32_t i;
   
   if (mCurSecAddr) {   //cache is dirty
      
      const uint32_t *dst = (const uint32_t*)mCurSecAddr;
      const uint32_t *src = (const uint32_t*)mCurSecCache;
      
      //pr("erz check for 0x%08x <- 0x%08x\n", dst, src);
      //check if we need an erase
      for (i = 0; i < FLASH_PAGE_SIZE / sizeof(uint32_t) && (dst[i] & src[i]) == src[i]; i++);
   
      if (i != FLASH_PAGE_SIZE / sizeof(uint32_t))
         imgStorePrvFlashPageErase(mCurSecAddr);
      
      imgStorePrvFlashPageWrite(mCurSecAddr, mCurSecCache);
      mCurSecAddr = 0;
   }
}

static bool imgStorePrvFlashWriteSector(uint32_t addr, const void *from)
{
   uint32_t flashAddr = addr / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
   
   //pr("write to data addr 0x%08x (rounded to 0x%08x) (cache addr is 0x%08x)\n", addr, flashAddr, mCurSecAddr);
   
   if (mCurSecAddr != flashAddr) {
      imgStorePrvFlashCacheFlush();
   // pr("post flush, mCurSecAddr = 0x%08x\n", mCurSecAddr);
      memcpy(mCurSecCache, (const void*)flashAddr, FLASH_PAGE_SIZE);
      mCurSecAddr = flashAddr;
   // pr("post update, mCurSecAddr = 0x%08x\n", mCurSecAddr);
   }
   
   //pr("copy in data (cache addr is 0x%08x) to offset 0x%08x\n", mCurSecAddr, (addr - flashAddr));
   memcpy(mCurSecCache + (addr - flashAddr), from, SECTOR_SIZE);
   //pr("copied data (cache addr is 0x%08x)\n", mCurSecAddr);
   
   mCacheLastAction = timebaseGet();
   
   return true;
}

static void imgStorePrvFlashReadSector(void* to, uint32_t addr)
{
   uint32_t flashAddr = addr / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
   
   //pr("read to data addr 0x%08x (rounded to 0x%08x) (cache addr is 0x%08x)\n", addr, flashAddr, mCurSecAddr);
   
   //check cache
   if (mCurSecAddr == flashAddr)
      memcpy(to, mCurSecCache + (addr - flashAddr), SECTOR_SIZE);
   else
      memcpy(to, (const void*)addr, SECTOR_SIZE);
}

static void imgStorePrvSecReadMbr(void* dst)
{
   uint8_t *ptr = (uint8_t*)dst;
   uint8_t *part = ptr + 0x1be;
   
   bzero(ptr, 512);
   
   //jump
   ptr[0x000] = 0xe9;
   ptr[0x001] = 0xfd;
   ptr[0x002] = 0xff;
   
   //marker
   ptr[0x1fe] = 0x55;
   ptr[0x1ff] = 0xaa;
   
   //the partition table entry
   part[0x000] = 0x80;                    //active
   part[0x004] = 0x01;                    //type: fat12
   *(uint32_t*)(part + 0x008) = SECTOR_NUM_PBR;
   *(uint32_t*)(part + 0x00c) = PARTITION_NUM_SECTORS;
}

static void imgStorePrvSecReadPbr(void* dst)
{
   char *ptr = (char*)dst;
   
   bzero(ptr, 512);
   
   //jump
   ptr[0x000] = 0xe9;
   ptr[0x001] = 0xfd;
   ptr[0x002] = 0xff;
   
   //marker
   ptr[0x1fe] = 0x55;
   ptr[0x1ff] = 0xaa;
   
   strcpy(ptr + 3, "mkdosfs");
   
   *(uint16_t*)(ptr + 0x00b) = SECTOR_SIZE;
   ptr[0x00d] = SECTORS_PER_CLUSTER;
   *(uint16_t*)(ptr + 0x00e) = NUM_RESERVED_SECTORS;
   ptr[0x010] = NUMBER_OF_FATS;
   *(uint16_t*)(ptr + 0x011) = NUM_ROOT_DIR_ENTRIES;
   ptr[0x015] = 0xf8;   //fixed disk
   *(uint16_t*)(ptr + 0x016) = SECTORS_PER_FAT;
   
   if (PARTITION_NUM_SECTORS < 65536)
      *(uint16_t*)(ptr + 0x013) = PARTITION_NUM_SECTORS;
   else
      *(uint32_t*)(ptr + 0x020) = PARTITION_NUM_SECTORS;
   
   ptr[0x026] = 0x29;   //EPB follows
   *(uint32_t*)(ptr + 0x27) = 0x44474447; //set a volume serial number
   strcpy(ptr + 0x2b, "NO NAME    FAT12   ");   //label and fs type
}

static bool imgStorePrvSectorRead(uint32_t sec, uint8_t *ptr)
{
   //pr(" *rd sec %d (fat is %d, root is %d data is %d)\n", (unsigned)sec, SECTOR_NUM_FAT, SECTOR_NUM_ROOT_DIR, SECTOR_NUM_DATA_START);
   
   if (sec == SECTOR_NUM_MBR)
      imgStorePrvSecReadMbr(ptr);
   else if (sec == SECTOR_NUM_PBR)
      imgStorePrvSecReadPbr(ptr);
   else if (sec < SECTOR_NUM_FAT)   //reserved sectors read as zero
      bzero(ptr, SECTOR_SIZE);
   else if (sec >= SECTOR_NUM_FAT && sec < DISK_NUM_SECTORS)
      imgStorePrvFlashReadSector(ptr, FLASH_DATA_START_ADDR + (sec - SECTOR_NUM_FAT) * SECTOR_SIZE);
   else
      return false;
   
   return true;
}

static bool imgStorePrvSectorWrite(uint32_t sec, const uint8_t *ptr)
{
   //pr(" *wr sec %d (fat is %d, root is %d data is %d)\n", (unsigned)sec, SECTOR_NUM_FAT, SECTOR_NUM_ROOT_DIR, SECTOR_NUM_DATA_START);
   
   if (sec == SECTOR_NUM_PBR)
      ;//allow but ignore
   else if (sec >= SECTOR_NUM_FAT && sec < DISK_NUM_SECTORS)
      return imgStorePrvFlashWriteSector(FLASH_DATA_START_ADDR + (sec - SECTOR_NUM_FAT) * SECTOR_SIZE, ptr);
   else
      return false;
   
   return true;
}

static uint32_t imgStorePrvMscDeviceRead(void* userData, uint32_t sector, uint32_t nSec, void *data)
{
   uint8_t *buf = (uint8_t*)data;
   uint32_t i;
   
   ledSet(-1, -1, -1, 10);
   
   for (i = 0; i < nSec; i++, sector++, buf += SECTOR_SIZE) {
      if (!imgStorePrvSectorRead(sector, buf))
         break;
   }
   
   ledSet(-1, -1, -1, 0);
   
   return i;
}

static uint32_t imgStorePrvMscDeviceWrite(void* userData, uint32_t sector, uint32_t nSec, const void *data)
{
   const uint8_t *buf = (const uint8_t*)data;
   uint32_t i;
   
   ledSet(-1, -1, -1, 100);
   
   for (i = 0; i < nSec; i++, sector++, buf += SECTOR_SIZE) {
      if (!imgStorePrvSectorWrite(sector, buf))
         break;
   }
   
   ledSet(-1, -1, -1, 0);
   
   return i;
}

static uint32_t imgStorePrvFatGet(uint32_t cluster)
{
   const uint8_t *fat;
   uint32_t entry;
   
   if (cluster > PARTITION_NUM_CLUSTERS + 2)
      return 0xfff;
   
   imgStorePrvFlashCacheFlush();    //so the fat is on disk :)
   
   fat = (const uint8_t*)FLASH_DATA_START_ADDR; //fat starts here
   entry = *(uint32_t*)(fat + (cluster / 2) * 3);
   
   if (cluster & 1)
      entry >>= 12;
   
   return entry & 0xfff;
}

bool imgStoreInit(void)
{
   if (imgStorePrvFatGet(0) != 0xff8 || imgStorePrvFatGet(1) != 0xfff) {
      
      //pre-set up cache to contain the valid FAT :D
      *(uint32_t*)mCurSecCache = 0xfffff8;
      bzero(mCurSecCache + 3, (PARTITION_NUM_CLUSTERS * 3 + 1)/ 2);
      mCurSecAddr = FLASH_DATA_START_ADDR;   //cache is set up for a flush
   }
   
   NRF_NVMC->ERASEPAGEPARTIALCFG = 1;
   
   return mscInit(DISK_NUM_SECTORS, SECTOR_SIZE, imgStorePrvMscDeviceRead, imgStorePrvMscDeviceWrite, NULL);
}

static bool imgStoreGetFileInfo(const char *desiredName, uint64_t *revP, uint32_t *lenP, uint32_t *handleP)
{
   const struct FatDirEntry *fde;
   uint32_t sec, ofst;

   for (sec = 0; sec < NUM_ROOT_DIR_SECTORS; sec++) {
      
      imgStorePrvSectorRead(SECTOR_NUM_ROOT_DIR + sec, mReadBuf);
      fde = (const struct FatDirEntry*)mReadBuf;
      for (ofst = 0; ofst <= SECTOR_SIZE - sizeof(struct FatDirEntry); fde++, ofst += sizeof(struct FatDirEntry)) {
         
         if (fde->name[0] == 0)  //end of list and not found? we're done
            return false;
         
         if (fde->name[0] == '.' || fde->name[0] == 0xe5)
            continue;
         
         if (fde->attrs & 0xc8)  ///skip reserved and volume label types
            continue;
         
         if (memcmp(fde->name, desiredName, 11))
            continue;
            
         //file found - revision is date
         if (revP)
            *revP = (((uint64_t)fde->modifiedDate) << 48) | (((uint64_t)fde->createDate) << 32) | (((uint64_t)fde->modifiedTime) << 16) | fde->createTime;
         if (lenP)
            *lenP = fde->size;
         if (handleP)
            *handleP = (((uint32_t)fde->clusterHi) << 16) | fde->clusterLo;
         
         return true;
      }
   }
   return false;
}

bool imgStoreGetUpdateInfo(uint16_t hwType, uint64_t *revP, uint32_t *lenP, uint32_t *handleP)
{
   char desiredName[11];
   uint32_t handle;
   
   if (!handleP)
      handleP = &handle;
   
   snprintf(desiredName, sizeof(desiredName), "UPDT%04XBIN", hwType);
      
   if (!imgStoreGetFileInfo(desiredName, revP, lenP, handleP))
      return false;
      
   //using file date for updates sucks, we use a special offset in the file, if we know if
   if (revP) {
      if (hwType == HW_TYPE_42_INCH_SAMSUNG) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_42_INCH_SAMSUNG_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_74_INCH_DISPDATA_Y) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_74_INCH_DISPDATA_Y_FRAME_MODE) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_74_INCH_DISPDATA_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_29_INCH_DISPDATA) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_29_INCH_DISPDATA_FRAME_MODE) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_29_INCH_DISPDATA_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_ZBD_EPOP50) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_ZBD_EPOP50_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_ZBD_EPOP900) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_ZBD_EPOP900_ROM_VER_OFST, sizeof(*revP));
      }
      else if (hwType == HW_TYPE_29_INCH_ZBS_026 || hwType == HW_TYPE_29_INCH_ZBS_026_FRAME_MODE || hwType == HW_TYPE_29_INCH_ZBS_025 || hwType == HW_TYPE_29_INCH_ZBS_025_FRAME_MODE) {
         
         *revP = 0;
         imgStoreGetFileData(revP, *handleP, HW_TYPE_29_INCH_ZBS_ROM_VER_OFST, sizeof(*revP));
      }
   }
   
   return true;
}

bool imgStoreGetImgInfo(const uint8_t *mac, uint64_t *revP, uint32_t *lenP, uint32_t *handleP)
{
   char desiredName[11];
   
   snprintf(desiredName, sizeof(desiredName), "%02X%02X%02X  IMG", mac[2], mac[1], mac[0]);
   
   return imgStoreGetFileInfo(desiredName, revP, lenP, handleP);
}

void imgStoreGetFileData(void* dstP, uint32_t clus, uint32_t ofst, uint32_t len)
{
   uint8_t *dst = (uint8_t*)dstP;
   
   imgStorePrvFlashCacheFlush();    //so the data is on disk
   
   while (len) {
      uint32_t availNow, now;
      
      while (ofst >= SECTOR_SIZE * SECTORS_PER_CLUSTER) {
         ofst -= SECTOR_SIZE * SECTORS_PER_CLUSTER;
         clus = imgStorePrvFatGet(clus);
      }
      
      if (clus == 0xfff || clus < 2)
         return;
      
      availNow = SECTORS_PER_CLUSTER * SECTOR_SIZE - ofst;
      now = availNow > len ? len : availNow;
      
      memcpy(dst, (const char*)FLASH_DATA_START_ADDR + SECTOR_SIZE * (SECTOR_NUM_DATA_START + (clus - 2) * SECTORS_PER_CLUSTER - SECTOR_NUM_FAT /* flash only starts at fat */) + ofst, now);
      
      dst += now;
      len -= now;
      ofst += now;
   }
}

void imgStoreProcess(void)
{
   mscProcess();
   
   if (timebaseGet() - mCacheLastAction > TIMER_TICKS_PER_SECOND)
      imgStorePrvFlashCacheFlush();
}