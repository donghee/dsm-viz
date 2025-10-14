/*****************************************************************************

The MIT License (MIT)
Copyright (c) 2012 cupday

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.

******************************************************************************/

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define DUMP_BYTES      16
#define DUMP_LINE_BYTES 16

int
libcup_dump_print(const char* title, unsigned char* data, int len)
{
    int i;
    int j;
    uint8_t ascii[64];

    if(len <= 0)
        return 0;

    printf("%s l=%4d, ", title, len);
    memset(ascii, 0, 64);
    for(i=0, j=0; i<len && i<DUMP_BYTES ; i++) {
        if( j >= DUMP_LINE_BYTES) {
            printf("%s\n", ascii);
            memset(ascii, 0, 64);
            j=0;
        }
        if( isprint(data[i]) ) {
            ascii[j++] = data[i];
        } else {
            ascii[j++] = '.';
        }
        printf("%02x ", data[i]);
        if(i != 0 && (i & 0x7) == 0x07 ) {
            printf(" ");
        }
    }

    for(;j<DUMP_LINE_BYTES;j++) {
        printf("   ");
        if(j != 0 && (j & 0x7) == 0x07 ) {
            printf(" ");
        }
        ascii[j] = ' ';
    }
    printf(" %s", ascii);

    return 0;
}
