# ESP32-FileServer
ESP32 FileServer Application for the Centronics Parallel Port

WORK IN PROGRESS!!

The idea is to connect a `ESP32` to the Amiga Parallel Port to act as a filehub where you can do file transfers to/from your PC and then fetch those files using a file transfer protocol like `Kermit` or `Zmodem` on the `Amiga ⇄ ESP32` side.

Level shifting will be needed since the `ESP32` is not 5V-tolerant on the GPIO-pins

This project currently uses these libraries: <br />

    Used library     Version Path                                                                                      
    SPI              1.0     C:\Users\Jorgen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6\libraries\SPI 
    SdFat            2.2.0   C:\Users\Jorgen\Documents\Arduino\libraries\SdFat                                         
    WiFi             1.0     C:\Users\Jorgen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6\libraries\WiFi
    ESP32-FTP-Server 1.0.0   C:\Users\Jorgen\Documents\Arduino\libraries\ESP32-FTP-Server                              
    ESP Telnet       2.1.1   C:\Users\Jorgen\Documents\Arduino\libraries\ESPTelnet-2.1.1
    
<br />

https://github.com/jbilander/ESP32-FTP-Server <br />
https://github.com/greiman/SdFat/ <br />
https://github.com/LennartHennigs/ESPTelnet <br />


Status:<br />
Currently it's possible to FTP with active-ftp to the `ESP32` and store files on a `FAT\FAT32\exFAT` formatted MicroSD-card connected to the SPI-pins `(MOSI 13, MISO 12, SCK 14, CS 4)` on the `ESP32`.
It is also possible to list the content by connecting a USB-to-TTL-serial-UART-converter to `UART2` (`TX2`, `RX2`) or connect via Telnet from your PC and type the command `ls`.
<br />
<br />
`esptool.exe` command to flash the ESP32-DEVKIT-V1 board:

    "C:\Users\Jorgen\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\3.0.0/esptool.exe" --chip esp32 --port "COM4" --baud 115200 --before default_reset --after hard_reset write_flash -z --flash_mode dio --flash_freq 40m --flash_size detect 0xe000 "C:\Users\Jorgen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6/tools/partitions/boot_app0.bin" 0x1000 "C:\Users\Jorgen\AppData\Local\Arduino15\packages\esp32\hardware\esp32\1.0.6/tools/sdk/bin/bootloader_dio_40m.bin" 0x10000 "c:\Users\Jorgen\Projects\ESP32-FileServer\build/ESP32-FileServer.ino.bin" 0x8000 "c:\Users\Jorgen\Projects\ESP32-FileServer\build/ESP32-FileServer.ino.partitions.bin"

ToDo:<br />
Implement a file transfer protocol for the ESP32 that will connect to the Amiga over the parallel port.<br />
`Kermit`, `Zmodem` or `HS/Link` are candidate protocols.

Module I use:<br />
`DOIT ESP32 DEVKIT V1`
