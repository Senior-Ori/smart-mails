// HTTP Client - FreeRTOS ESP IDF - POST

#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "my_data.h"

/** DEFINES **/
#define WIFI_SUCCESS 1 << 0
#define WIFI_FAILURE 1 << 1
#define TCP_SUCCESS 1 << 0
#define TCP_FAILURE 1 << 1
#define MAX_FAILURES 10

/** DEFINE GIOS**/
#define IR_SENSOR_1 32
#define IR_SENSOR_2 33
#define IR_SENSOR_3 34
#define IR_SENSOR_4 35
#define HT12E_D0 18
#define HT12E_D1 19
#define HT12E_D2 21
#define HT12E_D3 22
#define HT12E_TE 23

/** GLOBALS **/
int ir_sensor_data[4] = {0, 0, 0, 0};
int previous_ir_sensor_data[4] = {0, 0, 0, 0};

// event group to contain status information
static EventGroupHandle_t wifi_event_group;

// retry tracker
static int s_retry_num = 0;

// task tag
static const char *TAG = "WIFI";
/** FUNCTIONS **/
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data);
esp_err_t connect_wifi();
esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt);
static void post_rest_function(char *str);
void setup_gpio();
char *numbers_to_string(int a, int b, int c, int d);
void transmit_data();
char *combine_strings(char *str1, char *str2);
void separate_strings(char *combined_str, char **str1, char **str2);

// static const char *TAG2 = "POST_FUNCTION";

void app_main(void)
{
    setup_gpio();
    esp_err_t status = WIFI_FAILURE;

    // initialize storage
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // connect to wireless AP
    status = connect_wifi();
    if (WIFI_SUCCESS != status)
    {
        ESP_LOGI(TAG, "Failed to associate to AP, dying...");
        return;
    }
    ESP_LOGI("WIFI", "initiated ...........");

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI("transmit_data", "initiated ...........");
    xTaskCreate(transmit_data, "transmit_data", 4096, NULL, 5, NULL);
}

// event handler for wifi events
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        ESP_LOGI(TAG, "Connecting to AP...");
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        if (s_retry_num < MAX_FAILURES)
        {
            ESP_LOGI(TAG, "Reconnecting to AP...");
            esp_wifi_connect();
            s_retry_num++;
        }
        else
        {
            xEventGroupSetBits(wifi_event_group, WIFI_FAILURE);
        }
    }
}

// event handler for ip events
static void ip_event_handler(void *arg, esp_event_base_t event_base,
                             int32_t event_id, void *event_data)
{
    if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "STA IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(wifi_event_group, WIFI_SUCCESS);
    }
}

// connect to wifi and return the result
esp_err_t connect_wifi()
{
    int status = WIFI_FAILURE;

    /** INITIALIZE ALL THE THINGS **/
    // initialize the esp network interface
    ESP_ERROR_CHECK(esp_netif_init());

    // initialize default esp event loop
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // create wifi station in the wifi driver
    esp_netif_create_default_wifi_sta();

    // setup wifi station with the default wifi configuration
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    /** EVENT LOOP CRAZINESS **/
    wifi_event_group = xEventGroupCreate();

    esp_event_handler_instance_t wifi_handler_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        &wifi_handler_event_instance));

    esp_event_handler_instance_t got_ip_event_instance;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &ip_event_handler,
                                                        NULL,
                                                        &got_ip_event_instance));

    /** START THE WIFI DRIVER **/
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = SSID,
            .password = PASS,

            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false},
        },
    };

    // set the wifi controller to be a station
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // set the wifi config
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // start the wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "STA initialization complete");

    /** NOW WE WAIT **/
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
                                           WIFI_SUCCESS | WIFI_FAILURE,
                                           pdFALSE,
                                           pdFALSE,
                                           portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_SUCCESS)
    {
        ESP_LOGI(TAG, "Connected to ap");
        status = WIFI_SUCCESS;
    }
    else if (bits & WIFI_FAILURE)
    {
        ESP_LOGI(TAG, "Failed to connect to ap");
        status = WIFI_FAILURE;
    }
    else
    {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
        status = WIFI_FAILURE;
    }

    /* The event will not be processed after unregister */
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, got_ip_event_instance));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_handler_event_instance));
    vEventGroupDelete(wifi_event_group);

    return status;
}

esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt)
{
    switch (evt->event_id)
    {
    case HTTP_EVENT_ON_DATA:
        printf("HTTP_EVENT_ON_DATA: %.*s\n", evt->data_len, (char *)evt->data);
        break;

    default:
        break;
    }
    return ESP_OK;
}

static void post_rest_function(char *str)
{
    esp_http_client_config_t config_post = {
        .url = "https://ori-projects-default-rtdb.europe-west1.firebasedatabase.app/esp32project.json",
        .event_handler = client_event_post_handler,
        .is_async = true,
        .skip_cert_common_name_check = true,
        .keep_alive_enable = true,
        .max_redirection_count = 0,
        .timeout_ms = 5000};

    esp_http_client_handle_t client = esp_http_client_init(&config_post);
    esp_err_t err;
    char post_data[31];
    sprintf(post_data, "{\"irs\":\"%s\"}", str);
    esp_http_client_set_method(client, HTTP_METHOD_PUT);
    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    // Send the HTTP request

    // Check for errors
    while (1)
    {
        err = esp_http_client_perform(client);
        if (err != ESP_ERR_HTTP_EAGAIN)
        {
            break;
        }
    }
    if (err == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRIu64,
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    }
    else
    {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }

    // Cleanup the HTTP client
    esp_http_client_cleanup(client);
}

void setup_gpio()
{
    // configure the IR sensor pins as inputs
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << IR_SENSOR_1) | (1ULL << IR_SENSOR_2) | (1ULL << IR_SENSOR_3) | (1ULL << IR_SENSOR_4);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // configure the HT12E data pins as outputs
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << HT12E_D0) | (1ULL << HT12E_D1) | (1ULL << HT12E_D2) | (1ULL << HT12E_D3) | (1ULL << HT12E_TE);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
}

char *numbers_to_string(int a, int b, int c, int d)
{
    static char result[4];
    result[0] = a + '0';
    result[1] = b + '0';
    result[2] = c + '0';
    result[3] = d + '0';
    return result;
}

void transmit_data()
{
    while (true)
    {
        // send the data of the IR sensors to the HT12E
        ir_sensor_data[0] = gpio_get_level(IR_SENSOR_1);
        ir_sensor_data[1] = gpio_get_level(IR_SENSOR_2);
        ir_sensor_data[2] = gpio_get_level(IR_SENSOR_3);
        ir_sensor_data[3] = gpio_get_level(IR_SENSOR_4);

        // send the data to the HT12E
        int isChanged = 1;
        for (int j = 0; j < 4; j++)
        {
            if (ir_sensor_data[j] != previous_ir_sensor_data[j])
            {
                isChanged = 0;
                break;
            }
        }
        if (isChanged == 0)
        {
            post_rest_function(numbers_to_string(ir_sensor_data[0], ir_sensor_data[1], ir_sensor_data[2], ir_sensor_data[3]));

            gpio_set_level(HT12E_D0, ir_sensor_data[0]);
            gpio_set_level(HT12E_D1, ir_sensor_data[1]);
            gpio_set_level(HT12E_D2, ir_sensor_data[2]);
            gpio_set_level(HT12E_D3, ir_sensor_data[3]);
            gpio_set_level(HT12E_TE, 1);
            vTaskDelay(100 / portTICK_PERIOD_MS);
            gpio_set_level(HT12E_TE, 0);

            for (int i = 0; i < 4; i++)
                previous_ir_sensor_data[i] = ir_sensor_data[i];
            ESP_LOGI("executed", "transmit_data!");
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

char *combine_strings(char *str1, char *str2)
{
    int len1 = strlen(str1);
    int len2 = strlen(str2);
    int combined_len = len1 + len2 + 3; // +3 for the colons and null terminator
    char *combined_str = (char *)malloc(combined_len * sizeof(char));

    if (combined_str == NULL)
    {
        fprintf(stderr, "Error: memory allocation failed.\n");
        exit(1);
    }

    sprintf(combined_str, "%d:%d:%s%s", len1, len2, str1, str2);
    return combined_str;
}
void separate_strings(char *combined_str, char **str1, char **str2)
{
    int len1, len2, count1 = 0, count2 = 0;
    sscanf(combined_str, "%d:%d:", &len1, &len2);

    *str1 = (char *)malloc((len1 + 1) * sizeof(char));
    *str2 = (char *)malloc((len2 + 1) * sizeof(char));

    if (*str1 == NULL || *str2 == NULL)
    {
        fprintf(stderr, "Error: memory allocation failed.\n");
        exit(1);
    }

    while (combined_str[count1] != ':')
    {
        (count1)++;
    }
    count2 = count1 + 1;
    while (combined_str[count2] != ':')
    {
        (count2)++;
    }
    count2++;

    int i = 0;
    while (i < len1)
    {
        (*str1)[i] = combined_str[i + count2];
        i++;
    }
    (*str1)[i] = '\0';

    i = 0;
    while (i < len2)
    {
        (*str2)[i] = combined_str[i + len1 + count2];
        i++;
    }
    (*str2)[i] = '\0';
}
