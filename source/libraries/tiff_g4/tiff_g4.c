//
// TIFF_G4
// A TIFF G4 (T.6) / 1-bpp image library (Stripped Memory-Only RAW Build)
//
// SPDX-FileCopyrightText: 2020 Larry Bank <bitbank@pobox.com>
// SPDX-License-Identifier: Apache-2.0
//

/* 
    This is a stripped and slightly reformatted version. We basically only need the fax 
    decompression as part of some pdf juggling and we read the tags using \LUA. So, the 
    compiler will ignore what we don't need and keep things mimimalistic. (HH)
*/

# define REENTRANT

# include "tiff_g4.h"

// Forward references
static void TIFFGetMoreData(TIFFIMAGE *pPage);
static int Decode(TIFFIMAGE *pImage);

/* Table of byte flip values to mirror-image incoming CCITT data */
static const uint8_t ucMirror[256] PROGMEM = {
    0, 128, 64, 192, 32, 160, 96, 224, 16, 144, 80, 208, 48, 176, 112, 240,
    8, 136, 72, 200, 40, 168, 104, 232, 24, 152, 88, 216, 56, 184, 120, 248,
    4, 132, 68, 196, 36, 164, 100, 228, 20, 148, 84, 212, 52, 180, 116, 244,
    12, 140, 76, 204, 44, 172, 108, 236, 28, 156, 92, 220, 60, 188, 124, 252,
    2, 130, 66, 194, 34, 162, 98, 226, 18, 146, 82, 210, 50, 178, 114, 242,
    10, 138, 74, 202, 42, 170, 106, 234, 26, 154, 90, 218, 58, 186, 122, 250,
    6, 134, 70, 198, 38, 166, 102, 230, 22, 150, 86, 214, 54, 182, 118, 246,
    14, 142, 78, 206, 46, 174, 110, 238, 30, 158, 94, 222, 62, 190, 126, 254,
    1, 129, 65, 193, 33, 161, 97, 225, 17, 145, 81, 209, 49, 177, 113, 241,
    9, 137, 73, 201, 41, 169, 105, 233, 25, 153, 89, 217, 57, 185, 121, 249,
    5, 133, 69, 197, 37, 165, 101, 229, 21, 149, 85, 213, 53, 181, 117, 245,
    13, 141, 77, 205, 45, 173, 109, 237, 29, 157, 93, 221, 61, 189, 125, 253,
    3, 131, 67, 195, 35, 163, 99, 227, 19, 147, 83, 211, 51, 179, 115, 243,
    11, 139, 75, 203, 43, 171, 107, 235, 27, 155, 91, 219, 59, 187, 123, 251,
    7, 135, 71, 199, 39, 167, 103, 231, 23, 151, 87, 215, 55, 183, 119, 247,
    15, 143, 79, 207, 47, 175, 111, 239, 31, 159, 95, 223, 63, 191, 127, 255
};

/*
 The code tree that follows has: bit_length, decode routine
 These codes are for Group 4 (MMR) decoding
*/
static const uint8_t code_table[128] PROGMEM = {
    0x90, 0, 0x40, 0,       /* trash, uncompr mode - codes 0 and 1 */
    3, 7,                   /* V(-3) pos = 2 */
    0x13, 7,                /* V(3)  pos = 3 */
    2, 6, 2, 6,             /* V(-2) pos = 4,5 */
    0x12, 6, 0x12, 6,       /* V(2)  pos = 6,7 */
    0x30, 4, 0x30, 4, 0x30, 4, 0x30, 4,    /* pass  pos = 8->F */
    0x30, 4, 0x30, 4, 0x30, 4, 0x30, 4,
    0x20, 3, 0x20, 3, 0x20, 3, 0x20, 3,    /* horiz pos = 10->1F */
    0x20, 3, 0x20, 3, 0x20, 3, 0x20, 3,
    0x20, 3, 0x20, 3, 0x20, 3, 0x20, 3,
    0x20, 3, 0x20, 3, 0x20, 3, 0x20, 3,
    /* V(-1) pos = 20->2F */
    1, 3, 1, 3, 1, 3, 1, 3,
    1, 3, 1, 3, 1, 3, 1, 3,
    1, 3, 1, 3, 1, 3, 1, 3,
    1, 3, 1, 3, 1, 3, 1, 3,
    0x11, 3, 0x11, 3, 0x11, 3, 0x11, 3,   /* V(1)   pos = 30->3F */
    0x11, 3, 0x11, 3, 0x11, 3, 0x11, 3,
    0x11, 3, 0x11, 3, 0x11, 3, 0x11, 3,
    0x11, 3, 0x11, 3, 0x11, 3, 0x11, 3
};

/* Short white codes (first 4 bits != 0) */
static const int16_t white_s[1024] PROGMEM = {
    -1,-1,-1,-1,-1,-1,-1,-1,8,29,8,29,8,30,8,30,
    8,45,8,45,8,46,8,46,7,22,7,22,7,22,7,22,
    7,23,7,23,7,23,7,23,8,47,8,47,8,48,8,48,
    6,13,6,13,6,13,6,13,6,13,6,13,6,13,6,13,
    7,20,7,20,7,20,7,20,8,33,8,33,8,34,8,34,
    8,35,8,35,8,36,8,36,8,37,8,37,8,38,8,38,
    7,19,7,19,7,19,7,19,8,31,8,31,8,32,8,32,
    6,1,6,1,6,1,6,1,6,1,6,1,6,1,6,1,
    6,12,6,12,6,12,6,12,6,12,6,12,6,12,6,12,
    8,53,8,53,8,54,8,54,7,26,7,26,7,26,7,26,
    8,39,8,39,8,40,8,40,8,41,8,41,8,42,8,42,
    8,43,8,43,8,44,8,44,7,21,7,21,7,21,7,21,
    7,28,7,28,7,28,7,28,8,61,8,61,8,62,8,62,
    8,63,8,63,8,0,8,0,8,320,8,320,8,384,8,384,
    5,10,5,10,5,10,5,10,5,10,5,10,5,10,5,10,
    5,10,5,10,5,10,5,10,5,10,5,10,5,10,5,10,
    5,11,5,11,5,11,5,11,5,11,5,11,5,11,5,11,
    5,11,5,11,5,11,5,11,5,11,5,11,5,11,5,11,
    7,27,7,27,7,27,7,27,8,59,8,59,8,60,8,60,
    9,1472,9,1536,9,1600,9,1728,7,18,7,18,7,18,7,18,
    7,24,7,24,7,24,7,24,8,49,8,49,8,50,8,50,
    8,51,8,51,8,52,8,52,7,25,7,25,7,25,7,25,
    8,55,8,55,8,56,8,56,8,57,8,57,8,58,8,58,
    6,192,6,192,6,192,6,192,6,192,6,192,6,192,6,192,
    6,1664,6,1664,6,1664,6,1664,6,1664,6,1664,6,1664,6,1664,
    8,448,8,448,8,512,8,512,9,704,9,768,8,640,8,640,
    8,576,8,576,9,832,9,896,9,960,9,1024,9,1088,9,1152,
    9,1216,9,1280,9,1344,9,1408,7,256,7,256,7,256,7,256,
    4,2,4,2,4,2,4,2,4,2,4,2,4,2,4,2,
    4,2,4,2,4,2,4,2,4,2,4,2,4,2,4,2,
    4,2,4,2,4,2,4,2,4,2,4,2,4,2,4,2,
    4,2,4,2,4,2,4,2,4,2,4,2,4,2,4,2,
    4,3,4,3,4,3,4,3,4,3,4,3,4,3,4,3,
    4,3,4,3,4,3,4,3,4,3,4,3,4,3,4,3,
    4,3,4,3,4,3,4,3,4,3,4,3,4,3,4,3,
    4,3,4,3,4,3,4,3,4,3,4,3,4,3,4,3,
    5,128,5,128,5,128,5,128,5,128,5,128,5,128,5,128,
    5,128,5,128,5,128,5,128,5,128,5,128,5,128,5,128,
    5,8,5,8,5,8,5,8,5,8,5,8,5,8,5,8,
    5,8,5,8,5,8,5,8,5,8,5,8,5,8,5,8,
    5,9,5,9,5,9,5,9,5,9,5,9,5,9,5,9,
    5,9,5,9,5,9,5,9,5,9,5,9,5,9,5,9,
    6,16,6,16,6,16,6,16,6,16,6,16,6,16,6,16,
    6,17,6,17,6,17,6,17,6,17,6,17,6,17,6,17,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,4,
    4,5,4,5,4,5,4,5,4,5,4,5,4,5,4,5,
    4,5,4,5,4,5,4,5,4,5,4,5,4,5,4,5,
    4,5,4,5,4,5,4,5,4,5,4,5,4,5,4,5,
    4,5,4,5,4,5,4,5,4,5,4,5,4,5,4,5,
    6,14,6,14,6,14,6,14,6,14,6,14,6,14,6,14,
    6,15,6,15,6,15,6,15,6,15,6,15,6,15,6,15,
    5,64,5,64,5,64,5,64,5,64,5,64,5,64,5,64,
    5,64,5,64,5,64,5,64,5,64,5,64,5,64,5,64,
    4,6,4,6,4,6,4,6,4,6,4,6,4,6,4,6,
    4,6,4,6,4,6,4,6,4,6,4,6,4,6,4,6,
    4,6,4,6,4,6,4,6,4,6,4,6,4,6,4,6,
    4,6,4,6,4,6,4,6,4,6,4,6,4,6,4,6,
    4,7,4,7,4,7,4,7,4,7,4,7,4,7,4,7,
    4,7,4,7,4,7,4,7,4,7,4,7,4,7,4,7,
    4,7,4,7,4,7,4,7,4,7,4,7,4,7,4,7,
    4,7,4,7,4,7,4,7,4,7,4,7,4,7,4,7
};

/* Short black codes (first 4 bits != 0) */
static const int16_t black_s[128] PROGMEM = {
    -1,-1,-1,-1,-1,-1,-1,-1,6,9,6,8,5,7,5,7,
    4,6,4,6,4,6,4,6,4,5,4,5,4,5,4,5,
    3,1,3,1,3,1,3,1,3,1,3,1,3,1,3,1,
    3,4,3,4,3,4,3,4,3,4,3,4,3,4,3,4,
    2,3,2,3,2,3,2,3,2,3,2,3,2,3,2,3,
    2,3,2,3,2,3,2,3,2,3,2,3,2,3,2,3,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
    2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2
};

/* Long black codes (first 4 bits == 0) */
#define EOL -9999   /* End of line */
#define EO1D -9998  /* End of 1D coding */
static const int16_t black_l[1024] PROGMEM = {
    1,0,1,0,12,EOL,12,EOL,1,-1,1,-1,1,-1,1,-1,
    1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,
    1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,12,EO1D,12,EO1D,
    1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,1,-1,
    11,1792,11,1792,11,1792,11,1792,12,1984,12,1984,12,2048,12,2048,
    12,2112,12,2112,12,2176,12,2176,12,2240,12,2240,12,2304,12,2304,
    11,1856,11,1856,11,1856,11,1856,11,1920,11,1920,11,1920,11,1920,
    12,2368,12,2368,12,2432,12,2432,12,2496,12,2496,12,2560,12,2560,
    10,18,10,18,10,18,10,18,10,18,10,18,10,18,10,18,
    12,52,12,52,13,640,13,704,13,768,13,832,12,55,12,55,
    12,56,12,56,13,1280,13,1344,13,1408,13,1472,12,59,12,59,
    12,60,12,60,13,1536,13,1600,11,24,11,24,11,24,11,24,
    11,25,11,25,11,25,11,25,13,1664,13,1728,12,320,12,320,
    12,384,12,384,12,448,12,448,13,512,13,576,12,53,12,53,
    12,54,12,54,13,896,13,960,13,1024,13,1088,13,1152,13,1216,
    10,64,10,64,10,64,10,64,10,64,10,64,10,64,10,64,
    8,13,8,13,8,13,8,13,8,13,8,13,8,13,8,13,
    8,13,8,13,8,13,8,13,8,13,8,13,8,13,8,13,
    8,13,8,13,8,13,8,13,8,13,8,13,8,13,8,13,
    8,13,8,13,8,13,8,13,8,13,8,13,8,13,8,13,
    11,23,11,23,11,23,11,23,12,50,12,50,12,51,12,51,
    12,44,12,44,12,45,12,45,12,46,12,46,12,47,12,47,
    12,57,12,57,12,58,12,58,12,61,12,61,12,256,12,256,
    10,16,10,16,10,16,10,16,10,16,10,16,10,16,10,16,
    10,17,10,17,10,17,10,17,10,17,10,17,10,17,10,17,
    12,48,12,48,12,49,12,49,12,62,12,62,12,63,12,63,
    12,30,12,30,12,31,12,31,12,32,12,32,12,33,12,33,
    12,40,12,40,12,41,12,41,11,22,11,22,11,22,11,22,
    8,14,8,14,8,14,8,14,8,14,8,14,8,14,8,14,
    8,14,8,14,8,14,8,14,8,14,8,14,8,14,8,14,
    8,14,8,14,8,14,8,14,8,14,8,14,8,14,8,14,
    8,14,8,14,8,14,8,14,8,14,8,14,8,14,8,14,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,10,7,10,7,10,7,10,7,10,7,10,7,10,7,10,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    7,11,7,11,7,11,7,11,7,11,7,11,7,11,7,11,
    9,15,9,15,9,15,9,15,9,15,9,15,9,15,9,15,
    9,15,9,15,9,15,9,15,9,15,9,15,9,15,9,15,
    12,128,12,128,12,192,12,192,12,26,12,26,12,27,12,27,
    12,28,12,28,12,29,12,29,11,19,11,19,11,19,11,19,
    11,20,11,20,11,20,11,20,12,34,12,34,12,35,12,35,
    12,36,12,36,12,37,12,37,12,38,12,38,12,39,12,39,
    11,21,11,21,11,21,11,21,12,42,12,42,12,43,12,43,
    10,0,10,0,10,0,10,0,10,0,10,0,10,0,10,0,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12,
    7,12,7,12,7,12,7,12,7,12,7,12,7,12,7,12
};

//
// Helper functions for memory based images
//
static int32_t readMem(TIFFFILE *pFile, uint8_t *pBuf, int32_t iLen)
{
    int32_t iBytesRead;

    if (!pFile || !pFile->pData || iLen <= 0)
        return 0;
    iBytesRead = iLen;
    if ((pFile->iSize - pFile->iPos) < iLen)
        iBytesRead = pFile->iSize - pFile->iPos;
    if (iBytesRead <= 0)
        return 0;
    memcpy_P(pBuf, &pFile->pData[pFile->iPos], iBytesRead);
    pFile->iPos += iBytesRead;
    return iBytesRead;
} /* readMem() */

static int32_t seekMem(TIFFFILE *pFile, int32_t iPosition)
{
    if (!pFile || pFile->iSize <= 0)
        return 0;
    if (iPosition < 0)
        iPosition = 0;
    else if (iPosition >= pFile->iSize)
        iPosition = pFile->iSize - 1;
    pFile->iPos = iPosition;
    return iPosition;
} /* seekMem() */

//
// Public C API
//
int TIFF_openRAW(TIFFIMAGE *pImage, int iWidth, int iHeight, int iFillOrder, uint8_t *pData, int iDataSize, TIFF_DRAW_CALLBACK *pfnDraw)
{
    if (!pImage || !pData || iDataSize <= 0 || !pfnDraw) {
        if (pImage) pImage->iError = TIFF_INVALID_PARAMETER;
        return 0;
    }
    if (iWidth <= 0 || iHeight <= 0 || iWidth > MAX_IMAGE_WIDTH - 2) {
        pImage->iError = TIFF_TOO_WIDE;
        return 0;
    }
    memset(pImage, 0, sizeof(TIFFIMAGE));
    pImage->pfnRead = readMem;
    pImage->pfnSeek = seekMem;
    pImage->pfnDraw = pfnDraw;
    pImage->TIFFFile.iSize = iDataSize;
    pImage->TIFFFile.pData = pData;
    pImage->iWidth = iWidth;
    pImage->iHeight = iHeight;
    pImage->ucFillOrder = (uint8_t)iFillOrder;
    // Default output values (1:1 1BPP)
    pImage->window.iScale = 65536;
    pImage->window.x = 0;
    pImage->window.y = 0;
    pImage->window.iWidth = iWidth;
    pImage->window.iHeight = iHeight;
    pImage->window.ucPixelType = TIFF_PIXEL_1BPP;
    return 1;
} /* TIFF_openRAW() */

int TIFF_decode(TIFFIMAGE *pImage)
{
    if (!pImage) return TIFF_INVALID_PARAMETER;
    return Decode(pImage);
} /* TIFF_decode() */

int TIFF_getLastError(TIFFIMAGE *pImage)
{
    if (!pImage) return TIFF_INVALID_PARAMETER;
    return pImage->iError;
} /* TIFF_getLastError() */

//
// Read (and optionally bit flip) more VLC data for decoding
//
static void TIFFGetMoreData(TIFFIMAGE *pPage)
{
    if ((pPage->iVLCSize - pPage->iVLCOff) >= FILE_HIGHWATER)
        return; // buffer is already full
    if (pPage->iVLCOff > 0 && pPage->iVLCSize >= pPage->iVLCOff)
    {
        memmove(&pPage->ucFileBuf[0], &pPage->ucFileBuf[pPage->iVLCOff], pPage->iVLCSize - pPage->iVLCOff);
        pPage->iVLCSize -= pPage->iVLCOff;
        pPage->iVLCOff = 0;
    }
    if (pPage->TIFFFile.iPos < pPage->TIFFFile.iSize && pPage->iVLCSize < FILE_HIGHWATER)
    {
        int i, iBytesRead;
        iBytesRead = (*pPage->pfnRead)(&pPage->TIFFFile, &pPage->ucFileBuf[pPage->iVLCSize], TIFF_FILE_BUF_SIZE - pPage->iVLCSize);
        if (pPage->ucFillOrder == BITDIR_LSB_FIRST)
        {
            for (i = 0; i < iBytesRead; i++)
            {
                uint8_t c = pPage->ucFileBuf[i + pPage->iVLCSize];
                pPage->ucFileBuf[i + pPage->iVLCSize] = pgm_read_byte(&ucMirror[c]);
            }
        }
        pPage->iVLCSize += iBytesRead;
    }
    // Zero-pad remaining buffer space to prevent over-read garbage in TIFFMOTOLONG
    if (pPage->iVLCSize < TIFF_FILE_BUF_SIZE)
    {
        memset(&pPage->ucFileBuf[pPage->iVLCSize], 0, TIFF_FILE_BUF_SIZE - pPage->iVLCSize);
    }
} /* TIFFGetMoreData() */

static int TIFFDrawLine(TIFFIMAGE *pPage, int y, int16_t *pCurFlips)
{
    int x, len, run, sx, srun;
    uint8_t lBit, rBit, *p;
    int iStart = 0, xright = pPage->iWidth;
    uint32_t u32ScaleFactor;
    uint8_t *pDest;
    TIFFDRAW obgd;

    u32ScaleFactor = pPage->window.iScale;
    obgd.iWidth = pPage->iWidth;
    obgd.iHeight = pPage->iHeight;
    obgd.iDestX = pPage->window.dstx;
    obgd.iDestY = pPage->window.dsty;
    obgd.iScaledWidth = (pPage->window.iWidth * u32ScaleFactor) >> 16;
    obgd.iScaledHeight = (pPage->window.iHeight * u32ScaleFactor) >> 16;
    obgd.pUser = pPage->pUser;
    iStart = pPage->window.x;
    
    if (y >= pPage->window.y + pPage->window.iHeight)
        return 0; // stop decoding
    if (y == pPage->window.y) // start first line at white
    {
        pPage->u32Accum = 0;
        pPage->y = 0;
        pPage->iPitch = (pPage->iWidth + 7) >> 3;
        if (u32ScaleFactor != 0x10000)
            pPage->iPitch = (((pPage->iWidth * u32ScaleFactor) >> 16) + 7) >> 3;
        if (pPage->iPitch > MAX_BUFFERED_PIXELS) {
            pPage->iError = TIFF_TOO_WIDE;
            return 0;
        }
        memset(pPage->ucPixels, 0xff, pPage->iPitch); // start as 0xff (white)
    }
    pDest = pPage->ucPixels;
    pPage->u32Accum += u32ScaleFactor;
    if ((y < pPage->window.y) || (y & 1 && u32ScaleFactor < 0x4000))
        return 1;
    x = 0;
    while (x < xright)
    {
        x = *pCurFlips++; // black starting point
        run = *pCurFlips++ - x; // get the black run
        if (run < 0) {
            pPage->iError = TIFF_DECODE_ERROR;
            return 0; // negative run indicates corrupt stream
        }
        x -= iStart;
        if (x >= xright || run == 0)
            break;
        if ((x + run) > 0) {
            if (x < 0) {
                run += x;
                x = 0;
            }
            if ((x + run) > xright) {
                run = xright - x;
            }
            sx = (x * u32ScaleFactor) >> 16;
            srun = (run * u32ScaleFactor) >> 16;
            if (srun < 1)
                srun = 1;
            lBit = 0xff << (8 - (sx & 7));
            rBit = 0xff >> ((sx + srun) & 7);
            len = ((sx + srun) >> 3) - (sx >> 3);
            p = &pDest[sx >> 3];
            if (len == 0)
            {
                lBit |= rBit;
                *p &= lBit;
            }
            else
            {
                *p++ &= lBit;
                while (len > 1)
                {
                    *p++ = 0;
                    len--;
                }
                *p = rBit;
            }
        }
    }
    obgd.ucLast = 0;
    obgd.ucPixelType = pPage->window.ucPixelType;
    if ((pPage->u32Accum >> 16) >= 1)
    {
        obgd.pPixels = pPage->ucPixels;
        while (pPage->u32Accum >= 0x10000)
        {
            obgd.y = pPage->y;
            (*pPage->pfnDraw)(&obgd);
            pPage->y++;
            pPage->u32Accum -= 0x10000;
        }
        memset(pPage->ucPixels, 0xff, pPage->iPitch);
    }
    return 1;
} /* TIFFDrawLine() */

//
// Initialize internal structures to decode the image
//
static void Decode_Begin(TIFFIMAGE *pPage)
{
    int i, xsize;
    int16_t *CurFlips, *RefFlips;
    uint8_t *pBuf, *pBufEnd;
    
    xsize = pPage->iWidth;
    RefFlips = pPage->RefFlips;
    CurFlips = pPage->CurFlips;
    
    for (i = 0; i < xsize - 2; i++)
    {
        RefFlips[i] = xsize;
        CurFlips[i] = xsize;
    }
    CurFlips[i] = RefFlips[i] = 0x7fff;
    CurFlips[i + 1] = RefFlips[i + 1] = 0x7fff;
    
    pPage->pCur = CurFlips;
    pPage->pRef = RefFlips;

    pBuf = pPage->pBuf = pPage->ucFileBuf;
    pBufEnd = &pBuf[pPage->iVLCSize < 128 ? pPage->iVLCSize : 128];
    while (pBuf < pBufEnd && pBuf[0] == 0)
    {
        pBuf++;
    }
    
    pPage->pBuf = pBuf;
    pPage->ulBits = TIFFMOTOLONG(pBuf);
    pPage->ulBitOff = pPage->iVLCOff = 0;
} /* Decode_Begin() */

//
// Decode a single line of G4 data (private function)
//
static int Decode_one_line(TIFFIMAGE *pPage)
{
    signed int a0, a0_c, a0_p, b1;
    int16_t *pCur, *pRef, *RefFlips, *CurFlips;
    int16_t *pCurEnd, *pRefEnd;
    int xsize, tot_run, tot_run1 = 0;
    int32_t sCode;
    uint32_t lBits;
    uint32_t ulBits, ulBitOff;
    uint8_t *pBuf, *pBufEnd;

    pCur = CurFlips = pPage->pCur;
    pRef = RefFlips = pPage->pRef;
    pCurEnd = CurFlips + MAX_IMAGE_WIDTH - 2;
    pRefEnd = RefFlips + MAX_IMAGE_WIDTH - 2;
    ulBits = pPage->ulBits;
    ulBitOff = pPage->ulBitOff;
    pBuf = pPage->pBuf;
    pBufEnd = &pPage->ucFileBuf[TIFF_FILE_BUF_SIZE - 4];

    a0 = -1;
    a0_c = 0; /* start just to left and white */
    xsize = pPage->iWidth;
    
    while (a0 < xsize)
    {
        if (pBuf >= pBufEnd || pCur >= pCurEnd || pRef >= pRefEnd)
        {
            pPage->iError = TIFF_DECODE_ERROR;
            goto pilreadg4z;
        }
        if (ulBitOff > (REGISTER_WIDTH - 8))
        {
            pBuf += (ulBitOff >> 3);
            ulBitOff &= 7;
            ulBits = TIFFMOTOLONG(pBuf);
        }
        if ((int32_t)(ulBits << ulBitOff) < 0) /* V(0) code */
        {
            a0 = *pRef++;
            ulBitOff++;
            a0_c = 1 - a0_c;
            *pCur++ = a0;
        }
        else
        {
            lBits = (ulBits >> ((REGISTER_WIDTH - 8) - ulBitOff)) & 0xfe;
            sCode = pgm_read_byte(&code_table[lBits]);
            ulBitOff += pgm_read_byte(&code_table[lBits + 1]);
            switch (sCode)
            {
                case 1: /* V(-1) */
                case 2: /* V(-2) */
                case 3: /* V(-3) */
                    a0 = *pRef - sCode;
                    *pCur++ = a0;
                    if (pRef == RefFlips)
                        pRef += 2;
                    pRef--;
                    while (pRef < pRefEnd && a0 >= *pRef)
                        pRef += 2;
                    a0_c = 1 - a0_c;
                    break;

                case 0x11: /* V(1) */
                case 0x12: /* V(2) */
                case 0x13: /* V(3) */
                    a0 = *pRef++;
                    b1 = a0;
                    a0 += sCode & 7;
                    if (b1 != xsize && a0 < xsize)
                    {
                        while (pRef < pRefEnd && a0 >= *pRef)
                            pRef += 2;
                    }
                    if (a0 > xsize)
                        a0 = xsize;
                    a0_c = 1 - a0_c;
                    *pCur++ = a0;
                    break;

                case 0x20: /* Horizontal codes */
                    a0_p = a0;
                    if (a0 < 0)
                        a0_p = 0;
                    if (a0_c) /* Black case */
                    {
                        CLIMBBLACK_NEW(pBuf, ulBitOff, ulBits, sCode)
                        if (sCode < 0)
                        {
                            pPage->iError = TIFF_DECODE_ERROR;
                            goto pilreadg4z;
                        }
                        tot_run = sCode;
                        CLIMBWHITE_NEW(pBuf, ulBitOff, ulBits, sCode)
                        if (sCode < 0)
                        {
                            pPage->iError = TIFF_DECODE_ERROR;
                            goto pilreadg4z;
                        }
                        tot_run1 = sCode;
                    }
                    else /* White case */
                    {
                        CLIMBWHITE_NEW(pBuf, ulBitOff, ulBits, sCode)
                        if (sCode < 0)
                        {
                            pPage->iError = TIFF_DECODE_ERROR;
                            goto pilreadg4z;
                        }
                        tot_run = sCode;
                        CLIMBBLACK_NEW(pBuf, ulBitOff, ulBits, sCode)
                        if (sCode < 0)
                        {
                            pPage->iError = TIFF_DECODE_ERROR;
                            goto pilreadg4z;
                        }
                        tot_run1 = sCode;
                    }
                    a0 = a0_p + tot_run;
                    *pCur++ = a0;
                    a0 += tot_run1;
                    if (a0 < xsize)
                        while (pRef < pRefEnd && a0 >= *pRef)
                            pRef += 2;
                    *pCur++ = a0;
                    break;

                case 0x30: /* Pass code */
                    pRef++;
                    a0 = *pRef++;
                    break;

                case 0x40: /* Uncompressed mode */
                    lBits = ulBits << ulBitOff;
                    lBits &= 0xffc00000;
                    if (lBits != 0x3c00000)
                    {
                        pPage->iError = TIFF_DECODE_ERROR;
                        goto pilreadg4z;
                    }
                    ulBitOff += 10;
                    tot_run = 0;
                    if ((ulBits << ulBitOff) & TOP_BIT)
                        goto blkst;
              whtst:
                    if (pBuf > pBufEnd)
                    {
                        pPage->iError = TIFF_DECODE_ERROR;
                        goto pilreadg4z;
                    }
                    tot_run1++;
                    ulBitOff++;
                    if (ulBitOff > (REGISTER_WIDTH - 8))
                    {
                        pBuf += (ulBitOff >> 3);
                        ulBitOff &= 7;
                        ulBits = TIFFMOTOLONG(pBuf);
                    }
                    lBits = ulBits << ulBitOff;
                    if ((lBits & TOP_BIT) == 0)
                        goto whtst;
                    if (tot_run1 == 5)
                    {
                        tot_run += 5;
                        tot_run1 = -1;
                        goto whtst;
                    }
                    if (tot_run1 >= 6)
                    {
                        tot_run += tot_run1 - 6;
                        if (tot_run)
                        {
                            a0 += tot_run;
                            *pCur++ = a0;
                        }
                        ulBitOff++;
                        if (ulBitOff > (REGISTER_WIDTH - 8))
                        {
                            pBuf += (ulBitOff >> 3);
                            ulBitOff &= 7;
                            ulBits = TIFFMOTOLONG(pBuf);
                        }
                        lBits = ulBits << ulBitOff;
                        lBits >>= (REGISTER_WIDTH - 1);
                        if ((signed int)lBits != a0_c)
                            pRef++;
                        a0_c = (int)lBits;
                        while (pRef < pRefEnd && *pRef <= a0)
                            pRef += 2;
                        break;
                    }
                    else
                    {
                        tot_run += tot_run1;
                        a0 += tot_run;
                        *pCur++ = a0;
                        tot_run = 0;
                        tot_run1 = 0;
                    }
              blkst:
                    if (pBuf > pBufEnd)
                    {
                        pPage->iError = TIFF_DECODE_ERROR;
                        goto pilreadg4z;
                    }
                    tot_run++;
                    ulBitOff++;
                    if (ulBitOff > (REGISTER_WIDTH - 8))
                    {
                        pBuf += (ulBitOff >> 3);
                        ulBitOff &= 7;
                        ulBits = TIFFMOTOLONG(pBuf);
                    }
                    lBits = ulBits << ulBitOff;
                    if ((lBits & TOP_BIT) == TOP_BIT)
                        goto blkst;
                    a0 += tot_run;
                    *pCur++ = a0;
                    tot_run = 0;
                    goto whtst;

                default:
                    CLIMBWHITE_NEW(pBuf, ulBitOff, ulBits, sCode)
                    if (sCode != EOL)
                    {
                        pPage->iError = TIFF_DECODE_ERROR;
                        goto pilreadg4z;
                    }
                    CLIMBWHITE_NEW(pBuf, ulBitOff, ulBits, sCode)
                    if (sCode != EOL)
                    {
                        pPage->iError = TIFF_DECODE_ERROR;
                        goto pilreadg4z;
                    }
                    goto pilreadg4z;
            }
        }
    }
    *pCur++ = xsize;
    *pCur++ = xsize;
pilreadg4z:
    pPage->ulBits = ulBits;
    pPage->ulBitOff = ulBitOff;
    pPage->pBuf = pBuf;
    return pPage->iError;
} /* Decode_one_line() */

//
// Decompress the VLC data
//
static int Decode(TIFFIMAGE *pPage)
{
    int y, bContinue;
    uint8_t *pBufEnd;
    int16_t *t1;
    
    pPage->iVLCSize = pPage->iVLCOff = 0;
    (*pPage->pfnSeek)(&pPage->TIFFFile, 0);
    TIFFGetMoreData(pPage);
    Decode_Begin(pPage);
    pBufEnd = &pPage->ucFileBuf[FILE_HIGHWATER];
    
    bContinue = 1;
    for (y = 0; y < pPage->iHeight && bContinue; y++)
    {
        if (pPage->pBuf >= pBufEnd)
        {
            pPage->iVLCOff = (int)(pPage->pBuf - pPage->ucFileBuf);
            TIFFGetMoreData(pPage);
            pPage->pBuf = pPage->ucFileBuf;
        }
        if (Decode_one_line(pPage) != TIFF_SUCCESS)
        {
            break;
        }
        bContinue = TIFFDrawLine(pPage, y, pPage->pCur);
        t1 = pPage->pRef;
        pPage->pRef = pPage->pCur;
        pPage->pCur = t1;
    }
    return pPage->iError;
} /* Decode() */
