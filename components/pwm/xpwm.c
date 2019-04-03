#include <stdio.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "xpwm.h"

// pwm pin number
const uint32_t pinNum[CHANNLE_PWM_TOTAL] = {PWM_CW_OUT_IO_NUM, PWM_WW_OUT_IO_NUM};
// don't alter it !!! dutys table, (duty/PERIOD)*depth , init
uint32_t setDuties[CHANNLE_PWM_TOTAL] = {50, 50};

int apkCW = 0;
int apkWW = 0;
int apkBrightness = 0;
int apkColortemp = 0;
int apkWorkMode = 0;

bool pwm_init_data()
{

    bool isInitOK = true;
    pwm_init(PWM_PERIOD, setDuties, CHANNLE_PWM_TOTAL, pinNum);

//本地存储取出数据
#if IS_SAVE_PARAMS

    nvs_handle out_handle;

    if (nvs_open(NVS_CONFIG_NAME, NVS_READWRITE, &out_handle) != ESP_OK)
    {
        printf("open innet_conf fail\n");
        return;
    }

    //读取，类似数据读字段对应的值
    esp_err_t err = nvs_get_i8(out_handle, "dutyCW", &apkCW);

    if (err != ESP_OK)
        printf("nvs_i8 dutyCW Get Error \n");

    //读取，类似数据读字段对应的值
    err = nvs_get_i8(out_handle, "dutyWW", &apkWW);

    if (err != ESP_OK)
        printf("nvs_i8 dutyWW Get Error\n");

    //读取，类似数据读字段对应的值
    err = nvs_get_i8(out_handle, "Brightness", &apkBrightness);

    if (err != ESP_OK)
        printf("nvs_i8 dutyWW Get Error\n");

    //读取，类似数据读字段对应的值
    err = nvs_get_i16(out_handle, "Colortemp", &apkColortemp);

    if (err != ESP_OK)
        printf("nvs_i8 dutyWW Get Error\n");

    //读取，类似数据读字段对应的值
    err = nvs_get_i8(out_handle, "WorkMode", &apkWorkMode);

    if (err != ESP_OK)
    {
        printf("nvs_i8 WorkMode Get Error\n");
        isInitOK = false;
    }

    nvs_close(out_handle);

#endif

    return isInitOK;
}

void pwm_set_cw_ww(int dutyCW, int dutyWW, int Brightness, int Colortemp, int WorkMode)
{

    apkCW = dutyCW;
    apkWW = dutyWW;
    apkBrightness = Brightness;
    apkColortemp = Colortemp;
    apkWorkMode = WorkMode;
    pwm_set_duty(CHANNLE_PWM_CW, dutyCW);
    pwm_set_duty(CHANNLE_PWM_WW, dutyWW);
    pwm_start();

//每次设置都要保存数据
#if IS_SAVE_PARAMS
    nvs_handle out_handle;

    printf("save pwm params...\n");

    if (nvs_open(NVS_CONFIG_NAME, NVS_READWRITE, &out_handle) != ESP_OK)
    {
        printf("open innet_conf fail\n");
        return;
    }

    esp_err_t err = nvs_set_i8(out_handle, "dutyCW", dutyCW);
    if (err != ESP_OK)
        printf("nvs_i8 dutyCW Save Error \n");

    err = nvs_set_i8(out_handle, "dutyWW", dutyWW);
    if (err != ESP_OK)
        printf("nvs_i8 dutyWW Save Error\n");

    err = nvs_set_i8(out_handle, "Brightness", Brightness);
    if (err != ESP_OK)
        printf("nvs_i8 Brightness Save Error\n");

    err = nvs_set_i16(out_handle, "Colortemp", Colortemp);
    if (err != ESP_OK)
        printf("nvs_set_i16 Colortemp Save Error\n");

    err = nvs_set_i8(out_handle, "WorkMode", WorkMode);
    if (err != ESP_OK)
        printf("nvs_i8 WorkMode Save Error\n");

    nvs_close(out_handle);

#endif
}

void pwm_set_cw_ww_not_save(int dutyCW, int dutyWW)
{
    pwm_set_duty(CHANNLE_PWM_CW, dutyCW);
    pwm_set_duty(CHANNLE_PWM_WW, dutyWW);
    pwm_start();
}

int pwm_get_cw()
{
    return apkCW;
}

int pwm_get_ww()
{
    return apkWW;
}

int pwm_get_Brightness()
{
    return apkBrightness;
}

int pwm_get_Colortemp()
{
    return apkColortemp;
}

int pwm_get_WorkMode()
{
    return apkWorkMode;
}