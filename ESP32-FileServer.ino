#include <Arduino.h>
#include <SimpleCLI.h>
#include <SPI.h>
#include <SdFat.h>
#include <WiFi.h>
#include <ESP32-FTP-Server.h>

#define WIFI_SSID "WIFI_SSID"
#define WIFI_PASSWORD "WIFI_PASSWORD"
#define FTP_USER "ftp_user"
#define FTP_PASSWORD "ftp_password"

TaskHandle_t Task_CLI;
TaskHandle_t Task_FTP;
SPIClass spi = SPIClass(VSPI);
FTPServer ftpSrv;
SdFat sd;

/*
SPI  | MOSI     MISO     CLK      CS
-----|--------|--------|--------|-------
VSPI | GPIO23 | GPIO19 | GPIO18 | GPIO5
HSPI | GPIO13 | GPIO12 | GPIO14 | GPIO15
*/

#define MOSI 23
#define MISO 19
#define SCK 18
#define CS 5
#define SD_CONFIG SdSpiConfig(CS, DEDICATED_SPI, SD_SCK_MHZ(8), &spi)

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
}

void setup()
{
    Serial.begin(115200); // HardwareSerial 0
    Serial2.begin(9200);  // HardwareSerial 2
    Serial2.println();

    initWiFi();

    // create a task that will be executed in the TaskCLI() function, with priority 1 and executed on core 0
    xTaskCreatePinnedToCore(
        TaskCLI,   /* Task function. */
        "TaskCLI", /* name of task. */
        10000,     /* Stack size of task */
        NULL,      /* parameter of the task */
        1,         /* priority of the task */
        &Task_CLI, /* Task handle to keep track of created task */
        0);        /* pin task to core 0 */

    delay(500);

    // create a task that will be executed in the Task2code() function, with priority 1 and executed on core 1
    xTaskCreatePinnedToCore(
        TaskFTP,   /* Task function. */
        "TaskFTP", /* name of task. */
        10000,     /* Stack size of task */
        NULL,      /* parameter of the task */
        1,         /* priority of the task */
        &Task_FTP, /* Task handle to keep track of created task */
        1);        /* pin task to core 1 */

    delay(500);
}

void TaskCLI(void *pvParameters)
{
    SimpleCLI cli;
    Command ping;
    Command cmdLs;

    String input;
    bool showPrompt = true;

    Serial.print("TaskCLI running on core ");
    Serial.println(xPortGetCoreID());

    // Create the commands
    ping = cli.addCmd("ping");

    cmdLs = cli.addCmd("ls");
    cmdLs.addFlagArg("a");
    cmdLs.setDescription(" Lists files in directory (-a for all)");

    while (true)
    {
        if (showPrompt)
        {
            Serial2.print(":/# ");
            showPrompt = false;
        }

        if (Serial2.available())
        {
            int keycode = Serial2.read();
            char c = (char)keycode;

            switch (keycode)
            {
            case (13): // CR (Carriage return)
                Serial2.println();
                cli.parse(input);
                showPrompt = true;
                input.clear();
                break;

            case (8): // BS (Backspace)
                if (input.length() > 0)
                {
                    String tmp = "";
                    for (int i = 0; i < input.length() - 1; i++)
                    {
                        tmp += input[i];
                    }
                    input = tmp;

                    Serial2.print(c);
                    Serial2.print(' ');
                    Serial2.print(c);
                }
                break;

            default:
                input += c;
                Serial2.print(c);
            }
        }

        // Check for newly parsed commands
        if (cli.available())
        {
            Command cmd = cli.getCmd();

            if (cmd == ping)
            {
                Serial2.println("Pong!");
            }
            else if (cmd == cmdLs)
            {
                Argument a = cmd.getArgument("a");
                // listDir(SD, "/", 0, a.isSet());
            }
        }

        // Check for parsing errors
        if (cli.errored())
        {
            CommandError cmdError = cli.getError();
            Serial2.print("ERROR: ");
            Serial2.println(cmdError.toString());
            if (cmdError.hasCommand())
            {
                Serial2.print("Did you mean \"");
                Serial2.print(cmdError.getCommand().toString());
                Serial2.println("\"?");
            }
        }
        vTaskDelay(1);
    }
}

void TaskFTP(void *pvParameters)
{
    Serial.print("TaskFTP running on core ");
    Serial.println(xPortGetCoreID());

    while (true)
    {
        ftpSrv.handle();
        vTaskDelay(1);
    }
}

void loop()
{
}

/*
void listDir(fs::FS &fs, const char *dirname, uint8_t levels, bool listall)
{
    File root = fs.open(dirname);
    if (!root)
    {
        Serial2.println("Failed to open directory");
        return;
    }
    if (!root.isDirectory())
    {
        Serial2.println("Not a directory");
        return;
    }

    File file = root.openNextFile();
    while (file)
    {
        if (file.isDirectory())
        {
            Serial2.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1, listall);
            }
        }
        else
        {
            Serial2.print(file.name() + 1);

            if (listall)
            {
                time_t t = file.getLastWrite();
                struct tm *tmstruct = gmtime(&t);
                Serial2.printf("  [Size: %d, Date modified: %d-%02d-%02d %02d:%02d:%02d (UTC)]\n\r", file.size(), (tmstruct->tm_year) + 1900,
                               (tmstruct->tm_mon) + 1, tmstruct->tm_mday, tmstruct->tm_hour, tmstruct->tm_min, tmstruct->tm_sec);
            }
            else
            {
                Serial2.println();
            }
        }
        file = root.openNextFile();
    }
}
*/