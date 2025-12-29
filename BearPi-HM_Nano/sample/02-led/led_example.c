/*
 * Copyright (c) 2020 Nanjing Xiaoxiongpai Intelligent Technology Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <unistd.h>

#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"

#define LED_GPIO 2

enum LedState {
    LED_ON = 0,
    LED_OFF,
    LED_BLINK,
};

enum LedState g_LedState = LED_BLINK;

static void LedTask(void)
{
    //初始化GPIO_2
    IoTGpioInit(LED_GPIO);
    //设置GPIO_2为输出模式
    IoTGpioSetDir(LED_GPIO, IOT_GPIO_DIR_OUT);

    while (1)
    { 
        switch (g_LedState) {
            case LED_ON:
                //设置GPIO_2输出高电平点亮LED灯
                IoTGpioSetOutputVal(LED_GPIO, 1);
                osDelay(100);
                break;
            case LED_OFF:
                //设置GPIO_2输出低电平熄灭LED灯
                IoTGpioSetOutputVal(LED_GPIO, 0);
                osDelay(100);
                break;
            case LED_BLINK:
                //设置GPIO_2交替输出高低电平，使LED闪烁
                IoTGpioSetOutputVal(LED_GPIO, 0);
                osDelay(100);
                IoTGpioSetOutputVal(LED_GPIO, 1);
                osDelay(100);
                break;
            default:
                osDelay(100);
                break;
        }        
    }
}

static void LedExampleEntry(void)
{
    osThreadAttr_t attr;

    attr.name = "LedTask";
    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 512;
    attr.priority = osPriorityNormal;

    if (osThreadNew((osThreadFunc_t)LedTask, NULL, &attr) == NULL)
    {
        printf("Falied to create LedTask!\n");
    }
}

APP_FEATURE_INIT(LedExampleEntry);