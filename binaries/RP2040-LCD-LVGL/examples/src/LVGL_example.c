/*****************************************************************************
* | File      	:   LCD_1in28_LVGL_test.c
* | Author      :   Waveshare team
* | Function    :   1.28inch LCD  test demo
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

#include "LVGL_example.h" 
#include "src/core/lv_obj.h"
#include "src/misc/lv_area.h"

// LCD 
LCD_SetWindowsFunc LCD_SetWindows;
uint16_t DISP_HOR_RES;
uint16_t DISP_VER_RES;

// LVGL
static lv_disp_draw_buf_t disp_buf;
static lv_color_t *buf0;
static lv_color_t *buf1;
static lv_disp_drv_t disp_drv;

// Timer 
static struct repeating_timer lvgl_timer;

static void disp_flush_cb(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p);
static void dma_handler(void);
static bool repeating_lvgl_timer_callback(struct repeating_timer *t); 

static void widgets_up(lv_obj_t *widgets);
static void widgets_down(lv_obj_t *widgets, uint16_t DISP_VER_RES);
static void widgets_left(lv_obj_t *widgets);
static void widgets_right(lv_obj_t *widgets, uint16_t DISP_HOR_RES);
static bool click_valid(lv_obj_t *cur, lv_obj_t *widgets);

/********************************************************************************
function:	Initializes LVGL and enbable timers IRQ and DMA IRQ
parameter:
********************************************************************************/
void LVGL_Init(void)
{
    /*1.Init Timer*/ 
    add_repeating_timer_ms(5, repeating_lvgl_timer_callback, NULL, &lvgl_timer);
    
    /*2.Init LVGL core*/
    lv_init();

    /*3.Init LVGL display*/
    buf0 = (lv_color_t *)malloc(DISP_HOR_RES * DISP_VER_RES / 2 * sizeof(lv_color_t));
    buf1 = (lv_color_t *)malloc(DISP_HOR_RES * DISP_VER_RES / 2 * sizeof(lv_color_t));
    lv_disp_draw_buf_init(&disp_buf, buf0, buf1, DISP_HOR_RES * DISP_VER_RES / 2); 
    lv_disp_drv_init(&disp_drv);    
    disp_drv.flush_cb = disp_flush_cb;
    disp_drv.draw_buf = &disp_buf;        
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    lv_disp_t *disp= lv_disp_drv_register(&disp_drv);   

    /*4.Init DMA for transmit color data from memory to SPI*/
    dma_channel_set_irq0_enabled(dma_tx, true);
    irq_set_exclusive_handler(DMA_IRQ_0, dma_handler);
    irq_set_enabled(DMA_IRQ_0, true);
}

/********************************************************************************
function:	Refresh image by transferring the color data to the SPI bus by DMA
parameter:
********************************************************************************/
static void disp_flush_cb(lv_disp_drv_t * disp, const lv_area_t * area, lv_color_t * color_p)
{
    LCD_SetWindows(area->x1, area->y1, area->x2+1, area->y2+1);
    DEV_Digital_Write(LCD_DC_PIN, 1);
    DEV_Digital_Write(LCD_CS_PIN, 0);
    dma_channel_configure(dma_tx,
                          &c,
                          &spi_get_hw(LCD_SPI_PORT)->dr, 
                          color_p, // read address
                          ((area->x2 + 1 - area-> x1)*(area->y2 + 1 - area -> y1))*2,
                          true);
}

/********************************************************************************
function:   Indicate ready with the flushing when DMA complete transmission
parameter:
********************************************************************************/
static void dma_handler(void)
{
    if (dma_channel_get_irq0_status(dma_tx)) {
        dma_channel_acknowledge_irq0(dma_tx);
        DEV_Digital_Write(LCD_CS_PIN, 1);
        lv_disp_flush_ready(&disp_drv); // Indicate you are ready with the flushing
    }
}

/********************************************************************************
function:   Report the elapsed time to LVGL each 5ms
parameter:
********************************************************************************/
static bool repeating_lvgl_timer_callback(struct repeating_timer *t) 
{
    lv_tick_inc(5);
    return true;
}

/********************************************************************************
function:   Switch to the next screen
parameter:
********************************************************************************/
void switch_to_next_screen(lv_obj_t *scr[]) 
{
    /*Number of effective interfaces*/
    int scr_num = 0;
    while (scr[scr_num] != NULL) {
        scr_num++;
    }

    /*Determine which interface is the current one and switch to the next one*/
    lv_obj_t *scr_now = lv_scr_act();
    for (int i = 0; i < scr_num; i++) {
        if (scr_now == scr[i]) {
            int next_scr_idx = (i + 1) % scr_num;// If the current interface is the last one, switch to the first interface
            lv_scr_load_anim(scr[next_scr_idx], LV_SCR_LOAD_ANIM_FADE_ON, 500, 0, false);// The interface switching animation is fade-in and fade-out, which lasts for 500 milliseconds, which can effectively solve the jagged or ripple problems when switching interfaces
            break;
        }
    }
}

/********************************************************************************
function:   Move widgets up
parameter:
********************************************************************************/
static void widgets_up(lv_obj_t *widgets) 
{
    int y = lv_obj_get_y(widgets); // Get the y coordinate of widgets
    y -= 1; // The y coordinate is minus 1
    if (y < 0) y = 0; // Limit the y coordinate of widgets so that they do not exceed the top of the screen
    lv_obj_set_y(widgets, y); // Set the y coordinate of widgets
}

/********************************************************************************
function:   Move widgets down
parameter:
********************************************************************************/
static void widgets_down(lv_obj_t *widgets, uint16_t DISP_VER_RES) 
{
    int y = lv_obj_get_y(widgets); // Get the y coordinate of widgets
    int h = lv_obj_get_height(widgets); // Get the height of widgets
    y += 1; // Add 1 to the y coordinate
    if (y > DISP_VER_RES - h) y = DISP_VER_RES - h; // Control the y coordinate of widgets so that their bottom cannot exceed the bottom of the screen
    lv_obj_set_y(widgets, y); // Set the y coordinate of widgets
}

/********************************************************************************
function:   Move widgets left
parameter:
********************************************************************************/
static void widgets_left(lv_obj_t *widgets) 
{
    int x = lv_obj_get_x(widgets); // Get the x coordinate of widgets
    x -= 1; // The x coordinate is minus 1
    if (x < 0) x = 0; // Limit the x coordinate of widgets so that they do not exceed the left of the screen
    lv_obj_set_x(widgets, x); // Set the x coordinate of widgets
}

/********************************************************************************
function:   Move widgets right
parameter:
********************************************************************************/
static void widgets_right(lv_obj_t *widgets, uint16_t DISP_HOR_RES) 
{
    int x = lv_obj_get_x(widgets); // Get the x coordinate of widgets
    int w = lv_obj_get_width(widgets); // Get the width of widgets
    x += 1;// Add 1 to the x coordinate
    if (x > DISP_HOR_RES - w) x = DISP_HOR_RES - w; // Control the x coordinate of widgets so that their right side cannot exceed the right side of the screen
    lv_obj_set_x(widgets, x); // Set the y coordinate of widgets
}

/********************************************************************************
function:   Determine whether the click is valid
parameter:
********************************************************************************/
static bool click_valid(lv_obj_t *cur, lv_obj_t *widgets) 
{
    /*Get the coordinates and size of cursor*/
    int x1 = lv_obj_get_x(cur);
    int y1 = lv_obj_get_y(cur);
    int w1 = lv_obj_get_width(cur);
    int h1 = lv_obj_get_height(cur);

    /*Get the coordinates and size of widgets*/
    int x2 = lv_obj_get_x(widgets);
    int y2 = lv_obj_get_y(widgets);
    int w2 = lv_obj_get_width(widgets);
    int h2 = lv_obj_get_height(widgets);
    
    /*Determine whether the right border of cursor is within the range of widget*/
    if (x1 + w1 >= x2 && x1 + w1 <= x2 + w2 && y1 >= y2 && y1 <= y2 + h2)
    {
        return true;
    }
    
    return false;
}

/********************************************************************************
function:   Handling key press events
parameter:
********************************************************************************/
void handle_key_press(lvgl_data_struct *dat) 
{
#if LCD_HAS_JOY
    /*If KEY_B is currently pressed and its previous state is released*/
    if((dat->KEY_now & KEY_B) && !(dat->KEY_old & KEY_B))
    {
        switch_to_next_screen(dat->scr); // Switch to the next screen
    }

    /*If the active screen is the second*/
    if(lv_scr_act() == dat->scr[1])
    {
        if(dat->KEY_now & KEY_UP) // Joystick up
        {
            widgets_up(dat->cur); // Move the pointer up
        }
        else if(dat->KEY_now & KEY_DOWN) // Joystick down
        {
            widgets_down(dat->cur, DISP_VER_RES); // Move the pointer down
        }
        else if(dat->KEY_now & KEY_LEFT) // Joystick left
        {
            widgets_left(dat->cur); // Move the pointer left
        }
        else if(dat->KEY_now & KEY_RIGHT) // Joystick right
        {
            widgets_right(dat->cur, DISP_HOR_RES); //Move the pointer right
        }
        else if(dat->KEY_now & KEY_CTRL) // Joystick press down
        {
            lv_obj_set_pos(dat->cur, 0, 0); //Set the pointer coordinates to 0, 0
        }

        /*If KEY_A is currently pressed and its previous state is released*/
        if((dat->KEY_now & KEY_A) && !(dat->KEY_old & KEY_A))
        {
            /*Determine whether the click is valid*/
            if(click_valid(dat->cur, dat->btn) == true)
            {
                char label_text[64];
                dat->click_num++; // Click count plus one
                sprintf(label_text, "Click:%d", dat->click_num);
                lv_label_set_text(dat->label, label_text);  // Update label
                lv_obj_add_state(dat->btn, LV_STATE_PRESSED); // Set the button state to pressed
            }
        }
        /*If KEY_A is currently released*/
        else if(!(dat->KEY_now & KEY_A))
        {
            lv_obj_clear_state(dat->btn, LV_STATE_PRESSED); // Set the button state to released
        }
    }

    /*If the active screen is the third*/
    else if(lv_scr_act() == dat->scr[2])
    {
        /*If KEY_X is currently pressed and its previous state is released*/
        if((dat->KEY_now & KEY_X) && !(dat->KEY_old & KEY_X))
        {
            if (lv_obj_has_state(dat->sw_1, LV_STATE_CHECKED)) // If SW_1 is currently selected
                lv_obj_clear_state(dat->sw_1, LV_STATE_CHECKED); // Clear the selected state of SW_1
            else
                lv_obj_add_state(dat->sw_1, LV_STATE_CHECKED); // Set SW_1 to selected state
        }

        /*If KEY_Y is currently pressed and its previous state is released*/
        if((dat->KEY_now & KEY_Y) && !(dat->KEY_old & KEY_Y))
        {
            if (lv_obj_has_state(dat->sw_2, LV_STATE_CHECKED)) // If SW_2 is currently selected
                lv_obj_clear_state(dat->sw_2, LV_STATE_CHECKED); // Clear the selected state of SW_2
            else
                lv_obj_add_state(dat->sw_2, LV_STATE_CHECKED); // Set SW_2 to selected state
        }
    }
#else
    /*If KEY_0 is currently pressed and its previous state is released*/
    if((dat->KEY_now & KEY_0) && !(dat->KEY_old & KEY_0))
    {
        switch_to_next_screen(dat->scr); // Switch to the next screen
    }

    /*If the active screen is the second*/
    if(lv_scr_act() == dat->scr[1])
    {   
        /*If KEY_3 is currently pressed and its previous state is released*/
        if((dat->KEY_now & KEY_3) && !(dat->KEY_old & KEY_3))
        {
            char label_text[64];
            dat->click_num++;
            sprintf(label_text, "KEY_3 Click:%d", dat->click_num);
            lv_label_set_text(dat->label, label_text);  
            lv_obj_add_state(dat->btn, LV_STATE_PRESSED); 
        }
        /*If KEY_3 is currently released*/
        else if(!(dat->KEY_now & KEY_3))
        {
            lv_obj_clear_state(dat->btn, LV_STATE_PRESSED);
        }
    }

    /*If the active screen is the third*/
    else if(lv_scr_act() == dat->scr[2])
    {
        /*If KEY_2 is currently pressed and its previous state is released*/
        if((dat->KEY_now & KEY_2) && !(dat->KEY_old & KEY_2))
        {
            if (lv_obj_has_state(dat->sw_1, LV_STATE_CHECKED))
                lv_obj_clear_state(dat->sw_1, LV_STATE_CHECKED);
            else
                lv_obj_add_state(dat->sw_1, LV_STATE_CHECKED);
        }

        /*If KEY_1 is currently pressed and its previous state is released*/
        if((dat->KEY_now & KEY_1) && !(dat->KEY_old & KEY_1))
        {
            if (lv_obj_has_state(dat->sw_2, LV_STATE_CHECKED))
                lv_obj_clear_state(dat->sw_2, LV_STATE_CHECKED);
            else
                lv_obj_add_state(dat->sw_2, LV_STATE_CHECKED);
        }
    }
#endif
}
