/*****************************************************************************
* | File      	:   LCD_1in3_LVGL_test.c
* | Author      :   Waveshare team
* | Function    :   1.3inch LCD LVGL demo
* | Info        :
*----------------
* |	This version:   V1.0
* | Date        :   2024-07-08
* | Info        :
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documnetation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to  whom the Software is
# furished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS OR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.
#
******************************************************************************/
#include "LCD_Test.h"
#include "LCD_1in3.h"

static void Widgets_Init(lvgl_data_struct *dat);
static void LCD_1IN3_KEY_Init(void);
static uint16_t LCD_1IN3_Read_KEY(void);

/********************************************************************************
function:   Main function
parameter:
********************************************************************************/
int LCD_1in3_test(void)
{
    if(DEV_Module_Init()!=0){
        return -1;
    }
    
    /*KEY Init*/
    LCD_1IN3_KEY_Init();

    /*LCD Init*/
    printf("1.3inch LCD LVGL demo...\r\n");
    LCD_1IN3_Init(HORIZONTAL);
    LCD_1IN3_Clear(WHITE);

    /*Config parameters*/
    LCD_SetWindows = LCD_1IN3_SetWindows;
    DISP_HOR_RES = LCD_1IN3_HEIGHT;
    DISP_VER_RES = LCD_1IN3_WIDTH;

    /*Init LVGL data structure*/    
    lvgl_data_struct *dat = (lvgl_data_struct *)malloc(sizeof(lvgl_data_struct));
    memset(dat->scr, 0, sizeof(dat->scr));
    dat->click_num = 0;
    
    /*Init LVGL*/
    LVGL_Init();
    Widgets_Init(dat);

    while(1)
    {
        lv_task_handler();
        DEV_Delay_ms(5); 

        /*Read the key value and save it to the current key value*/
        dat->KEY_now = LCD_1IN3_Read_KEY();

        /*Handling key press events*/
        handle_key_press(dat);

        /*Save the current key value as the previous key value*/
        dat->KEY_old = dat->KEY_now;
    }

    DEV_Module_Exit();
    return 0;
}

/********************************************************************************
function:   Initialize Widgets
parameter:
********************************************************************************/
static void Widgets_Init(lvgl_data_struct *dat)
{
    static lv_style_t style_label;
    lv_style_init(&style_label);
    lv_style_set_text_font(&style_label, &lv_font_montserrat_16);

    /*Screen1: Just a picture*/
    dat->scr[0] = lv_obj_create(NULL);

    /*Declare and load the image resource*/
    LV_IMG_DECLARE(LCD_1inch3);
    lv_obj_t *img1 = lv_img_create(dat->scr[0]);
    lv_img_set_src(img1, &LCD_1inch3);

    /*Align the image to the center of the screen*/
    lv_obj_align(img1, LV_ALIGN_CENTER, 0, 0);

    /*Load the first screen as the current active screen*/
    lv_scr_load(dat->scr[0]);

    /*Screen2: User Interface 1*/
    dat->scr[1] = lv_obj_create(NULL);
    lv_obj_clear_flag(dat->scr[1], LV_OBJ_FLAG_SCROLLABLE);

    /*Create a 110x35 pixel button and add it to the center of the second screen*/
    dat->btn = lv_btn_create(dat->scr[1]);     
    lv_obj_set_size(dat->btn, 120, 40);                
    lv_obj_align(dat->btn, LV_ALIGN_CENTER, 0, 0);

    /*Create a label on the button and set its initial text to "Click:0"*/
    dat->label = lv_label_create(dat->btn);          
    lv_label_set_text(dat->label, "Click:0");               
    lv_obj_center(dat->label);
    lv_obj_add_style(dat->label,&style_label,0);

    /*Create an icon and add it to the second screen and set the image source of the icon to the GPS symbol*/
    dat->cur = lv_img_create(dat->scr[1]);
    lv_img_set_src(dat->cur, LV_SYMBOL_GPS);

    /*Screen3: User Interface 2*/
    dat->scr[2] = lv_obj_create(NULL);
    lv_obj_clear_flag(dat->scr[2], LV_OBJ_FLAG_SCROLLABLE);

    /*Create two switches and two labels in the third screen*/
    dat->sw_1 = lv_switch_create(dat->scr[2]);
    lv_obj_set_size(dat->sw_1, 75, 40);                          
    lv_obj_align(dat->sw_1, LV_ALIGN_RIGHT_MID, -40, -40);
    lv_obj_t *label_sw = lv_label_create(dat->scr[2]);          
    lv_obj_align(label_sw, LV_ALIGN_LEFT_MID, 40, -40);    
    lv_label_set_text(label_sw, "KEY_X");   

    /*Add style to the label and set the font size to 16*/
    lv_obj_add_style(label_sw,&style_label,0);

    dat->sw_2 = lv_switch_create(dat->scr[2]);
    lv_obj_set_size(dat->sw_2, 75, 40);                          
    lv_obj_align(dat->sw_2, LV_ALIGN_RIGHT_MID, -40, 40);
    label_sw = lv_label_create(dat->scr[2]); 
    lv_obj_align(label_sw, LV_ALIGN_LEFT_MID, 40, 40);    
    lv_label_set_text(label_sw, "KEY_Y");   
    lv_obj_add_style(label_sw,&style_label,0);
}

/********************************************************************************
function:   Initialize all keys
parameter:
********************************************************************************/
static void LCD_1IN3_KEY_Init()
{
    DEV_KEY_Config(LCD_KEY_A);
    DEV_KEY_Config(LCD_KEY_B);
    DEV_KEY_Config(LCD_KEY_X);
    DEV_KEY_Config(LCD_KEY_Y);
    DEV_KEY_Config(LCD_KEY_UP);
    DEV_KEY_Config(LCD_KEY_DOWN);
    DEV_KEY_Config(LCD_KEY_LEFT);
    DEV_KEY_Config(LCD_KEY_RIGHT);
    DEV_KEY_Config(LCD_KEY_CTRL);
}

/********************************************************************************
function:   Read the current status of KEY
parameter:
********************************************************************************/
static uint16_t LCD_1IN3_Read_KEY(void) 
{
    uint16_t KEY_Value = 0;

    if (DEV_Digital_Read(LCD_KEY_A)  == 0)          KEY_Value |= KEY_A;
    if (DEV_Digital_Read(LCD_KEY_B)  == 0)          KEY_Value |= KEY_B;
    if (DEV_Digital_Read(LCD_KEY_X)  == 0)          KEY_Value |= KEY_X;
    if (DEV_Digital_Read(LCD_KEY_Y)  == 0)          KEY_Value |= KEY_Y;

    if (DEV_Digital_Read(LCD_KEY_UP) == 0)          KEY_Value |= KEY_UP;
    else if (DEV_Digital_Read(LCD_KEY_DOWN)  == 0)  KEY_Value |= KEY_DOWN;
    else if (DEV_Digital_Read(LCD_KEY_LEFT)  == 0)  KEY_Value |= KEY_LEFT;
    else if (DEV_Digital_Read(LCD_KEY_RIGHT) == 0)  KEY_Value |= KEY_RIGHT;
    else if (DEV_Digital_Read(LCD_KEY_CTRL)  == 0)  KEY_Value |= KEY_CTRL;

    return KEY_Value;
}