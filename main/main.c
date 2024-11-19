// Gps and Modem - By Matheus Menezes

#include "gps.h"
#include "modem.h"
#include "esp_log.h"

#define ENABLE_GPS
#define ENABLE_MODEM

static const char *TAG_GPS = "GPS_MOD";
static const char *TAG_MODEM = "MODEM_MOD";

void app_main() {
    ESP_LOGI(TAG_GPS, "Starting app_main");

#ifdef ENABLE_GPS
    ESP_LOGI(TAG_GPS, "Initializing UART for GPS");
    init_uart();
    ESP_LOGI(TAG_GPS, "UART for GPS initialized");
    position_queue = xQueueCreate(QUEUE_SIZE, sizeof(char[32]));
    if (position_queue == NULL) {
        ESP_LOGE(TAG_GPS, "Failed to create position queue");
        return;
    }
    if (xTaskCreate(gps_task, "gps_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG_GPS, "Failed to create GPS task");
    }
#endif

#ifdef ENABLE_MODEM
    ESP_LOGI(TAG_MODEM, "Initializing modem");
    init_modem();
    if (xTaskCreate(sms_task, "sms_task", 4096, NULL, 5, NULL) != pdPASS) {
        ESP_LOGE(TAG_MODEM, "Failed to create SMS task");
    }
#endif

    ESP_LOGI(TAG_MODEM, "app_main finished");
}