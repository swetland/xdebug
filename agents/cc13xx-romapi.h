
#define ROM_API_TABLE           ((uint32_t*) 0x10000180)
#define ROM_API_FLASH_TABLE     ((uint32_t*) (ROM_API_TABLE[10]))

#define ROM_FlashPowerModeGet \
    ((uint32_t (*)(void)) \
    ROM_API_FLASH_TABLE[1])

#define ROM_FlashProtectionSet \
    ((void (*)(uint32_t ui32SectorAddress, uint32_t ui32ProtectMode)) \
    ROM_API_FLASH_TABLE[2])

#define ROM_FlashProtectionGet \
    ((uint32_t (*)(uint32_t ui32SectorAddress)) \
    ROM_API_FLASH_TABLE[3])

#define ROM_FlashProtectionSave \
    ((uint32_t (*)(uint32_t ui32SectorAddress)) \
    ROM_API_FLASH_TABLE[4])

#define ROM_FlashSectorErase \
    ((uint32_t (*)(uint32_t ui32SectorAddress)) \
    ROM_API_FLASH_TABLE[5])

#define ROM_FlashProgram \
    ((uint32_t (*)(const void *pui8DataBuffer, uint32_t ui32Address, uint32_t ui32Count)) \
    ROM_API_FLASH_TABLE[6])

#define ROM_FlashEfuseReadRow \
    ((uint32_t (*)(uint32_t* pui32EfuseData, uint32_t ui32RowAddress)) \
    ROM_API_FLASH_TABLE[8])

#define ROM_FlashDisableSectorsForWrite \
    ((void (*)(void)) \
    ROM_API_FLASH_TABLE[9])
