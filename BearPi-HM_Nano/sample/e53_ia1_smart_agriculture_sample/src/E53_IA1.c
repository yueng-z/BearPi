/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include "cmsis_os2.h"
#include "E53_IA1.h"
#include "iot_gpio.h"
#include "iot_gpio_ex.h"
#include "iot_errno.h"
#include "iot_i2c.h"
#include "iot_i2c_ex.h"

#define IOT_GPIO_FUNC_GPIO_0_I2C1_SDA  6
#define IOT_GPIO_FUNC_GPIO_1_I2C1_SCL 6
#define IOT_GPIO_FUNC_GPIO_8_GPIO 0
#define IOT_GPIO_FUNC_GPIO_14_GPIO 4
#define IOT_I2C_IDX_1 1


/***************************************************************
* 函数名称: E53_IA1_IO_Init
* 说    明: E53_IA1_GPIO初始化
* 参    数: 无
* 返 回 值: 无
***************************************************************/
static void E53_IA1_IO_Init(void)
{

    IoTGpioInit(8);
    IoTGpioSetFunc(8,IOT_GPIO_FUNC_GPIO_8_GPIO);
    IoTGpioSetDir(8, IOT_GPIO_DIR_OUT);//设置GPIO_8为输出模式
    
    IoTGpioInit(14);
    IoTGpioSetFunc(14,IOT_GPIO_FUNC_GPIO_14_GPIO);
    IoTGpioSetDir(14, IOT_GPIO_DIR_OUT);//设置GPIO_14为输出模式
    
    IoTGpioInit(0);
    IoTGpioSetFunc(0, IOT_GPIO_FUNC_GPIO_0_I2C1_SDA);   // GPIO_0复用为I2C1_SDA
    IoTGpioInit(1);
    IoTGpioSetFunc(1, IOT_GPIO_FUNC_GPIO_1_I2C1_SCL);   // GPIO_1复用为I2C1_SCL
    IoTI2cInit(IOT_I2C_IDX_1, 400000); /* baudrate: 400kbps */

    
}

/***************************************************************
* 函数名称: Init_BH1750
* 说    明: 写命令初始化BH1750
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void Init_BH1750(void)
{
    uint8_t send_data[1] = { 0x01 };
	IoTI2cWrite(IOT_I2C_IDX_1,(BH1750_Addr<<1)|0x00,send_data,1); 
}

/***************************************************************
* 函数名称: Start_BH1750
* 说    明: 启动BH1750
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void Start_BH1750(void)
{
    uint8_t send_data[1] = { 0x10 };
	IoTI2cWrite(IOT_I2C_IDX_1,(BH1750_Addr<<1)|0x00,send_data,1); 
}

/***************************************************************
* 函数名称: SHT30_reset
* 说    明: SHT30复位
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void SHT30_reset(void)
{
    uint8_t send_data[2] = { 0x30,0xA2 };
	IoTI2cWrite(IOT_I2C_IDX_1,(SHT30_Addr<<1)|0x00,send_data,2); 
}

/***************************************************************
* 函数名称: Init_SHT30
* 说    明: 初始化SHT30，设置测量周期
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void Init_SHT30(void)
{
    uint8_t send_data[2] = { 0x22,0x36 };
    IoTI2cWrite(IOT_I2C_IDX_1,(SHT30_Addr<<1)|0x00,send_data,2); 
}

/***************************************************************
* 函数名称: SHT3x_CheckCrc
* 说    明: 检查数据正确性
* 参    数: data：读取到的数据
						nbrOfBytes：需要校验的数量
						checksum：读取到的校对比验值
* 返 回 值: 校验结果，0-成功		1-失败
***************************************************************/
static uint8_t SHT3x_CheckCrc(uint8_t data[], uint8_t nbrOfBytes, uint8_t checksum)
{

    uint8_t crc = 0xFF;
    uint8_t bit = 0;
    uint8_t byteCtr;
    const int16_t POLYNOMIAL = 0x131;
    //calculates 8-Bit checksum with given polynomial
    for (byteCtr = 0; byteCtr < nbrOfBytes; ++byteCtr)
    {
        crc ^= (data[byteCtr]);
        for (bit = 8; bit > 0; --bit)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ POLYNOMIAL;
            else
                crc = (crc << 1);
        }
    }

    if (crc != checksum)
        return 1;
    else
        return 0;
}

/***************************************************************
* 函数名称: SHT3x_CalcTemperatureC
* 说    明: 温度计算
* 参    数: u16sT：读取到的温度原始数据
* 返 回 值: 计算后的温度数据
***************************************************************/
static float SHT3x_CalcTemperatureC(uint16_t u16sT)
{

    float temperatureC = 0;

    u16sT &= ~0x0003;

    temperatureC = (175 * (float)u16sT / 65535 - 45);

    return temperatureC;
}

/***************************************************************
* 函数名称: SHT3x_CalcRH
* 说    明: 湿度计算
* 参    数: u16sRH：读取到的湿度原始数据
* 返 回 值: 计算后的湿度数据
***************************************************************/
static float SHT3x_CalcRH(uint16_t u16sRH)
{

    float humidityRH = 0;

    u16sRH &= ~0x0003;

    humidityRH = (100 * (float)u16sRH / 65535);

    return humidityRH;
}

/***************************************************************
* 函数名称: E53_IA1_Init
* 说    明: 初始化E53_IA1
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void E53_IA1_Init(void)
{
    E53_IA1_IO_Init();
    Init_BH1750();
    Init_SHT30();
}

/***************************************************************
* 函数名称: E53_IA1_Read_Data
* 说    明: 测量光照强度、温度、湿度
* 参    数: 无
* 返 回 值: 无
***************************************************************/
void E53_IA1_Read_Data(E53_IA1_Data_TypeDef *ReadData)
{
    //启动传感器采集数据
    Start_BH1750();

    usleep(180000);
    uint8_t recv_data[2] = { 0 };
 	IoTI2cRead(IOT_I2C_IDX_1, (BH1750_Addr<<1)|0x01,recv_data,2);   // 读取传感器数据
    ReadData->Lux = (float)(((recv_data[0] << 8) + recv_data[1]) / 1.2);

    uint8_t data[3];
    uint16_t dat, tmp;
    uint8_t SHT3X_Data_Buffer[6];

    IotI2cData sht30_i2c_data = { 0 };
    uint8_t send_data[2] = { 0xE0,0x00};
    sht30_i2c_data.sendBuf = send_data;
    sht30_i2c_data.sendLen = 2;
    sht30_i2c_data.receiveBuf = SHT3X_Data_Buffer;
    sht30_i2c_data.receiveLen = 6;
	IoTI2cWriteread(IOT_I2C_IDX_1,(SHT30_Addr<<1)|0x00,&sht30_i2c_data); 	

    data[0] = SHT3X_Data_Buffer[0];
    data[1] = SHT3X_Data_Buffer[1];
    data[2] = SHT3X_Data_Buffer[2];

    tmp = SHT3x_CheckCrc(data, 2, data[2]);
    if (!tmp)
    {
        dat = ((uint16_t)data[0] << 8) | data[1];
        ReadData->Temperature = SHT3x_CalcTemperatureC(dat);
    }

    data[0] = SHT3X_Data_Buffer[3];
    data[1] = SHT3X_Data_Buffer[4];
    data[2] = SHT3X_Data_Buffer[5];

    tmp = SHT3x_CheckCrc(data, 2, data[2]);
    if (!tmp)
    {
        dat = ((uint16_t)data[0] << 8) | data[1];
        ReadData->Humidity = SHT3x_CalcRH(dat);
    }
}

/***************************************************************
* 函数名称: Light_StatusSet
* 说    明: 灯状态设置
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Light_StatusSet(E53_IA1_Status_ENUM status)
{
    if (status == ON)

        //设置GPIO_14输出高电平点亮灯
        IoTGpioSetOutputVal(14, 1);
    if (status == OFF)

        //设置GPIO_14输出低电平关闭灯
        IoTGpioSetOutputVal(14, 0);
}

/***************************************************************
* 函数名称: Motor_StatusSet
* 说    明: 电机状态设置
* 参    数: status,ENUM枚举的数据
*									OFF,关
*									ON,开
* 返 回 值: 无
***************************************************************/
void Motor_StatusSet(E53_IA1_Status_ENUM status)
{
    if (status == ON)

        //设置GPIO_8输出高电平打开电机
        IoTGpioSetOutputVal(8, 1);

    if (status == OFF)

        //设置GPIO_8输出低电平关闭电机
        IoTGpioSetOutputVal(8, 0);
}
