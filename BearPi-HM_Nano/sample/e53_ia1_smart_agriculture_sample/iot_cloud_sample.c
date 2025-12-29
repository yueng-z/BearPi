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
#include <string.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include "ohos_init.h"
#include "cmsis_os2.h"

#include "wifi_connect.h"
#include "huawei_iot.h"
#include "nfc_app.h"

#include <cJSON.h>
#include "E53_IA1.h"


#define CONFIG_WIFI_SSID          "yydz"                                        //修改为自己的WiFi 热点账号
#define CONFIG_WIFI_PWD           "12345678"                                  //修改为自己的WiFi 热点密码

#define CONFIG_APP_SERVERIP       "117.78.5.125"                               //华为IoT平台IP
#define CONFIG_APP_SERVERPORT     "1883"                                        //华为IoT平台端口

#define CONFIG_APP_DEVICEID       "6847e00ed582f200182fe513_senser"         //替换为注册设备后生成的deviceid
#define CONFIG_APP_DEVICEPWD      "26ec91649ea91482611c23adbae949c1"                                   //替换为注册设备后生成的密钥

#define CONFIG_APP_LIFETIME       60     ///< seconds
#define CONFIG_QUEUE_TIMEOUT      (5*1000)


extern int button_task_entry(void);
extern char product[50],wifissid[50], wifipwd[50],deviceid[50], devicepwd[50];
app_cb_t  g_app_cb;

/**************************************************
 * deal_report_msg
 * 处理数据，并将数据上报到平台
 * ***********************************************/
static void deal_report_msg(report_t *report)
{
    oc_mqtt_profile_service_t    service;
    oc_mqtt_profile_kv_t         temperature;
    oc_mqtt_profile_kv_t         humidity;
    oc_mqtt_profile_kv_t         luminance;
    oc_mqtt_profile_kv_t         led;
    oc_mqtt_profile_kv_t         motor;

    if(g_app_cb.connected != 1){
        return;
    }

    /* 打包要发送的数据 */
    service.event_time = NULL;
    service.service_id = "Agriculture";
    service.service_property = &temperature;
    service.nxt = NULL;

    temperature.key = "Temperature";
    temperature.value = &report->temp;
    temperature.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    temperature.nxt = &humidity;

    humidity.key = "Humidity";
    humidity.value = &report->hum;
    humidity.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    humidity.nxt = &luminance;

    luminance.key = "Luminance";
    luminance.value = &report->lum;
    luminance.type = EN_OC_MQTT_PROFILE_VALUE_INT;
    luminance.nxt = &led;

    led.key = "LightStatus";
    led.value = g_app_cb.led?"ON":"OFF";
    led.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    led.nxt = &motor;

    motor.key = "MotorStatus";
    motor.value = g_app_cb.motor?"ON":"OFF";
    motor.type = EN_OC_MQTT_PROFILE_VALUE_STRING;
    motor.nxt = NULL;

    /* 将数据发送到华为IoT平台 */
    oc_mqtt_profile_propertyreport(NULL,&service);
    return;
}


/**************************************************
 * msg_rcv_callback
 * 接收从平台下发的数据
 * ***********************************************/
static int msg_rcv_callback(oc_mqtt_profile_msgrcv_t *msg)
{
    int    ret = 0;
    char  *buf;
    int    buf_len;
    app_msg_t *app_msg;

    if((NULL == msg)|| (msg->request_id == NULL) || (msg->type != EN_OC_MQTT_PROFILE_MSG_TYPE_DOWN_COMMANDS)){
        return ret;
    }

    buf_len = sizeof(app_msg_t) + strlen(msg->request_id) + 1 + msg->msg_len + 1;
    buf = malloc(buf_len);
    if(NULL == buf){
        return ret;
    }
    app_msg = (app_msg_t *)buf;
    buf += sizeof(app_msg_t);

    //将接收到的数据打包到结构体
    app_msg->msg_type = en_msg_cmd;
    app_msg->msg.cmd.request_id = buf;
    buf_len = strlen(msg->request_id);
    buf += buf_len + 1;
    memcpy(app_msg->msg.cmd.request_id, msg->request_id, buf_len);
    app_msg->msg.cmd.request_id[buf_len] = '\0';
    buf_len = msg->msg_len;
    app_msg->msg.cmd.payload = buf;
    memcpy(app_msg->msg.cmd.payload, msg->msg, buf_len);
    app_msg->msg.cmd.payload[buf_len] = '\0';

    //将结构体数据放到队列中
    ret = queue_push(g_app_cb.app_msg,app_msg,10);
    if(ret != 0){
        free(app_msg);
    }

    return ret;
}


/**************************************************
 * deal_cmd_msg
 * 处理平台下发的命令
 * ***********************************************/
static void deal_cmd_msg(cmd_t *cmd)
{
    cJSON *obj_root;
    cJSON *obj_cmdname;
    cJSON *obj_paras;
    cJSON *obj_para;

    int cmdret = 1;
    oc_mqtt_profile_cmdresp_t cmdresp;
    obj_root = cJSON_Parse(cmd->payload);
    if (NULL == obj_root)
    {
        goto EXIT_JSONPARSE;
    }

    obj_cmdname = cJSON_GetObjectItem(obj_root, "command_name");
    if (NULL == obj_cmdname)
    {
        goto EXIT_CMDOBJ;
    }
    if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_light"))
    {
        obj_paras = cJSON_GetObjectItem(obj_root, "paras");
        if (NULL == obj_paras)
        {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Light");
        if (NULL == obj_para)
        {
            goto EXIT_OBJPARA;
        }

        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON"))
        {
            g_app_cb.led = 1;
            
            /* 打开灯 */
            Light_StatusSet(ON);
            printf("Light On!\r\n");
        }
        else
        {
            g_app_cb.led = 0;

            /* 关闭灯 */
            Light_StatusSet(OFF);
            printf("Light Off!\r\n");
        }
        cmdret = 0;
    }
    else if (0 == strcmp(cJSON_GetStringValue(obj_cmdname), "Agriculture_Control_Motor"))
    {
        obj_paras = cJSON_GetObjectItem(obj_root, "Paras");
        if (NULL == obj_paras)
        {
            goto EXIT_OBJPARAS;
        }
        obj_para = cJSON_GetObjectItem(obj_paras, "Motor");
        if (NULL == obj_para)
        {
            goto EXIT_OBJPARA;
        }

        if (0 == strcmp(cJSON_GetStringValue(obj_para), "ON"))
        {
            g_app_cb.motor = 1;

            /* 打开电机 */
            Motor_StatusSet(ON);
            printf("Motor On!\r\n");
        }
        else
        {
            g_app_cb.motor = 0;

            /* 关闭电机 */
            Motor_StatusSet(OFF);
            printf("Motor Off!\r\n");
        }
        cmdret = 0;
    }

EXIT_OBJPARA:
EXIT_OBJPARAS:
EXIT_CMDOBJ:
    cJSON_Delete(obj_root);
EXIT_JSONPARSE:
    ///< do the response
    cmdresp.paras = NULL;
    cmdresp.request_id = cmd->request_id;
    cmdresp.ret_code = cmdret;
    cmdresp.ret_name = NULL;
    (void)oc_mqtt_profile_cmdresp(NULL, &cmdresp);
    return;
}


/**************************************************
 * 任务：iot_cloud_task_entry
 * 端云互通任务
 * ***********************************************/
static int iot_cloud_task_entry(void)
{
    app_msg_t *app_msg;
    uint32_t ret ;

    /* 从NFC中读取连接平台相关设备信息 */
    ret = nfc_get_devived_data();

    /* 连接WiFi热点 */
    if(ret == 0){
    WifiConnect(wifissid, wifipwd);
    }
    else{
    WifiConnect(CONFIG_WIFI_SSID, CONFIG_WIFI_PWD);
    }

    /* 初始化华为云IoT平台 */
    huawei_iot_init();

    /* 打包连接华为云IoT平台所需要的数据 */
    g_app_cb.app_msg = queue_create("queue_rcvmsg",10,1);
    if(NULL ==  g_app_cb.app_msg){
        printf("Create receive msg queue failed");        
    }
    oc_mqtt_profile_connect_t  connect_para;
    (void) memset( &connect_para, 0, sizeof(connect_para));
    connect_para.boostrap =      0;
    if(ret == 0){
    connect_para.device_id =     deviceid;              //平台所需要的deviceid
    connect_para.device_passwd = devicepwd;             //平台所需要的devicepwd
    }
    else{
    connect_para.device_id =     CONFIG_APP_DEVICEID;   //平台所需要的deviceid
    connect_para.device_passwd = CONFIG_APP_DEVICEPWD;  //平台所需要的devicepwd
    }
    connect_para.server_addr =   CONFIG_APP_SERVERIP;   //平台IP
    connect_para.server_port =   CONFIG_APP_SERVERPORT; //平台端口

    connect_para.life_time =     CONFIG_APP_LIFETIME;
    connect_para.rcvfunc =       msg_rcv_callback;
    connect_para.security.type = EN_DTLS_AL_SECURITY_TYPE_NONE;

    /* 连接到华为云IoT平台 */
    printf("Connecting huawei iot cloud!\r\n");
    ret = oc_mqtt_profile_connect(&connect_para);
    if((ret == (int)en_oc_mqtt_err_ok)){
        g_app_cb.connected = 1;
        printf("Huawei iot cloud connect succed!\r\n");
    }
    else{
        printf("Huawei iot cloud connect faild!\r\n");
    }

    /* 循环读取队列中的数据并根据 msg_type 类型调用对接的处理函数 */
    while (1)
    {
        app_msg = NULL;
        (void)queue_pop(g_app_cb.app_msg,(void **)&app_msg,0xFFFFFFFF);
        if(NULL != app_msg){
            switch(app_msg->msg_type){

                case en_msg_cmd:
                    /* 调用命令处理函数，处理平台下发的命令 */
                    deal_cmd_msg(&app_msg->msg.cmd);
                    break;

                case en_msg_report:
                    /* 调用消息上报处理函数，向平台发送数据 */
                    deal_report_msg(&app_msg->msg.report);
                    break;

                default:
                    break;
            }
            free(app_msg);
        }
    }
    return 0;
}


// /**************************************************
//  * 任务：sensor_task_entry
//  * 传感器采集任务
//  * ***********************************************/
// static int sensor_task_entry(void)
// {
//     app_msg_t *app_msg;
//     E53_IA1_Data_TypeDef data;

//     /* 初始化传感器 */
//     E53_IA1_Init(); 
//     while (1)
//     {
        
//         /* 读取传感器数据到结构体变量中 */
//         E53_IA1_Read_Data(&data);
//         app_msg = malloc(sizeof(app_msg_t));

//         g_app_cb.led = 1;
//         Light_StatusSet(ON);
//         printf("SENSOR:lum:%.2f temp:%.2f hum:%.2f\r\n", data.Lux, data.Temperature, data.Humidity);
//         if (NULL != app_msg)
//         {
//             /* 打包数据并将数据发送到队列中 */
//             app_msg->msg_type = en_msg_report;
//             app_msg->msg.report.hum = (int)data.Humidity;
//             app_msg->msg.report.lum = (int)data.Lux;
//             app_msg->msg.report.temp = (int)data.Temperature;
//             if(0 != queue_push(g_app_cb.app_msg,app_msg,CONFIG_QUEUE_TIMEOUT)){
//                 free(app_msg);
//             }
//         }
//         sleep(3);
        
//     }
//     return 0;
// }

// /**************************************************
//  * 任务：sensor_task_entry
//  * 传感器采集任务（带补光功能）
//  * ***********************************************/
// static int sensor_task_entry(void)
// {
//     app_msg_t *app_msg;
//     E53_IA1_Data_TypeDef data;
//     static int last_lum = 0;  // 记录上一次亮度值
  
//     bool motor_index = true;

//     const int LUM_THRESHOLD = 50;  // 亮度阈值，低于此值开启补光
//     const int LUM_HYSTERESIS = 10; // 迟滞范围，防止频繁开关
    
//     /* 初始化传感器 */
//     E53_IA1_Init(); 
    
//     /* 初始化补光灯 */
//     // E53_IA1_Light_Init();  // 假设有这个初始化函数
    
//     while (1)
//     {
//         /* 读取传感器数据到结构体变量中 */
//         E53_IA1_Read_Data(&data);
        
//         /* 亮度控制逻辑 */
//         int current_lum = (int)data.Lux;
        
//         // 如果亮度低于阈值且之前亮度较高（避免在阈值附近频繁切换）
//         if (current_lum < LUM_THRESHOLD && last_lum > (LUM_THRESHOLD - LUM_HYSTERESIS)) {
//             Light_StatusSet(ON);  // 开启补光灯
//             printf("SENSOR: Light turned ON (lum:%.2f)\r\n", data.Lux);
//         } 
//         // 如果亮度回升到阈值+迟滞范围以上且之前亮度较低
//         else if (current_lum > (LUM_THRESHOLD + LUM_HYSTERESIS) && last_lum < (LUM_THRESHOLD + LUM_HYSTERESIS)) {
//             Light_StatusSet(OFF);  // 关闭补光灯
//             printf("SENSOR: Light turned OFF (lum:%.2f)\r\n", data.Lux);
//         }

//         last_lum = current_lum;  // 更新上一次亮度值
//         if(motor_index == true ){
//             Motor_StatusSet(ON);
//         }
//         else{
//             Motor_StatusSet(OFF);
//         }
//         motor_index = !motor_index;
//         /* 上报数据 */
//         app_msg = malloc(sizeof(app_msg_t));
//         printf("SENSOR:lum:%.2f temp:%.2f hum:%.2f\r\n", data.Lux, data.Temperature, data.Humidity);
//         if (NULL != app_msg)
//         {
//             /* 打包数据并将数据发送到队列中 */
//             app_msg->msg_type = en_msg_report;
//             app_msg->msg.report.hum = (int)data.Humidity;
//             app_msg->msg.report.lum = (int)data.Lux;
//             app_msg->msg.report.temp = (int)data.Temperature;
//             if(0 != queue_push(g_app_cb.app_msg,app_msg,CONFIG_QUEUE_TIMEOUT)){
//                 free(app_msg);
//             }
//         }
//         sleep(3);
//     }
//     return 0;
// }


/**************************************************
 * 任务：sensor_task_entry
 * 传感器采集任务（带补光、电机功能）
 * ***********************************************/
static int sensor_task_entry(void)
{
    app_msg_t *app_msg;
    E53_IA1_Data_TypeDef data;
    static int last_lum = 0;  // 记录上一次亮度值
  
    bool motor_index = true;

    const int LUM_THRESHOLD = 50;  // 亮度阈值，低于此值开启补光
    const int LUM_HYSTERESIS = 10; // 迟滞范围，防止频繁开关
    
    /* 初始化传感器 */
    E53_IA1_Init(); 
    
    /* 初始化补光灯 */
    // E53_IA1_Light_Init();  // 假设有这个初始化函数
    
    while (1)
    {
        /* 读取传感器数据到结构体变量中 */
        E53_IA1_Read_Data(&data);
        
        /* 亮度控制逻辑 */
        int current_lum = (int)data.Lux;
        
        // 如果亮度低于阈值且之前亮度较高（避免在阈值附近频繁切换）
        if (current_lum < LUM_THRESHOLD && last_lum > (LUM_THRESHOLD - LUM_HYSTERESIS)) {
            Light_StatusSet(ON);  // 开启补光灯
            printf("SENSOR: Light turned ON (lum:%.2f)\r\n", data.Lux);
        } 
        // 如果亮度回升到阈值+迟滞范围以上且之前亮度较低
        else if (current_lum > (LUM_THRESHOLD + LUM_HYSTERESIS) && last_lum < (LUM_THRESHOLD + LUM_HYSTERESIS)) {
            Light_StatusSet(OFF);  // 关闭补光灯
            printf("SENSOR: Light turned OFF (lum:%.2f)\r\n", data.Lux);
        }

        last_lum = current_lum;  // 更新上一次亮度值

        // if(motor_index == true ){//响一下停一下
        //     Motor_StatusSet(ON);
        // }
        // else{
        //     Motor_StatusSet(OFF);
        // }

        motor_index = !motor_index;
        /* 上报数据 */
        app_msg = malloc(sizeof(app_msg_t));
        printf("SENSOR:lum:%.2f temp:%.2f hum:%.2f\r\n", data.Lux, data.Temperature, data.Humidity);
        if (NULL != app_msg)
        {
            /* 打包数据并将数据发送到队列中 */
            app_msg->msg_type = en_msg_report;
            app_msg->msg.report.hum = (int)data.Humidity;
            app_msg->msg.report.lum = (int)data.Lux;
            app_msg->msg.report.temp = (int)data.Temperature;
            if(0 != queue_push(g_app_cb.app_msg,app_msg,CONFIG_QUEUE_TIMEOUT))
            {
                free(app_msg);
            }
            if(data.Temperature>28)
            {
                Motor_StatusSet(ON);
            }
            else if(data.Temperature<=28)
            {
                Motor_StatusSet(OFF);
            }
        }
        sleep(3);
    }
    return 0;
}


/**************************************************
 * 任务：OC_Demo
 * 主任务，创建几个任务，如平台任务、传感器任务
 * ***********************************************/
static void main_entry(void)
{
    osThreadAttr_t attr;

    attr.attr_bits = 0U;
    attr.cb_mem = NULL;
    attr.cb_size = 0U;
    attr.stack_mem = NULL;
    attr.stack_size = 2048;

    printf("=======================================\r\n");
    printf("************iot_cloud_sample***********\r\n");
    printf("=======================================\r\n");

    /* 创建传感器数据采集任务 */
    attr.name = "sensor_task_entry";
    attr.priority = 25;
    if (osThreadNew((osThreadFunc_t)sensor_task_entry, NULL, &attr) == NULL)
    {
        printf("Falied to create sensor_task_entry!\n");
    }

    /* 创建本地按键控制任务 */
    attr.name = "button_task_entry";
    attr.priority = 26;
    if (osThreadNew((osThreadFunc_t)button_task_entry, NULL, &attr) == NULL)
    {
        printf("Falied to create button_task_entry!\n");
    }

    /* 创建端云互通任务 */
    attr.name = "report_task_entry";
    attr.stack_size = 1024*20;
    attr.priority = 24;
    if (osThreadNew((osThreadFunc_t)iot_cloud_task_entry, NULL, &attr) == NULL) 
    {
        printf("Falied to create report_task_entry!\n");
    }
}

/* 将main_entry任务加入到harmonyOS系统主任务中 */
APP_FEATURE_INIT(main_entry);