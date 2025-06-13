# CH32V00X DMA ST7789 LCD Display

## ST7789 DMA Library for CH32V00X
Compiled with MounRiverStudio 2.10<br>
Tested with CH32V006E8R6 <br>
SPI Remapped :<br>

 PA4  BL<br>
 PA5  CS<br>
 PA1  DC<br>
 PA0  RST<br>
 PD2  SCK<br>
 PD3  MOSI<br>


Library supports C font array created by GLCD Font Creator by MikroElektronika.<br>
https://www.mikroe.com/glcd-font-creator<br>

Please refer to the following link to get more details about fonts.<br>
https://os.mbed.com/users/dreschpe/code/SPI_TFT_ILI9341/<br>

[ How to add new fonts ]
1. Run GLCD Font Creator
2. Click File-New Font-Import An Existing System Font
3. Select font, style and size from font dialog.
4. GLCD Font Cretor makes Bitmap fonts
5. Click Export for GLCD menu
6. Select mikroC tab.
7. Copy generated code to fonts.c file
8. Modify data type from unsigned short to uint8_t
9. Add optional bytes (offset, width, height, bpl) to the array !!! IMPORTANT !!!
10. Add extern declaration to fonts.h file

