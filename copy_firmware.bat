@echo off
xcopy ".pio\build\esp32dev\firmware.bin" "BinFiles\" /v /f /y
xcopy ".pio\build\esp32dev\partitions.bin" "BinFiles\" /v /f /y
xcopy "%userprofile%\.platformio\packages\framework-arduinoespressif32\tools\sdk\esp32\bin\bootloader_dio_40m.bin" "BinFiles\" /v /f /y
xcopy "%userprofile%\.platformio\packages\framework-arduinoespressif32\tools\partitions\boot_app0.bin" "BinFiles\" /v /f /y
pause