/*
 *  SSLv3/TLSv1 server-side functions
 *
 *  Copyright (C) 2006-2015, ARM Limited, All Rights Reserved
 *  Copyright (c) 2021, GigaDevice Semiconductor Inc.
 *  SPDX-License-Identifier: Apache-2.0
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may
 *  not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 *  WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  This file is part of mbed TLS (https://tls.mbed.org)
 */

/* This file is part of mbed TLS (https://tls.mbed.org), some adjustments are
 * made according to GigaDevice chips.
 */

#if !defined(MBEDTLS_CONFIG_FILE)
#include "mbedtls/config.h"
#else
#include MBEDTLS_CONFIG_FILE
#endif

#if defined(MBEDTLS_X509_CRT_PARSE_C)
#include "mbedtls/x509_crt.h"
#endif


#if defined(MBEDTLS_X509_CRT_PARSE_C)

const mbedtls_x509_crt_profile mbedtls_x509_crt_profile_default =
	{
// #if defined(MBEDTLS_TLS_DEFAULT_ALLOW_SHA1_IN_CERTIFICATES)
		/* Allow SHA-1 (weak, but still safe in controlled environments) */
		MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA1) |
// #endif
			/* Only SHA-2 hashes */
			MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA224) |
			MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA256) |
			MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA384) |
			MBEDTLS_X509_ID_FLAG(MBEDTLS_MD_SHA512),
		0xFFFFFFF, /* Any PK alg    */
		0xFFFFFFF, /* Any curve     */
		1024,  // 2048,
};
#endif