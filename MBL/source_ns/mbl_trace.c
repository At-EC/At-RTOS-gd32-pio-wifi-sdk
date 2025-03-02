/*!
    \file    mbl_trace.c
    \brief   Non-secure MBL trace file for GD32W51x WiFi SDK

    \version 2023-07-06, V1.0.0, firmware for GD32W51x
*/

/*
    Copyright (c) 2023, GigaDevice Semiconductor Inc.

    Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.
    2. Redistributions in binary form must reproduce the above copyright notice,
       this list of conditions and the following disclaimer in the documentation
       and/or other materials provided with the distribution.
    3. Neither the name of the copyright holder nor the names of its contributors
       may be used to endorse or promote products derived from this software without
       specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
OF SUCH DAMAGE.
*/

#ifndef __MBL_TRACE_C__
#define __MBL_TRACE_C__

#include "mbl_trace.h"

extern void uart_putc(uint8_t c);

static void printchar(char **str, int c)
{
    if (str) {
        **str = c;
        ++(*str);
    } else {
        uart_putc((char)c);
    }
}

#define PAD_RIGHT 1
#define PAD_ZERO 2

int prints(char **out, const char *string, int width, int pad)
{
    register int pc = 0, padchar = ' ';

    if (width > 0) {
        register int len = 0;
        register const char *ptr;

        for (ptr = string; *ptr; ++ptr) {
            ++len;
        }

        if (len >= width) {
            width = 0;
        }
        else {
            width -= len;
        }

        if (pad & PAD_ZERO) {
            padchar = '0';
        }
    }

    if (!(pad & PAD_RIGHT)) {
        for ( ; width > 0; --width) {
            printchar (out, padchar);
            ++pc;
        }
    }

    for ( ; *string ; ++string) {
        printchar (out, *string);
        ++pc;
    }

    for ( ; width > 0; --width) {
        printchar (out, padchar);
        ++pc;
    }

    return pc;
}

/* the following should be enough for 32 bit int */
#define PRINT_BUF_LEN 12

static int printi(char **out, int i, int b, int sg, int width, int pad, int letbase)
{
    char print_buf[PRINT_BUF_LEN];
    register char *s;
    register int t, neg = 0, pc = 0;
    register unsigned int u = i;

    if (i == 0) {
        print_buf[0] = '0';
        print_buf[1] = '\0';
        return prints (out, print_buf, width, pad);
    }

    if (sg && b == 10 && i < 0) {
        neg = 1;
        u = -i;
    }

    s = print_buf + PRINT_BUF_LEN-1;
    *s = '\0';

    while (u) {
        t = u % b;

        if( t >= 10 ) {
            t += letbase - '0' - 10;
        }

        *--s = t + '0';
        u /= b;
    }

    if (neg) {
        if( width && (pad & PAD_ZERO) ) {
            printchar (out, '-');
            ++pc;
            --width;
        }
        else {
            *--s = '-';
        }
    }

    return pc + prints (out, s, width, pad);
}

static int print(char **out, const char *format, va_list args)
{
    register int width, pad;
    register int pc = 0;
    char scr[2];

    for (; *format != 0; ++format) {
        if (*format == '%') {
            ++format;
            width = pad = 0;

            if (*format == '\0') {
                break;
            }

            if (*format == '%') {
                goto out;
            }

            if (*format == '-') {
                ++format;
                pad = PAD_RIGHT;
            }

            while (*format == '0') {
                ++format;
                pad |= PAD_ZERO;
            }

            for ( ; *format >= '0' && *format <= '9'; ++format) {
                width *= 10;
                width += *format - '0';
            }

            if (*format == 's') {
                register char *s = (char *)va_arg(args, int);
                pc += prints(out, s ? s : "(null)", width, pad);
                continue;
            }

            if (*format == 'd') {
                pc += printi(out, va_arg(args, int), 10, 1, width, pad, 'a');
                continue;
            }

            if (*format == 'p') {
                register unsigned char *addr;
                register unsigned char i;
                switch (*(++format)) {
                case 'M':
                    width = 2;
                    pad |= PAD_ZERO;
                    addr = (unsigned char *)va_arg(args, int);
                    for (i = 0; i < 5; i++) {
                        pc += printi(out, addr[i], 16, 1, width, pad, 'a');
                        printchar(out, ':');
                    }
                    pc += printi(out, addr[i], 16, 1, width, pad, 'a');
                    continue;
                case 'I':
                    addr = (unsigned char *)va_arg(args, int);
                    for (i = 0; i < 3; i++) {
                        pc += printi(out, addr[i], 10, 1, width, pad, 'a');
                        printchar(out, '.');
                    }
                    pc += printi(out, addr[i], 10, 1, width, pad, 'a');
                    continue;
                default:
                    format--;
                    pc += printi(out, va_arg(args, int), 16, 0, width, pad, 'a');
                    continue;
                }
            }

            if ((*format == 'x')) {
                pc += printi(out, va_arg(args, int), 16, 0, width, pad, 'a');
                continue;
            }

            if (*format == 'X') {
                pc += printi(out, va_arg(args, int), 16, 0, width, pad, 'A');
                continue;
            }

            if (*format == 'u') {
                pc += printi(out, va_arg(args, int), 10, 0, width, pad, 'a');
                continue;
            }

            if (*format == 'c') {
                /* char are converted to int then pushed on the stack */
                scr[0] = (char)va_arg(args, int);
                scr[1] = '\0';
                pc += prints (out, scr, width, pad);
                continue;
            }
        } else {
out:
            printchar(out, *format);
            ++pc;
        }
    }

    if (out) {
        **out = '\0';
    }

    return pc;
}

/*!
    \brief      configurable print function
    \param[in]  format: printf format
    \param[out] none
    \retval     none
*/
int co_printf(const char *format, ...)
{
    int ret;
    va_list args;

    va_start(args, format);
    ret = print(0, format, args);
    va_end(args);

    return ret;

}

#endif  // __MBL_TRACE_C__
