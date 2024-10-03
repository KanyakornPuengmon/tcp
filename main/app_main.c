/* MQTT (over TCP) and LED Control via MQTT Example */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"
#include "driver/gpio.h"  // For GPIO control

// Define constants
#define LED_GPIO 2
#define ESP_INTR_FLAG_DEFAULT 0  // Manually define ESP_INTR_FLAG_DEFAULT

static const char *TAG = "mqtt_led_example";

esp_mqtt_client_handle_t client;  // Global MQTT client handle

static void log_error_if_nonzero(const char *message, int error_code)
{
    if (error_code != 0) {
        ESP_LOGE(TAG, "Last error %s: 0x%x", message, error_code);
    }
}

/*
 * @brief Event handler registered to receive MQTT events
 */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data)
{
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32 "", base, event_id);
    esp_mqtt_event_handle_t event = event_data;
    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        // Subscribe to the topic to control the LED
        esp_mqtt_client_subscribe(client, "KMITL/SIET/65030018/topic/LED", 0);
        ESP_LOGI(TAG, "Subscribed to topic: KMITL/SIET/65030018/topic/LED");
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);

        // Trim the data length and handle the control logic for the LED
        if (strncmp(event->data, "ON", event->data_len) == 0) {
            gpio_set_level(LED_GPIO, 1);  // Turn LED on
            ESP_LOGI(TAG, "LED turned ON");
        } else if (strncmp(event->data, "OFF", event->data_len) == 0) {
            gpio_set_level(LED_GPIO, 0);  // Turn LED off
            ESP_LOGI(TAG, "LED turned OFF");
        } else {
            ESP_LOGW(TAG, "Unknown command received");
        }
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
            log_error_if_nonzero("reported from esp-tls", event->error_handle->esp_tls_last_esp_err);
            log_error_if_nonzero("reported from tls stack", event->error_handle->esp_tls_stack_err);
            log_error_if_nonzero("captured as transport's socket errno",  event->error_handle->esp_transport_sock_errno);
            ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
        }
        break;
    default:
        ESP_LOGI(TAG, "Other event id:%d", event->event_id);
        break;
    }
}

/*
 * @brief Initializes GPIO for the LED
 */
static void led_init(void)
{
    // LED GPIO configuration
    gpio_reset_pin(LED_GPIO);  // Reset and configure the LED pin
    gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);  // Set LED as output
    gpio_set_level(LED_GPIO, 0);  // Initially turn the LED off
}

static void mqtt_app_start(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = CONFIG_BROKER_URL,  // Ensure CONFIG_BROKER_URL is defined in menuconfig
    };

    client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(client);
}

void app_main(void)
{
    ESP_LOGI(TAG, "[APP] Startup..");
    ESP_LOGI(TAG, "[APP] Free memory: %" PRIu32 " bytes", esp_get_free_heap_size());
    ESP_LOGI(TAG, "[APP] IDF version: %s", esp_get_idf_version());

    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(example_connect());

    // Initialize the LED GPIO
    led_init();

    // Start MQTT client
    mqtt_app_start();
}
