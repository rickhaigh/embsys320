/*
    mp3Util.h
    Some utility functions for controlling the MP3 decoder.

    Developed for University of Washington embedded systems programming certificate
    
    2016/2 Nick Strathy wrote/arranged it
*/

#ifndef __MP3UTIL_H
#define __MP3UTIL_H

/* SCI registers */
#define SCI_MODE        0x00
#define SCI_STATUS      0x01
#define SCI_BASS        0x02
#define SCI_CLOCKF      0x03
#define SCI_DECODE_TIME 0x04
#define SCI_AUDATA      0x05
#define SCI_WRAM        0x06
#define SCI_WRAMADDR    0x07
#define SCI_HDAT0       0x08 /* VS1063, VS1053, VS1033, VS1003, VS1011 */
#define SCI_HDAT1       0x09 /* VS1063, VS1053, VS1033, VS1003, VS1011 */
#define SCI_AIADDR      0x0A
#define SCI_VOL         0x0B
#define SCI_AICTRL0     0x0C /* VS1063, VS1053, VS1033, VS1003, VS1011 */
#define SCI_AICTRL1     0x0D /* VS1063, VS1053, VS1033, VS1003, VS1011 */
#define SCI_AICTRL2     0x0E
#define SCI_AICTRL3     0x0F

/* SCI_MODE bits */
#define SM_DIFF           (1<< 0)
#define SM_LAYER12        (1<< 1) /* VS1063, VS1053, VS1033, VS1011 */
#define SM_RESET          (1<< 2)
#define SM_CANCEL         (1<< 3) /* VS1063, VS1053 */
#define SM_EARSPEAKER_LO  (1<< 4) /* VS1053, VS1033 */
#define SM_TESTS          (1<< 5)
#define SM_STREAM         (1<< 6) /* VS1053, VS1033, VS1003, VS1011 */
#define SM_EARSPEAKER_HI  (1<< 7) /* VS1053, VS1033 */
#define SM_DACT           (1<< 8)
#define SM_SDIORD         (1<< 9)
#define SM_SDISHARE       (1<<10)
#define SM_SDINEW         (1<<11)
#define SM_ADPCM          (1<<12) /* VS1053, VS1033, VS1003 */
#define SM_LINE1          (1<<14) /* VS1063, VS1053 */
#define SM_CLK_RANGE      (1<<15) /* VS1063, VS1053, VS1033 */

// for MP3 Control
typedef enum mp3_ctrl{
    Mp3Vol_Up = 1,
    Mp3Vol_Down,
    Mp3_Skip,
    Mp3_Play,
    Mp3_Pause,
    Mp3_Stop,
    Mp3InvalidCmd,
    Mp3_End_of_List
} Mp3_Ctrl;

PjdfErrCode Mp3GetRegister(HANDLE hMp3, INT8U *cmdInDataOut, INT32U bufLen);
void Mp3Init(HANDLE hMp3);
void Mp3Test(HANDLE hMp3);
void Mp3Stream(HANDLE hMp3, INT8U *pBuf, INT32U bufLen);
void Mp3StreamSDFile(HANDLE hMp3, char *pFilename);
void Mp3ReadStatus(HANDLE hMp3, uint8_t *buffer);
void Mp3ReadVol(HANDLE hMp3, uint8_t *buffer);
void Mp3SoftReset(HANDLE hMp3);

void Mp3SetVolume(uint8_t leftchannel, uint8_t rightchannel);

#endif