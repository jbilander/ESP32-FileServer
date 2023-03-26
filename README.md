# ESP32-FileServer
ESP32 FileServer Application for the Centronics Parallel Port

WORK IN PROGRESS!!

The idea is to connect a `ESP32` to the Amiga Parallel Port to act as a filehub where you can do file transfers to/from your PC and then fetch those files using a file transfer protocol like `Kermit` or `Zmodem` on the `Amiga ⇄ ESP32` side.

Level shifting will be needed since the `ESP32` is not 5V-tolerant on the GPIO-pins

This project currently uses these libraries: <br />
https://github.com/jbilander/ESP32-FTP-Server <br />
https://github.com/greiman/SdFat/ <br />
https://github.com/SpacehuhnTech/SimpleCLI <br />

Status:<br />
Currently it's possible to FTP with active-ftp to the `ESP32` and store files on a `FAT32` MicroSD-card connected to the SPI-pins `(MOSI 23, MISO 19, SCK 18, CS 5)` on the `ESP32`.
It is also possible to list the content by connecting a USB-to-TTL-serial-UART-converter to `UART2` (`TX2`, `RX2`) on the `ESP32` and type the command `ls` or `ls -a`.

ToDo:<br />
Implement a file transfer protocol for the ESP32 that will connect to the Amiga over the parallel port.<br />
`Kermit`, `Zmodem` or `HS/Link` are candidate protocols.

Module I use:<br />
`DOIT ESP32 DEVKIT V1`
