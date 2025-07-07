#include "SD_Card.h"

// Define them only here
uint16_t SDCard_Size = 0;
uint16_t Flash_Size = 0;
bool SDCard_Flag = false;
bool SDCard_Finish = false;

#define EXIO_PIN4 4  // Replace with actual pin if needed

void SD_Init() {
  // SD MMC
  if(!SD_MMC.setPins(SD_CLK_PIN, SD_CMD_PIN, SD_D0_PIN,-1,-1,-1)){
    Serial.printf("SD MMC: Pin change failed!\r\n");
    return;
  }
  // SD_D3_EN();
  if (SD_MMC.begin("/sdcard", true, false)) {                                          
    Serial.printf("SD Card initialization successful!\r\n");
  } else {
    Serial.printf("SD Card initialization failed!\r\n");
  }
  uint8_t cardType = SD_MMC.cardType();
  if(cardType == CARD_NONE){
    Serial.printf("No SD Card attached\r\n");
    return;
  }
  else{
    Serial.printf("SD Card Type: ");
    if(cardType == CARD_MMC){
      Serial.printf("MMC\r\n");
    } else if(cardType == CARD_SD){
      Serial.printf("SDSC\r\n");
    } else if(cardType == CARD_SDHC){
      Serial.printf("SDHC\r\n");
    } else {
      Serial.printf("UNKNOWN\r\n");
    }
    uint64_t totalBytes = SD_MMC.totalBytes();
    uint64_t usedBytes = SD_MMC.usedBytes();
    SDCard_Size = totalBytes/(1024*1024);
    Serial.printf("Total SD Card space: %llu\n", totalBytes);
    Serial.printf("Used SD Card space: %llu\n", usedBytes);
    Serial.printf("Free SD Card space: %llu\n", totalBytes - usedBytes);
  }
}

bool File_Search(const char* directory, const char* fileName)    
{
  File Path = SD_MMC.open(directory);
  if (!Path) {
    Serial.printf("Path: <%s> does not exist\r\n",directory);
    return false;
  }
  File file = Path.openNextFile();
  while (file) {
    if (strcmp(file.name(), fileName) == 0) {                           
      if (strcmp(directory, "/") == 0)
        Serial.printf("File '%s%s' found in root directory.\r\n",directory,fileName);  
      else
        Serial.printf("File '%s/%s' found in root directory.\r\n",directory,fileName); 
      Path.close();                                                     
      return true;                                                     
    }
    file = Path.openNextFile();                                        
  }
  if (strcmp(directory, "/") == 0)
    Serial.printf("File '%s%s' not found in root directory.\r\n",directory,fileName);           
  else
    Serial.printf("File '%s/%s' not found in root directory.\r\n",directory,fileName);          
  Path.close();                                                         
  return false;                                                         
}

uint16_t Folder_retrieval(const char* directory, const char* fileExtension, char File_Name[][100], size_t File_Sizes[], uint16_t maxFiles)
{
  File Path = SD_MMC.open(directory);
  if (!Path || !Path.isDirectory()) {
    Serial.printf("Path: <%s> does not exist or is not a directory\r\n", directory);
    return 0;
  }

  uint16_t fileCount = 0;
  File file = Path.openNextFile();
  while (file && fileCount < maxFiles) {
    if (!file.isDirectory() && 
        (strlen(fileExtension) == 0 || strstr(file.name(), fileExtension))) {
      
      strncpy(File_Name[fileCount], file.name(), sizeof(File_Name[fileCount]) - 1);
      File_Name[fileCount][sizeof(File_Name[fileCount]) - 1] = '\0';
      File_Sizes[fileCount] = file.size();

      Serial.printf("File: %s (%llu bytes)\r\n", file.name(), (uint64_t)file.size());

      fileCount++;
    }
    file = Path.openNextFile();
  }

  Path.close();
  return fileCount;
}


bool Create_File_If_Not_Exists(const char* path) {
  if (SD_MMC.exists(path)) {
    Serial.printf("File already exists: %s\n", path);
    return false;
  }

  File file = SD_MMC.open(path, FILE_WRITE);
  if (!file) {
    Serial.printf("Failed to create file: %s\n", path);
    return false;
  }

  file.println("123");
  file.close();
  Serial.printf("File created and written: %s\n", path);
  return true;
}


void LoadSDCardMP3Files(std::vector<String>* fileList, const char* path) {
    if (!fileList) return;

    fileList->clear();  // Clear previous entries

    File dir = SD_MMC.open(path);
    if (!dir || !dir.isDirectory()) {
        Serial.printf("Failed to open directory: %s\n", path);
        return;
    }

    File file = dir.openNextFile();
    while (file) {
        String name = file.name();
        String lower = name;
        lower.toLowerCase();
        if (!file.isDirectory() && (name.endsWith(".mp3") || name.endsWith(".wav"))) {
            // Only store relative names for consistency
            if (name.startsWith(path)) {
                name = name.substring(strlen(path));
                if (name.startsWith("/")) name = name.substring(1);
            }
            fileList->push_back(name);
        }
        file = dir.openNextFile();
    }
    dir.close();
}

std::vector<std::pair<String, String>> ReadInternetStations(const char* path) {
    std::vector<std::pair<String, String>> stations;

    File file = SD_MMC.open(path, FILE_READ);
    if (!file || file.isDirectory()) {
        Serial.printf("Failed to open %s\n", path);
        return stations;
    }

    while (file.available()) {
        String line = file.readStringUntil('\n');
        line.trim();
        if (line.isEmpty()) continue;

        int sepIndex = line.indexOf('|');
        if (sepIndex <= 0 || sepIndex >= line.length() - 1) continue;

        String name = line.substring(0, sepIndex);
        String url = line.substring(sepIndex + 1);

        stations.emplace_back(name, url);
    }

    file.close();
    return stations;
}


int recordIndex = 1;

String generateRotatingFileName() {
    char filename[20];
    snprintf(filename, sizeof(filename), "/record_%02d.wav", recordIndex);
    recordIndex++;
    if (recordIndex > 9) recordIndex = 1;
    return String(filename);
}

// Function to write srmodels.bin to the model partition if needed
// This function checks if the file exists on the SD card, reads it, and compares it with the flash partition.
void write_srmodels_bin_to_partition_if_needed(bool force) {
    printf("Checking for srmodels.bin on SD card...\n");
    const char* path = "/srmodels.bin";

    File file = SD_MMC.open(path, FILE_READ);
    if (!file) {
        printf("srmodels.bin not found on SD card\n");
        return;
    }
    printf("Found srmodels.bin on SD card, checking flash...\n");

    const esp_partition_t* model_partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "model"
    );
    if (!model_partition) {
        printf("model partition not found!\n");
        file.close();
        return;
    }
    printf("Model partition found: %s, size: %d bytes\n", model_partition->label, model_partition->size);

    if (!force) {
      const size_t COMPARE_SIZE = 512; // Size to compare, adjust as needed
      uint8_t sdcard_buf[COMPARE_SIZE] = {0};
      uint8_t flash_buf[COMPARE_SIZE] = {0};

      size_t bytes_read = file.read(sdcard_buf, COMPARE_SIZE);
      if (bytes_read == 0) {
          printf("Failed to read from srmodels.bin\n");
          file.close();
          return;
      }

      esp_err_t err = esp_partition_read(model_partition, 0, flash_buf, bytes_read);
      if (err != ESP_OK) {
          printf("Failed to read flash partition: %s\n", esp_err_to_name(err));
          file.close();
          return;
      }
      printf("Read %d bytes from srmodels.bin and flash partition\n", bytes_read);

      if (memcmp(sdcard_buf, flash_buf, bytes_read) == 0) {
          printf("Model partition already up-to-date, skipping flash\n");
          file.close();
          return;
      }

      printf("Hex dump of first 32 bytes:\n");
      printf("SDCard : ");
      for (int i = 0; i < 32; ++i) {
          printf("%02X ", sdcard_buf[i]);
      }
      printf("\nFlash  : ");
      for (int i = 0; i < 32; ++i) {
          printf("%02X ", flash_buf[i]);
      }
      printf("\n");

      if ((file.size()) > model_partition->size) {
          printf("srmodels.bin is too large for model partition!\n");
          file.close();
          return;
      }
    }

    printf("Writing srmodels.bin to model partition...\n");
    file.seek(0);

    esp_err_t err = esp_partition_erase_range(model_partition, 0, model_partition->size);
    if (err != ESP_OK) {
        printf("Failed to erase flash: %s\n", esp_err_to_name(err));
        file.close();
        return;
    }

    uint8_t* buffer = (uint8_t*)malloc(4096);
    size_t offset = 0;
    size_t read_bytes;

    while ((read_bytes = file.read(buffer, 4096)) > 0) {
        err = esp_partition_write(model_partition, offset, buffer, read_bytes);
        if (err != ESP_OK) {
            printf("Flash write failed at offset %d: %s\n", offset, esp_err_to_name(err));
            file.close();
            free(buffer);
            return;
        }
        offset += read_bytes;
    }

    file.close();
    free(buffer);
    printf("srmodels.bin written to model partition (%d bytes)\n", offset);
}
