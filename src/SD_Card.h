#ifndef SD_CARD_H
#define SD_CARD_H

#include "Arduino.h"
#include <cstring>
#include "FS.h"
#include "SD_MMC.h"

#define SD_CLK_PIN      14
#define SD_CMD_PIN      17 
#define SD_D0_PIN       16 
#define SD_CS_PIN       21

extern uint16_t SDCard_Size;
extern uint16_t Flash_Size;
extern bool SDCard_Flag;
extern bool SDCard_Finish;

void SD_Init();
bool Create_File_If_Not_Exists(const char* path);
bool File_Search(const char* directory, const char* fileName);
uint16_t Folder_retrieval(const char* directory, const char* fileExtension, char File_Name[][100], size_t File_Sizes[], uint16_t maxFiles); 

// If srmodels.bin exists on SD card, write it to the model partition if needed
void write_srmodels_bin_to_partition_if_needed(bool force = false);

String generateRotatingFileName();
#endif