// HTTP Client - FreeRTOS ESP IDF - POST

#include <stdio.h>
#include <string.h>
#include <driver/gpio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/event_groups.h"
#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_http_client.h"
#include "esp_smartconfig.h"
#include "lwip/err.h"
#include "lwip/sys.h"
// #include "my_data.h"

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
char *wifi_pass = NULL;
char *wifi_user = NULL;

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
static const int CONNECTED_BIT = BIT0;
static const int ESPTOUCH_DONE_BIT = BIT1;
static const char *TAG = "smartconfig_example";

void setup_gpio();
void transmit_data();
char *combine_strings(char *str1, char *str2);
void separate_strings(char *combined_str, char **str1, char **str2);
char *numbers_to_string(int a, int b, int c, int d);
static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void initialise_wifi(void);
static void smartconfig_example_task(void *parm);
static void post_rest_function(char *str);
esp_err_t client_event_post_handler(esp_http_client_event_handle_t evt);

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED)
    {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI("wifi softAP", "station " MACSTR " join, AID=%d", MAC2STR(event->mac), event->aid);
    }
    else if (event_id == WIFI_EVENT_AP_STADISCONNECTED)
    {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI("wifi softAP", "station " MACSTR " leave, AID=%d", MAC2STR(event->mac), event->aid);
    }
}

void wifi_init_softap(char *wifi_user, char *wifi_pass)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = wifi_user,
            .ssid_len = strlen(wifi_user),
            .channel = EXAMPLE_ESP_WIFI_CHANNEL,
            .password = wifi_pass,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
            .pmf_cfg = {
                .required = false,
            },
        },
    };
    if (strlen(wifi_pass) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI("wifi softAP", "wifi_init_softap finished. SSID:%s password:%s channel:%d",
             wifi_user, wifi_pass, EXAMPLE_ESP_WIFI_CHANNEL);
}

void app_main(void)
{

    ESP_ERROR_CHECK(nvs_flash_init());
    vTaskDelay(1000 / portTICK_PERIOD_MS);

    initialise_wifi();
    ESP_LOGI("WIFI", "initiated ...........");
    setup_gpio();
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI("transmit_data", "initiated ...........");
    xTaskCreate(transmit_data, "transmit_data", 2048, NULL, 4, NULL);
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
    /*
    int main()
    {
        char* str1 = "Ori";
        char* str2 = "Lober";
        char* combined_str = combine_strings(str1, str2);
        printf("Combined string: %s\n", combined_str);

        char* separated_str1 = NULL;
        char* separated_str2 = NULL;
        separate_strings(combined_str, &separated_str1, &separated_str2);
        printf("Separated string 1: %s\n", separated_str1);
        printf("Separated string 2: %s\n", separated_str2);

        // free memory
        free(combined_str);
        free(separated_str1);
        free(separated_str2);

        return 0;
    }
    */
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

static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        xTaskCreate(smartconfig_example_task, "smartconfig_example_task", 4096, NULL, 3, NULL);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        esp_wifi_connect();
        xEventGroupClearBits(s_wifi_event_group, CONNECTED_BIT);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        xEventGroupSetBits(s_wifi_event_group, CONNECTED_BIT);
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SCAN_DONE)
    {
        ESP_LOGI(TAG, "Scan done");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_FOUND_CHANNEL)
    {
        ESP_LOGI(TAG, "Found channel");
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_GOT_SSID_PSWD)
    {
        ESP_LOGI(TAG, "Got SSID and password");

        smartconfig_event_got_ssid_pswd_t *evt = (smartconfig_event_got_ssid_pswd_t *)event_data;
        wifi_config_t wifi_config;
        uint8_t ssid[33] = {0};
        uint8_t password[65] = {0};
        uint8_t rvd_data[33] = {0};

        bzero(&wifi_config, sizeof(wifi_config_t));
        memcpy(wifi_config.sta.ssid, evt->ssid, sizeof(wifi_config.sta.ssid));
        memcpy(wifi_config.sta.password, evt->password, sizeof(wifi_config.sta.password));
        wifi_config.sta.bssid_set = evt->bssid_set;
        if (wifi_config.sta.bssid_set == true)
        {
            memcpy(wifi_config.sta.bssid, evt->bssid, sizeof(wifi_config.sta.bssid));
        }

        memcpy(ssid, evt->ssid, sizeof(evt->ssid));
        memcpy(password, evt->password, sizeof(evt->password));
        ESP_LOGI(TAG, "SSID:%s", ssid);
        ESP_LOGI(TAG, "PASSWORD:%s", password);
        if (evt->type == SC_TYPE_ESPTOUCH_V2)
        {
            ESP_ERROR_CHECK(esp_smartconfig_get_rvd_data(rvd_data, sizeof(rvd_data)));
            ESP_LOGI(TAG, "RVD_DATA:");
            for (int i = 0; i < 33; i++)
            {
                printf("%02x ", rvd_data[i]);
            }
            printf("\n");
        }

        ESP_ERROR_CHECK(esp_wifi_disconnect());
        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
        esp_wifi_connect();
    }
    else if (event_base == SC_EVENT && event_id == SC_EVENT_SEND_ACK_DONE)
    {
        xEventGroupSetBits(s_wifi_event_group, ESPTOUCH_DONE_BIT);
    }
}

static void initialise_wifi(void)
{
    nvs_handle_t my_handle;
    nvs_open("storage", NVS_READWRITE, &my_handle);
    // Read data, key - "data", value - "read_data"
    size_t required_size = 0;
    nvs_get_str(my_handle, "data", NULL, &required_size);
    char *server_name = malloc(required_size);
    nvs_get_str(my_handle, "data", server_name, &required_size);

    separate_strings(server_name, &wifi_pass, &wifi_user);

    ESP_ERROR_CHECK(esp_netif_init());
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(SC_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
}

static void smartconfig_example_task(void *parm)
{
    EventBits_t uxBits;
    ESP_ERROR_CHECK(esp_smartconfig_set_type(SC_TYPE_ESPTOUCH));
    smartconfig_start_config_t cfg = SMARTCONFIG_START_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_smartconfig_start(&cfg));
    while (1)
    {
        uxBits = xEventGroupWaitBits(s_wifi_event_group, CONNECTED_BIT | ESPTOUCH_DONE_BIT, true, false, portMAX_DELAY);
        if (uxBits & CONNECTED_BIT)
        {
            ESP_LOGI(TAG, "WiFi Connected to ap");
        }
        if (uxBits & ESPTOUCH_DONE_BIT)
        {
            ESP_LOGI(TAG, "smartconfig over");
            esp_smartconfig_stop();
            vTaskDelete(NULL);
        }
    }
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
        .max_redirection_count = 0,
        .keep_alive_enable = true,
        .timeout_ms = 5000};

    esp_http_client_handle_t client = esp_http_client_init(&config_post);
    esp_err_t err;
    char post_data[18] = {'\0'};
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

char *numbers_to_string(int a, int b, int c, int d)
{
    static char result[4];
    result[0] = a + '0';
    result[1] = b + '0';
    result[2] = c + '0';
    result[3] = d + '0';
    return result;
}
