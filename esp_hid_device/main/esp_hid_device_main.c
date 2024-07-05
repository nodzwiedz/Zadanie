/*zadanie Bluetooth ESP32s3*/

#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#if CONFIG_BT_NIMBLE_ENABLED
#include "host/ble_hs.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#else
#include "esp_bt_defs.h"
#if CONFIG_BT_BLE_ENABLED
#include "esp_gap_ble_api.h"
#include "esp_gatts_api.h"
#include "esp_gatt_defs.h"
#endif
#include "esp_bt_main.h"
#include "esp_bt_device.h"
#endif

#include "esp_hidd.h"
#include "esp_hid_gap.h"

static const char *TAG = "HID_DEV_DEMO";

typedef struct
{
    TaskHandle_t task_hdl;
    esp_hidd_dev_t *hid_dev;
    uint8_t protocol_mode;
    uint8_t *buffer;
} local_param_t;

#if CONFIG_BT_BLE_ENABLED || CONFIG_BT_NIMBLE_ENABLED
static local_param_t s_ble_hid_param = {0};

const unsigned char mediaReportMap[] = {
    0x05, 0x0C,        // Usage Page (Consumer)
    0x09, 0x01,        // Usage (Consumer Control)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x02,        //   Usage (Numeric Key Pad)
    0xA1, 0x02,        //   Collection (Logical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x0A,        //     Usage Maximum (0x0A)
    0x15, 0x01,        //     Logical Minimum (1)
    0x25, 0x0A,        //     Logical Maximum (10)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x00,        //     Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x09, 0x86,        //   Usage (Channel)
    0x15, 0xFF,        //   Logical Minimum (-1)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x02,        //   Report Size (2)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x46,        //   Input (Data,Var,Rel,No Wrap,Linear,Preferred State,Null State)
    0x09, 0xE9,        //   Usage (Volume Increment)
    0x09, 0xEA,        //   Usage (Volume Decrement)
    0x15, 0x00,        //   Logical Minimum (0)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x02,        //   Report Count (2)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0xE2,        //   Usage (Mute)
    0x09, 0x30,        //   Usage (Power)
    0x09, 0x83,        //   Usage (Recall Last)
    0x09, 0x81,        //   Usage (Assign Selection)
    0x09, 0xB0,        //   Usage (Play)
    0x09, 0xB1,        //   Usage (Pause)
    0x09, 0xB2,        //   Usage (Record)
    0x09, 0xB3,        //   Usage (Fast Forward)
    0x09, 0xB4,        //   Usage (Rewind)
    0x09, 0xB5,        //   Usage (Scan Next Track)
    0x09, 0xB6,        //   Usage (Scan Previous Track)
    0x09, 0xB7,        //   Usage (Stop)
    0x15, 0x01,        //   Logical Minimum (1)
    0x25, 0x0C,        //   Logical Maximum (12)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x80,        //   Usage (Selection)
    0xA1, 0x02,        //   Collection (Logical)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x03,        //     Usage Maximum (0x03)
    0x15, 0x01,        //     Logical Minimum (1)
    0x25, 0x03,        //     Logical Maximum (3)
    0x75, 0x02,        //     Report Size (2)
    0x81, 0x00,        //     Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};

//Keyboard codes and configuration 
#define CASE(a, b, c)  \
                case a: \
				buffer[0] = b;  \
				buffer[2] = c; \
                break;\

// USB keyboard codes
#define USB_HID_MODIFIER_LEFT_CTRL      0x01
#define USB_HID_MODIFIER_LEFT_SHIFT     0x02
#define USB_HID_MODIFIER_LEFT_ALT       0x04
#define USB_HID_MODIFIER_RIGHT_CTRL     0x10
#define USB_HID_MODIFIER_RIGHT_SHIFT    0x20
#define USB_HID_MODIFIER_RIGHT_ALT      0x40

#define USB_HID_SPACE                   0x2C
#define USB_HID_DOT                     0x37
#define USB_HID_NEWLINE                 0x28
#define USB_HID_FSLASH                  0x38
#define USB_HID_BSLASH                  0x31
#define USB_HID_COMMA                   0x36
#define USB_HID_DOT                     0x37

static int tableWithNumbers[2000];

const unsigned char keyboardReportMap[] = { //7 bytes input (modifiers, resrvd, keys*5), 1 byte output
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x06,        // Usage (Keyboard)
    0xA1, 0x01,        // Collection (Application)
    0x85, 0x01,        //   Report ID (1)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0xE0,        //   Usage Minimum (0xE0)
    0x29, 0xE7,        //   Usage Maximum (0xE7)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x08,        //   Report Count (8)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x08,        //   Report Size (8)
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x01,        //   Report Size (1)
    0x05, 0x08,        //   Usage Page (LEDs)
    0x19, 0x01,        //   Usage Minimum (Num Lock)
    0x29, 0x05,        //   Usage Maximum (Kana)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x03,        //   Report Size (3)
    0x91, 0x03,        //   Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x95, 0x05,        //   Report Count (5)
    0x75, 0x08,        //   Report Size (8)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x65,        //   Logical Maximum (101)
    0x05, 0x07,        //   Usage Page (Kbrd/Keypad)
    0x19, 0x00,        //   Usage Minimum (0x00)
    0x29, 0x65,        //   Usage Maximum (0x65)
    0x81, 0x00,        //   Input (Data,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection

    // 65 bytes
};

static void char_to_code(uint8_t *buffer, char ch)
{
	// Check if lower or upper case
	if(ch >= 'a' && ch <= 'z')
	{
		buffer[0] = 0;
		// convert ch to HID letter, starting at a = 4
		buffer[2] = (uint8_t)(4 + (ch - 'a'));
	}
	else if(ch >= 'A' && ch <= 'Z')
	{
		// Add left shift
		buffer[0] = USB_HID_MODIFIER_LEFT_SHIFT;
		// convert ch to lower case
		ch = ch - ('A'-'a');
		// convert ch to HID letter, starting at a = 4
		buffer[2] = (uint8_t)(4 + (ch - 'a'));
	}
	else if(ch >= '0' && ch <= '9') // Check if number
	{
		buffer[0] = 0;
		// convert ch to HID number, starting at 1 = 30, 0 = 39
		if(ch == '0')
		{
			buffer[2] = 39;
		}
		else
		{
			buffer[2] = (uint8_t)(30 + (ch - '1'));
		}
	}
	else // not a letter nor a number
	{
		switch(ch)
		{
            CASE(' ', 0, USB_HID_SPACE);
			CASE('.', 0,USB_HID_DOT);
            CASE('\n', 0, USB_HID_NEWLINE);
			CASE('?', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_FSLASH);
			CASE('/', 0 ,USB_HID_FSLASH);
			CASE('\\', 0, USB_HID_BSLASH);
			CASE('|', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_BSLASH);
			CASE(',', 0, USB_HID_COMMA);
			CASE('<', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_COMMA);
			CASE('>', USB_HID_MODIFIER_LEFT_SHIFT, USB_HID_COMMA);
			CASE('@', USB_HID_MODIFIER_LEFT_SHIFT, 31);
			CASE('!', USB_HID_MODIFIER_LEFT_SHIFT, 30);
			CASE('#', USB_HID_MODIFIER_LEFT_SHIFT, 32);
			CASE('$', USB_HID_MODIFIER_LEFT_SHIFT, 33);
			CASE('%', USB_HID_MODIFIER_LEFT_SHIFT, 34);
			CASE('^', USB_HID_MODIFIER_LEFT_SHIFT,35);
			CASE('&', USB_HID_MODIFIER_LEFT_SHIFT, 36);
			CASE('*', USB_HID_MODIFIER_LEFT_SHIFT, 37);
			CASE('(', USB_HID_MODIFIER_LEFT_SHIFT, 38);
			CASE(')', USB_HID_MODIFIER_LEFT_SHIFT, 39);
			CASE('-', 0, 0x2D);
			CASE('_', USB_HID_MODIFIER_LEFT_SHIFT, 0x2D);
			CASE('=', 0, 0x2E);
			CASE('+', USB_HID_MODIFIER_LEFT_SHIFT, 39);
			CASE(8, 0, 0x2A); // backspace
			CASE('\t', 0, 0x2B);
			default:
				buffer[0] = 0;
				buffer[2] = 0;
		}
	}
}

void change_random_number_in_table()    //generate random number in random place in table
{
    while(1)
    {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        srand(time(NULL));
        int rIndex = rand() % (9 + 1 - 0) + 0;
        int rNewNumber = rand() % (999 + 1 - 0) + 0;
        tableWithNumbers[rIndex] = rNewNumber;
        printf("Wygenerowalem nowa liczbe o wartosci %d", rNewNumber);
        printf(" i wstawilem ja do tablicy w miejsce %d\n", rIndex);
    }
}


static local_param_t randomNumberTask = {0};

void ble_hid_demo_task_kbd(void *pvParameters)
{
    static const char* help_string = "########################################################################\n"\
                                      "Podaj indeks z tablicy 0-1999, aby sprawdzic co jest w srodku\n"\
                                      "########################################################################\n";
                                    /* TODO : Add support for function keys and ctrl, alt, esc, etc. */
    printf("%s\n", help_string);
    char c;
    int cInt;
    static const char* messageBack = "W tablicy pod podanym indeksem znajduje sie liczba: ";
    static const char* messageIncorrectData = "Nieprawidlowe dane wejsciowe. Wprowadz prawidlowy indeks z zakresu 0-1999";
    while (1) {

        c = fgetc(stdin);
        if(c >= '0' && c <= '9'){
            cInt = c - '0';
            printf("%s %i\n", messageBack, cInt);
        }
        else if (c == '\0'){
            printf("%s\n", messageIncorrectData);
        }
        else{
            ;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static esp_hid_raw_report_map_t ble_report_maps[] = {
#if !CONFIG_BT_NIMBLE_ENABLED || CONFIG_EXAMPLE_HID_DEVICE_ROLE == 1
    /* This block is compiled for bluedroid as well */
    {
        .data = keyboardReportMap,
        .len = sizeof(keyboardReportMap)
    }
#elif CONFIG_EXAMPLE_HID_DEVICE_ROLE && CONFIG_EXAMPLE_HID_DEVICE_ROLE == 2
    {
        .data = keyboardReportMap,
        .len = sizeof(keyboardReportMap)
    },
#elif CONFIG_EXAMPLE_HID_DEVICE_ROLE && CONFIG_EXAMPLE_HID_DEVICE_ROLE == 3
    {
        .data = mouseReportMap,
        .len = sizeof(mouseReportMap)
    },
#endif
};

static esp_hid_device_config_t ble_hid_config = {
    .vendor_id          = 0x16C0,
    .product_id         = 0x05DF,
    .version            = 0x0100,
    .device_name        = "ESP32s3 Zadanie",
    .manufacturer_name  = "Espressif",
    .serial_number      = "1234567890",
    .report_maps        = ble_report_maps,
    .report_maps_len    = 1
};

// HID Consumer Control report bitmasks
#define HID_CC_RPT_NUMERIC_BITS         0xF0
#define HID_CC_RPT_CHANNEL_BITS         0xCF
#define HID_CC_RPT_VOLUME_BITS          0x3F
#define HID_CC_RPT_BUTTON_BITS          0xF0
#define HID_CC_RPT_SELECTION_BITS       0xCF

// Macros for the HID Consumer Control 2-byte report
#define HID_CC_RPT_SET_NUMERIC(s, x)    (s)[0] &= HID_CC_RPT_NUMERIC_BITS;   (s)[0] = (x)
#define HID_CC_RPT_SET_CHANNEL(s, x)    (s)[0] &= HID_CC_RPT_CHANNEL_BITS;   (s)[0] |= ((x) & 0x03) << 4
#define HID_CC_RPT_SET_VOLUME_UP(s)     (s)[0] &= HID_CC_RPT_VOLUME_BITS;    (s)[0] |= 0x40
#define HID_CC_RPT_SET_VOLUME_DOWN(s)   (s)[0] &= HID_CC_RPT_VOLUME_BITS;    (s)[0] |= 0x80
#define HID_CC_RPT_SET_BUTTON(s, x)     (s)[1] &= HID_CC_RPT_BUTTON_BITS;    (s)[1] |= (x)
#define HID_CC_RPT_SET_SELECTION(s, x)  (s)[1] &= HID_CC_RPT_SELECTION_BITS; (s)[1] |= ((x) & 0x03) << 4

#define HID_RPT_ID_CC_IN        3   // Consumer Control input report ID
#define HID_CC_IN_RPT_LEN       2   // Consumer Control input report Len

void ble_hid_task_start_up(void)
{
    if (s_ble_hid_param.task_hdl) {
        // Task already exists
        return;
    }
    /* Executed for bluedroid */   
    xTaskCreate(ble_hid_demo_task_kbd, "ble_hid_demo_task_kbd", 3 * 1024, NULL, configMAX_PRIORITIES - 3,
                &s_ble_hid_param.task_hdl);

    xTaskCreate(change_random_number_in_table, "change_random_number",3*1024,NULL, configMAX_PRIORITIES - 3,
                &randomNumberTask.task_hdl);

}

void ble_hid_task_shut_down(void)
{
    if (s_ble_hid_param.task_hdl) {
        vTaskDelete(s_ble_hid_param.task_hdl);
        s_ble_hid_param.task_hdl = NULL;
    }
    if (randomNumberTask.task_hdl) {
        vTaskDelete(randomNumberTask.task_hdl);
        randomNumberTask.task_hdl = NULL;
    }
}

static void ble_hidd_event_callback(void *handler_args, esp_event_base_t base, int32_t id, void *event_data)
{
    esp_hidd_event_t event = (esp_hidd_event_t)id;
    esp_hidd_event_data_t *param = (esp_hidd_event_data_t *)event_data;
    static const char *TAG = "HID_DEV_BLE";

    switch (event) {
    case ESP_HIDD_START_EVENT: {
        ESP_LOGI(TAG, "START");
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_CONNECT_EVENT: {
        ESP_LOGI(TAG, "CONNECT");
        break;
    }
    case ESP_HIDD_PROTOCOL_MODE_EVENT: {
        ESP_LOGI(TAG, "PROTOCOL MODE[%u]: %s", param->protocol_mode.map_index, param->protocol_mode.protocol_mode ? "REPORT" : "BOOT");
        break;
    }
    case ESP_HIDD_CONTROL_EVENT: {
        ESP_LOGI(TAG, "CONTROL[%u]: %sSUSPEND", param->control.map_index, param->control.control ? "EXIT_" : "");
        if (param->control.control)
        {
            // exit suspend
            ble_hid_task_start_up();
        } else {
            // suspend
            ble_hid_task_shut_down();
        }
    break;
    }
    case ESP_HIDD_OUTPUT_EVENT: {
        ESP_LOGI(TAG, "OUTPUT[%u]: %8s ID: %2u, Len: %d, Data:", param->output.map_index, esp_hid_usage_str(param->output.usage), param->output.report_id, param->output.length);
        ESP_LOG_BUFFER_HEX(TAG, param->output.data, param->output.length);
        break;
    }
    case ESP_HIDD_FEATURE_EVENT: {
        ESP_LOGI(TAG, "FEATURE[%u]: %8s ID: %2u, Len: %d, Data:", param->feature.map_index, esp_hid_usage_str(param->feature.usage), param->feature.report_id, param->feature.length);
        ESP_LOG_BUFFER_HEX(TAG, param->feature.data, param->feature.length);
        break;
    }
    case ESP_HIDD_DISCONNECT_EVENT: {
        ESP_LOGI(TAG, "DISCONNECT: %s", esp_hid_disconnect_reason_str(esp_hidd_dev_transport_get(param->disconnect.dev), param->disconnect.reason));
        ble_hid_task_shut_down();
        esp_hid_ble_gap_adv_start();
        break;
    }
    case ESP_HIDD_STOP_EVENT: {
        ESP_LOGI(TAG, "STOP");
        break;
    }
    default:
        break;
    }
    return;
}
#endif

void app_main(void)
{
    esp_err_t ret;
#if HID_DEV_MODE == HIDD_IDLE_MODE
    ESP_LOGE(TAG, "Please turn on BT HID device or BLE!");
    return;
#endif
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    ESP_LOGI(TAG, "setting hid gap, mode:%d", HID_DEV_MODE);
    ret = esp_hid_gap_init(HID_DEV_MODE);
    ESP_ERROR_CHECK( ret );

#if CONFIG_BT_BLE_ENABLED
        ret = esp_hid_ble_gap_adv_init(ESP_HID_APPEARANCE_KEYBOARD, ble_hid_config.device_name);
        ESP_ERROR_CHECK( ret );
    #if CONFIG_BT_BLE_ENABLED
        if ((ret = esp_ble_gatts_register_callback(esp_hidd_gatts_event_handler)) != ESP_OK) {
            ESP_LOGE(TAG, "GATTS register callback failed: %d", ret);
            return;
        }
    #endif
        ESP_LOGI(TAG, "setting ble device");
        ESP_ERROR_CHECK(
            esp_hidd_dev_init(&ble_hid_config, ESP_HID_TRANSPORT_BLE, ble_hidd_event_callback, &s_ble_hid_param.hid_dev));
#endif

#if CONFIG_BT_HID_DEVICE_ENABLED
    ESP_LOGI(TAG, "setting device name");
    esp_bt_gap_set_device_name(bt_hid_config.device_name);
    ESP_LOGI(TAG, "setting cod major, peripheral");
    esp_bt_cod_t cod;
    cod.major = ESP_BT_COD_MAJOR_DEV_PERIPHERAL;
    esp_bt_gap_set_cod(cod, ESP_BT_SET_COD_MAJOR_MINOR);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "setting bt device");
    ESP_ERROR_CHECK(
        esp_hidd_dev_init(&bt_hid_config, ESP_HID_TRANSPORT_BT, bt_hidd_event_callback, &s_bt_hid_param.hid_dev));
#endif
}
