#include "modem.h"
#include "gps.h"
#include <string.h>
#include <stdlib.h>


// Command to put on sleep mod: AT+CSCLK=1 -> Analisar e verificar essa opção


static const char *TAG = "MODEM_MOD";
int mode = MODE_REQUEST;

static bool send_at_command(const char *command, const char *expected_response, int timeout_ms) {
    uart_write_bytes(MODEM_UART_NUM, command, strlen(command));
    vTaskDelay(pdMS_TO_TICKS(timeout_ms));

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, timeout_ms / portTICK_PERIOD_MS);
    if (len > 0) {
        data[len] = '\0';
        if (strstr((char *)data, expected_response) != NULL) {
            return true;
        }
    }
    return false;
}

static void clear_sms_buffer() {
    if (!send_at_command(AT_CMD_DELETE_ALL_SMS, AT_CMD_OK, 1000)) {
        ESP_LOGE(TAG, "Failed to clear SMS buffer");
    }
}

void init_modem() {
    ESP_LOGI(TAG, "Configuring UART for modem");
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

    if (!send_at_command(AT_CMD, AT_CMD_OK, 1000)) {
        ESP_LOGE(TAG, "Failed to initialize modem");
        return;
    }

    ESP_LOGI(TAG, "Modem initialized successfully");
    clear_sms_buffer();
}

// SMS example: +CMGR: "REC UNREAD","+5511998765432","","23/11/14,17:30:45+08"
//                      Hello, this is a test message!
// Verificar formato da mensagem

static void parse_sms_response(char *data) {
    char *line = strtok(data, "\r\n");
    while (line != NULL) {
        if (strstr(line, "+CMGL:") != NULL) {
            char *sender = strtok(NULL, ",");
            sender = strtok(NULL, ",");
            sender = strtok(NULL, "\"");
            sender = strtok(NULL, "\"");

            char *message = strtok(NULL, "\r\n");
            message = strtok(NULL, "\r\n");

            handle_received_sms(sender, message);
        }
        line = strtok(NULL, "\r\n");
    }
}

static void read_sms_from_modem() {
    if (!send_at_command(AT_CMD_SET_TEXT_MODE, AT_CMD_OK, 1000)) {
        ESP_LOGE(TAG, "Failed to set text mode");
        return;
    }

    if (!send_at_command(AT_CMD_READ_ALL_SMS, "+CMGL:", 1000)) {
        ESP_LOGE(TAG, "Failed to read SMS messages");
        return;
    }

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, 1000 / portTICK_PERIOD_MS);
    if (len > 0) {
        data[len] = '\0';
        parse_sms_response((char *)data);
        clear_sms_buffer();
    }
}

void check_for_received_sms() {
    read_sms_from_modem();
}

static void continuous_location_task(void *arg) {
    const char *sender = (const char *)arg;
    while (mode == MODE_CONTINUOUS) {
        char response[160];
        snprintf(response, sizeof(response), "GPS Location: https://maps.google.com/?q=%s,%s", get_latitude(), get_longitude());
        send_sms(sender, response);
        add_position_to_queue(get_latitude(), get_longitude());
        vTaskDelay(pdMS_TO_TICKS(60000)); // 1 minute
    }
    vTaskDelete(NULL);
}

void handle_received_sms(const char *sender, const char *message) {
    if (strcmp(message, REQUEST_POSITION_CMD) == 0) {
        if (mode == MODE_REQUEST) {
            char response[160];
            snprintf(response, sizeof(response), "GPS Location: https://maps.google.com/?q=%s,%s", get_latitude(), get_longitude());
            send_sms(sender, response);
        } else if (mode == MODE_CONTINUOUS) {
            xTaskCreate(continuous_location_task, "continuous_location_task", 4096, (void *)sender, 5, NULL);
        }
    } else if (strcmp(message, "SET_MODE_REQUEST") == 0) {
        mode = MODE_REQUEST;
    } else if (strcmp(message, "SET_MODE_CONTINUOUS") == 0) {
        mode = MODE_CONTINUOUS;
    }
}

void send_sms(const char *phone_number, const char *message) {
    if (!send_at_command(AT_CMD_SET_TEXT_MODE, AT_CMD_OK, 1000)) {
        ESP_LOGE(TAG, "Failed to set text mode");
        return;
    }

    char cmd[32];
    snprintf(cmd, sizeof(cmd), AT_CMD_SEND_SMS, phone_number);
    if (!send_at_command(cmd, AT_CMD_SEND_SMS_PROMPT, 1000)) {
        ESP_LOGE(TAG, "Failed to initiate SMS sending");
        return;
    }

    uart_write_bytes(MODEM_UART_NUM, message, strlen(message));
    uart_write_bytes(MODEM_UART_NUM, "\x1A", 1);
    vTaskDelay(pdMS_TO_TICKS(1000));

    uint8_t data[BUF_SIZE];
    int len = uart_read_bytes(MODEM_UART_NUM, data, BUF_SIZE - 1, 1000 / portTICK_PERIOD_MS);
    if (len > 0) {
        data[len] = '\0';
        if (strstr((char *)data, AT_CMD_OK) != NULL) {
            ESP_LOGI(TAG, "SMS sent successfully");
        } else {
            ESP_LOGE(TAG, "Failed to send SMS");
        }
    }
}

void sms_task(void *arg) {
    ESP_LOGI(TAG, "Starting SMS task");
    while (1) {
        ESP_LOGI(TAG, "Checking for received SMS");
        check_for_received_sms();
        vTaskDelay(pdMS_TO_TICKS(60000));
    }
}