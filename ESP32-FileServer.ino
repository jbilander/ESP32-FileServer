#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <WiFi.h>
#include <ESP32-FTP-Server.h>
#include <ESP32-ParIO-Server.h>
#include <ESPTelnet.h>
#include <StreamString.h>
#include "Util.h"

#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"
#define FTP_USER "ftp"
#define FTP_PASSWORD "ftp"
#define RX1 36    // Re-assign to GPIO36 (VP-pin) since UART1 pins on ESP-WROOM-32 are reserved for the integrated flash memory chip.
#define TX1 15    // Re-assign to GPIO15 since UART1 pins on ESP-WROOM-32 are reserved for the integrated flash memory chip.
#define ACT_LED 2 // Activity LED pin

using namespace util;

TaskHandle_t task_TelnetCLI;
TaskHandle_t task_SerialCLI;
TaskHandle_t task_FTP;
TaskHandle_t task_ParIO;

SPIClass spi;
ESPTelnet telnet;
FTPServer ftpSrv;
ParIOServer parioSrv;
SdFat sd;

boolean kermit_cli; // Keeps track if telnet cli is in kermit interactive mode
uint16_t port = 23; // Telnet port, default is 23
String path = "/";  // Filesystem root
String prompt;      // The prompt to display on Serial or Telnet CLI

/*
SPI  | MOSI     MISO     CLK      CS
-----|--------|--------|--------|-------
VSPI | GPIO23 | GPIO19 | GPIO18 | GPIO5
HSPI | GPIO13 | GPIO12 | GPIO14 | GPIO15
*/

#define MOSI 13
#define MISO 12
#define SCK 14
#define CS 4 // Assign CS to GPIO4 since GPIO15 is routed to TX1, and GPIO4 is used for CS on carrier board (Parallel_to_ESP32 PCB).
#define SD_CONFIG SdSpiConfig(CS, DEDICATED_SPI, SD_SCK_MHZ(16), &spi)

// Initialize SD card
bool initSDCard()
{
    uint64_t cardSize;

    spi.begin(SCK, MISO, MOSI, CS);

    if (!sd.begin(SD_CONFIG))
    {
        Serial.println("Card Mount Failed");
        sd.printSdError(&Serial);
        return false;
    }

    cardSize = sd.card()->sectorCount();

    /*
    Capacity | Type
    ---------|---------------------
    SD       | 2GB and under
    SDHC     | More than 2GB, up to 32GB
    SDXC     | More than 32GB, up to 2TB
    SDUC     | More than 2TB, up to 128TB
    */

    Serial.print("Card type: ");
    switch (sd.card()->type())
    {
    case SD_CARD_TYPE_SD1:
        Serial.println("SD1"); // Standard capacity V1 SD card
        break;
    case SD_CARD_TYPE_SD2:
        Serial.println("SD2"); // Standard capacity V2 SD card
        break;
    case SD_CARD_TYPE_SDHC:
        if (cardSize < 70000000) // The number of 512 byte data sectors in the card
        {
            Serial.println("SDHC"); // High Capacity SD card
        }
        else
        {
            Serial.println("SDXC"); // Extended Capacity SD card
        }
        break;
    default:
        Serial.println("Unknown");
        break;
    }

    Serial.println("Scanning FAT, please wait.");
    if (sd.fatType() <= FAT_TYPE_FAT32)
    {
        Serial.print("Volume type is: FAT");
        Serial.println(int(sd.vol()->fatType()), DEC);
    }
    else
    {
        Serial.println("Volume type is: exFAT");
    }

    cardSize = cardSize * 512 / (1024 * 1024); // Card size in real MegaBytes
    Serial.printf("Card size: %llu MB", cardSize);
    Serial.println();

    return true;
}

// Initialize WiFi
void initWiFi()
{
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED)
    {
        delay(500);
        Serial.print(".");
    }

    Serial.println();
    Serial.print("Connected to: ");
    Serial.println(WiFi.SSID());
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    // FTP Setup, ensure SD with FAT/exFAT is ok before ftp
    Serial.println("Initializing SD card...");
    if (initSDCard())
    {
        Serial.println("...done.");
    }
    else
    {
        Serial.println("...fail.");
        while (true)
        {
            delay(1000);
        }
    }

    ftpSrv.addUser(FTP_USER, FTP_PASSWORD);
    ftpSrv.addFilesystem(&sd);
    ftpSrv.begin();

    telnet.onConnect(onTelnetConnect);
    telnet.onConnectionAttempt(onTelnetConnectionAttempt);
    telnet.onReconnect(onTelnetReconnect);
    telnet.onDisconnect(onTelnetDisconnect);
    telnet.onInputReceived(onTelnetInput);
    telnet.begin(port);
}

void setup()
{
    setPrompt();
    Serial.begin(115200);                       // HardwareSerial 0, UART0 pins are connected to the USB-to-Serial converter and are used for flashing and debugging.
    Serial2.begin(38400);                       // HardwareSerial 2, used for Kermit over serial transfer
    Serial1.begin(19200, SERIAL_8N1, RX1, TX1); // HardwareSerial 1, used for CLI over serial (optional instead of using telnet)
    pinMode(ACT_LED, OUTPUT);

    initWiFi();

    parioSrv.setup();

    // Disable watchdog trigger on core0
    disableCore0WDT();

    // create a task that will be executed in the TaskTelnetCLI() function
    xTaskCreate(
        TaskTelnetCLI,    /* Task function. */
        "TaskTelnetCLI",  /* name of task. */
        10000,            /* Stack size of task */
        NULL,             /* parameter of the task */
        2,                /* priority of the task */
        &task_TelnetCLI); /* Task handle to keep track of created task */

    // create a task that will be executed in the TaskSerialCLI() function
    xTaskCreate(
        TaskSerialCLI,    /* Task function. */
        "TaskSerialCLI",  /* name of task. */
        10000,            /* Stack size of task */
        NULL,             /* parameter of the task */
        2,                /* priority of the task */
        &task_SerialCLI); /* Task handle to keep track of created task */

    // create a task that will be executed in the TaskFTP() function
    xTaskCreate(
        TaskFTP,    /* Task function. */
        "TaskFTP",  /* name of task. */
        10000,      /* Stack size of task */
        NULL,       /* parameter of the task */
        2,          /* priority of the task */
        &task_FTP); /* Task handle to keep track of created task */

    // create a task that will be executed in the TaskParIO() function
    xTaskCreate(
        TaskParIO,    /* Task function. */
        "TaskParIO",  /* name of task. */
        10000,        /* Stack size of task */
        NULL,         /* parameter of the task */
        2,            /* priority of the task */
        &task_ParIO); /* Task handle to keep track of created task */
}

void TaskTelnetCLI(void *pvParameters)
{
    delay(100);
    Serial.print("Task_TelnetCLI running on core ");
    Serial.println(xPortGetCoreID());

    while (true)
    {
        telnet.loop();
        vTaskDelay(1);
    }
}

void TaskSerialCLI(void *pvParameters)
{
    delay(200);
    Serial.print("Task_SerialCLI running on core ");
    Serial.println(xPortGetCoreID());

    String input;
    Serial1.println();
    Serial1.print(prompt);

    while (true)
    {
        if (Serial1.available())
        {
            int keycode = Serial1.read();
            char c = (char)keycode;

            switch (keycode)
            {
            case 13: // CR  (Carriage return)
                onCliInput(input, UART);
                input.clear();
                break;

            default:
                input += c;
                Serial1.print(c);
                break;
            }
        }

        vTaskDelay(1);
    }
}

void TaskFTP(void *pvParameters)
{
    delay(300);
    Serial.print("Task_FTP running on core ");
    Serial.println(xPortGetCoreID());

    while (true)
    {
        ftpSrv.handle();
    }
}

void TaskParIO(void *pvParameters)
{
    delay(400);
    Serial.print("Task_ParIO running on core ");
    Serial.println(xPortGetCoreID());

    while (true)
    {
        parioSrv.handle();
    }
}

// (optional) callback functions for telnet events
void onTelnetConnect(String ip)
{
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" connected");

    telnet.println("Welcome " + telnet.getIP());
    telnet.println("(Escape character is '^]', type exit to disconnect)");
    telnet.print("\n" + prompt);
}

void onTelnetDisconnect(String ip)
{
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" disconnected");
}

void onTelnetReconnect(String ip)
{
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" reconnected");
}

void onTelnetConnectionAttempt(String ip)
{
    Serial.print("- Telnet: ");
    Serial.print(ip);
    Serial.println(" tried to connect");
}

void onTelnetInput(String str)
{
    onCliInput(str, TELNET);
}

void cmdResponse(String str, ClientEnum client)
{
    if (client == TELNET)
    {
        telnet.println();
        telnet.println(str);
        telnet.print(prompt);
    }
    else if (client == UART)
    {
        Serial1.println();
        Serial1.println(str);
        Serial1.print(prompt);
    }
}

void setPrompt()
{
    if (kermit_cli)
    {
        prompt = "(" + path + ") C-Kermit>";
    }
    else
    {
        prompt = path + ">";
    }
}

// Cli input command from either Telnet or Serial1 (UART1) client gets parsed here:
void onCliInput(String input, ClientEnum client)
{
    input.trim();
    String line = input;
    std::vector<String> lineSplit = util::Split<std::vector<String>>(line, ' ');
    String cmd = lineSplit[0];
    cmd.toUpperCase();
    CommandEnum c = CommandMap()[cmd];

    switch (c)
    {
    case LS:
    case DIR:
    {
        File dir = sd.open(path);
        File f = dir.openNextFile();
        StreamString streamString;

        while (f)
        {
            f.printFileSize(&streamString);
            streamString.write(' ');
            f.printModifyDateTime(&streamString);
            streamString.write(' ');
            f.printName(&streamString);

            if (f.isDir())
            {
                // Indicate a directory.
                streamString.write('/');
            }
            streamString.println();
            f.close();
            f = dir.openNextFile();
        }

        cmdResponse(streamString, client);
        break;
    }
    case EXIT:
    {
        if (kermit_cli)
        {
            kermit_cli = false;
            setPrompt();
            cmdResponse("", client);
        }
        else
        {
            if (client == TELNET)
            {
                telnet.print("\ndisconnecting you...");
                telnet.disconnectClient();
            }
            else
            {
                cmdResponse("", client);
            }
        }
        break;
    }
    case CD:
    {
        String arg = lineSplit.size() > 1 ? lineSplit[1] : "";

        if (sd.chdir(arg))
        {
            if (arg == "/")
            {
                path = arg;
            }
            else
            {
                sd.chdir(path);
                std::vector<String> splitPath = util::Split<std::vector<String>>(arg, '/');

                for (int i = 0; i < splitPath.size(); i++)
                {
                    if (splitPath[i] != "")
                    {
                        File dir = sd.open(splitPath[i]);
                        char name[263];
                        dir.getName(name, 263);
                        if (path == "/")
                            path = "";
                        path += "/" + String(name);
                        sd.chdir(splitPath[i]);
                    }
                }
            }
            setPrompt();
            cmdResponse("", client);
        }
        else
        {
            if (arg == "..")
            {
                if (path != "/")
                {
                    int index = path.lastIndexOf("/");
                    index == 0 ? path = "/" : path = path.substring(0, index);
                }
                sd.chdir(path);
                setPrompt();
                cmdResponse("", client);
            }
            else
            {
                cmdResponse("The system cannot find the path specified.", client);
            }
        }
        break;
    }
    case KERMIT:
    {
        kermit_cli = true;
        setPrompt();
        cmdResponse("", client);
        break;
    }

    default:

        if (input != "")
        {
            cmdResponse("Unknown command: " + input, client);
        }
        else
        {
            cmdResponse("" + input, client);
        }
        break;
    }
}

void loop()
{
}
