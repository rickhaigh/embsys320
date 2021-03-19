/************************************************************************************

Copyright (c) 2001-2016  University of Washington Extension.

Module Name:

    tasks.c

Module Description:

    The tasks that are executed by the test application.

2016/2 Nick Strathy adapted it for NUCLEO-F401RE 

************************************************************************************/
#include <stdarg.h>

#include "bsp.h"
#include "print.h"
#include "mp3Util.h"

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h>
#include <Adafruit_FT6206.h>
#include "SD.h"

#define train 0


// Create button to skip Mp3 song
Adafruit_GFX_Button skip_btn = Adafruit_GFX_Button();
// Create button to stop Mp3
Adafruit_GFX_Button stop_btn = Adafruit_GFX_Button();
Adafruit_GFX_Button play_pause_btn = Adafruit_GFX_Button();
// Create button to reduce Mp3 volume
Adafruit_GFX_Button voldn_btn = Adafruit_GFX_Button();
// Create button to increase Mp3 volume
Adafruit_GFX_Button volup_btn = Adafruit_GFX_Button();
// Create button to use for testing
Adafruit_GFX_Button test_btn = Adafruit_GFX_Button();

// message queue components
#define QMAXENTRIES 4            // maximum entries in the queue

typedef struct{                   // a queue entry
	uint8_t msg;
}QMsg_t;

OS_EVENT *qMsg;                  // pointer to a uCOS message queue
void * qMsgVPtrs[QMAXENTRIES];   // an array of void pointers which is the actual queue
QMsg_t qMsgBlocks[QMAXENTRIES];  // a pool of message nodes that may be used as queue entries
OS_MEM *qMsgMemPart;             // pointer to a uCOS memory partition to manage the pool of message nodes

// Mailbox components
//INT32U touch_msgCount = 0;
OS_EVENT *mbox_touch;
OS_EVENT *mbox_mp3;

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller
Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller

uint8_t mp3vol = 10;

#define PENRADIUS 3

// x point to map
// in_min
// in_max
// out_min
// out_max
// return mapped point
long MapTouchToScreen(long x, long in_min, long in_max, long out_min, long out_max)
{
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

#if train
#include "train_crossing.h"
#endif

#define BUFSIZE 256

/************************************************************************************

   Allocate the stacks for each task.
   The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h

************************************************************************************/

static OS_STK   LcdTouchDemoTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK   Mp3DemoTaskStk[APP_CFG_TASK_START_STK_SIZE];
static OS_STK   ControlTaskStk[APP_CFG_TASK_START_STK_SIZE];
     
// Task prototypes
void LcdTouchDemoTask(void* pdata);
void Mp3DemoTask(void* pdata);
void ControlTask(void* pdata);

// Useful functions
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);
void InitButtons();

// Globals
BOOLEAN nextSong = OS_FALSE;

/************************************************************************************

   This task is the initial task running, started by main(). It starts
   the system tick timer and creates all the other tasks. Then it deletes itself.

************************************************************************************/
void StartupTask(void* pdata)
{
	char buf[BUFSIZE];
    //uint8_t err;
    PjdfErrCode pjdfErr;
    INT32U length;
    static HANDLE hSD = 0;
    static HANDLE hSPI = 0;

	PrintWithBuf(buf, BUFSIZE, "StartupTask: Begin\n");
	PrintWithBuf(buf, BUFSIZE, "StartupTask: Starting timer tick\n");

    // Create qMsg as a uCOS queue that uses qMsgVPtrs to store queue entry pointers
    //qMsg = OSQCreate(qMsgVPtrs, QMAXENTRIES);
	// Create qMsgMemPart as a uCOS memory partition containing a pool
	// of QMAXENTRIES messages where each entry is of type QMsg_t
    //qMsgMemPart = OSMemCreate(qMsgBlocks, QMAXENTRIES, sizeof(QMsg_t), &err);
    
    // code to create 2 mailboxs mbox_touch and mbox_mp3, initially empty
    mbox_touch = OSMboxCreate(NULL);
    mbox_mp3 = OSMboxCreate(NULL);
    
    // Start the system tick
    SetSysTick(OS_TICKS_PER_SEC);
    
    // Initialize SD card
    PrintWithBuf(buf, PRINTBUFMAX, "Opening handle to SD driver: %s\n", PJDF_DEVICE_ID_SD_ADAFRUIT);
    hSD = Open(PJDF_DEVICE_ID_SD_ADAFRUIT, 0);
    if (!PJDF_IS_VALID_HANDLE(hSD)) while(1);


    PrintWithBuf(buf, PRINTBUFMAX, "Opening SD SPI driver: %s\n", SD_SPI_DEVICE_ID);
    // We talk to the SD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to 
    // the SD driver.
    hSPI = Open(SD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);
    
    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hSD, PJDF_CTRL_SD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);
    
    // Start the SD card
    if(!SD.begin(hSD))
    {
        PrintWithBuf(buf, BUFSIZE, "Attempt to initialize SD card failed.\n");
    }

    // Create the test tasks
    PrintWithBuf(buf, BUFSIZE, "StartupTask: Creating the application tasks\n");

    // The maximum number of tasks the application can have is defined by OS_MAX_TASKS in os_cfg.h
    OSTaskCreate(Mp3DemoTask, (void*)0, &Mp3DemoTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TEST1_PRIO);
    OSTaskCreate(LcdTouchDemoTask, (void*)0, &LcdTouchDemoTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TEST2_PRIO);
    OSTaskCreate(ControlTask, (void*)0, &ControlTaskStk[APP_CFG_TASK_START_STK_SIZE-1], APP_TASK_TEST3_PRIO);
    
    // Delete ourselves, letting the work be done in the new tasks.
    PrintWithBuf(buf, BUFSIZE, "StartupTask: deleting self\n");
	OSTaskDel(OS_PRIO_SELF);
}
static void drawPlayPause()
{
    play_pause_btn.drawButton(0);
    Mp3_Ctrl mp3_control_state = Mp3GetControl();
    if(mp3_control_state == Mp3_Pause) {
        // First cover up pause button and then draw play button
        int16_t x1 = 200 - 15;
        int16_t y1 = 210 - 23;
        int16_t w = 10;
        int16_t h = 46;
        int16_t r = 5;    
        
        // Draw pause button 
        lcdCtrl.fillRoundRect(x1, y1, w, h, r, ILI9341_BLACK);
        lcdCtrl.fillRoundRect(x1 + 20, y1, w, h, r, ILI9341_BLACK);
        
        // Play button triangle coords
        // x0 and y0 coords are using button center coords as basis
        int16_t tx0 = 200 - 23;  // started at 23
        int16_t tx1 = tx0;
        int16_t tx2 = tx0 + 46;
        int16_t ty0 = 210 + 23;
        int16_t ty1 = ty0 - 46;
        int16_t ty2 = ty0 - 23;
        
        // Draw play button triangle after drawButtons so it will be on top
        lcdCtrl.fillTriangle (tx0, ty0,
                              tx1, ty1,
                              tx2, ty2, ILI9341_WHITE);
    } else if (mp3_control_state == Mp3_Play) {
        // First cover up Play button and then draw pause button
                // Play button triangle coords
        // x0 and y0 coords are using button center coords as basis
        int16_t tx0 = 200 - 23;  // started at 23
        int16_t tx1 = tx0;
        int16_t tx2 = tx0 + 46;
        int16_t ty0 = 210 + 23;
        int16_t ty1 = ty0 - 46;
        int16_t ty2 = ty0 - 23;
        
        // Draw play button triangle after drawButtons so it will be on top
        lcdCtrl.fillTriangle (tx0, ty0,
                              tx1, ty1,
                              tx2, ty2, ILI9341_BLACK);
        // Now draw pause button
        int16_t x1 = 200 - 15;
        int16_t y1 = 210 - 23;
        int16_t w = 10;
        int16_t h = 46;
        int16_t r = 5;    
        
        // Draw pause button 
        lcdCtrl.fillRoundRect(x1, y1, w, h, r, ILI9341_WHITE);
        lcdCtrl.fillRoundRect(x1 + 20, y1, w, h, r, ILI9341_WHITE);
    }
    
}
static void drawButtons()
{
    //InitButtons();  // do this in case button text has changed
    skip_btn.drawButton(0);
    //play_pause_btn.drawButton(0);
    voldn_btn.drawButton(0);
    volup_btn.drawButton(0);
    test_btn.drawButton(0);
    
    // Draw pretty button images
//    Mp3_Ctrl mp3_control_state = Mp3GetControl();
//    if(mp3_control_state == Mp3_Pause) {
//        // First cover up pause button and then draw play button
//        int16_t x1 = 200 - 15;
//        int16_t y1 = 210 - 23;
//        int16_t w = 10;
//        int16_t h = 46;
//        int16_t r = 5;    
//        
//        // Draw pause button 
//        lcdCtrl.fillRoundRect(x1, y1, w, h, r, ILI9341_BLACK);
//        lcdCtrl.fillRoundRect(x1 + 20, y1, w, h, r, ILI9341_BLACK);
//        
//        // Play button triangle coords
//        // x0 and y0 coords are using button center coords as basis
//        int16_t tx0 = 200 - 23;  // started at 23
//        int16_t tx1 = tx0;
//        int16_t tx2 = tx0 + 46;
//        int16_t ty0 = 210 + 23;
//        int16_t ty1 = ty0 - 46;
//        int16_t ty2 = ty0 - 23;
//        
//        // Draw play button triangle after drawButtons so it will be on top
//        lcdCtrl.fillTriangle (tx0, ty0,
//                              tx1, ty1,
//                              tx2, ty2, ILI9341_WHITE);
//    } else if (mp3_control_state == Mp3_Play) {
//        // First cover up Play button and then draw pause button
//                // Play button triangle coords
//        // x0 and y0 coords are using button center coords as basis
//        int16_t tx0 = 200 - 23;  // started at 23
//        int16_t tx1 = tx0;
//        int16_t tx2 = tx0 + 46;
//        int16_t ty0 = 210 + 23;
//        int16_t ty1 = ty0 - 46;
//        int16_t ty2 = ty0 - 23;
//        
//        // Draw play button triangle after drawButtons so it will be on top
//        lcdCtrl.fillTriangle (tx0, ty0,
//                              tx1, ty1,
//                              tx2, ty2, ILI9341_BLACK);
//        // Now draw pause button
//        int16_t x1 = 200 - 15;
//        int16_t y1 = 210 - 23;
//        int16_t w = 10;
//        int16_t h = 46;
//        int16_t r = 5;    
//        
//        // Draw pause button 
//        lcdCtrl.fillRoundRect(x1, y1, w, h, r, ILI9341_WHITE);
//        lcdCtrl.fillRoundRect(x1 + 20, y1, w, h, r, ILI9341_WHITE);
//    }
    
    // Draw skip button
    // x0 and y0 coords are using button center coords as basis
    int16_t x0 = 280 - 24;  
    int16_t x1 = x0;
    int16_t x2 = x0 + 24;
    int16_t y0 = 210 + 12;
    int16_t y1 = y0 - 24;
    int16_t y2 = y0 - 12;
    
    // Draw skip button triangles after drawButtons so it will be on top
    lcdCtrl.fillTriangle (x0, y0, 
                          x1, y1, 
                          x2, y2, ILI9341_WHITE);
    lcdCtrl.fillTriangle (x0 + 24, y0, 
                          x1 + 24, y1, 
                          x2 + 24, y2, ILI9341_WHITE);
    
    // Draw Volume Up Button
    x1 = 120;
    y1 = 210;
    int16_t w1 = 10;
    int16_t h1 = 46;
    int16_t w2 = 46;
    int16_t h2 = 10;
    int16_t r = 5;    
    
    // Draw + button 
    lcdCtrl.fillRoundRect(x1-5, y1-23, w1, h1, r, ILI9341_WHITE);
    lcdCtrl.fillRoundRect(x1-23, y1-5, w2, h2, r, ILI9341_WHITE);
    // Draw - button
    lcdCtrl.fillRoundRect(x1-(23+80), y1-5, w2, h2, r, ILI9341_WHITE);
    
}

static void InitButtons() 
{
    char play_pause_text[6] = "Play";
    Mp3_Ctrl mp3_control_state = Mp3GetControl();
    
    // Play Pause text will be reversed so that Play shows when paused and Pause shows when playing
    if(mp3_control_state == Mp3_Play){
        strcpy(play_pause_text, "Pause");
    } else if (mp3_control_state == Mp3_Pause) {
        strcpy(play_pause_text, "Play");
    }
    
    // All of these buttons are the same color as the background, so they are invisible
    skip_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-40, ILI9341_TFTWIDTH-30, // x, y center of button
                       //320-40, 240-30,
                       75, 55,          // Width, Height
                       ILI9341_BLACK,  // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_BLACK,  // text color
                       "Skip",          // button label
                       2);              // text size
    
    // Create button to Play/Pause Mp3
    play_pause_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-120, ILI9341_TFTWIDTH-30, // x, y center of button
                       //320-120, 240-30
                       75, 55,          // Width, Height
                       ILI9341_BLACK,  // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_BLACK,  // text color
                       play_pause_text, // button label
                       2);              // text size
    
    // Create button to increase Mp3 volume
    volup_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-200, ILI9341_TFTWIDTH-30, // x, y center of button
                       75, 55,          // Width, Height
                       ILI9341_BLACK,   // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_BLACK,   // text color
                       "Vol +",         // button label
                       2);              // text size
    
    // Create button to reduce Mp3 volume
    voldn_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-280, ILI9341_TFTWIDTH-30, // x, y center of button
                       75, 55,          // Width, Height
                       ILI9341_BLACK,   // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_BLACK,   // text color
                       "Vol -",         // button label
                       2);              // text size

    // Test button
    test_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-40, ILI9341_TFTWIDTH-200, // x, y center of button
                       75, 55,          // Width, Height
                       ILI9341_YELLOW,   // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_YELLOW,   // text color
                       "Test",         // button label
                       2);              // text size
}

static void DrawLcdContents(char *buffer)
{    
    char buf[BUFSIZE];
    
    OS_CPU_SR cpu_sr;
    
    // allow slow lower pri drawing operation to finish without preemption
    OS_ENTER_CRITICAL(); 
    lcdCtrl.setRotation(1);  // rotate screen 90 deg CW (thick bar on right)
    // this call has the undsired side effect of stopping audio while the screen fills
    lcdCtrl.fillScreen(ILI9341_BLACK);
    
    // Print a message on the LCD
    lcdCtrl.setCursor(40, 60);
    lcdCtrl.setTextColor(ILI9341_WHITE);  
    lcdCtrl.setTextSize(2);
    if (buffer == NULL){
        PrintToLcdWithBuf(buf, BUFSIZE, "EMBSYS 320");
        lcdCtrl.setCursor(40, 80);
        PrintToLcdWithBuf(buf, BUFSIZE, "MP3 Player");
    } else {
        PrintToLcdWithBuf(buf, BUFSIZE, buffer);
    }
    
    // This draws the touchable button shapes   
    drawButtons();  
    drawPlayPause();
    
    OS_EXIT_CRITICAL();

}

/************************************************************************************

   Runs Control code

************************************************************************************/
void ControlTask(void* pdata)
{

}

/************************************************************************************

   Runs LCD/Touch demo code

************************************************************************************/
void LcdTouchDemoTask(void* pdata)
{
    PjdfErrCode pjdfErr;
    INT32U length;
    INT8U err;
    char temp[50];
    char temp2[50];
    char temp3[50];
    char *lastsong = temp;
    char *currPlaying = temp2;
    char *mp3MsgRx = temp3;
    
    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "LcdTouchDemoTask: starting\n");

    PrintWithBuf(buf, BUFSIZE, "Opening LCD driver: %s\n", PJDF_DEVICE_ID_LCD_ILI9341);
    // Open handle to the LCD driver
    HANDLE hLcd = Open(PJDF_DEVICE_ID_LCD_ILI9341, 0);
    if (!PJDF_IS_VALID_HANDLE(hLcd)) while(1);

	PrintWithBuf(buf, BUFSIZE, "Opening LCD SPI driver: %s\n", LCD_SPI_DEVICE_ID);
    // We talk to the LCD controller over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to 
    // the LCD driver.
    HANDLE hSPI = Open(LCD_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hLcd, PJDF_CTRL_LCD_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    PrintWithBuf(buf, BUFSIZE, "Initializing LCD controller\n");
    lcdCtrl.setPjdfHandle(hLcd);
    lcdCtrl.begin();

    // Receive a message in msgReceived from mbox_touch
    // Make sure to use timeout or it will wait here for the message to arrive
    currPlaying = (char*)OSMboxPend(mbox_mp3, 50, &err);
    PrintWithBuf(buf, BUFSIZE, "Mp3 Message Rx: %s\n", currPlaying);
    
    InitButtons();
    DrawLcdContents(currPlaying);
    
    PrintWithBuf(buf, BUFSIZE, "Initializing FT6206 touchscreen controller\n");
    
    // DRIVER TODO - RJH
    // Open a HANDLE for accessing device PJDF_DEVICE_ID_I2C1
    HANDLE hI2C = Open(PJDF_DEVICE_ID_I2C1, 0);
    if (!PJDF_IS_VALID_HANDLE(hI2C)) while(1);
    // Call Ioctl on that handle to set the I2C device address to FT6206_ADDR
    int arg[] = {FT6206_ADDR};
    pjdfErr = Ioctl(hI2C, PJDF_CTRL_I2C_SET_DEVICE_ADDRESS, &arg, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);
    // Call setPjdfHandle() on the touch contoller to pass in the I2C handle
    //PrintWithBuf(buf, BUFSIZE, "Initializing Touch I2C controller\n");
    touchCtrl.setPjdfHandle(hI2C);
    
    if (! touchCtrl.begin(40)) {  // pass in 'sensitivity' coefficient
        PrintWithBuf(buf, BUFSIZE, "Couldn't start FT6206 touchscreen controller\n");
        while (1);
    }
      
    // Color of touch point when enabled
    //int currentcolor = ILI9341_RED;
    
    // Touch detection loop
    while (1) { 
        boolean touched;
        
        strcpy(lastsong, currPlaying);
        // Receive a message in msgReceived from mbox_touch
        // Make sure to use timeout or it will wait here for the message to arrive
        mp3MsgRx = (char*)OSMboxPend(mbox_mp3, 50, &err);
        // Make sure there was a message received and that we did not timeout
        if (err == OS_ERR_NONE) {
            strcpy(currPlaying, mp3MsgRx);
            DrawLcdContents(currPlaying);
            PrintWithBuf(buf, BUFSIZE, "Mp3 Message Rx: %s\n", currPlaying);
        }
        // This loop could run pretty fast, so add a little time here to do other things
        OSTimeDly(20); 
        
        touched = touchCtrl.touched();
        if (! touched) {
            OSTimeDly(5);
            continue;
        }
        
        TS_Point point;
        
        point = touchCtrl.getPoint();
        if (point.x == 0 && point.y == 0)
        {
            continue; // usually spurious, so ignore
        }
        
        // transform touch orientation to screen orientation.
        TS_Point p = TS_Point();
        
        // Portrait mode rotation 0deg
        //p.x = MapTouchToScreen(point.x, 0, ILI9341_TFTWIDTH, ILI9341_TFTWIDTH, 0);
        //p.y = MapTouchToScreen(point.y, 0, ILI9341_TFTHEIGHT, ILI9341_TFTHEIGHT, 0);
        
        // Landscape mode rotated 90deg CCW
        p.x = ILI9341_TFTHEIGHT - MapTouchToScreen(point.y, 0, ILI9341_TFTHEIGHT, 0, ILI9341_TFTHEIGHT);
        p.y = MapTouchToScreen(point.x, 0, ILI9341_TFTWIDTH, 0, ILI9341_TFTWIDTH);
        
        // Draw touched points on the screen
        // lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
        
        touched = touchCtrl.touched();
        
        if (skip_btn.contains(p.x, p.y)) {
            // clear screen and start over
            Mp3SetControl(Mp3_Skip);
            OSTimeDly(200); // need enough time to debounce   
        }
        
        if (volup_btn.contains(p.x, p.y)) {
            // make sure we are not trying to change volume when it is out of range 0 - 125
            if (mp3vol > 125) {
                mp3vol = 125;
            }
            // Max volume is 0, min volume is 100
            if (mp3vol > 5) {
                mp3vol -= 5;
                Mp3SetVolume(mp3vol, mp3vol);
            } else {
                mp3vol = 0;
                Mp3SetVolume(mp3vol, mp3vol);
            }
            
            // Mailbox mbox_touch: send msg to MP3 using mailbox mbox_touch
            //OSMboxPost(mbox_touch, (void *)&cmd);
            //PrintWithBuf(buf, BUFSIZE, "Touch:Mp3 Message sent Mp3Vol_Up: %d\n", cmd);
            //err = OS_ERR_NONE;
                        
            OSTimeDly(90);    
        }
        
        if (voldn_btn.contains(p.x, p.y)) {
            // make sure we are not trying to change volume when it is out of range 0 - 100
            if (mp3vol < 125) { // no reason to use volumes between 100 and 254 through headphones you cannot hear it
                mp3vol += 5;
                Mp3SetVolume(mp3vol, mp3vol);
            } else {
                mp3vol = 125;  
                Mp3SetVolume(mp3vol, mp3vol);
            }
            
            OSTimeDly(90);    
        }
                
        if (play_pause_btn.contains(p.x, p.y)) {
            Mp3_Ctrl mp3_current_state = Mp3GetControl();
            
            if (mp3_current_state == Mp3_Play) {
                Mp3SetControl(Mp3_Pause);
                
            } else { 
                Mp3SetControl(Mp3_Play);
            }
            // refresh play pause button on the screen and keep current song on the screen
            //DrawLcdContents(currPlaying);
            drawPlayPause();
            
            OSTimeDly(200); // need enough time to debounce   
        }    
        
        if (test_btn.contains(p.x, p.y)) {
            lcdCtrl.setCursor(40, 60);
            lcdCtrl.setTextColor(ILI9341_BLUE);  
            lcdCtrl.setTextSize(2);
    
            PrintToLcdWithBuf(buf, BUFSIZE, currPlaying);
    
        }
        
        
    }
}
/************************************************************************************

   Runs MP3 demo code

************************************************************************************/
void Mp3DemoTask(void* pdata)
{
    PjdfErrCode pjdfErr;
    INT32U length;
    
    INT8U err;
    uint8_t msg = Mp3InvalidCmd;
    uint8_t *msgReceived = &msg;
    
    // Mp3 Initialization
    char buf[BUFSIZE];
    PrintWithBuf(buf, BUFSIZE, "Mp3DemoTask: starting\n");

    PrintWithBuf(buf, BUFSIZE, "Opening MP3 driver: %s\n", PJDF_DEVICE_ID_MP3_VS1053);
    // Open handle to the MP3 decoder driver
    HANDLE hMp3 = Open(PJDF_DEVICE_ID_MP3_VS1053, 0);
    if (!PJDF_IS_VALID_HANDLE(hMp3)) while(1);

	PrintWithBuf(buf, BUFSIZE, "Opening MP3 SPI driver: %s\n", MP3_SPI_DEVICE_ID);
    // We talk to the MP3 decoder over a SPI interface therefore
    // open an instance of that SPI driver and pass the handle to 
    // the MP3 driver.
    HANDLE hSPI = Open(MP3_SPI_DEVICE_ID, 0);
    if (!PJDF_IS_VALID_HANDLE(hSPI)) while(1);

    length = sizeof(HANDLE);
    pjdfErr = Ioctl(hMp3, PJDF_CTRL_MP3_SET_SPI_HANDLE, &hSPI, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);

    // Send initialization data to the MP3 decoder and run a test
	PrintWithBuf(buf, BUFSIZE, "Starting MP3 device test\n");
    Mp3Init(hMp3);
    
    ///////////////////////////////////////////////////////////////////////////
    // Mp3 Playback
    uint8_t read_buff[10];
    Mp3ReadVol(hMp3, read_buff, mp3vol);
    mp3vol = read_buff[2]; // both ears have the same volume level
#if !train   
    // Demo using mp3 files from SD card 
    File dir = SD.open("/");
    
    //uint16_t vol = 50;
    
    // uint8_t Mp3GetStatus[10];
    msg = 0;
    
    while (1)
    {
        //Mp3ReadStatus(hMp3, Mp3GetStatus);
        //PrintWithBuf(buf, BUFSIZE, "Mp3ReadStatus = %X %X %X %X\n", Mp3GetStatus[0], Mp3GetStatus[1], Mp3GetStatus[2], Mp3GetStatus[3]); 
                
        char filename[13];
        int count = 0;
        while (1)
        {
            /////////////////////////////////
            // Search SD card for next MP3
            /////////////////////////////////
            File entry = dir.openNextFile();
            if (!entry)
            {
                break;
            }
            if (entry.isDirectory())  // skip directories
            {
                entry.close();
                continue;
            }
            
            PrintWithBuf(buf, BUFSIZE, "Now Playing: %s\n", entry.name());
            
            count++;
            // Only 8.3 file names supported by SD library            
            memcpy(filename, entry.name(), 13);
            char longfilename[50] = {0};
            if (!strcmp(filename, "HOUNDDOG.MP3")) {
                strcpy(longfilename, "Hound Dog");
            } else if(!strcmp(filename, "REBELRSR.MP3")) {
                strcpy(longfilename, "Rebel Rouser");
            } else if(!strcmp(filename, "BUTIDO.MP3")) {
                strcpy(longfilename, "(I Don't Know Why) But I Do");
            } else if(!strcmp(filename, "WALKRTIN.MP3")) {
                strcpy(longfilename, "Walk Right In");
            } else if(!strcmp(filename, "LANDDNCS.MP3")) {
                strcpy(longfilename, "Land Of 1000 Dance");
            } else if(!strcmp(filename, "BLWNWIND.MP3")) {
                strcpy(longfilename, "Blowin' In The Wind");
            } else if(!strcmp(filename, "FORTSON.MP3")) {
                strcpy(longfilename, "Fortunate Son");
            } else if(!strcmp(filename, "SUGARPIE.MP3")) {
                strcpy(longfilename, "I Can't Help Myself");
            } else if(!strcmp(filename, "RESPECT.MP3")) {
                strcpy(longfilename, "Respect");
            } else if(!strcmp(filename, "RAINYDAY.MP3")) {
                strcpy(longfilename, "Rainy Day Women");
            } else if(!strcmp(filename, "SLOOPJHN.MP3")) {
                strcpy(longfilename, "Sloop John B");
            } else if(!strcmp(filename, "CALIDRMN.MP3")) {
                strcpy(longfilename, "California Dreamin");
            } else if(!strcmp(filename, "4WHATW.MP3")) {
                strcpy(longfilename, "For What It's Worth");
            } else if(!strcmp(filename, "NOWISLOV.MP3")) {
                strcpy(longfilename, "What The World Needs Now Is Love");
            } else if(!strcmp(filename, "BRAKONTH.MP3")) {
                strcpy(longfilename, "Break On Through");
            } else if(!strcmp(filename, "MRSROBSN.MP3")) {
                strcpy(longfilename, "Mrs. Robinson");
            } else if(!strcmp(filename, "SWTHMALA.MP3")) {
                strcpy(longfilename, "Sweet Home Alabama");
            } else if(!strcmp(filename, "JOY2WRLD.MP3")) {
                strcpy(longfilename, "Joy To The World");
            } else {
                strcpy(longfilename, "File Missing from long name data");
            }
            
            // song title sending message through mbox
            // Mailbox mbox_mp3: send msg to touch using mailbox mbox_mp3
            OSMboxPost(mbox_mp3, (void *)&longfilename);
            PrintWithBuf(buf, BUFSIZE, "Mp3:Touch Message sent filename: %d-%s\n", count, filename);
            
            OSTimeDly(50);
            
            // control functions need to have a way to get done in this function or else they will not get done
            Mp3StreamSDFile(hMp3, entry.name()); 
                        
            entry.close();
            
            ///////////////////////////////////////////////////////////////////
            // Listen for commands from touch task
            ///////////////////////////////////////////////////////////////////
            
            // Receive a message in msgReceived from mbox_touch
            // Make sure to use timeout or it will wait here for the message to arrive
            msgReceived = (uint8_t*)OSMboxPend(mbox_touch, 50, &err);
            PrintWithBuf(buf, BUFSIZE, "Touch:Mp3 Message received: %d\n", *msgReceived);
        }
        dir.seek(0); // reset directory file to read again;
    }
#else
//    // simple mp3 demo using train_crossing mp3
//    //int count = 0;
//    uint16_t vol = 254;
//    // { sci write reg, sci mode reg, left vol, right vol }
//    uint16_t Mp3SetVol[] = { 0x02, 0x0B, 0x10, 0x10 };
//    uint32_t Mp3SetVolLen = sizeof(Mp3SetVol);
//    
//    uint8_t Mp3GetStatus[10];
//    
//    while (1)
//    {
//        
//        Mp3ReadStatus(hMp3, Mp3GetStatus);
//        PrintWithBuf(buf, BUFSIZE, "Mp3ReadStatus = %X %X %X %X\n", Mp3GetStatus[0], Mp3GetStatus[1], Mp3GetStatus[2], Mp3GetStatus[3]); 
//        // Receive a message in msgReceived from mbox_touch
//        // Make sure to use timeout or it will wait here for the message to arrive
//        msgReceived = (uint8_t*)OSMboxPend(mbox_touch, 50, &err);
//        uint8_t read_buff[10];
//        if (*msgReceived == Mp3Vol_Up){
//            // Now get the volume setting on the device            
//            Mp3ReadVol(hMp3, read_buff, vol);
//            // make sure we are not trying to change volume when it is out of range 0 - 100
//            if (vol > 100) {
//                vol = 100;
//            }
//            // Max volume is 0, min volume is 100
//            if (vol > 15) {
//                vol -= 15;
//                // use same volume in both sides
//                Mp3SetVol[2] = vol;
//                Mp3SetVol[3] = vol;
//                // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
//                Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);                     
//                // Set volume                       
//                Write(hMp3, (void*)Mp3SetVol, &Mp3SetVolLen);
//                Mp3ReadVol(hMp3, read_buff, vol);
//            } else {
//                vol = 0;
//            }
//            
//            PrintWithBuf(buf, BUFSIZE, "Mp3Vol_Up Vol = %d\n", vol);             
//        } else if (*msgReceived == Mp3Vol_Down){
//            if (vol < 100) { // no reason to use volumes between 100 and 254 through headphones you cannot hear it
//                vol += 15;
//                // use same volume in both sides
//                Mp3SetVol[2] = vol;
//                Mp3SetVol[3] = vol;
//                // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
//                Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);                     
//                // Set volume                       
//                Write(hMp3, (void*)Mp3SetVol, &Mp3SetVolLen);
//                Mp3ReadVol(hMp3, read_buff, vol);
//            }
//            
//            PrintWithBuf(buf, BUFSIZE, "Mp3Vol_Down Vol = %d\n", vol);
//        } else if (*msgReceived == Mp3_Stop){
//            // Stop playing the Mp3
//            uint32_t length = BspMp3DeactLen;
//            //Write(hMp3, (void*)BspMp3Deact, &length);
//            Mp3SoftReset(hMp3);
//            
//            PrintWithBuf(buf, BUFSIZE, "Mp3_Stop received: %d\n", *msgReceived);
//        } else if (*msgReceived >= Mp3InvalidCmd){
//            PrintWithBuf(buf, BUFSIZE, "MP3 Queue: Invalid message received msg %d\n", *msgReceived);
//        }
//        
//        //PrintWithBuf(buf, BUFSIZE, "Begin streaming sound file  count=%d\n", ++count);
//        Mp3Stream(hMp3, (INT8U*)Train_Crossing, sizeof(Train_Crossing)); 
//        //PrintWithBuf(buf, BUFSIZE, "Done streaming sound file  count=%d\n", count);
//        
//        OSTimeDly(50);
//        
//    }
#endif // train
}

// Renders a character at the current cursor position on the LCD
static void PrintCharToLcd(char c)
{
    lcdCtrl.write(c);
}

/************************************************************************************

   Print a formated string with the given buffer to LCD.
   Each task should use its own buffer to prevent data corruption.

************************************************************************************/
void PrintToLcdWithBuf(char *buf, int size, char *format, ...)
{
    va_list args;
    va_start(args, format);
    PrintToDeviceWithBuf(PrintCharToLcd, buf, size, format, args);
    va_end(args);
}

//// Read vs1053 volume register
//// handle Mp3 handle
//// buffer used to store register read results
//// vol current volume setting to compare against, assumes both left and right are using same volume
//void mp3ReadVol(HANDLE handle, uint8_t buffer[10], uint8_t vol){
//    // Now get the volume setting on the device
//    memcpy(buffer, BspMp3ReadVol, BspMp3ReadVolLen); // copy command from flash to a ram buffer
//    Mp3GetRegister(handle, buffer, BspMp3ReadVolLen);
//    
//    // buffer[2] is left and buffer[3] is right volume
//    if (buffer[2] != vol || buffer[3] != vol)
//    {
//        //while(1); // failed to get data back from the device
//    }
//}

