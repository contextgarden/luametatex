Remark: 

This library is one of those nicely optimized and mimimalized ones for embedded devices. Both files 
has been stripped from e.g. Arduino and similar specifics because we need to make sure it compiles fine 
with MSVC, Mingw64 cross compilers, GNU Posix, CLANG for OSX, etc. It makes it easier for me to see  
what it actually does in so little fast code. The reference is at: 

	https://github.com/bitbank2/TIFF_G4.git

After writing the Lua library that uses this code I had to make sure that we always went the RAM route 
so I fed the two |tiff_g4| files and |lmttifflib.c| into Gemini and it suggested to add some guards and 
as we're not bound to tiny devices to bump some memory values. But: consider all introduced error to be 
mine. We also made sure no (casting related) compiler messages were issued. 

The usage is as with other graphic libraries: we do in Lua what Lua does best, in this case filtering 
the tags, and delegate to C what can best be done there (complexity, performance). Because this was 
added relatively late (2026) the usage pattern resembles the other graphics. 

Hans 



Here is the summary of what the 'cleanup' did (in markdown speak):

### Summary of Changes & Fixes

* **Memory & Buffer Safety:**

* **Zero-padding tail bytes in `TIFFGetMoreData`:** Because `TIFFMOTOLONG` reads 4 bytes ahead at a time, short strings or the end of a strip could previously read uninitialized stack bytes in `ucFileBuf`. The remaining bytes in `ucFileBuf` are now explicitly zeroed after every read.

* **Bounds guards in `Decode_one_line` & `Decode_Begin`:** Added explicit upper-bound checks on `pCur`, `pRef` (`MAX_IMAGE_WIDTH - 2`), and `pBuf` (`TIFF_FILE_BUF_SIZE - 4`) so malformed G4 runs cannot walk past `CurFlips`, `RefFlips`, or `ucFileBuf`. `Decode_Begin` also caps its leading-zero scan at `iVLCSize` rather than unconditionally scanning 128 bytes.

* **Width & pitch validation:** `TIFF_openRAW` now rejects `iWidth <= 0`, `iHeight <= 0`, or `iWidth > MAX_IMAGE_WIDTH - 2` (reserving the 2 sentinel slots `0x7fff` at the end of `CurFlips`/`RefFlips`), and `TIFFDrawLine` validates `iPitch <= MAX_BUFFERED_PIXELS` instead of silently skipping `memset`.

* **Error Handling:**

* **Propagated line-decode errors:** In `Decode()`, `Decode_one_line(pPage)` previously ignored its return value and kept decoding subsequent rows after an error; it now breaks immediately on `!= TIFF_SUCCESS` and returns `pPage->iError`.

* **Negative run detection & uncompressed mode bug:** `TIFFDrawLine` now sets `pPage->iError = TIFF_DECODE_ERROR` when `run < 0`. In `Decode_one_line`'s uncompressed mode exit, fixed `a0_c = sCode` (which assigned the opcode `0x40` instead of the decoded color bit `lBits`).

* **Parameter checks:** Added null/zero-length guards in `TIFF_openRAW`, `TIFF_decode`, `TIFF_getLastError`, `readMem`, and `seekMem`.

* **Size Bumps & Stripping:**

* Increased `TIFF_FILE_BUF_SIZE` and `MAX_IMAGE_WIDTH` to `8192` (with `FILE_HIGHWATER` automatically scaling to `6144`), and bumped `MAX_BUFFERED_PIXELS` to `2048` (supporting up to 16,384-pixel wide 1-bpp rows).

* Removed all file I/O (`TIFF_openTIFFFile`, `readFile`, `seekFile`, `closeFile`), TIFF IFD/tag parsing (`TIFFParseInfo`, `TIFFInit`), incremental APIs (`TIFF_decodeInc*`, `TIFF_addData`), `NO_RAM` preprocessor branches, and 2-bpp/4-bpp/RGB565 scale-to-gray tables and routines (`ucGray2BPP`, `ucGray4BPP`, `Scale2Color`, `Scale2Gray`, `Scale2Gray4BPP`).
