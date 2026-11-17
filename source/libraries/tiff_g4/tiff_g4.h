// CCITT G4 (T.6) decompressor - Cross-Platform Version
// SPDX-FileCopyrightText: 2020 Larry Bank <bitbank@pobox.com>
// SPDX-License-Identifier: Apache-2.0

/* 
    This is a stripped and slightly reformatted version. We basically only need the fax 
    decompression as part of some pdf juggling and we read the tags using \LUA. So, the 
    compiler will ignore what we don't need and keep things mimimalistic. (HH)
*/

# ifndef __TIFFG4__
# define __TIFFG4__

# include <stdlib.h>
# include <string.h>
# include <stdint.h>
# include <stdio.h>

/* POSIX / Windows / MinGW / OSX compatibility macros: */

# define memcpy_P memcpy

# define REENTRANT
# define PROGMEM
# define pgm_read_byte(p) (*(const uint8_t *)(p))
# define pgm_read_word(p) (*(const int16_t *)(p))

/* Standard desktop/server buffer sizes and limits */

# define MAX_BUFFERED_PIXELS 2048
# define TIFF_FILE_BUF_SIZE  8192
# define MAX_IMAGE_WIDTH     8192
# define FILE_HIGHWATER      ((TIFF_FILE_BUF_SIZE * 3) >> 2)
# define BITDIR_MSB_FIRST       1
# define BITDIR_LSB_FIRST       2

// Error codes returned by getLastError()
enum {
    TIFF_SUCCESS = 0,
    TIFF_INVALID_PARAMETER,
    TIFF_DECODE_ERROR,
    TIFF_UNSUPPORTED_FEATURE,
    TIFF_INVALID_FILE,
    TIFF_NEED_MORE_DATA,
    TIFF_TOO_WIDE
};

// Output pixel types
enum {
    TIFF_PIXEL_1BPP = 0,
    TIFF_PIXEL_2BPP,
    TIFF_PIXEL_4BPP,
    TIFF_PIXEL_16BPP
};

typedef struct tiff_file_tag
{
  int32_t  iPos;    // current file position
  int32_t  iSize;   // file size
  uint8_t *pData;   // memory file pointer
} TIFFFILE;

typedef struct tiff_window_tag
{
    int      x, y;            // upper left corner of interest (source pixels)
    int      dstx, dsty;      // destination on output
    float    fScale;
    uint32_t iScale;          // 16:16 fixed scale factor
    int      iWidth, iHeight; // destination window size
    uint8_t *p4BPP;           // user-supplied buffer for 4-bpp grayscale output
    uint8_t  ucPixelType;
} TIFFWINDOW;

typedef struct tiff_draw_tag
{
    int      y;                           // current line
    int      iScaledWidth, iScaledHeight; // width & height of scaled region
    int      iWidth, iHeight;             // size of entire image in pixels
    int      iDestX, iDestY;              // destination coordinates on output
    void    *pUser;                       // user pointer
    uint8_t *pPixels;                     // 1, 2 or 16-bit pixels
    uint8_t  ucPixelType, ucLast;
} TIFFDRAW;

// Callback function prototypes

typedef int32_t  (TIFF_READ_CALLBACK ) (TIFFFILE *pFile, uint8_t *pBuf, int32_t iLen);
typedef int32_t  (TIFF_SEEK_CALLBACK ) (TIFFFILE *pFile, int32_t iPosition);
typedef void     (TIFF_DRAW_CALLBACK ) (TIFFDRAW *pDraw);

typedef struct tiff_image_tag
{
    int                  iWidth, iHeight;               // image size
    int                  iError;
    int                  y;                             // last y value drawn
    int                  iVLCOff, iVLCSize;
    int                  iPitch;                        // width in bytes of output buffer
    uint32_t             u32Accum;                      // fractional scaling accumulator
    uint32_t             ulBitOff, ulBits;              // vlc decode variables
    uint8_t             *pBuf;                          // current buffer pointer
    uint8_t              ucFillOrder;
    TIFF_READ_CALLBACK  *pfnRead;
    TIFF_SEEK_CALLBACK  *pfnSeek;
    TIFF_DRAW_CALLBACK  *pfnDraw;
    TIFFFILE             TIFFFile;
    TIFFWINDOW           window;
    void                *pUser;
    int16_t             *pCur, *pRef;                   // current state of current vs reference flips
    int16_t              CurFlips[MAX_IMAGE_WIDTH];
    int16_t              RefFlips[MAX_IMAGE_WIDTH];
    uint8_t              ucPixels[MAX_BUFFERED_PIXELS];
    uint8_t              ucFileBuf[TIFF_FILE_BUF_SIZE]; // holds temp data and pixel stack
} TIFFIMAGE;

int  TIFF_openRAW           (TIFFIMAGE *pImage, int iWidth, int iHeight, int iFillOrder, uint8_t *pData, int iDataSize, TIFF_DRAW_CALLBACK *pfnDraw);
int  TIFF_decode            (TIFFIMAGE *pImage);
int  TIFF_getLastError      (TIFFIMAGE *pImage);

// Endianness helper and macros

# define TIFFMOTOLONG(p)   (((uint32_t)(*p)<<24UL) + ((uint32_t)(*(p+1))<<16UL) + ((uint32_t)(*(p+2))<<8UL) + (uint32_t)(*(p+3)))
# define TOP_BIT           0x80000000
# define LONGWHITECODEMASK 0x2000000
# define LONGBLACKCODEMASK 0x10000000
# define REGISTER_WIDTH    32

# define CLIMBWHITE_NEW(pBuf, ulBitOff, ulBits, sCode) \
    { uint32_t ul; int iLen = 64; sCode = 0; while (iLen > 63) \
    { if (ulBitOff > (REGISTER_WIDTH-17)) \
      { pBuf += (ulBitOff>>3); ulBitOff &= 7; ulBits = TIFFMOTOLONG(pBuf); } \
      if ((ulBits << ulBitOff) < LONGWHITECODEMASK) \
                  { ul = (ulBits >> ((REGISTER_WIDTH-14) - ulBitOff)) & 0x3fe; ulBitOff += pgm_read_word(&black_l[ul]); iLen = pgm_read_word(&black_l[ul+1]);} \
                else {ul = (ulBits >> ((REGISTER_WIDTH - 10) - ulBitOff)) & 0x3fe; ulBitOff += pgm_read_word(&white_s[ul]); iLen = pgm_read_word(&white_s[ul+1]);} \
     sCode += iLen; }}

# define CLIMBBLACK_NEW(pBuf, ulBitOff, ulBits, sCode) \
    { uint32_t ul; int iLen = 64; sCode = 0; while (iLen > 63) \
    { if (ulBitOff > (REGISTER_WIDTH-15)) \
      { pBuf += (ulBitOff>>3); ulBitOff &= 7; ulBits = TIFFMOTOLONG(pBuf); } \
      if ((ulBits << ulBitOff) < LONGBLACKCODEMASK) \
         { ul = (ulBits >> ((REGISTER_WIDTH-14) - ulBitOff)) & 0x3fe; ulBitOff += pgm_read_word(&black_l[ul]); iLen = pgm_read_word(&black_l[ul+1]);} \
        else {ul = (ulBits >> ((REGISTER_WIDTH - 7) - ulBitOff)) & 0x7e; ulBitOff += pgm_read_word(&black_s[ul]); iLen = pgm_read_word(&black_s[ul+1]);} \
     sCode += iLen; }}

# endif
