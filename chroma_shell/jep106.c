#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

static const char *JEP106_LookupTbl[][0x7e] = {
#include "jep106.inc"
};

static bool CheckParity(uint8_t Data,bool bOdd)
{
   bool bIsOdd = false;
   uint8_t Mask = 1;

   for(int i = 0; i < 8; i++) {
      if(Data & Mask) {
         bIsOdd = !bIsOdd;
      }
      Mask <<= 1;
   }
   return bOdd == bIsOdd;
}

const char *JEP106_ID_2_string(uint8_t *pData,int DataLen,uint8_t *pDevId,uint16_t *pManId)
{
   uint8_t ManufactureID;
   int Bank = 0;
   const char *Ret = "Unknown";
   const char *ParityErr = "Invaild - parity error";
   const char *BankErr = "Invaild - bank";

   for(int i = 0; i < DataLen; i++) {
      if((ManufactureID = pData[i]) == 0x7f) {
         Bank++;
         if(Bank > 16) {
            Ret = BankErr;
            break;
         }
      }
      else {
      // found bank, check parity
         if(!CheckParity(ManufactureID,true)) {
            Ret = ParityErr;
            break;
         }
         ManufactureID &= 0x7f;
         if(JEP106_LookupTbl[Bank][ManufactureID - 1] != NULL) {
            Ret = JEP106_LookupTbl[Bank][ManufactureID - 1];
         }
         if(pManId != NULL) {
            *pManId = ManufactureID | (Bank << 8);
         }
         if(pDevId != NULL && i < DataLen - 1) {
            *pDevId = pData[i + 1];
         }
         break;
      }
   }

   return Ret;
}
