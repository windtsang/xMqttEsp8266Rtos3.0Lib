
#ifndef _IOT_PWMUTILS_H_
#define _IOT_PWMUTILS_H_

#include "esp8266/gpio_register.h"
#include "esp8266/pin_mux_register.h"
#include "driver/pwm.h"

//PWM period 100us(10Khz)
#define PWM_PERIOD (100)
//pwm gpio口配置
#define CHANNLE_PWM_TOTAL 2
#define CHANNLE_PWM_CW 0
#define CHANNLE_PWM_WW 1
#define PWM_CW_OUT_IO_NUM 14
#define PWM_WW_OUT_IO_NUM 12

//是否带有记忆功能
#define IS_SAVE_PARAMS true
//other
#define NVS_CONFIG_NAME "PWM_CONFIG"

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
bool pwm_init_data();

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
void pwm_set_cw_ww(int dutyCW, int dutyWW, int BrightnessNow, int ColortempNow, int WorkMode);

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
void pwm_set_cw_ww_not_save(int dutyCW, int dutyWW);

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
int pwm_get_cw();

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
int pwm_get_ww();

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
int pwm_get_Brightness();

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
int pwm_get_Colortemp();

/**
 * @description: 
 * @param {type} 
 * @return: 
 */
int pwm_get_WorkMode();

#endif
