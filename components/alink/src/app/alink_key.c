/* GPIO Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "alink_user_config.h"

static const char *TAG = "alink_key";
#define ESP_INTR_FLAG_DEFAULT 0
static xQueueHandle gpio_evt_queue = NULL;

void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void alink_key_init(uint32_t key_gpio_pin)
{
    gpio_config_t io_conf;
    //interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
    //bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = key_gpio_pin;
    //set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    //enable pull-up mode
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);

    //change gpio intrrupt type for one pin
    gpio_set_intr_type(key_gpio_pin, GPIO_INTR_ANYEDGE);

    //create a queue to handle gpio event from isr
    gpio_evt_queue = xQueueCreate(2, sizeof(uint32_t));

    //install gpio isr service
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
    //hook isr handler for specific gpio pin
    gpio_isr_handler_add(key_gpio_pin, gpio_isr_handler, (void*) key_gpio_pin);
}


alink_err_t alink_key_scan(TickType_t ticks_to_wait)
{
    uint32_t io_num;
    alink_err_t ret;
    BaseType_t press_key = pdFALSE;
    BaseType_t lift_key = pdFALSE;
    for (;;) {
        ret = xQueueReceive(gpio_evt_queue, &io_num, ticks_to_wait);
        ALINK_ERROR_CHECK(ret != pdTRUE, ALINK_ERR, "xQueueReceive, ret:%d", ret);

        if (gpio_get_level(io_num) == 0) {
            lift_key = pdTRUE;
        } else if (lift_key) {
            press_key = pdTRUE;
        }
        if (press_key & lift_key) {
            press_key = pdFALSE;
            lift_key = pdFALSE;
            return ALINK_OK;
        }
    }
}
