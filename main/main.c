#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include <soc/soc_caps.h>
#include <esp_log.h>

#include "tbk_wifi.h"
#include "tbk_nvs.h"
#include "tbk_cmd.h"

#include <pb_encode.h>
#include <pb_decode.h>
#include "tbk_msg.pb.c"

void init_tbk(){
    initialize_nvs();
    initialize_wifi();

    register_system_common();
    // register_system_sleep();
#if SOC_WIFI_SUPPORTED
    register_wifi();
#else
    ESP_LOGW("main", "WiFi is not supported on this SoC");
#endif
    register_nvs();

    initialize_console();
    start_console();
}

void app_main(void){
    esp_log_level_set("*", LOG_LOCAL_LEVEL);
    init_tbk();

    ESP_LOGI("main", "test nanopb");
    // from nanopb/examples/simple/simple.c
    uint8_t buffer[128];
    size_t message_length;
    bool status;
    {
        TestMsg msg = TestMsg_init_zero;
        pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));

        msg.id = 12321;
        const char *pw = "Hello, TBK!";
        strcpy(msg.str, pw);

        status = pb_encode(&stream, TestMsg_fields, &msg);
        message_length = stream.bytes_written;

        if (!status){
            ESP_LOGE("main", "Encoding failed: %s", PB_GET_ERROR(&stream));
        }
    }

    {
        TestMsg msg = TestMsg_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(buffer, message_length);

        status = pb_decode(&stream, TestMsg_fields, &msg);

        if (!status){
            ESP_LOGE("main", "Decoding failed: %s", PB_GET_ERROR(&stream));
        }

        ESP_LOGI("main", "Decoded: id=%ld, str=%s", msg.id, msg.str);
    }
}