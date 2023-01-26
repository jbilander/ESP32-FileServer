#include <Arduino.h>
#include <SimpleCLI.h>

TaskHandle_t Task_CLI;
TaskHandle_t Task_FTP;

// const int LED = 2;

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
    SimpleCLI cli;
    Command ping;
    Command dir;

    String input;
    bool showPrompt = true;

    Serial.print("TaskCLI running on core ");
    Serial.println(xPortGetCoreID());

    // Create the commands
    ping = cli.addCmd("ping");
    dir = cli.addCmd("dir");

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

            if (cmd == dir)
            {
                Serial2.println("Dur!");
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
