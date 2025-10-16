#include <LittleFS.h>

void handleOTAUpdate() {
    Serial.println(F("OTA_HANDLER: Checking for firmware update..."));
    
    if (!LittleFS.exists("/firmware.bin")) {
        Serial.println(F("OTA_HANDLER: No firmware file found"));
        return;
    }
    
    File fwFile = LittleFS.open("/firmware.bin", "r");
    if (!fwFile) {
        Serial.println(F("OTA_HANDLER: Failed to open firmware file"));
        return;
    }
    
    size_t fwSize = fwFile.size();
    fwFile.close();
    
    Serial.print(F("OTA_HANDLER: Firmware size: "));
    Serial.println(fwSize);
    
    if (fwSize < 100000) {
        Serial.println(F("OTA_HANDLER: Invalid firmware size"));
        LittleFS.remove("/firmware.bin");
        return;
    }
    
    Serial.println(F("OTA_HANDLER: Firmware is valid"));
    Serial.println(F("OTA_HANDLER: Please use web interface to complete OTA update"));
    Serial.println(F("OTA_HANDLER: Visit http://192.168.10.10/update"));
    
    // Файл прошивки остается в LittleFS для веб-обновления
    // Пользователь должен завершить OTA через веб-интерфейс
}