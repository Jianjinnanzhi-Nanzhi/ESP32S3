/**
 ****************************************************************************************************
 * @file        demo.c
 * @author      正点原子团队(ALIENTEK)
 * @version     V1.0
 * @date        2022-06-21
 * @brief       ATK-MC5640模块测试实验（JPEG）
 * @license     Copyright (c) 2020-2032, 广州市星翼电子科技有限公司
 ****************************************************************************************************
 * @attention
 *
 * 实验平台:正点原子 探索者 F407开发板
 * 在线视频:www.yuanzige.com
 * 技术论坛:www.openedv.com
 * 公司网址:www.alientek.com
 * 购买地址:openedv.taobao.com
 *
 ****************************************************************************************************
 */

#include "demo.h"
#include "./BSP/ATK_MC5640/atk_mc5640.h"
#include "./SYSTEM/usart/usart.h"
#include "./SYSTEM/delay/delay.h"
#include "./BSP/lcd/lcd.h"
#include "./BSP/led/led.h"
#include <string.h>

/* 定义与上位机通讯的串口波特率 */
#define DEMO_UART_BAUDRATE  921600

/* 定义输出JPEG图像尺寸 */
#define DEMO_JPEG_OUTPUT_WIDTH  320
#define DEMO_JPEG_OUTPUT_HEIGHT 240

/* 定义JPEG图像缓存空间大小 */
#define DEMO_JPEG_BUF_SIZE  (35*1024)

/* JPEG图像缓存空间 */
uint32_t g_jpeg_buf[DEMO_JPEG_BUF_SIZE / sizeof(uint32_t)];

/**
 * @brief       初始化与上位机通讯的串口
 * @param       无
 * @retval      无
 */
static void demo_uart_init(void)
{
    UART_HandleTypeDef uart_handle = {0};
    GPIO_InitTypeDef gpio_init_struct = {0};
    
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
    
    gpio_init_struct.Pin        = GPIO_PIN_10 | GPIO_PIN_11;
    gpio_init_struct.Mode       = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull       = GPIO_PULLUP;
    gpio_init_struct.Speed      = GPIO_SPEED_FREQ_HIGH;
    gpio_init_struct.Alternate  = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &gpio_init_struct);
    
    uart_handle.Instance        = USART3;
    uart_handle.Init.BaudRate   = DEMO_UART_BAUDRATE;
    uart_handle.Init.WordLength = UART_WORDLENGTH_8B;
    uart_handle.Init.StopBits   = UART_STOPBITS_1;
    uart_handle.Init.Parity     = UART_PARITY_NONE;
    uart_handle.Init.HwFlowCtl  = UART_HWCONTROL_NONE;
    uart_handle.Init.Mode       = UART_MODE_TX;
    HAL_UART_Init(&uart_handle);
}

/**
 * @brief       例程演示入口函数
 * @param       无
 * @retval      无
 */
void demo_run(void)
{
    uint8_t ret;
    uint8_t *p_jpeg_buf;
    uint32_t jpeg_len;
    uint32_t jpeg_index;
    uint32_t jpeg_start_index;
    uint32_t jpeg_end_index;
    
    /* 初始化与上位机通讯的串口 */
    demo_uart_init();
    
    /* 初始化ATK-MC5640模块 */
    ret  = atk_mc5640_init();
    ret += atk_mc5640_set_output_format(ATK_MC5640_OUTPUT_FORMAT_JPEG);
    ret += atk_mc5640_auto_focus_init();
    ret += atk_mc5640_auto_focus_continuance();
    ret += atk_mc5640_set_light_mode(ATK_MC5640_LIGHT_MODE_ADVANCED_AWB);
    ret += atk_mc5640_set_color_saturation(ATK_MC5640_COLOR_SATURATION_4);
    ret += atk_mc5640_set_brightness(ATK_MC5640_BRIGHTNESS_4);
    ret += atk_mc5640_set_contrast(ATK_MC5640_CONTRAST_4);
    ret += atk_mc5640_set_hue(ATK_MC5640_HUE_6);
    ret += atk_mc5640_set_special_effect(ATK_MC5640_SPECIAL_EFFECT_NORMAL);
    ret += atk_mc5640_set_exposure_level(ATK_MC5640_EXPOSURE_LEVEL_5);
    ret += atk_mc5640_set_sharpness_level(ATK_MC5640_SHARPNESS_OFF);
    ret += atk_mc5640_set_mirror_flip(ATK_MC5640_MIRROR_FLIP_1);
    ret += atk_mc5640_set_test_pattern(ATK_MC5640_TEST_PATTERN_OFF);
    ret += atk_mc5640_set_pre_scaling_window(4, 0);
    ret += atk_mc5640_set_output_size(DEMO_JPEG_OUTPUT_WIDTH, DEMO_JPEG_OUTPUT_HEIGHT);
    if (ret != 0)
    {
        printf("ATK-MC5640 init failed!\r\n");
        while (1)
        {
            LED0_TOGGLE();
            delay_ms(200);
        }
    }
    
    while (1)
    {
        p_jpeg_buf = (uint8_t *)g_jpeg_buf;
        jpeg_len = DEMO_JPEG_BUF_SIZE / (sizeof(uint32_t));
        memset((void *)g_jpeg_buf, 0, DEMO_JPEG_BUF_SIZE);
        
        /* 获取ATK-MC5640模块输出的一帧JPEG图像数据 */
        atk_mc5640_get_frame((uint32_t)g_jpeg_buf, ATK_MC5640_GET_TYPE_DTS_32B_INC, NULL);
        
        /* 获取JPEG图像数据起始位置 */
        for (jpeg_start_index=UINT32_MAX, jpeg_index=0; jpeg_index<DEMO_JPEG_BUF_SIZE - 1; jpeg_index++)
        {
            if ((p_jpeg_buf[jpeg_index] == 0xFF) && (p_jpeg_buf[jpeg_index + 1] == 0xD8))
            {
                jpeg_start_index = jpeg_index;
                break;
            }
        }
        if (jpeg_start_index == UINT32_MAX)
        {
            continue;
        }
        
        /* 获取JPEG图像数据结束位置 */
        for (jpeg_end_index=UINT32_MAX, jpeg_index=jpeg_start_index; jpeg_index<DEMO_JPEG_BUF_SIZE - 1; jpeg_index++)
        {
            if ((p_jpeg_buf[jpeg_index] == 0xFF) && (p_jpeg_buf[jpeg_index + 1] == 0xD9))
            {
                jpeg_end_index = jpeg_index;
                break;
            }
        }
        if (jpeg_end_index == UINT32_MAX)
        {
            continue;
        }
        
        /* 获取JPEG图像数据的长度 */
        jpeg_len = jpeg_end_index - jpeg_start_index + (sizeof(uint32_t) >> 1);
        
        /* 发送JPEG图像数据 */
        for (jpeg_index=jpeg_start_index; jpeg_index<jpeg_len; jpeg_index++)
        {
            USART3->DR = p_jpeg_buf[jpeg_index];
            while ((USART3->SR & 0x40) == 0);
        }
    }
}
