/*!
    \file    debug_print.c
    \brief   Debug print for GD32W51x WiFi SDK

    \version 2021-10-30, V1.0.0, firmware for GD32W51x
*/

/*
    Copyright (c) 2021, GigaDevice Semiconductor Inc.

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

#include <stdarg.h>
#include "debug_print.h"
#include "uart.h"

#ifdef CONFIG_TELNET_SERVER
extern void check_and_write_revc_message(char* str);
#endif

#ifdef CONFIG_PRINT_IN_SEQUENCE
#include "wrapper_os.h"
#define MAX_BUF_LEN    1024
static char print_buf[MAX_BUF_LEN];
static os_sema_t print_sema;
static int print_task_init = 0;
static int w_point = 0, r_point = 0, used_len = 0;

/*!
    \brief      print task handler
    \param[in]  argv: the pointer to the array that holds the parameters
    \param[out] none
    \retval     none
*/
static void print_task_handle(void *argv)
{
    do{
        sys_sema_down(&print_sema, 0);
        while ( used_len > 0){
            //printf("%c", print_buf[r_point]);
            uart_putc_noint(print_buf[r_point]);
            r_point ++;
            if ( r_point >= MAX_BUF_LEN)
                r_point -= MAX_BUF_LEN;
            SYS_CRITICAL_ENTER();
            used_len--;
            SYS_CRITICAL_EXIT();
        }
    }while(1);
}
#endif /* CONFIG_PRINT_IN_SEQUENCE */

/*!
    \brief      configurable print function
    \param[in]  format: printf format
    \param[out] none
    \retval     none
*/
int co_printf(const char *format, ...)
{
#ifndef CONFIG_PRINT_IN_SEQUENCE
    int ret;
    va_list args;
    char str[384];

    va_start(args, format);
    ret = vsnprintf(str, sizeof(str), format, args);
    printf("%s", str);
#ifdef CONFIG_TELNET_SERVER
    check_and_write_revc_message(str);
#endif

#if defined(__GNUC__)
    fflush(stdout);
#endif
    va_end(args);

    return ret;
#else  /* CONFIG_PRINT_IN_SEQUENCE */
    char out[256], *pout = &out[0];
    char len = 0;
    va_list args;
    int cur_wp = 0;
    int pc = 0;

    if ( print_task_init == 0) {
        SYS_CRITICAL_ENTER();
        if ( print_task_init == 0 ){
            sys_sema_init(&print_sema, 0);
            sys_task_create(NULL, (const uint8_t *)"Print", NULL, 512, 0, (TASK_PRIO_IDLE + TASK_PRIO_HIGHER(1)), print_task_handle, NULL);
            print_task_init = 1;
        }
        SYS_CRITICAL_EXIT();
    }

    va_start( args, format );
    pc = vsnprintf(pout, sizeof(out), format, args);
    va_end( args );

    len = strlen(out);
    //printf("len=%d used_len=%d w_point=%d r_point=%d\r\n", len, used_len, w_point, r_point);
    if ( len < ( MAX_BUF_LEN - used_len )){
        /* Remaining print buffer is enough for this print. */
        SYS_CRITICAL_ENTER();
        cur_wp = w_point;
        used_len += len;
        w_point += len;
        w_point = ( w_point >= MAX_BUF_LEN ) ? (w_point - MAX_BUF_LEN) : w_point;
        SYS_CRITICAL_EXIT();

        if ( cur_wp >= r_point ) {
            if ( (MAX_BUF_LEN - cur_wp) >= len ) {
                sys_memcpy(&print_buf[cur_wp], out, len);
            } else {
                sys_memcpy(&print_buf[cur_wp], out, ( MAX_BUF_LEN - cur_wp ));
                sys_memcpy(&print_buf[0], &out[MAX_BUF_LEN - cur_wp], (len - (MAX_BUF_LEN - cur_wp)));
            }
        } else {
            sys_memcpy(&print_buf[cur_wp], out, len);
        }
        /* Inform print task to read. */
        sys_sema_up(&print_sema);
    }else{
        /* The print buffer is full. Ignore the new message. */
    }
    return pc;
#endif  /* CONFIG_PRINT_IN_SEQUENCE */
}

#define MAX_LINE_LENGTH_BYTES (64)
#define DEFAULT_LINE_LENGTH_BYTES (16)
/*!
    \brief      copy from memory into linebuf and print hex values
    \param[in]  addr: the address of the place to be copied
    \param[in]  data: the pointer of data in memory
    \param[in]  width: the width of data
    \param[in]  count: the counter of data
    \param[in]  linelen: line length of line buffer
    \param[out] none
    \retval     none
*/
int buffer_print(unsigned long addr, void *data, unsigned long width, unsigned long count, unsigned long linelen)
{
    unsigned char linebuf[MAX_LINE_LENGTH_BYTES];
    unsigned long *uip = (void *)linebuf;
    unsigned short *usp = (void *)linebuf;
    unsigned char *ucp = (void *)linebuf;
    unsigned char *pdata = (unsigned char *)data;
    int i;

    if (linelen*width > MAX_LINE_LENGTH_BYTES)
        linelen = MAX_LINE_LENGTH_BYTES / width;
    if (linelen < 1)
        linelen = DEFAULT_LINE_LENGTH_BYTES / width;

    while (count) {
        printf("%08x:", (unsigned int)addr);

        /* check for overflow condition */
        if (count < linelen)
            linelen = count;

        /* Copy from memory into linebuf and print hex values */
        for (i = 0; i < linelen; i++) {
            if (width == 4) {
                uip[i] = *(volatile unsigned long *)pdata;
                printf(" %08x", (unsigned int)uip[i]);
            } else if (width == 2) {
                usp[i] = *(volatile unsigned short *)pdata;
                printf(" %04x", usp[i]);
            } else {
                ucp[i] = *(volatile unsigned char *)pdata;
                printf(" %02x", ucp[i]);
            }
            pdata += width;
        }

        printf("\r\n");

        /* update references */
        addr += linelen * width;
        count -= linelen;
    }

    return 0;
}

/*!
    \brief      print memory data with title
    \param[in]  title: the pointer of data title
    \param[in]  mem: the pointer of data in memory
    \param[in]  mem_size: the size of memory
    \param[out] none
    \retval     none
*/
void buffer_dump(char *title, uint8_t *mem, int mem_size)
{
    int i;
    if (mem_size == 0) return;
    printf("%s\r\n\t", title);
    for (i = 0; i < mem_size; i++) {
        if ((i > 0) && (i % 16 == 0))
            printf("\r\n\t");
        printf("%02x ", *(mem + i));
    }
    printf("\r\n");
}

/*!
    \brief      convert the string to hexadecimal format
    \param[in]  input: the pointer of input data
    \param[in]  input_len: the length of input data
    \param[in]  output: the pointer of output data
    \param[in]  output_len: the length of output data
    \param[out] none
    \retval     none
*/
int str2hex(char *input, int input_len, unsigned char *output, int output_len)
{
    int index = 0;
    char iter_char = 0;

    if (input == NULL || input_len <= 0 || input_len % 2 != 0 ||
        output == NULL || output_len < input_len / 2) {
        return -1;
    }

    memset(output, 0, output_len);

    for (index = 0; index < input_len; index += 2) {
        if (input[index] >= '0' && input[index] <= '9') {
            iter_char = input[index] - '0';
        } else if (input[index] >= 'A' && input[index] <= 'F') {
            iter_char = input[index] - 'A' + 0x0A;
        } else if (input[index] >= 'a' && input[index] <= 'f') {
            iter_char = input[index] - 'a' + 0x0A;
        } else {
            return -2;
        }
        output[index / 2] |= (iter_char << 4) & 0xF0;

        if (input[index + 1] >= '0' && input[index + 1] <= '9') {
            iter_char = input[index + 1] - '0';
        } else if (input[index + 1] >= 'A' && input[index + 1] <= 'F') {
            iter_char = input[index + 1] - 'A' + 0x0A;
        } else if (input[index + 1] >= 'a' && input[index + 1] <= 'f') {
            iter_char = input[index + 1] - 'a' + 0x0A;
        } else {
            return -3;
        }
        output[index / 2] |= (iter_char) & 0x0F;
    }

    return 0;
}
