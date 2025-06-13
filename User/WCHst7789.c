/*
 * Fast ST7789 DMA Library for CH32V006
 *
 PA4 BL
 PA5 CS
 PA1 DC
 PA0 RST
 PD2  SCK
 PD3  MOSI
 */
#include <ch32v00X.h>
#include <ch32v00X_gpio.h>
#include "WCHst7789_cfg.h"
#include "WCHst7789.h"
#include "stdlib.h"

#define TFT_BL_H() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_BL_Pin, Bit_SET)
#define TFT_BL_L() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_BL_Pin, Bit_RESET)
#define TFT_CS_H() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_CS_Pin, Bit_SET)
#define TFT_CS_L() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_CS_Pin, Bit_RESET)
#define TFT_DC_D() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_DC_Pin, Bit_SET)
#define TFT_DC_C() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_DC_Pin, Bit_RESET)
#define TFT_RES_H() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_RES_Pin, Bit_SET)
#define TFT_RES_L() GPIO_WriteBit(ST7789_CS_GPIO_Port, ST7789_RES_Pin, Bit_RESET)

#define SWAP_INT16_T(a, b) \
    {                      \
        int16_t t = a;     \
        a = b;             \
        b = t;             \
    }
#define DELAY 0x80
uint8_t Y_SHIFT = 0, X_SHIFT = 0;
static uint8_t _value_rotation = 0;

static int16_t _height = ST7789_TFTHEIGHT, _width = ST7789_TFTWIDTH;
volatile uint32_t flag_DMA_CH3_bsy = 0;

static const uint8_t
    cmd_240x240[] = { // Initialization commands for 7789 screens
        9, // 9 commands in list:
        ST7789_SWRESET, DELAY, // 1: Software reset, no args, w/delay
        150, // 150 ms delay
        ST7789_SLPOUT, DELAY, // 2: Out of sleep mode, no args, w/delay
        10, // 255 = 500 ms delay
        ST7789_COLMOD, 1 + DELAY, // 3: Set color mode, 1 arg + delay:
        0x55, // 16-bit color
        10, // 10 ms delay
        ST7789_MADCTL, 1, // 4: Memory access ctrl (directions), 1 arg:
        0x00, // Row addr/col addr, bottom to top refresh
        ST7789_CASET, 4, //  5: Column addr set, 4 args, no delay:
        0x00,
        0, //     XSTART = 0
        0,
        240, //     XEND = 240
        ST7789_RASET, 4, //  6: Row addr set, 4 args, no delay:
        0x00,
        0, //     YSTART = 0
        320 >> 8,
        320 & 0xFF, //     YEND = 320
        ST7789_INVON, DELAY, // 7: Inversion ON
        10,
        ST7789_NORON, DELAY, // 8: Normal display on, no args, w/delay
        10, // 10 ms delay
        ST7789_DISPON, DELAY, // 9: Main screen turn on, no args, w/delay
        255
    }; // 255 = 500 ms delay

//static void ST7789_GPIO_Init(void);
static void ST7789_WriteCommand(uint8_t cmd);
static void ST7789_WriteData(uint8_t* buff, size_t buff_size);
static void ST7789_ExecuteCommandList(const uint8_t* addr);
static void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

static void Send_Data8(uint8_t data);
static void Send_Data16(uint16_t data);
static void Send_DMA_Data16(uint16_t*, uint16_t);

void ST7789_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = ST7789_RES_Pin |  ST7789_DC_Pin | ST7789_CS_Pin | ST7789_BL_Pin;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(ST7789_CS_GPIO_Port, &GPIO_InitStructure);
}

void SPI1_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure = {0};
    SPI_InitTypeDef  SPI_InitStructure = {0};

    RCC_PB2PeriphClockCmd(RCC_PB2Periph_GPIOD | RCC_PB2Periph_SPI1 | RCC_PB2Periph_AFIO, ENABLE);
    GPIO_PinRemapConfig(GPIO_PartialRemap2_SPI1, ENABLE);

    // SCK
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);

    // MOSI
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_30MHz;
    GPIO_Init(GPIOD, &GPIO_InitStructure);


    SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
    SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
    SPI_InitStructure.SPI_DataSize = SPI_DataSize_16b;
    SPI_InitStructure.SPI_CPOL = SPI_CPOL_High;
    SPI_InitStructure.SPI_CPHA = SPI_CPHA_2Edge;
    SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_2;
    SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStructure.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStructure);

    SPI_I2S_DMACmd(SPI1, SPI_I2S_DMAReq_Tx, ENABLE);

    RCC_HBPeriphClockCmd(RCC_HBPeriph_DMA1, ENABLE);
    DMA_DeInit(DMA1_Channel3);

    //Set DMA1 - Channel 3, Very high priority, 16bit size, mem -> spi direction
    //No memory or perpheral increment
    DMA1_Channel3->CFGR |= DMA_CFGR1_PL_0 | DMA_CFGR1_PL_1 | DMA_CFGR1_PSIZE_0 \
                        | DMA_CFGR1_MSIZE_0 | DMA_CFGR1_DIR;                    
    //Take SPI1 Data register address
    DMA1_Channel3->PADDR = (uint32_t)&SPI1->DATAR;
    //Enable DMA interrupt for channel 3
    NVIC_EnableIRQ(DMA1_Channel3_IRQn);

    SPI_Cmd(SPI1, ENABLE);
}

void Send_Data8(uint8_t data)
{
    SPI_DATA_8B();
    while((SPI1->STATR & SPI_STATR_TXE) != SPI_STATR_TXE){};
    SPI1->DATAR = data;
    while((SPI1->STATR & SPI_STATR_BSY) == SPI_STATR_BSY){};
}

void Send_Data16(uint16_t data)
{
    SPI_DATA_16B();
    while((SPI1->STATR & SPI_STATR_TXE) != SPI_STATR_TXE){};
    SPI1->DATAR = data;
    while((SPI1->STATR & SPI_STATR_BSY) == SPI_STATR_BSY){};
}

uint8_t spi_recv8(uint8_t dummy)
{
    SPI1->DATAR = dummy;
    while((SPI1->STATR & SPI_STATR_RXNE) != SPI_STATR_RXNE){};
    
    return (uint8_t)SPI1->DATAR;
}

void Send_DMA_Data16(uint16_t *data, uint16_t size)
{
    //Change data length to 16bit
    SPI_DATA_16B();
    //First disable DMA
    DMA1_Channel3->CFGR &= ~DMA_CFGR3_EN;
    //Buffer address
    DMA1_Channel3->MADDR = (uint32_t)data;
    //Number of data transfer
    DMA1_Channel3->CNTR = (uint16_t)size;
    //Enable DMA Channel
    DMA1_Channel3->CFGR |= DMA_CFGR3_EN;    
    //Wait end of transfer
	while((DMA1->INTFR & DMA_TCIF3) != DMA_TCIF3){};
	//Clear DMA global flag  
	DMA1->INTFCR |= DMA_CGIF3;
}



static void ST7789_Reset()
{
    TFT_RES_L();
    Delay_Us(20);
    TFT_RES_H();
    Delay_Us(150);
}

static void ST7789_WriteCommand(uint8_t cmd)
{
    TFT_DC_C();
    Send_Data8(cmd);
}

static void ST7789_WriteData(uint8_t* buff, size_t buff_size)
{

    TFT_DC_D();

    for (uint16_t index = 0; index < buff_size; index++) {
        Send_Data8(buff[index]);
    }
}

static void ST7789_ExecuteCommandList(const uint8_t* addr)
{
    uint8_t numCommands, numArgs;
    uint16_t ms;

    numCommands = *addr++;
    while (numCommands--) {
        uint8_t cmd = *addr++;
        ST7789_WriteCommand(cmd);

        numArgs = *addr++;
        // If high bit set, delay follows args
        ms = numArgs & DELAY;
        numArgs &= ~DELAY;
        if (numArgs) {
            ST7789_WriteData((uint8_t*)addr, numArgs);
            addr += numArgs;
        }

        if (ms) {
            ms = *addr++;
            if (ms == 255)
                ms = 500;
            Delay_Us(ms);
        }
    }
}

static void ST7789_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{

    uint16_t x_start = x0 + X_SHIFT, x_end = x1 + X_SHIFT;
    uint16_t y_start = y0 + Y_SHIFT, y_end = y1 + Y_SHIFT;

    /* Column Address set */
    ST7789_WriteCommand(ST7789_CASET);
    {
        uint8_t data[] = { x_start >> 8, x_start & 0xFF, x_end >> 8, x_end & 0xFF };
        ST7789_WriteData(data, sizeof(data));
    }

    /* Row Address set */
    ST7789_WriteCommand(ST7789_RASET);
    {
        uint8_t data[] = { y_start >> 8, y_start & 0xFF, y_end >> 8, y_end & 0xFF };
        ST7789_WriteData(data, sizeof(data));
    }
    /* Write to RAM */
    ST7789_WriteCommand(ST7789_RAMWR);
}
void ST7789_Init()
{
    ST7789_GPIO_Init();
    SPI1_Init();

    TFT_CS_L();
    ST7789_Reset();
    ST7789_ExecuteCommandList(cmd_240x240);

    TFT_CS_H();
}

void ST7789_DrawPixel(uint16_t x, uint16_t y, uint16_t color)
{
    if ((x >= _width) || (y >= _height))
        return;

    TFT_CS_L();

    ST7789_SetAddressWindow(x, y, x + 1, y + 1);
    uint8_t data[] = { color >> 8, color & 0xFF };
    ST7789_WriteData(data, sizeof(data));

    TFT_CS_H();
}

void ST7789_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    // clipping

    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        w = _width - x;
    if ((y + h - 1) >= _height)
        h = _height - y;

    TFT_CS_L();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    TFT_DC_D();
    // DMA Memory increment disabled
    DMA1_Channel3->CFGR &= ~DMA_CFGR1_MINC;	
    uint16_t tbuf = color;
    Send_DMA_Data16(&tbuf, w * h);
    // DMA Memory increment enabled	
    DMA1_Channel3->CFGR |= DMA_CFGR1_MINC;
    TFT_CS_H();
}

void ST7789_FillScreen(uint16_t color)
{
    ST7789_FillRectangle(0, 0, _width, _height, color);
}

void ST7789_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t* data)
{
    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        return;
    if ((y + h - 1) >= _height)
        return;

    TFT_CS_L();
    ST7789_SetAddressWindow(x, y, x + w - 1, y + h - 1);
    TFT_DC_D();
    Send_DMA_Data16((uint16_t*)data, w * h);
    TFT_CS_H();
}

void ST7789_InvertColors(bool invert)
{
    TFT_CS_L();
    ST7789_WriteCommand(invert ? ST7789_INVON : ST7789_INVOFF);
    TFT_CS_H();
}

void ST7789_Backlight_On(void)
{
    TFT_BL_H();
}

void ST7789_Backlight_Off(void)
{
    TFT_BL_L();
}

/***************************************************************************************
** Function name:           drawCircle
** Description:             Draw a circle outline
***************************************************************************************/
void ST7789_DrawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -r - r;
    int16_t x = 0;

    ST7789_DrawPixel(x0 + r, y0, color);
    ST7789_DrawPixel(x0 - r, y0, color);
    ST7789_DrawPixel(x0, y0 - r, color);
    ST7789_DrawPixel(x0, y0 + r, color);

    while (x < r) {
        if (f >= 0) {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        ST7789_DrawPixel(x0 + x, y0 + r, color);
        ST7789_DrawPixel(x0 - x, y0 + r, color);
        ST7789_DrawPixel(x0 - x, y0 - r, color);
        ST7789_DrawPixel(x0 + x, y0 - r, color);

        ST7789_DrawPixel(x0 + r, y0 + x, color);
        ST7789_DrawPixel(x0 - r, y0 + x, color);
        ST7789_DrawPixel(x0 - r, y0 - x, color);
        ST7789_DrawPixel(x0 + r, y0 - x, color);
    }
}

/***************************************************************************************
** Function name:           drawCircleHelper
** Description:             Support function for circle drawing
***************************************************************************************/
void ST7789_DrawCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;

    while (x < r) {
        if (f >= 0) {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
        if (cornername & 0x8) {
            ST7789_DrawPixel(x0 - r, y0 + x, color);
            ST7789_DrawPixel(x0 - x, y0 + r, color);
        }
        if (cornername & 0x4) {
            ST7789_DrawPixel(x0 + x, y0 + r, color);
            ST7789_DrawPixel(x0 + r, y0 + x, color);
        }
        if (cornername & 0x2) {
            ST7789_DrawPixel(x0 + r, y0 - x, color);
            ST7789_DrawPixel(x0 + x, y0 - r, color);
        }
        if (cornername & 0x1) {
            ST7789_DrawPixel(x0 - x, y0 - r, color);
            ST7789_DrawPixel(x0 - r, y0 - x, color);
        }
    }
}

/***************************************************************************************
** Function name:           fillCircle
** Description:             draw a filled circle
***************************************************************************************/
void ST7789_FillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color)
{
    ST7789_DrawFastVLine(x0, y0 - r, r + r + 1, color);
    ST7789_FillCircleHelper(x0, y0, r, 3, 0, color);
}

/***************************************************************************************
** Function name:           fillCircleHelper
** Description:             Support function for filled circle drawing
***************************************************************************************/
// Used to do circles and roundrects
void ST7789_FillCircleHelper(int16_t x0, int16_t y0, int16_t r, uint8_t cornername, int16_t delta, uint16_t color)
{
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -r - r;
    int16_t x = 0;

    delta++;
    while (x < r) {
        if (f >= 0) {
            r--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        if (cornername & 0x1) {
            ST7789_DrawFastVLine(x0 + x, y0 - r, r + r + delta, color);
            ST7789_DrawFastVLine(x0 + r, y0 - x, x + x + delta, color);
        }
        if (cornername & 0x2) {
            ST7789_DrawFastVLine(x0 - x, y0 - r, r + r + delta, color);
            ST7789_DrawFastVLine(x0 - r, y0 - x, x + x + delta, color);
        }
    }
}

/***************************************************************************************
** Function name:           drawEllipse
** Description:             Draw a ellipse outline
***************************************************************************************/
void ST7789_DrawEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color)
{
    if (rx < 2)
        return;
    if (ry < 2)
        return;
    int16_t x, y;
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t fx2 = 4 * rx2;
    int32_t fy2 = 4 * ry2;
    int32_t s;

    for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
        ST7789_DrawPixel(x0 + x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color);
        if (s >= 0) {
            s += fx2 * (1 - y);
            y--;
        }
        s += ry2 * ((4 * x) + 6);
    }

    for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
        ST7789_DrawPixel(x0 + x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 + y, color);
        ST7789_DrawPixel(x0 - x, y0 - y, color);
        ST7789_DrawPixel(x0 + x, y0 - y, color);
        if (s >= 0) {
            s += fy2 * (1 - x);
            x--;
        }
        s += rx2 * ((4 * y) + 6);
    }
}

/***************************************************************************************
** Function name:           fillEllipse
** Description:             draw a filled ellipse
***************************************************************************************/
void ST7789_FillEllipse(int16_t x0, int16_t y0, int16_t rx, int16_t ry, uint16_t color)
{
    if (rx < 2)
        return;
    if (ry < 2)
        return;
    int16_t x, y;
    int32_t rx2 = rx * rx;
    int32_t ry2 = ry * ry;
    int32_t fx2 = 4 * rx2;
    int32_t fy2 = 4 * ry2;
    int32_t s;

    for (x = 0, y = ry, s = 2 * ry2 + rx2 * (1 - 2 * ry); ry2 * x <= rx2 * y; x++) {
        ST7789_DrawFastHLine(x0 - x, y0 - y, x + x + 1, color);
        ST7789_DrawFastHLine(x0 - x, y0 + y, x + x + 1, color);

        if (s >= 0) {
            s += fx2 * (1 - y);
            y--;
        }
        s += ry2 * ((4 * x) + 6);
    }

    for (x = rx, y = 0, s = 2 * rx2 + ry2 * (1 - 2 * rx); rx2 * y <= ry2 * x; y++) {
        ST7789_DrawFastHLine(x0 - x, y0 - y, x + x + 1, color);
        ST7789_DrawFastHLine(x0 - x, y0 + y, x + x + 1, color);

        if (s >= 0) {
            s += fy2 * (1 - x);
            x--;
        }
        s += rx2 * ((4 * y) + 6);
    }
}

/***************************************************************************************
** Function name:           drawRect
** Description:             Draw a rectangle outline
***************************************************************************************/
// Draw a rectangle
void ST7789_DrawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color)
{
    ST7789_DrawFastHLine(x, y, w, color);
    ST7789_DrawFastHLine(x, y + h - 1, w, color);
    ST7789_DrawFastVLine(x, y, h, color);
    ST7789_DrawFastVLine(x + w - 1, y, h, color);
}

/***************************************************************************************
** Function name:           drawRoundRect
** Description:             Draw a rounded corner rectangle outline
***************************************************************************************/
// Draw a rounded rectangle
void ST7789_DrawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    // smarter version
    ST7789_DrawFastHLine(x + r, y, w - r - r, color); // Top
    ST7789_DrawFastHLine(x + r, y + h - 1, w - r - r, color); // Bottom
    ST7789_DrawFastVLine(x, y + r, h - r - r, color); // Left
    ST7789_DrawFastVLine(x + w - 1, y + r, h - r - r, color); // Right
    // draw four corners
    ST7789_DrawCircleHelper(x + r, y + r, r, 1, color);
    ST7789_DrawCircleHelper(x + r, y + h - r - 1, r, 8, color);
    ST7789_DrawCircleHelper(x + w - r - 1, y + r, r, 2, color);
    ST7789_DrawCircleHelper(x + w - r - 1, y + h - r - 1, r, 4, color);
}

/***************************************************************************************
** Function name:           fillRoundRect
** Description:             Draw a rounded corner filled rectangle
***************************************************************************************/
// Fill a rounded rectangle
void ST7789_FillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color)
{
    // smarter version
    ST7789_FillRectangle(x + r, y, w - r - r, h, color);

    // draw four corners
    ST7789_FillCircleHelper(x + w - r - 1, y + r, r, 1, h - r - r - 1, color);
    ST7789_FillCircleHelper(x + r, y + r, r, 2, h - r - r - 1, color);
}

/***************************************************************************************
** Function name:           drawTriangle
** Description:             Draw a triangle outline using 3 arbitrary points
***************************************************************************************/
// Draw a triangle
void ST7789_DrawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    ST7789_DrawLine(x0, y0, x1, y1, color);
    ST7789_DrawLine(x1, y1, x2, y2, color);
    ST7789_DrawLine(x2, y2, x0, y0, color);
}

/***************************************************************************************
** Function name:           fillTriangle
** Description:             Draw a filled triangle using 3 arbitrary points
***************************************************************************************/
// Fill a triangle - original Adafruit function works well and code footprint is small
void ST7789_FillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color)
{
    int16_t a, b, y, last;

    // Sort coordinates by Y order (y2 >= y1 >= y0)
    if (y0 > y1) {
        SWAP_INT16_T(y0, y1);
        SWAP_INT16_T(x0, x1);
    }

    if (y1 > y2) {
        SWAP_INT16_T(y2, y1);
        SWAP_INT16_T(x2, x1);
    }

    if (y0 > y1) {
        SWAP_INT16_T(y0, y1);
        SWAP_INT16_T(x0, x1);
    }

    if (y0 == y2) { // Handle awkward all-on-same-line case as its own thing
        a = b = x0;
        if (x1 < a)
            a = x1;
        else if (x1 > b)
            b = x1;
        if (x2 < a)
            a = x2;
        else if (x2 > b)
            b = x2;
        ST7789_DrawFastHLine(a, y0, b - a + 1, color);
        return;
    }

    int16_t
        dx01 = x1 - x0,
        dy01 = y1 - y0,
        dx02 = x2 - x0,
        dy02 = y2 - y0,
        dx12 = x2 - x1,
        dy12 = y2 - y1,
        sa = 0,
        sb = 0;

    // For upper part of triangle, find scanline crossings for segments
    // 0-1 and 0-2.  If y1=y2 (flat-bottomed triangle), the scanline y1
    // is included here (and second loop will be skipped, avoiding a /0
    // error there), otherwise scanline y1 is skipped here and handled
    // in the second loop...which also avoids a /0 error here if y0=y1
    // (flat-topped triangle).
    if (y1 == y2)
        last = y1; // Include y1 scanline
    else
        last = y1 - 1; // Skip it

    for (y = y0; y <= last; y++) {
        a = x0 + sa / dy01;
        b = x0 + sb / dy02;
        sa += dx01;
        sb += dx02;

        if (a > b)
            SWAP_INT16_T(a, b);
        ST7789_DrawFastHLine(a, y, b - a + 1, color);
    }

    // For lower part of triangle, find scanline crossings for segments
    // 0-2 and 1-2.  This loop is skipped if y1=y2.
    sa = dx12 * (y - y1);
    sb = dx02 * (y - y0);
    for (; y <= y2; y++) {
        a = x1 + sa / dy12;
        b = x0 + sb / dy02;
        sa += dx12;
        sb += dx02;

        if (a > b)
            SWAP_INT16_T(a, b);
        ST7789_DrawFastHLine(a, y, b - a + 1, color);
    }
}

/***************************************************************************************
** Function name:           drawLine
** Description:             draw a line between 2 arbitrary points
***************************************************************************************/

// Slower but more compact line drawing function
void ST7789_DrawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color)
{
    int16_t steep = abs(y1 - y0) > abs(x1 - x0);
    if (steep) {
        SWAP_INT16_T(x0, y0);
        SWAP_INT16_T(x1, y1);
    }

    if (x0 > x1) {
        SWAP_INT16_T(x0, x1);
        SWAP_INT16_T(y0, y1);
    }

    int16_t dx, dy;
    dx = x1 - x0;
    dy = abs(y1 - y0);

    int16_t err = dx / 2;
    int16_t ystep;

    if (y0 < y1) {
        ystep = 1;
    }
    else {
        ystep = -1;
    }

    for (; x0 <= x1; x0++) {
        if (steep) {
            ST7789_DrawPixel(y0, x0, color);
        }
        else {
            ST7789_DrawPixel(x0, y0, color);
        }
        err -= dy;
        if (err < 0) {
            y0 += ystep;
            err += dx;
        }
    }
}

/***************************************************************************************
** Function name:           drawFastVLine
** Description:             draw a vertical line
***************************************************************************************/
void ST7789_DrawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color)
{

    // Rudimentary clipping
    if ((x >= _width) || (y >= _height))
        return;
    if ((y + h - 1) >= _height)
        h = _height - y;

#ifdef USE_SPI_DMA
    ST7789_FillRectangle(x, y, 1, h, color);
#else
    ST7789_DrawLine(x, y, x, y + h - 1, color);
#endif
}

/***************************************************************************************
** Function name:           drawFastHLine
** Description:             draw a horizontal line
***************************************************************************************/
void ST7789_DrawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color)
{
    // Rudimentary clipping

    if ((x >= _width) || (y >= _height))
        return;
    if ((x + w - 1) >= _width)
        w = _width - x;

#ifdef USE_SPI_DMA
    ST7789_FillRectangle(x, y, w, 1, color);
#else
    ST7789_DrawLine(x, y, x + w - 1, y, color);
#endif
}

/***************************************************************************************
** Function name:           setRotation
** Description:             rotate the screen orientation m = 0-3
***************************************************************************************/
void ST7789_SetRotation(uint8_t m)
{
    _value_rotation = m % 4;

    TFT_CS_L();

    ST7789_WriteCommand(ST7789_MADCTL);

    switch (_value_rotation) {
    case 0: {
        uint8_t d_r = (ST7789_MADCTL_MX | ST7789_MADCTL_MY | ST7789_MADCTL_RGB);
        ST7789_WriteData(&d_r, sizeof(d_r));
        _width = ST7789_TFTWIDTH;
        _height = ST7789_TFTHEIGHT;
        X_SHIFT = 0;
        Y_SHIFT = 80;
    } break;
    case 1: {
        uint8_t d_r = (ST7789_MADCTL_MY | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
        ST7789_WriteData(&d_r, sizeof(d_r));
        _width = ST7789_TFTHEIGHT;
        _height = ST7789_TFTWIDTH;
        X_SHIFT = 80;
        Y_SHIFT = 0;
    } break;
    case 2: {
        uint8_t d_r = (ST7789_MADCTL_RGB);
        ST7789_WriteData(&d_r, sizeof(d_r));
        _width = ST7789_TFTWIDTH;
        _height = ST7789_TFTHEIGHT;
        X_SHIFT = 0;
        Y_SHIFT = 0;
    } break;
    case 3: {
        uint8_t d_r = (ST7789_MADCTL_MX | ST7789_MADCTL_MV | ST7789_MADCTL_RGB);
        ST7789_WriteData(&d_r, sizeof(d_r));
        _width = ST7789_TFTHEIGHT;
        _height = ST7789_TFTWIDTH;
        X_SHIFT = 0;
        Y_SHIFT = 0;
    } break;
    }
    TFT_CS_H();
}

uint8_t ST7789_GetRotation(void)
{
    return _value_rotation;
}

int16_t ST7789_GetHeight(void)
{
    return _height;
}

int16_t ST7789_GetWidth(void)
{
    return _width;
}

uint8_t get_bit_from_byte(uint8_t byte, uint8_t bit)
{
    return byte & (1 << bit);
}

void ST7789_DrawChar(char ch, const uint8_t font[], uint16_t X, uint16_t Y, uint16_t color, uint16_t bgcolor)
{
    if ((ch < 31) || (ch > 127))
        return;

    uint8_t fOffset, fWidth, fHeight, fBPL;
    uint8_t* tempChar;

    fOffset = font[0];
    fWidth = font[1];
    fHeight = font[2];
    fBPL = font[3];
    uint16_t buffer[fHeight * fWidth];
    uint16_t index = 0;

    tempChar = (uint8_t*)&font[((ch - 0x20) * fOffset) + 4]; /* Current Character = Meta + (Character Index * Offset) */

    for (int j = 0; j < fHeight; j++) {
        for (int i = 0; i < fWidth; i++) {
            uint8_t z = tempChar[fBPL * i + ((j & 0xF8) >> 3) + 1]; /* (j & 0xF8) >> 3, increase one by 8-bits */
            uint8_t b = 1 << (j & 0x07);
            if ((z & b) != 0x00)
                buffer[index] = color;
            else
                buffer[index] = bgcolor;
            index++;
        }
    }
    ST7789_DrawImage(X, Y, fWidth, fHeight, buffer);
}

void ST7789_DrawText(const char* str, const uint8_t font[], uint16_t X, uint16_t Y, uint16_t color, uint16_t bgcolor)
{
    uint8_t charWidth; /* Width of character */
    uint8_t fOffset = font[0]; /* Offset of character */
    uint8_t fWidth = font[1]; /* Width of font */

    while (*str) {
        ST7789_DrawChar(*str, font, X, Y, color, bgcolor);

        /* Check character width and calculate proper position */
        uint8_t* tempChar = (uint8_t*)&font[((*str - 0x20) * fOffset) + 4];
        charWidth = tempChar[0];

        if (charWidth + 2 < fWidth) {
            /* If character width is smaller than font width */
            X += (charWidth + 2);
        }
        else {
            X += fWidth;
        }

        str++;
    }
}

uint16_t ST7789_Color565(uint8_t r, uint8_t g, uint8_t b)
{
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}
