
/*
 * Copyright (c) Hisilicon Technologies Co., Ltd. 2019-2019. All rights reserved.
 * Description: app led demo 
 * Author: Hisilicon
 * Create: 2020-10-26
 */
#include <app_demo_io_gpio.h>
#include <hi_adc.h>
#include <hi_time.h>
#include <hi_wifi_api.h>
#include <global_config.h>
#include <smart_can_driver_log.h>

#define  TRASH_DEMO_TASK_STAK_SIZE    (1024*4)
#define  TRASH_DEMO_TASK_PRIORITY     (25)
#define  ADC_TEST_LENGTH              (20)
#define  VLT_MIN                      (100)

hi_u32      g_trash_demo_task_id =0;
hi_u32      g_num = 0;
hi_bool     g_trash_can_is_open = HI_FALSE;
/*
步骤：
    1、GPIO复用关系
    2、设置GPIO的方向
    3、设置GPIO的输出值
*/
hi_void gpio_control_trash(hi_io_name name, hi_u8 val, hi_gpio_idx idx, hi_gpio_dir dir, hi_gpio_value value)
{
    hi_io_set_func(name, val);
    hi_gpio_set_dir(idx, dir);
    if (HI_GPIO_DIR_OUT == dir) {  
        hi_gpio_set_ouput_val(idx, value);
    }
}

hi_u16  g_adc_buf_trash[ADC_TEST_LENGTH] = { 0 };
float get_adc_value(hi_adc_channel_index channel) 
{
    hi_u32 i = 0;
    hi_u32 ret = 0;
    hi_u16 data = 0;
    hi_u16 vlt = 0;
    float voltage = 0;
    float vlt_max = 0;
    float vlt_min = VLT_MIN;

    for (i = 0; i < ADC_TEST_LENGTH; i++) {
        ret = hi_adc_read(channel, &data, HI_ADC_EQU_MODEL_4, HI_ADC_CUR_BAIS_DEFAULT, 0xF0);
        if (ret != HI_ERR_SUCCESS) {
            SMART_CAN_LOG_INFO("hi_adc_read failed");
        }
        g_adc_buf_trash[i] = data;
    }

    for (i = 0; i < ADC_TEST_LENGTH; i++) {  
        vlt = g_adc_buf_trash[i]; 
        voltage = (float)vlt * 1.8 * 4 / 4096.0;  /* vlt * 1.8 * 4 / 4096.0为将码字转换为电压 */
        vlt_max = (voltage > vlt_max) ? voltage : vlt_max;
        vlt_min = (voltage < vlt_min) ? voltage : vlt_min;
    }

    // printf("GPIO9 ADC4 value is %lf\r\n", vlt_max);
    return vlt_max;
}

/* 人体红外感应 GPIO7 ADC3 */
hi_bool is_someone_there(hi_void)
{
    float value = 0;
    value = get_adc_value(HI_ADC_CHANNEL_3);
   
    if (value > 3.0) {
        return HI_TRUE;
    }
    return HI_FALSE;
}

/* 光敏传感器 GPIO9 ADC4 */
hi_bool is_night_time(hi_void)
{
    float value = 0;
    value = get_adc_value(HI_ADC_CHANNEL_4);
    if (value > 3.0) {
        return HI_TRUE;
    }
    return HI_FALSE;
}

/*红外循迹模块 GPIO11 ADC5*/
hi_bool is_trash_can_full(hi_void) 
{
    float value = 0;
    value = get_adc_value(HI_ADC_CHANNEL_5);
    if (value > 3.0) {
        return HI_TRUE;
    }
    return HI_FALSE;
}

// 设置舵机转动的角度
hi_void set_engine_angle(hi_s32 duty)
{
    // hi_gpio_set_ouput_val(HI_GPIO_IDX_6,HI_GPIO_VALUE1);
    // hi_udelay(duty);
    // hi_gpio_set_ouput_val(HI_GPIO_IDX_6,HI_GPIO_VALUE0);
    // hi_udelay(15000-duty); 
    hi_u32 count = duty/10;

    for(int i=1; i <count+1; i++) {
        hi_gpio_set_ouput_val(HI_GPIO_IDX_6,HI_GPIO_VALUE1);
        hi_udelay(i*10);  
        hi_gpio_set_ouput_val(HI_GPIO_IDX_6,HI_GPIO_VALUE0);
        hi_udelay(15000-i*10);  
        hi_sleep(50); 
    }  
}

// 垃圾桶关
hi_void trash_can_close(hi_void)
{
    SMART_CAN_LOG_INFO("trash_can_close");
    set_engine_angle(2000);
    g_trash_can_is_open = HI_FALSE;
}

// 垃圾桶开
hi_void trash_can_open(hi_void)
{   
    SMART_CAN_LOG_INFO("trash_can_open");
    set_engine_angle(1500);
    g_trash_can_is_open = HI_TRUE;
}

// 垃圾箱初始化
hi_void smart_trash_can_init(hi_void)
{
    //舵机初始化 GPIO6
    gpio_control_trash(HI_IO_NAME_GPIO_6,HI_IO_FUNC_GPIO_6_GPIO,HI_GPIO_IDX_6,HI_GPIO_DIR_OUT,HI_GPIO_VALUE0);
    //绿色LED灯初始化 GPIO8
    gpio_control_trash(HI_IO_NAME_GPIO_8,HI_IO_FUNC_GPIO_8_GPIO,HI_GPIO_IDX_8,HI_GPIO_DIR_OUT,HI_GPIO_VALUE0);
}
extern hi_void iot_publish_persion_time(hi_u32 time);
extern hi_void iot_publish_tarsh_is_full(char * string);

hi_void *app_demo_smart_trash_can(hi_void *param)
{
    hi_gpio_init();
    smart_trash_can_init();
    hi_u8 full_flag = 0;
    while (1) {

        if (is_someone_there() == HI_TRUE) { // 有人
            if(is_night_time() == HI_TRUE) { // 晚上
                hi_gpio_set_ouput_val(HI_GPIO_IDX_8, HI_GPIO_VALUE1); // 开灯
            }
            if (g_trash_can_is_open == HI_FALSE){ // 如果垃圾箱盖子是关闭的，打开垃圾箱盖子
                trash_can_open();
                g_num++;
                if (wifi_status == HI_WIFI_EVT_CONNECTED) {
                    iot_publish_persion_time(g_num); //向云端上报此时丢垃圾的人数
                }
            }
        }

        if (is_someone_there() == HI_FALSE) { // 没有人
            if (g_trash_can_is_open == HI_TRUE){ // 如果垃圾箱是打开的，关闭垃圾箱盖子

                trash_can_close();
            }
            hi_gpio_set_ouput_val(HI_GPIO_IDX_8, HI_GPIO_VALUE0);
        }

        if (is_trash_can_full() == HI_TRUE && full_flag == 0) { // 垃圾桶满了，通过上报到云端通过云端下发信息给环卫工人倒垃圾
            SMART_CAN_LOG_INFO("trash can is full");
            if (wifi_status == HI_WIFI_EVT_CONNECTED) {
                iot_publish_tarsh_is_full("true");
            }
            full_flag = 1;
        } else if (is_trash_can_full() == HI_FALSE && full_flag == 1) {
            full_flag = 0;
            if (wifi_status == HI_WIFI_EVT_CONNECTED) {
                iot_publish_tarsh_is_full("false");
            }
        }
        hi_sleep(100);
    }
}

hi_void app_smart_trash_can_task(hi_void)
{
    SMART_CAN_LOG_INFO("app_smart_trash_can_task start!\n");
    hi_u32 ret = 0;
    hi_task_attr attr = {0};

    attr.stack_size = TRASH_DEMO_TASK_STAK_SIZE;
    attr.task_prio  = TRASH_DEMO_TASK_PRIORITY;
    attr.task_name  = (hi_char*)"app_smart_trash_can_task";
    ret = hi_task_create(&g_trash_demo_task_id, &attr, app_demo_smart_trash_can, HI_NULL);
    if (ret != HI_ERR_SUCCESS) {
        SMART_CAN_LOG_ERROR("Failed to create app_demo_smart_trash_can");
    }
}