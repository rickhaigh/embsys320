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

// Defines to use with queue for MP3 Control
typedef enum Mp3_Ctrl{
    Mp3Vol_Up = 1,
    Mp3Vol_Down,
    Mp3_Skip,
    Mp3_Play,
    Mp3_Pause,
    Mp3_Stop,
    Mp3InvalidCmd,
    Mp3_End_of_List
} Mp3_Ctrl;

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
INT32U touch_msgCount = 0;
OS_EVENT *mbox_touch;

Adafruit_ILI9341 lcdCtrl = Adafruit_ILI9341(); // The LCD controller
Adafruit_FT6206 touchCtrl = Adafruit_FT6206(); // The touch controller

//void mp3ReadVol(HANDLE handle, uint8_t buffer[10], uint8_t vol);

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
     
// Task prototypes
void LcdTouchDemoTask(void* pdata);
void Mp3DemoTask(void* pdata);

// Useful functions
void PrintToLcdWithBuf(char *buf, int size, char *format, ...);

// Globals
BOOLEAN nextSong = OS_FALSE;

/************************************************************************************

   This task is the initial task running, started by main(). It starts
   the system tick timer and creates all the other tasks. Then it deletes itself.

************************************************************************************/
void StartupTask(void* pdata)
{
	char buf[BUFSIZE];
    uint8_t err;
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
    
    // code to create 1 mailbox mbox_touch, initially empty
    mbox_touch = OSMboxCreate(NULL);
    
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

    // Delete ourselves, letting the work be done in the new tasks.
    PrintWithBuf(buf, BUFSIZE, "StartupTask: deleting self\n");
	OSTaskDel(OS_PRIO_SELF);
}

static void DrawLcdContents()
{    
    char buf[BUFSIZE];
    //char *line1 = "EMBSYS 320";
    //char *line2 = "MP3 Player";
    // With textsize = 2
    // Letter box height = 14, width = 10, spacing = 2
    //int offsetx1 = strlen(line1) * 14;
    //int offsetx2 = strlen(line2) * 14;
    //uint8_t posx1 = (240 / 2) - ((strlen(line1) * 14) / 2);
    //uint8_t posx2 = (240 / 2) - ((strlen(line2) * 14) / 2);
    
    OS_CPU_SR cpu_sr;
    
    // allow slow lower pri drawing operation to finish without preemption
    OS_ENTER_CRITICAL(); 
    lcdCtrl.setRotation(1);  // rotate screen 90 deg CW (thick bar on right)
    lcdCtrl.fillScreen(ILI9341_BLACK);
    
    // Print a message on the LCD
    lcdCtrl.setCursor(40, 60);
    lcdCtrl.setTextColor(ILI9341_WHITE);  
    lcdCtrl.setTextSize(2);
    PrintToLcdWithBuf(buf, BUFSIZE, "EMBSYS 320");
    
    lcdCtrl.setCursor(40, 80);
    PrintToLcdWithBuf(buf, BUFSIZE, "MP3 Player");
    
    //lcdCtrl.drawPixel(120,160, ILI9341_GREEN); 
    //lcdCtrl.drawCircle(120,160,30, ILI9341_GREEN);
    // lcdCtrl.drawRoundRect(10, 200, 50, 30, 5, ILI9341_GREEN);
    
    OS_EXIT_CRITICAL();

}

/************************************************************************************

   Runs LCD/Touch demo code

************************************************************************************/
void LcdTouchDemoTask(void* pdata)
{
    PjdfErrCode pjdfErr;
    INT32U length;
    
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

    DrawLcdContents();
    
    PrintWithBuf(buf, BUFSIZE, "Initializing FT6206 touchscreen controller\n");
    
    // DRIVER TODO - RJH
    // Open a HANDLE for accessing device PJDF_DEVICE_ID_I2C1
    // <your code here>
    HANDLE hI2C = Open(PJDF_DEVICE_ID_I2C1, 0);
    if (!PJDF_IS_VALID_HANDLE(hI2C)) while(1);
    // Call Ioctl on that handle to set the I2C device address to FT6206_ADDR
    // <your code here>
    int arg[] = {FT6206_ADDR};
    pjdfErr = Ioctl(hI2C, PJDF_CTRL_I2C_SET_DEVICE_ADDRESS, &arg, &length);
    if(PJDF_IS_ERROR(pjdfErr)) while(1);
    // Call setPjdfHandle() on the touch contoller to pass in the I2C handle
    // <your code here>
    PrintWithBuf(buf, BUFSIZE, "Initializing Touch I2C controller\n");
    touchCtrl.setPjdfHandle(hI2C);
    
    if (! touchCtrl.begin(40)) {  // pass in 'sensitivity' coefficient
        PrintWithBuf(buf, BUFSIZE, "Couldn't start FT6206 touchscreen controller\n");
        while (1);
    }
    // Create button to clear the screen
    Adafruit_GFX_Button button1 = Adafruit_GFX_Button();  // Declare the button
    button1.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-40, ILI9341_TFTWIDTH-30, // x, y center of button
                       //320-40, 240-30,
                       75, 55,          // Width, Height
                       ILI9341_YELLOW,  // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_YELLOW,  // text color
                       "Clear",         // button label
                       1);              // text size
    
    // Create button to stop Mp3
    Adafruit_GFX_Button stop_btn = Adafruit_GFX_Button();  // Declare the button
    stop_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-120, ILI9341_TFTWIDTH-30, // x, y center of button
                       75, 55,          // Width, Height
                       ILI9341_YELLOW,  // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_YELLOW,  // text color
                       "Stop",         // button label
                       2);              // text size
    
    // Create button to reduce Mp3 volume
    Adafruit_GFX_Button voldn_btn = Adafruit_GFX_Button();  // Declare the button
    voldn_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-200, ILI9341_TFTWIDTH-30, // x, y center of button
                       75, 55,          // Width, Height
                       ILI9341_YELLOW,  // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_YELLOW,  // text color
                       "Vol -",         // button label
                       2);              // text size
    
    // Create button to increase Mp3 volume
    Adafruit_GFX_Button volup_btn = Adafruit_GFX_Button();  // Declare the button
    volup_btn.initButton(
                       &lcdCtrl,
                       ILI9341_TFTHEIGHT-280, ILI9341_TFTWIDTH-30, // x, y center of button
                       75, 55,          // Width, Height
                       ILI9341_YELLOW,  // Outline
                       ILI9341_BLACK,   // Fill
                       ILI9341_YELLOW,  // text color
                       "Vol +",         // button label
                       2);              // text size
    
    button1.drawButton(0);
    stop_btn.drawButton(0);
    voldn_btn.drawButton(0);
    volup_btn.drawButton(0);
        
    int currentcolor = ILI9341_RED;
    //int i = 0;  // use to keep track of volume setting
    while (1) { 
        boolean touched;
        
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
        lcdCtrl.fillCircle(p.x, p.y, PENRADIUS, currentcolor);
        
        if (button1.contains(p.x, p.y)) {
            // clear screen and start over
            DrawLcdContents();
            button1.drawButton(0);
            stop_btn.drawButton(0);
            voldn_btn.drawButton(0);
            volup_btn.drawButton(0);
        }
        
        if (volup_btn.contains(p.x, p.y)) {
            uint8_t cmd = Mp3Vol_Up;
            //uint8_t *p_cmd = &cmd;
            
            // Mailbox mbox_touch: send msg to MP3 using mailbox mbox_touch
            OSMboxPost(mbox_touch, (void *)&cmd);
                        
            // Set volume by sending message through queue
            //length = BspMp3SetVol1010Len;
            //Write(hMp3, (void*)BspMp3SetVol1010, &length);
                        
            PrintWithBuf(buf, BUFSIZE, "Touch:Mp3 Message sent Mp3Vol_Up: %d\n", cmd);
            //err = OS_ERR_NONE;
                        
            OSTimeDly(90);    
        }
        
        if (voldn_btn.contains(p.x, p.y)) {
            uint8_t cmd = Mp3Vol_Down;
            // Set volume by sending message through queue
            // Mailbox mbox_touch: send msg to MP3 using mailbox mbox_touch
            OSMboxPost(mbox_touch, (void *)&cmd);
            PrintWithBuf(buf, BUFSIZE, "Touch:Mp3 Message sent Mp3Vol_Down: %d\n", cmd);
            
            OSTimeDly(90);    
        }
                
        if (stop_btn.contains(p.x, p.y)) {
            uint8_t cmd = Mp3_Stop;
            // Set volume by sending message through queue
            // Mailbox mbox_touch: send msg to MP3 using mailbox mbox_touch
            OSMboxPost(mbox_touch, (void *)&cmd);
            
            PrintWithBuf(buf, BUFSIZE, "Mp3 Message sent Mp3_Stop: %d\n", cmd);
                        
            OSTimeDly(90);    
        }    
        
        // PrintWithBuf(buf, BUFSIZE, "input: (%d, %d) :: map:(%d, %d)\n", point.x, point.y, p.x, p.y);
        
        
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

#if !train   
    // Demo using mp3 files from SD card 
    File dir = SD.open("/");
    
    uint16_t vol = 50;
    // { sci write reg, sci mode reg, left vol, right vol }
    uint16_t Mp3SetVol[] = { 0x02, 0x0B, 0x10, 0x10 };
    uint32_t Mp3SetVolLen = sizeof(Mp3SetVol);
    
    uint8_t Mp3GetStatus[10];
    msg = 0;
    
    while (1)
    {
        Mp3ReadStatus(hMp3, Mp3GetStatus);
        PrintWithBuf(buf, BUFSIZE, "Mp3ReadStatus = %X %X %X %X\n", Mp3GetStatus[0], Mp3GetStatus[1], Mp3GetStatus[2], Mp3GetStatus[3]); 
        // Receive a message in msgReceived from mbox_touch
        // Make sure to use timeout or it will wait here for the message to arrive
        msgReceived = (uint8_t*)OSMboxPend(mbox_touch, 50, &err);
        
        uint8_t read_buff[10];
        if (*msgReceived == Mp3Vol_Up){
            // Now get the volume setting on the device            
            Mp3ReadVol(hMp3, read_buff, vol);
            // make sure we are not trying to change volume when it is out of range 0 - 100
            if (vol > 100) {
                vol = 100;
            }
            // Max volume is 0, min volume is 100
            if (vol > 15) {
                vol -= 15;
                // use same volume in both sides
                Mp3SetVol[2] = vol;
                Mp3SetVol[3] = vol;
                // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
                Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);                     
                // Set volume                       
                Write(hMp3, (void*)Mp3SetVol, &Mp3SetVolLen);
                Mp3ReadVol(hMp3, read_buff, vol);
            } else {
                vol = 0;
            }
            
            PrintWithBuf(buf, BUFSIZE, "Mp3Vol_Up Vol = %d\n", vol);             
        } else if (*msgReceived == Mp3Vol_Down){
            if (vol < 100) { // no reason to use volumes between 100 and 254 through headphones you cannot hear it
                vol += 15;
                // use same volume in both sides
                Mp3SetVol[2] = vol;
                Mp3SetVol[3] = vol;
                // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
                Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);                     
                // Set volume                       
                Write(hMp3, (void*)Mp3SetVol, &Mp3SetVolLen);
                Mp3ReadVol(hMp3, read_buff, vol);
            }
            
            PrintWithBuf(buf, BUFSIZE, "Mp3Vol_Down Vol = %d\n", vol);
        } else if (*msgReceived == Mp3_Stop){
            // Stop playing the Mp3
            uint32_t length = BspMp3DeactLen;
            //Write(hMp3, (void*)BspMp3Deact, &length);
            Mp3SoftReset(hMp3);
            
            PrintWithBuf(buf, BUFSIZE, "Mp3_Stop received: %d\n", *msgReceived);
        } else if (*msgReceived >= Mp3InvalidCmd){
            //PrintWithBuf(buf, BUFSIZE, "MP3 Queue: Invalid message received msg %d\n", *msgReceived);
        }
        
        while (1)
        {
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
                            
            Mp3StreamSDFile(hMp3, entry.name()); 
            PrintWithBuf(buf, BUFSIZE, "Now Playing: %s\n", entry.name());
            
            entry.close();
        }
        dir.seek(0); // reset directory file to read again;
    }
#else
    // simple mp3 demo using train_crossing mp3
    //int count = 0;
    uint16_t vol = 254;
    // { sci write reg, sci mode reg, left vol, right vol }
    uint16_t Mp3SetVol[] = { 0x02, 0x0B, 0x10, 0x10 };
    uint32_t Mp3SetVolLen = sizeof(Mp3SetVol);
    
    uint8_t Mp3GetStatus[10];
    
    while (1)
    {
        
        Mp3ReadStatus(hMp3, Mp3GetStatus);
        PrintWithBuf(buf, BUFSIZE, "Mp3ReadStatus = %X %X %X %X\n", Mp3GetStatus[0], Mp3GetStatus[1], Mp3GetStatus[2], Mp3GetStatus[3]); 
        // Receive a message in msgReceived from mbox_touch
        // Make sure to use timeout or it will wait here for the message to arrive
        msgReceived = (uint8_t*)OSMboxPend(mbox_touch, 50, &err);
        uint8_t read_buff[10];
        if (*msgReceived == Mp3Vol_Up){
            // Now get the volume setting on the device            
            Mp3ReadVol(hMp3, read_buff, vol);
            // make sure we are not trying to change volume when it is out of range 0 - 100
            if (vol > 100) {
                vol = 100;
            }
            // Max volume is 0, min volume is 100
            if (vol > 15) {
                vol -= 15;
                // use same volume in both sides
                Mp3SetVol[2] = vol;
                Mp3SetVol[3] = vol;
                // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
                Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);                     
                // Set volume                       
                Write(hMp3, (void*)Mp3SetVol, &Mp3SetVolLen);
                Mp3ReadVol(hMp3, read_buff, vol);
            } else {
                vol = 0;
            }
            
            PrintWithBuf(buf, BUFSIZE, "Mp3Vol_Up Vol = %d\n", vol);             
        } else if (*msgReceived == Mp3Vol_Down){
            if (vol < 100) { // no reason to use volumes between 100 and 254 through headphones you cannot hear it
                vol += 15;
                // use same volume in both sides
                Mp3SetVol[2] = vol;
                Mp3SetVol[3] = vol;
                // Place MP3 driver in command mode (subsequent writes will be sent to the decoder's command interface)
                Ioctl(hMp3, PJDF_CTRL_MP3_SELECT_COMMAND, 0, 0);                     
                // Set volume                       
                Write(hMp3, (void*)Mp3SetVol, &Mp3SetVolLen);
                Mp3ReadVol(hMp3, read_buff, vol);
            }
            
            PrintWithBuf(buf, BUFSIZE, "Mp3Vol_Down Vol = %d\n", vol);
        } else if (*msgReceived == Mp3_Stop){
            // Stop playing the Mp3
            uint32_t length = BspMp3DeactLen;
            //Write(hMp3, (void*)BspMp3Deact, &length);
            Mp3SoftReset(hMp3);
            
            PrintWithBuf(buf, BUFSIZE, "Mp3_Stop received: %d\n", *msgReceived);
        } else if (*msgReceived >= Mp3InvalidCmd){
            PrintWithBuf(buf, BUFSIZE, "MP3 Queue: Invalid message received msg %d\n", *msgReceived);
        }
        
        //PrintWithBuf(buf, BUFSIZE, "Begin streaming sound file  count=%d\n", ++count);
        Mp3Stream(hMp3, (INT8U*)Train_Crossing, sizeof(Train_Crossing)); 
        //PrintWithBuf(buf, BUFSIZE, "Done streaming sound file  count=%d\n", count);
        
        OSTimeDly(50);
        
    }
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

