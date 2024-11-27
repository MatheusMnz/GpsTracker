#ifndef MODEM_H
#define MODEM_H

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Configurations
#define MODEM_UART_NUM UART_NUM_1
#define MODEM_TX_PIN 27
#define MODEM_RX_PIN 26
#define MODEM_BAUDRATE 115200
#define BUF_SIZE 1024

// Commands
#define REQUEST_POSITION_CMD "GET_POSITION"
#define MODE_REQUEST 0
#define MODE_CONTINUOUS 1

// AT Commands
#define AT_CMD "AT\r"
#define AT_CMD_OK "OK"
#define AT_CMD_SET_TEXT_MODE "AT+CMGF=1\r"
#define AT_CMD_READ_ALL_SMS "AT+CMGL=\"ALL\"\r"
#define AT_CMD_DELETE_ALL_SMS "AT+CMGDA=6\r"
#define AT_CMD_SEND_SMS "AT+CMGS=\"%s\"\r"
#define AT_CMD_SEND_SMS_PROMPT ">"

extern int mode;

void init_modem();
void sms_task(void *arg);
void send_sms(const char *phone_number, const char *message);
void check_for_received_sms();
void handle_received_sms(const char *sender, const char *message);

#endif // MODEM_H