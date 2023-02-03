#include <Arduino.h>
#include <SimpleCLI.h>
#include <SD.h>
#include <SimpleFTPServer.h>

TaskHandle_t Task_CLI;
TaskHandle_t Task_FTP;

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

SPIClass spi = SPIClass(VSPI);

// Initialize SD card
void initSDCard()
{
    uint64_t totalB;   // total Bytes
    uint64_t freeB;    // free Bytes
    uint64_t usedB;    // used Bytes = total Bytes - free Bytes
    uint64_t cardSize; // only for SD: size of card

    spi.begin(SCK, MISO, MOSI, CS);

    Serial.println("\nMounting SD card");
    if (!SD.begin(CS, spi, 80000000))
    {
        Serial.println("Card Mount Failed");
        return;
    }

    switch (SD.cardType())
    {
    case (CARD_NONE):
        Serial.println("No SD card attached");
        return;
    case (CARD_MMC):
        Serial.println("MMC");
        break;
    case (CARD_SD):
        Serial.println("SDSC");
        break;
    case (CARD_SDHC):
        Serial.println("SDHC");
        break;
    default:
        Serial.println("UNKNOWN");
    }

    Serial.println();
    totalB = SD.totalBytes();
    usedB = SD.usedBytes();
    freeB = totalB - usedB; // Number Of freeBytes in SD
    cardSize = SD.cardSize() / (1024 * 1024);
    Serial.printf("SD: total: %llu,  free: %llu,  used: %llu, card size: %llu MB\n", totalB, freeB, usedB, cardSize);
    Serial.println();
}

void setup()
{
    Serial.begin(115200); // HardwareSerial 0
    Serial2.begin(9200);  // HardwareSerial 2
    Serial2.println();

    initSDCard();

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
                listDir(SD, "/", 0, a.isSet());
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
        vTaskDelay(1);
    }
}

void loop()
{
}

void listDir(fs::FS &fs, const char *dirname, uint8_t levels, bool listall)
{
    // Serial2.printf("Listing directory: %s\n\r", dirname);
    // Serial2.println();

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
            // Serial2.print("  DIR: ");
            Serial2.println(file.name());
            if (levels)
            {
                listDir(fs, file.name(), levels - 1, listall);
            }
        }
        else
        {
            // Serial2.print(" FILE: ");
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
