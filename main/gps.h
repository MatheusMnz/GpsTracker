#ifndef GPS_H
#define GPS_H

#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

// Configurations
#define GPS_UART_NUM UART_NUM_2
#define GPS_TX_PIN 19
#define GPS_RX_PIN 18
#define GPS_BAUDRATE 9600
#define BUF_SIZE 1024
#define QUEUE_SIZE 300

extern QueueHandle_t position_queue;

void init_uart();
void parse_nmea(char *data);
void gps_task(void *arg);
void add_position_to_queue(const char *latitude, const char *longitude);
void get_last_positions_from_queue(int num_positions);

void set_latitude(const char *lat);
void set_longitude(const char *lon);
const char* get_latitude();
const char* get_longitude();

#endif // GPS_H