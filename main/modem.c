#include "modem.h"
#include "gps.h"
#include <string.h>
#include <stdlib.h>

// Command to put on sleep mode: AT+CSCLK=1 -> Analyze and verify this option

static const char *TAG = "MODEM_MOD";
int mode = MODE_REQUEST;

static bool send_at_command(const char *command, const char *expected_response, int timeout_ms)
{
    uart_write_bytes(MODEM_UART_NUM, command, strlen(command));
    ESP_LOGI(TAG, "Command sent: %s", command);

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, timeout_ms / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Response received: %s", data);
    // Check if the commands returns 'OK'
    if (len > 0)
    {
        data[len] = '\0';
        if (strstr((char *)data, expected_response) != NULL)
        {
            return true;
        }
    }
    return false;
}


static char* send_read_all_at_command(const char *command, const char *expected_response, int timeout_ms)
{
    uart_write_bytes(MODEM_UART_NUM, command, strlen(command));
    ESP_LOGI(TAG, "Command sent: %s", command);

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, timeout_ms / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Response received: %s", data);
    // Check if the commands returns 'OK'
    if (len > 0)
    {
        data[len] = '\0';
        if (strstr((char *)data, expected_response) != NULL)
        {
            char *response = malloc(len + 1);
            if (response != NULL)
            {
                strcpy(response, (char *)data);
                return response;
            }
        }
    }
    return NULL;
}

static void clear_sms_buffer()
{
    if (!send_at_command(AT_CMD_DELETE_ALL_SMS, AT_CMD_OK, 1000))
    {
        ESP_LOGE(TAG, "Failed to clear SMS buffer");
    }
    else
    {
        ESP_LOGI(TAG, "SMS buffer cleared succesfully");
    }
}

static void put_modem_to_sleep()
{
    if (!send_at_command("AT+CSCLK=1\r\n", AT_CMD_OK, 1000))
    {
        ESP_LOGE(TAG, "Failed to put modem to sleep");
    }
    else
    {
        ESP_LOGI(TAG, "Modem is now in sleep mode");
    }
}

static void put_modem_to_sleep_if_needed()
{
    if (mode == MODE_REQUEST)
    {
        put_modem_to_sleep(); // Coloca o modem em modo de suspensão se não houver operações pendentes
    }
}

void init_modem()
{
    ESP_LOGI(TAG, "Configuring UART for modem");
    const uart_config_t uart_config = {
        .baud_rate = MODEM_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
    uart_param_config(MODEM_UART_NUM, &uart_config);
    uart_set_pin(MODEM_UART_NUM, MODEM_TX_PIN, MODEM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(MODEM_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

    if (!send_at_command(AT_CMD, AT_CMD_OK, 1000))
    {
        ESP_LOGE(TAG, "Failed to initialize modem");
        return;
    }

    ESP_LOGI(TAG, "Modem initialized successfully");
    // clear_sms_buffer();
    // put_modem_to_sleep_if_needed(); // Verifica se o modem deve ser colocado em modo de suspensão
}

// SMS example: +CMGR: "REC UNREAD","+5511998765432","","23/11/14,17:30:45+08"
//                      Hello, this is a test message!
// Verify message format

static void parse_sms_response(char *data)
{
    char *line = strtok(data, "\r\n");
    while (line != NULL)
    {
        if (strstr(line, "+CMGL:") != NULL)
        {
            char *sender = strtok(NULL, ",");
            sender = strtok(NULL, ",");
            sender = strtok(NULL, ",");

            char *message = strtok(NULL, "\r\n");
            message = strtok(NULL, "\r\n");

            ESP_LOGI(TAG, "Received SMS from %s: %s", sender, message);

            handle_received_sms(sender, message);
        }
        line = strtok(NULL, "\r\n");
    }
}

static void read_sms_from_modem()
{
    // List all messages
    char *response = send_read_all_at_command(AT_CMD_READ_ALL_SMS, "+CMGL:", 1000);
    if (response != NULL)
    {
        parse_sms_response(response);
        free(response); // Free the allocated memory
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read SMS messages");
    }
    // clear_sms_buffer();
    
}

void check_for_received_sms()
{
    read_sms_from_modem();
    // put_modem_to_sleep_if_needed(); // Verifica se o modem deve ser colocado em modo de suspensão após ler SMS
}

static void continuous_location_task(void *arg)
{
    const char *sender = (const char *)arg;
    while (mode == MODE_CONTINUOUS)
    {
        char response[160];
        snprintf(response, sizeof(response), "GPS Location: https://maps.google.com/?q=%s,%s", get_latitude(), get_longitude());
        send_sms(sender, response);
        add_position_to_queue(get_latitude(), get_longitude());
        vTaskDelay(pdMS_TO_TICKS(60000)); // 1 minute
    }
    vTaskDelete(NULL);
}

void handle_received_sms(const char *sender, const char *message)
{
    if (strcmp(message, REQUEST_POSITION_CMD) == 0)
    {
        if (mode == MODE_REQUEST)
        {
            char response[160];
            snprintf(response, sizeof(response), "GPS Location: https://maps.google.com/?q=%s,%s", get_latitude(), get_longitude());
            send_sms(sender, response);
            ESP_LOGI(TAG, "Location sent to %s", sender);
        }
        else if (mode == MODE_CONTINUOUS)
        {
            xTaskCreate(continuous_location_task, "continuous_location_task", 4096, (void *)sender, 5, NULL);
        }
    }
    else if (strcmp(message, "SET_MODE_REQUEST") == 0)
    {
        mode = MODE_REQUEST;
        // put_modem_to_sleep_if_needed(); // Verifica se o modem deve ser colocado em modo de suspensão ao mudar para modo de requisição
    }
    else if (strcmp(message, "SET_MODE_CONTINUOUS") == 0)
    {
        mode = MODE_CONTINUOUS;
    }
}

void send_sms(const char *phone_number, const char *message)
{


    // Set text
    while (!send_at_command(AT_CMD_SET_TEXT_MODE, AT_CMD_OK, 1000))
    {
        ESP_LOGE(TAG, "Failed to set text mode, retrying...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Retry every 1 second until successful
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), AT_CMD_SEND_SMS, phone_number);
    ESP_LOGI(TAG, "Command send %s: ", cmd);
    if (!send_at_command(cmd, AT_CMD_SEND_SMS_PROMPT, 1000))
    {
        ESP_LOGE(TAG, "Failed to initiate SMS sending");
        return;
    }

    uart_write_bytes(MODEM_UART_NUM, message, strlen(message));
    uart_write_bytes(MODEM_UART_NUM, "\0x1A", 1);

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, 5000 / portTICK_PERIOD_MS);
    if (len > 0)
    {
        data[len] = '\0';
        if (strstr((char *)data, AT_CMD_OK) != NULL)
        { // Verifica se a resposta contém "OK"
            ESP_LOGI(TAG, "SMS sent successfully");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to send SMS");
        }
    }
}

void sms_task(void *arg)
{
    ESP_LOGI(TAG, "Starting SMS task");

    // Set text mode outside the task loop
    while (!send_at_command(AT_CMD_SET_TEXT_MODE, AT_CMD_OK, 1000))
    {
        ESP_LOGE(TAG, "Failed to set text mode, retrying...");
        vTaskDelay(pdMS_TO_TICKS(1000)); // Retry every 1 second until successful
    }

    while (1)
    {
        ESP_LOGI(TAG, "Checking for received SMS");
        check_for_received_sms();
        vTaskDelay(pdMS_TO_TICKS(60000)); // Wait for 1 minute to check if received SMS message
    }
}
