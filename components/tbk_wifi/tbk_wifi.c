/*
 * SPDX-FileCopyrightText: 2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */
/* Console example — WiFi commands

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"
#include "tbk_wifi.h"

#define JOIN_TIMEOUT_MS (10000)

static EventGroupHandle_t wifi_event_group;
const int CONNECTED_BIT = BIT0;

static void event_handler(void* arg, esp_event_base_t event_base,
                                int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        esp_wifi_connect();
        xEventGroupClearBits(wifi_event_group, CONNECTED_BIT);
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        xEventGroupSetBits(wifi_event_group, CONNECTED_BIT);
    }
}

static struct wifi_credentials {
    char ssid[16];
    char pass[16];
} wifi_credentials;

static bool store_wifi_credentials(const char *ssid, const char *pass){
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("tbk", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(__func__, "Failed to open NVS (%s)", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (err != ESP_OK) {
        ESP_LOGE(__func__, "Failed to set SSID in NVS (%s)", esp_err_to_name(err));
        return false;
    }
    err = nvs_set_str(nvs_handle, "wifi_pswd", pass);
    if (err != ESP_OK) {
        ESP_LOGE(__func__, "Failed to set password in NVS (%s)", esp_err_to_name(err));
        return false;
    }
    err = nvs_commit(nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(__func__, "Failed to commit NVS (%s)", esp_err_to_name(err));
        return false;
    }
    return true;
}
static bool read_wifi_credentials(char *ssid, char *pass){
    nvs_handle_t nvs_handle;
    size_t required_size;
    esp_err_t err = nvs_open("tbk", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGW(__func__, "Failed to open NVS (%s)", esp_err_to_name(err));
        return false;
    }
    if ((err = nvs_get_str(nvs_handle, "wifi_ssid", NULL, &required_size)) == ESP_OK) {
        if (required_size > 16) {
            ESP_LOGE(__func__, "SSID is too long");
            return false;
        }
        if ( (err = nvs_get_str(nvs_handle, "wifi_ssid", ssid, &required_size)) != ESP_OK) {
            ESP_LOGE(__func__, "Failed to read SSID from NVS (%s)", esp_err_to_name(err));
            return false;
        }
    }
    if ((err = nvs_get_str(nvs_handle, "wifi_pswd", NULL, &required_size)) == ESP_OK) {
        if (required_size > 16) {
            ESP_LOGE(__func__, "Password is too long");
            return false;
        }
        if ( (err = nvs_get_str(nvs_handle, "wifi_pswd", pass, &required_size)) != ESP_OK) {
            ESP_LOGE(__func__, "Failed to read password from NVS (%s)", esp_err_to_name(err));
            return false;
        }
    }
    return true;
}
static bool wifi_join(const char *ssid, const char *pass, int timeout_ms);

void initialize_wifi(void){
    esp_log_level_set("wifi", ESP_LOG_WARN);
    static bool initialized = false;
    if (initialized) {
        return;
    }
    ESP_ERROR_CHECK(esp_netif_init());
    wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    assert(ap_netif);
    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    assert(sta_netif);
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_NULL) );
    ESP_ERROR_CHECK( esp_wifi_start() );

    initialized = true;

    wifi_credentials.ssid[0] = '\0';
    wifi_credentials.pass[0] = '\0';
    if(read_wifi_credentials(wifi_credentials.ssid, wifi_credentials.pass)){
        ESP_LOGI(__func__, "Read SSID: %s", wifi_credentials.ssid);
        wifi_join(wifi_credentials.ssid, wifi_credentials.pass, JOIN_TIMEOUT_MS);
    }
}

static bool wifi_join(const char *ssid, const char *pass, int timeout_ms){
    initialize_wifi();

    ESP_LOGI(__func__, "Connecting to '%s'", ssid);

    wifi_config_t wifi_config = { 0 };
    strlcpy((char *) wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (pass) {
        strlcpy((char *) wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    ESP_ERROR_CHECK( esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK( esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    esp_wifi_connect();

    int bits = xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                                   pdFALSE, pdTRUE, timeout_ms / portTICK_PERIOD_MS);
    bool connected = (bits & CONNECTED_BIT) != 0;

    if (!connected) {
        ESP_LOGW(__func__, "Connection timed out [%s](%s)", ssid, pass);
    }else{
        ESP_LOGI(__func__, "Connected to '%s'", ssid);
    }
    return connected;
}

/** Arguments used by 'join' function */
static struct {
    struct arg_int *timeout;
    struct arg_str *ssid;
    struct arg_str *password;
    struct arg_end *end;
} join_args;

static int connect(int argc, char **argv){
    int nerrors = arg_parse(argc, argv, (void **) &join_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, join_args.end, argv[0]);
        return 1;
    }

    /* set default value*/
    if (join_args.timeout->count == 0) {
        join_args.timeout->ival[0] = JOIN_TIMEOUT_MS;
    }

    bool connected = wifi_join(join_args.ssid->sval[0],
                               join_args.password->sval[0],
                               join_args.timeout->ival[0]);
    if (!connected) {
        return 1;
    }
    store_wifi_credentials(join_args.ssid->sval[0], join_args.password->sval[0]);
    return 0;
}

void register_wifi(void){
    join_args.timeout = arg_int0(NULL, "timeout", "<t>", "Connection timeout, ms");
    join_args.ssid = arg_str1(NULL, NULL, "<ssid>", "SSID of AP");
    join_args.password = arg_str0(NULL, NULL, "<pass>", "PSK of AP");
    join_args.end = arg_end(2);

    const esp_console_cmd_t join_cmd = {
        .command = "join",
        .help = "Join WiFi AP as a station",
        .hint = NULL,
        .func = &connect,
        .argtable = &join_args
    };

    ESP_ERROR_CHECK( esp_console_cmd_register(&join_cmd) );
}
