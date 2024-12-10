#include "modem.h"
#include "gps.h"
#include <string.h>
#include <stdlib.h>
#include "freertos/semphr.h"

static const char *TAG = "MODEM_MOD";
int mode = MODE_REQUEST;
static SemaphoreHandle_t modem_semaphore;

// Função genérica para enviar comandos AT
static bool send_at_command(const char *command, const char *expected_response, int timeout_ms)
{
    if (xSemaphoreTake(modem_semaphore, pdMS_TO_TICKS(3000)) == pdTRUE) // Take the semaphore with a timeout of 3 seconds
    {
        uart_write_bytes(MODEM_UART_NUM, command, strlen(command));
        ESP_LOGI(TAG, "Command sent: %s", command);

        uint8_t data[BUF_SIZE];
        int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(timeout_ms));
        
        bool result = false;
        if (len > 0)
        {
            data[len] = '\0';
            ESP_LOGI(TAG, "Response received: %s", data);
            if (strstr((char *)data, expected_response) != NULL)
            {
                result = true;
            }
        }
        else
        {
            ESP_LOGW(TAG, "No response received.");
        }

        xSemaphoreGive(modem_semaphore);
        return result;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take semaphore");
        return false;
    }
}

// Função para enviar comando AT e retornar a resposta completa
static char *send_read_all_at_command(const char *command, const char *expected_response, int timeout_ms)
{
    if (xSemaphoreTake(modem_semaphore, pdMS_TO_TICKS(3000)) == pdTRUE) // Take the semaphore with a timeout of 3 seconds
    {
        uart_write_bytes(MODEM_UART_NUM, command, strlen(command));
        ESP_LOGI(TAG, "Command sent: %s", command);

        uint8_t data[BUF_SIZE];
        int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(timeout_ms));

        char *response = NULL;
        if (len > 0)
        {
            data[len] = '\0';
            ESP_LOGI(TAG, "Response received: %s", data);
            if (strstr((char *)data, expected_response) != NULL)
            {
                response = malloc(len + 1);
                if (response != NULL)
                {
                    strcpy(response, (char *)data);
                }
            }
        }
        else
        {
            ESP_LOGW(TAG, "No response received.");
        }

        xSemaphoreGive(modem_semaphore);
        return response;
    }
    else
    {
        ESP_LOGE(TAG, "Failed to take semaphore");
        return NULL;
    }
}

// Inicialização do modem
void init_modem()
{
    modem_semaphore = xSemaphoreCreateMutex();

    const uart_config_t uart_config = {
        .baud_rate = MODEM_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    uart_param_config(MODEM_UART_NUM, &uart_config);
    uart_set_pin(MODEM_UART_NUM, MODEM_TX_PIN, MODEM_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    uart_driver_install(MODEM_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

    while (!send_at_command(AT_CMD, AT_CMD_OK, 2000))
    {
        ESP_LOGE(TAG, "Failed to initialize modem");
    }

    while (!send_at_command("AT+CMGF=1\r\n", AT_CMD_OK, 3000))
    {
        ESP_LOGE(TAG, "Failed to set text mode");
    }

    ESP_LOGI(TAG, "Modem initialized and text mode set.");
}

// Parsing da resposta SMS
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

            handle_received_sms(sender, message);
        }
        line = strtok(NULL, "\r\n");
    }
}

// Ler mensagens SMS do modem
static void read_sms_from_modem()
{
    char *response = send_read_all_at_command(AT_CMD_READ_ALL_SMS, "+CMGL:", 5000);
    if (response != NULL)
    {
        parse_sms_response(response);
        free(response);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to read SMS messages");
    }
}

// Verificar mensagens recebidas
void check_for_received_sms()
{
    read_sms_from_modem();
}

// Função para envio de SMS
void send_sms(const char *phone_number, const char *message)
{
    char cmd[64];
    snprintf(cmd, sizeof(cmd), AT_CMD_SEND_SMS, phone_number);
    if (!send_at_command(cmd, AT_CMD_SEND_SMS_PROMPT, 5000))
    {
        ESP_LOGE(TAG, "Failed to initiate SMS sending");
        return;
    }

    uart_write_bytes(MODEM_UART_NUM, message, strlen(message));
    ESP_LOGI(TAG, "Message sent: %s", message);

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, pdMS_TO_TICKS(30000));

    if (len > 0)
    {
        data[len] = '\0';
        ESP_LOGI(TAG, "Response: %s", data);
        if (strstr((char *)data, AT_CMD_OK) != NULL)
        {
            ESP_LOGI(TAG, "SMS sent successfully");
        } 
        else
        {
            ESP_LOGE(TAG, "Failed to send SMS");
        }
    }
}

// Gerenciar SMS recebidos
void handle_received_sms(const char *sender, const char *message)
{
    if (strcmp(message, REQUEST_POSITION_CMD) == 0)
    {
        char response[160];
        snprintf(response, sizeof(response), "GPS Location: https://maps.google.com/?q=%s,%s", get_latitude(), get_longitude());
        strcat(response, "\x1a\r\n");
        send_sms(sender, response);
    }
    else if (strcmp(message, "SET_MODE_REQUEST") == 0)
    {
        mode = MODE_REQUEST;
    }
    else if (strcmp(message, "SET_MODE_CONTINUOUS") == 0)
    {
        mode = MODE_CONTINUOUS;
    }
}

// Tarefa principal de SMS
void sms_task(void *arg)
{
    while (1)
    {
        check_for_received_sms();
        vTaskDelay(pdMS_TO_TICKS(600)); // Checar SMS a cada 1 minuto
    }
}
