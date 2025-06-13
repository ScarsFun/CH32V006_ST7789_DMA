/********************************** (C) COPYRIGHT *******************************
 * File Name          : main.c
 * Author             : WCH
 * Version            : V1.0.0
 * Date               : 2024/01/01
 * Description        : Main program body.
 *********************************************************************************
 * Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
 * Attention: This software (modified or not) and binary are used for 
 * microcontroller manufactured by Nanjing Qinheng Microelectronics.
 *******************************************************************************/


#include "debug.h"
#include "WCHst7789.h"
#include "testimg.h"


/* Global define */


/* Global Variable */
uint8_t r = 0, randomChar, nchars;


void demoTFT(void)
{
    while (1){
    ST7789_SetRotation(r);

    ST7789_FillScreen(ST7789_BLACK);

    ST7789_FillScreen(ST7789_BLACK);

    for (int i = 0; i < ST7789_GetHeight(); i += 4) {
        ST7789_DrawFastHLine(0, i, ST7789_GetWidth() - 1, ST7789_WHITE);
    }

    for (int i = 0; i < ST7789_GetWidth(); i += 4) {
        ST7789_DrawFastVLine(i, 0, ST7789_GetHeight() - 1, ST7789_WHITE);
    }

   

    // Check colors
    ST7789_FillScreen(ST7789_BLACK);
    ST7789_DrawText("BLACK", FONT5, 5, 1, ST7789_WHITE, ST7789_BLACK);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_Color565(0, 0, 255));
    ST7789_DrawText("BLUE", FONT5, 5, 1, ST7789_WHITE, ST7789_BLUE);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_Color565(255, 0, 0));
    ST7789_DrawText("RED", FONT5, 5, 1, ST7789_WHITE, ST7789_RED);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_Color565(0, 255, 0));
    
    ST7789_DrawText("GREEN", FONT5, 5, 1, ST7789_BLACK, ST7789_GREEN);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_CYAN);
    ST7789_DrawText("CYAN", FONT5, 5, 1, ST7789_BLACK, ST7789_CYAN);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_MAGENTA);
    ST7789_DrawText("MAGENTA", FONT5, 5, 1, ST7789_BLACK, ST7789_MAGENTA);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_YELLOW);
    ST7789_DrawText("YELLOW", FONT5, 5, 1, ST7789_BLACK, ST7789_YELLOW);
    Delay_Ms(500);

    ST7789_FillScreen(ST7789_WHITE);
    ST7789_DrawText("WHITE", FONT5, 5, 1, ST7789_BLACK, ST7789_WHITE);
    Delay_Ms(500);

    // Draw circles
    ST7789_FillScreen(ST7789_BLACK);
    for (int i = 0; i < ST7789_GetHeight() / 2; i += 2) {
        ST7789_DrawCircle(ST7789_GetWidth() / 2, ST7789_GetHeight() / 2, i, ST7789_YELLOW);
    }
    Delay_Ms(1000);

    ST7789_FillScreen(ST7789_BLACK);
    ST7789_FillTriangle(0, 0, ST7789_GetWidth() / 2, ST7789_GetHeight(), ST7789_GetWidth(), 0, ST7789_RED);
    Delay_Ms(1000);

    ST7789_FillScreen(ST7789_BLACK);
    ST7789_DrawImage(0, 0, 128, 128,  (uint16_t *)test_img_128x128);
    Delay_Ms(2000);
    

    ST7789_FillScreen(ST7789_BLACK);
    ST7789_DrawText("Custom font", FONT6, 5, 15, ST7789_WHITE, ST7789_BLACK);
    ST7789_DrawText("ABCDEFGHI", FONT6, 5, 40, ST7789_WHITE, ST7789_BLACK);
    ST7789_DrawText("lmnopqrst", FONT6, 5, 65, ST7789_WHITE, ST7789_BLACK);
    ST7789_DrawText("0123456789", FONT6, 5, 90, ST7789_WHITE, ST7789_BLACK);
    Delay_Ms(2000);
    r++;
    }
}

/*********************************************************************
/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    
    RCC_ClocksTypeDef RCC_ClocksStatus={0};
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_1);
    SystemCoreClockUpdate();
    Delay_Init();
#if (SDI_PRINT == SDI_PR_OPEN)
    SDI_Printf_Enable();
#else
    USART_Printf_Init(115200);
#endif
    printf("SystemClk:%d\r\n",SystemCoreClock);
    printf( "ChipID:%08x\r\n", DBGMCU_GetCHIPID() );
    
    RCC_GetClocksFreq(&RCC_ClocksStatus);
    printf("SYSCLK_Frequency-%d\r\n", RCC_ClocksStatus.SYSCLK_Frequency);
    printf("HCLK_Frequency-%d\r\n", RCC_ClocksStatus.HCLK_Frequency);
    printf("PCLK1_Frequency-%d\r\n", RCC_ClocksStatus.PCLK1_Frequency);
    printf("PCLK2_Frequency-%d\r\n", RCC_ClocksStatus.PCLK2_Frequency);

    ST7789_Init();
    ST7789_Backlight_On();
   
    demoTFT();
    while (1)
    {}
    
}
