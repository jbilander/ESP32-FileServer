#include <Arduino.h>
#include <SimpleCLI.h>

SimpleCLI cli;
Command ping;

TaskHandle_t Task_CLI;
TaskHandle_t Task_FTP;

// const int LED = 2;
bool showPrompt = true;

void setup()
{
    Serial.begin(115200); // HardwareSerial 0
    Serial2.begin(38400); // HardwareSerial 2
    Serial2.println();
    // pinMode(LED, OUTPUT);

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
    Serial.print("TaskCLI running on core ");
    Serial.println(xPortGetCoreID());

    // Create the commands
    ping = cli.addCmd("ping");

    if (!ping)
    {
        Serial.println("Something went wrong :(");
    }
    else
    {
        Serial.println("Ping was added to the CLI!");
    }

    while (true)
    {
        if (showPrompt)
        {
            Serial2.print(":/# ");
            showPrompt = false;
        }

        if (Serial2.available())
        {
            // Read out string from the serial monitor
            String input = Serial2.readStringUntil('\n');

            // Echo the user input
            Serial2.println(input);
            showPrompt = true;

            // Parse the user input into the CLI
            cli.parse(input);
        }

        // Check for newly parsed commands
        if (cli.available())
        {
            // Get command out of queue
            Command cmd = cli.getCmd();

            // React on our ping command
            if (cmd == ping)
            {
                Serial2.println("Pong!");
            }
        }

        // Check for parsing errors
        if (cli.errored())
        {
            // Get error out of queue
            CommandError cmdError = cli.getError();

            // Print the error
            Serial2.print("ERROR: ");
            Serial2.println(cmdError.toString());

            // Print correct command structure
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
