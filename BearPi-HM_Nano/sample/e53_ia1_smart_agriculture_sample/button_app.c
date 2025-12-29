
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"

#include "huawei_iot.h"
#include "E53_IA1.h"

enum{
    button_off,     //按键状态：关
    button_on       //按键状态：开
}F1_flag, F2_flag;

osEventFlagsId_t evt_id;
extern app_cb_t  g_app_cb;

/**************************************************
 * 任务：F1_Pressed
 * F1按键回调
 * ***********************************************/
static void F1_Pressed(char *arg)
{
    (void)arg;
    F1_flag = button_on;    
    osEventFlagsSet(evt_id, 0x00000001U);
}

/**************************************************
 * 任务：F2_Pressed
 * F2按键回调
 * ***********************************************/
static void F2_Pressed(char *arg)
{
    (void)arg;
    F2_flag = button_on;
    osEventFlagsSet(evt_id, 0x00000001U); 
}


/**************************************************
 * 任务：button_init
 * 按键硬件初始化
 * ***********************************************/
static void button_init(void)
{
    //初始化F1按键，设置为下降沿触发中断
    IoTGpioSetFunc(11, IOT_GPIO_FUNC_GPIO_11_GPIO);

    IoTGpioSetDir(11, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(11, IOT_GPIO_PULL_UP);

    //初始化F2按键，设置为下降沿触发中断
    IoTGpioSetFunc(12, IOT_GPIO_FUNC_GPIO_12_GPIO);

    IoTGpioSetDir(12, IOT_GPIO_DIR_IN);
    IoTGpioSetPull(12, IOT_GPIO_PULL_UP);

    IoTGpioRegisterIsrFunc(11, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_FALL_LEVEL_LOW, F1_Pressed, NULL);
    IoTGpioRegisterIsrFunc(12, IOT_INT_TYPE_EDGE, IOT_GPIO_EDGE_FALL_LEVEL_LOW, F2_Pressed, NULL); 
}


/**************************************************
 * 任务：button_task_entry
 * 按键处理任务
 * ***********************************************/
int button_task_entry(void)
{    
    uint8_t Motor_Status, Light_Status;

    /* 按键硬件初始化 */
    button_init();

    /* 创建按键事件 */
    evt_id = osEventFlagsNew(NULL);
    if (evt_id == NULL)
    {
        printf("Falied to create EventFlags!\n");
    }

    /* 等待按键事件 */
    while (1)
    {
        osEventFlagsWait(evt_id, 0x00000001U, osFlagsWaitAny, osWaitForever);
        if(F1_flag == button_on)
        {
            osDelay(20);
            if(F1_flag == button_on)
            {
                F1_flag = button_off;
                if(g_app_cb.led == 0)
                {
                    g_app_cb.led = 1;
                    Light_StatusSet(ON);    //开灯
                }
                else
                {
                    g_app_cb.led = 0;
                    Light_StatusSet(OFF);   //关灯
                }
            }            
        }

        if(F2_flag == button_on)
        {
            osDelay(20);
            if(F2_flag == button_on)
            {
                F2_flag = button_off;
                if(g_app_cb.motor == 0)
                {
                    g_app_cb.motor = 1;
                    Motor_StatusSet(ON);    //开电机
                }
                else
                {
                    g_app_cb.motor = 0;
                    Motor_StatusSet(OFF);   //关电机
                }
            }            
        }
    }
    return 0;
}
