#include "gps.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "GPS_MOD";

static char latitude[16] = {0};
static char longitude[16] = {0};
QueueHandle_t position_queue;

void init_uart() {
    ESP_LOGI(TAG, "Configuring UART for GPS");
    const uart_config_t uart_config = {
        .baud_rate = GPS_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_LOGI(TAG, "Baud rate: %d", uart_config.baud_rate);
    if (uart_param_config(GPS_UART_NUM, &uart_config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART parameters");
        return;
    }
    if (uart_set_pin(GPS_UART_NUM, GPS_TX_PIN, GPS_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to configure UART pins");
        return;
    }
    if (uart_driver_install(GPS_UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to install UART driver");
        return;
    }
    ESP_LOGI(TAG, "UART for GPS configured successfully");
}

void set_latitude(const char *lat) {
    strncpy(latitude, lat, sizeof(latitude) - 1);
}

void set_longitude(const char *lon) {
    strncpy(longitude, lon, sizeof(longitude) - 1);
}

const char* get_latitude() {
    return latitude;
}

const char* get_longitude() {
    return longitude;
}

static void log_gps_data(const char *time, const char *latitude, const char *longitude, const char *speed, const char *course, const char *date) {
    ESP_LOGI(TAG, "Time (UTC): %c%c:%c%c:%c%c", time[0], time[1], time[2], time[3], time[4], time[5]);
    ESP_LOGI(TAG, "Latitude: %s", latitude);
    ESP_LOGI(TAG, "Longitude: %s", longitude);
    ESP_LOGI(TAG, "Speed (km/h): %s", speed);
    ESP_LOGI(TAG, "Date: %c%c/%c%c/20%c%c", date[0], date[1], date[2], date[3], date[4], date[5]);
}

void add_position_to_queue(const char *latitude, const char *longitude) {
    char position[32];
    snprintf(position, sizeof(position), "%s,%s", latitude, longitude);

    // If queue does not have any space available, discard the oldest data
    if (uxQueueSpacesAvailable(position_queue) == 0) {
        char discarded_position[32];
        if (xQueueReceive(position_queue, discarded_position, 0) == pdPASS) {
            ESP_LOGW(TAG, "Queue full, discarding oldest position: %s", discarded_position);
        }
    }

    // Add position to queue
    if (xQueueSend(position_queue, position, portMAX_DELAY) != pdPASS) {
        ESP_LOGE(TAG, "Failed to add position to queue");
    }
}

void get_last_positions_from_queue(int num_positions) {
    char position[32];
    int count = 0;

    ESP_LOGI(TAG, "Last %d positions:", num_positions);
    while (count < num_positions && xQueueReceive(position_queue, position, 0) == pdPASS) {
        char *rest_position = position;
        char *lat = strtok_r(rest_position, ",", &rest_position);
        char *lon = strtok_r(NULL, ",", &rest_position);
        if (lat && lon) {
            ESP_LOGI(TAG, "Position %d: Latitude: %s, Longitude: %s", count + 1, lat, lon);
        } else {
            ESP_LOGI(TAG, "Position %d: Invalid data", count + 1);
        }
        count++;
    }

    if (count == 0) {
        ESP_LOGI(TAG, "No positions available in the queue");
    }
}

// Convert coordinates in degrees and minutes to decimal degrees
static float convert_to_decimal_degrees(const char *coord, char direction) {
    // Verifica se a entrada é válida
    if (!coord || strlen(coord) < 4) {
        fprintf(stderr, "Error: Invalid coordinate.\n");
        return 0.0;
    }

    // Determina o tamanho da string para graus e minutos
    size_t coord_length = strlen(coord);

    // Latitude: 2 dígitos para graus; Longitude: 3 dígitos para graus
    size_t degree_length = (coord_length == 9 || coord_length == 10) ? 2 : 3;

    // Inicializa as strings para graus e minutos
    char degrees_str[4] = {0};  // Máximo 3 dígitos + nulo
    char minutes_str[10] = {0}; // Restante para minutos

    // Separa graus e minutos
    strncpy(degrees_str, coord, degree_length);
    strncpy(minutes_str, coord + degree_length, coord_length - degree_length);

    // Converte strings para valores numéricos
    float degrees = atof(degrees_str);
    float minutes = atof(minutes_str);

    // Calcula graus decimais
    float decimal_degrees = degrees + (minutes / 60.0);

    // Aplica o sinal negativo se necessário
    if (direction == 'S' || direction == 'W') {
        decimal_degrees = -decimal_degrees;
    }

    return decimal_degrees;
}


// Analyze NMEA sentence
// Example NMEA: $GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A
void parse_nmea(char *data) {
    char *token;
    char *rest = data;

    if (strncmp(data, "$GPRMC", 6) == 0) {
        // ESP_LOGI(TAG, "Parsing $GPRMC sentence: %s", data);

        token = strtok_r(rest, ",", &rest); // $GPRMC
        token = strtok_r(rest, ",", &rest); // Time
        char *time = token;
        token = strtok_r(rest, ",", &rest); // Status
        char *status = token;

        // if (status) {
        //     ESP_LOGI(TAG, "Status: %s", status);
        // }

        if (status && strcmp(status, "A") == 0) {
            ESP_LOGI(TAG, "GPS Data: Valid");

            char *lat = strtok_r(rest, ",", &rest);
            char *lat_dir = strtok_r(rest, ",", &rest);
            char *lon = strtok_r(rest, ",", &rest);
            char *lon_dir = strtok_r(rest, ",", &rest);
            char *speed = strtok_r(rest, ",", &rest);
            // char *course = strtok_r(rest, ",", &rest);
            char *date = strtok_r(rest, ",", &rest);

            // Convert speed in knots to km/h
            float speed_knots = atof(speed);
            float speed_kmh = speed_knots * 1.852;
            char speed_kmh_str[16];
            snprintf(speed_kmh_str, sizeof(speed_kmh_str), "%.2f", speed_kmh);

            // Convert latitude and longitude to decimal degrees
            float lat_decimal = convert_to_decimal_degrees(lat, lat_dir[0]);
            float lon_decimal = convert_to_decimal_degrees(lon, lon_dir[0]);
            char lat_decimal_str[16];
            char lon_decimal_str[16];
            snprintf(lat_decimal_str, sizeof(lat_decimal_str), "%.5f", lat_decimal);
            snprintf(lon_decimal_str, sizeof(lon_decimal_str), "%.5f", lon_decimal);

            // Update latitude and longitude
            if (lat && lon) {
                set_latitude(lat_decimal_str);
                set_longitude(lon_decimal_str);
                add_position_to_queue(get_latitude(), get_longitude());
            }

            log_gps_data(time, latitude, longitude, speed_kmh_str, NULL, date);

        } else {
            ESP_LOGW(TAG, "GPS Data: Invalid");
            get_last_positions_from_queue(10); // Obtém as últimas 10 Posições
        }
    }
}

void gps_task(void *arg) {
    ESP_LOGI(TAG, "Starting GPS task");
    uint8_t data[BUF_SIZE];
    while (1) {
        int len = uart_read_bytes(GPS_UART_NUM, data, BUF_SIZE - 1, 20 / portTICK_PERIOD_MS);
        if (len > 0) {
            data[len] = '\0';
            parse_nmea((char *)data);
        }
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}