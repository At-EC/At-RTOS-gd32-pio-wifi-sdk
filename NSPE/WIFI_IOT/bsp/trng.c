/*!
    \file    trng.c
    \brief   True random number generator BSP for GD32W51x WiFi SDK

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

#include "app_cfg.h"
#include "app_type.h"
#include "wrapper_os.h"
#include "bsp_inc.h"
#include "debug_print.h"
#include "trng.h"
#include "mbedtls/md5.h"

unsigned char btrng_seed = 1;
static unsigned int randPool;   /* Pool of randomness. */
static unsigned int randCount;      /* Pseudo-random incrementer */
os_mutex_t trng_lock;

/*!
    \brief       check whether TRNG is ready
    \param[in]   none
    \param[out]  none
    \retval      1: TRNG is ready 0: TRNG is not ready
*/
int trng_ready_check(void)
{
    uint32_t timeout = 0;
    uint32_t trng_flag = STATUS_RESET;
    int reval = 1;

    /* check wherther the random data is valid */
    do {
        timeout++;
        trng_flag = trng_flag_get(TRNG_FLAG_DRDY);
    } while((STATUS_RESET == trng_flag) &&(0xFFFF > timeout));

    if (STATUS_RESET == trng_flag) {
        /* ready check timeout */
        trng_flag = trng_flag_get(TRNG_FLAG_CECS);
        DEBUGPRINT("TRNG clock error(%d).\r\n", trng_flag);
        trng_flag = trng_flag_get(TRNG_FLAG_SECS);
        DEBUGPRINT("TRNG seed error(%d).\r\n", trng_flag);
        reval = 0;
    }

    /* return check status */
    return reval;
}

/*!
    \brief      configure TRNG module
    \param[in]  none
    \param[out] none
    \retval     ErrStatus: SUCCESS or ERROR
*/
int trng_configuration(void)
{
    int reval = 0;

    /* TRNG module clock enable */
    rcu_periph_clock_enable(RCU_TRNG);

    /* TRNG registers reset */
    trng_deinit();
    trng_enable();
    /* check TRNG work status */
    if (!trng_ready_check()) {
        reval = -1;
        return reval;
    }

    return reval;
}

/*!
    \brief      get random value from trng
    \param[out] output: pointer to the random value
    \retval     0 if succeed, non-zero otherwise
*/
static int trng_data_get(unsigned int *rand)
{
    int ret;

    sys_mutex_get(&trng_lock);

    do {
        ret = trng_configuration();
        if (ret < 0)
            break;
        *rand = trng_get_true_random_data();
    } while(0);

    trng_close();

    sys_mutex_put(&trng_lock);
    return ret;
}

/*!
    \brief      get random value
    \param[in]  len: length of random value want to get
    \param[out] output: pointer to the random value
    \retval     0 if succeed, non-zero otherwise
*/
int random_get(unsigned char *output, unsigned int len)
{
    mbedtls_md5_context md5;
    unsigned char tmp[16];
    unsigned int n, total = len;
    unsigned char *p = output;
    unsigned int rand;
    int ret;

    if (randCount == 0)
        randCount = 0x39017842;

    if (btrng_seed) {
        ret = trng_data_get(&rand);
        if (ret < 0) {
            return ret;
        }
        sys_memcpy(tmp, &rand, sizeof(rand));
    } else {
        if (randPool == 0) {
            randPool = 0xDEADB00B;
        }
        sys_memcpy(tmp, &randPool, sizeof(randPool));
    }

    while (len > 0) {
        n = len;
        if (n > 16)
            n = 16;

        *(uint32_t *)(tmp + 12) += randCount;
        mbedtls_md5_ret((unsigned char *)tmp, 16, tmp);

        randCount++;
        sys_memcpy(p, tmp, n);
        p += n;
        len -= n;
    }
    if (!btrng_seed)
        randPool = *(unsigned int*)tmp;
    return 0;
}

/*!
    \brief      Entropy poll callback for gd hardware source
    \param[in]  data: pointer to the input data
    \param[in]  len: get random value length
    \param[out] output: pointer to the get random value
    \param[out] olen: pointer to the get random value length
    \retval     0: ok -1: not ok
*/
int gd_hardware_poll( void *data,
                           unsigned char *output, size_t len, size_t *olen )
{
    int ret;
    ((void) data);

    ret = random_get(output, len);
    if (ret < 0) {
        return -1;
    }
    if (olen)
        *olen = len;

    return( 0 );
}

/*!
    \brief      get TRNG value
    \param[in]  none
    \param[out] none
    \retval     getted TRNG value(0x00000000-0xffffffff)
*/
uint32_t trng_get(void)
{
    uint32_t rand;
    int ret;

    ret = trng_data_get((unsigned int *)&rand);
    if (ret < 0) {
        return 0xFFFFFFFF;
    }

    return rand;
}

/*!
    \brief      close TRNG
    \param[in]  none
    \param[out] none
    \retval     none
*/
void trng_close(void)
{
    trng_deinit();
    rcu_periph_clock_disable(RCU_TRNG);
}

/*!
    \brief      trng lock init
    \param[in]  none
    \param[out] none
    \retval     none
*/
void trng_lock_init(void)
{
    sys_mutex_init(&trng_lock);
}