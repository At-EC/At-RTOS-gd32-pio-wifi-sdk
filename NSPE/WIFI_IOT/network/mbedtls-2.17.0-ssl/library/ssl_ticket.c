/*
 *  TLS server tickets callbacks implementation
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

#if defined(MBEDTLS_SSL_TICKET_C)

#if defined(MBEDTLS_PLATFORM_C)
#include "mbedtls/platform.h"
#else
#include <stdlib.h>
#define mbedtls_calloc    calloc
#define mbedtls_free      free
#endif

#include "mbedtls/ssl_ticket.h"
#include "mbedtls/platform_util.h"
#ifndef MBEDTLS_ROM_TEST
#include <string.h>
#endif

/*
 * Initialze context
 */
void mbedtls_ssl_ticket_init( mbedtls_ssl_ticket_context *ctx )
{
    memset( ctx, 0, sizeof( mbedtls_ssl_ticket_context ) );

#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_init( &ctx->mutex );
#endif
}

#define MAX_KEY_BYTES 32    /* 256 bits */

#define TICKET_KEY_NAME_BYTES    4
#define TICKET_IV_BYTES         12
#define TICKET_CRYPT_LEN_BYTES   2
#define TICKET_AUTH_TAG_BYTES   16

#define TICKET_MIN_LEN ( TICKET_KEY_NAME_BYTES  +        \
                         TICKET_IV_BYTES        +        \
                         TICKET_CRYPT_LEN_BYTES +        \
                         TICKET_AUTH_TAG_BYTES )
#define TICKET_ADD_DATA_LEN ( TICKET_KEY_NAME_BYTES  +        \
                              TICKET_IV_BYTES        +        \
                              TICKET_CRYPT_LEN_BYTES )

/*
 * Generate/update a key
 */
static int ssl_ticket_gen_key( mbedtls_ssl_ticket_context *ctx,
                               unsigned char index )
{
    int ret;
    unsigned char buf[MAX_KEY_BYTES];
    mbedtls_ssl_ticket_key *key = ctx->keys + index;

#if defined(MBEDTLS_HAVE_TIME)
    key->generation_time = (uint32_t) mbedtls_time( NULL );
#endif

    if( ( ret = ctx->f_rng( ctx->p_rng, key->name, sizeof( key->name ) ) ) != 0 )
        return( ret );

    if( ( ret = ctx->f_rng( ctx->p_rng, buf, sizeof( buf ) ) ) != 0 )
        return( ret );

    /* With GCM and CCM, same context can encrypt & decrypt */
    ret = mbedtls_cipher_setkey( &key->ctx, buf,
                                 mbedtls_cipher_get_key_bitlen( &key->ctx ),
                                 MBEDTLS_ENCRYPT );

    mbedtls_platform_zeroize( buf, sizeof( buf ) );

    return( ret );
}

/*
 * Rotate/generate keys if necessary
 */
static int ssl_ticket_update_keys( mbedtls_ssl_ticket_context *ctx )
{
#if !defined(MBEDTLS_HAVE_TIME)
    ((void) ctx);
#else
    if( ctx->ticket_lifetime != 0 )
    {
        uint32_t current_time = (uint32_t) mbedtls_time( NULL );
        uint32_t key_time = ctx->keys[ctx->active].generation_time;

        if( current_time >= key_time &&
            current_time - key_time < ctx->ticket_lifetime )
        {
            return( 0 );
        }

        ctx->active = 1 - ctx->active;

        return( ssl_ticket_gen_key( ctx, ctx->active ) );
    }
    else
#endif /* MBEDTLS_HAVE_TIME */
        return( 0 );
}

/*
 * Setup context for actual use
 */
int mbedtls_ssl_ticket_setup( mbedtls_ssl_ticket_context *ctx,
    int (*f_rng)(void *, unsigned char *, size_t), void *p_rng,
    mbedtls_cipher_type_t cipher,
    uint32_t lifetime )
{
    int ret;
    const mbedtls_cipher_info_t *cipher_info;

    ctx->f_rng = f_rng;
    ctx->p_rng = p_rng;

    ctx->ticket_lifetime = lifetime;

    cipher_info = mbedtls_cipher_info_from_type( cipher);
    if( cipher_info == NULL )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    if( cipher_info->mode != MBEDTLS_MODE_GCM &&
        cipher_info->mode != MBEDTLS_MODE_CCM )
    {
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );
    }

    if( cipher_info->key_bitlen > 8 * MAX_KEY_BYTES )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    ret = mbedtls_cipher_setup_psa( &ctx->keys[0].ctx,
                                    cipher_info, TICKET_AUTH_TAG_BYTES );
    if( ret != 0 && ret != MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE )
        return( ret );
    /* We don't yet expect to support all ciphers through PSA,
     * so allow fallback to ordinary mbedtls_cipher_setup(). */
    if( ret == MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE )
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    if( ( ret = mbedtls_cipher_setup( &ctx->keys[0].ctx, cipher_info ) ) != 0 )
        return( ret );

#if defined(MBEDTLS_USE_PSA_CRYPTO)
    ret = mbedtls_cipher_setup_psa( &ctx->keys[1].ctx,
                                    cipher_info, TICKET_AUTH_TAG_BYTES );
    if( ret != 0 && ret != MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE )
        return( ret );
    if( ret == MBEDTLS_ERR_CIPHER_FEATURE_UNAVAILABLE )
#endif /* MBEDTLS_USE_PSA_CRYPTO */
    if( ( ret = mbedtls_cipher_setup( &ctx->keys[1].ctx, cipher_info ) ) != 0 )
        return( ret );

    if( ( ret = ssl_ticket_gen_key( ctx, 0 ) ) != 0 ||
        ( ret = ssl_ticket_gen_key( ctx, 1 ) ) != 0 )
    {
        return( ret );
    }

    return( 0 );
}

/*
 * Serialize a session in the following format:
 *
 * - If MBEDTLS_SSL_KEEP_PEER_CERTIFICATE is enabled:
 *    0       .   n-1   session structure, n = sizeof(mbedtls_ssl_session)
 *    n       .   n+2   peer_cert length = m (0 if no certificate)
 *    n+3     .   n+2+m peer cert ASN.1
 *
 * - If MBEDTLS_SSL_KEEP_PEER_CERTIFICATE is disabled:
 *    0       .   n-1   session structure, n = sizeof(mbedtls_ssl_session)
 *    n       .   n     length of peer certificate digest = k (0 if no digest)
 *    n+1     .   n+k   peer certificate digest (digest type encoded in session)
 */
static int ssl_save_session( const mbedtls_ssl_session *session,
                             unsigned char *buf, size_t buf_len,
                             size_t *olen )
{
    unsigned char *p = buf;
    size_t left = buf_len;
#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    size_t cert_len;
#else
    size_t cert_digest_len;
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_X509_CRT_PARSE_C */

    if( left < sizeof( mbedtls_ssl_session ) )
        return( MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL );

    /* This also copies the values of pointer fields in the
     * session to be serialized, but they'll be ignored when
     * loading the session through ssl_load_session(). */
    memcpy( p, session, sizeof( mbedtls_ssl_session ) );
    p += sizeof( mbedtls_ssl_session );
    left -= sizeof( mbedtls_ssl_session );

#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    if( session->peer_cert == NULL )
        cert_len = 0;
    else
        cert_len = session->peer_cert->raw.len;

    if( left < 3 + cert_len )
        return( MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL );

    *p++ = (unsigned char)( ( cert_len >> 16 ) & 0xFF );
    *p++ = (unsigned char)( ( cert_len >>  8 ) & 0xFF );
    *p++ = (unsigned char)( ( cert_len       ) & 0xFF );
    left -= 3;

    if( session->peer_cert != NULL )
        memcpy( p, session->peer_cert->raw.p, cert_len );

    p += cert_len;
    left -= cert_len;
#else /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
    if( session->peer_cert_digest != NULL )
        cert_digest_len = 0;
    else
        cert_digest_len = session->peer_cert_digest_len;

    if( left < 1 + cert_digest_len )
        return( MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL );

    *p++ = (unsigned char) cert_digest_len;
    left--;

    if( session->peer_cert_digest != NULL )
        memcpy( p, session->peer_cert_digest, cert_digest_len );

    p    += cert_digest_len;
    left -= cert_digest_len;
#endif /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_X509_CRT_PARSE_C */

    *olen = p - buf;

    return( 0 );
}

/*
 * Unserialise session, see ssl_save_session()
 */
static int ssl_load_session( mbedtls_ssl_session *session,
                             const unsigned char *buf, size_t len )
{
    const unsigned char *p = buf;
    const unsigned char * const end = buf + len;
#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    size_t cert_len;
#else
    size_t cert_digest_len;
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_X509_CRT_PARSE_C */

    if( sizeof( mbedtls_ssl_session ) > (size_t)( end - p ) )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    memcpy( session, p, sizeof( mbedtls_ssl_session ) );
    p += sizeof( mbedtls_ssl_session );

    /* Non-NULL pointer fields of `session` are meaningless
     * and potentially harmful. Zeroize them for safety. */
#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    session->peer_cert = NULL;
#else
    session->peer_cert_digest = NULL;
#endif /* !MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_X509_CRT_PARSE_C */
#if defined(MBEDTLS_SSL_SESSION_TICKETS) && defined(MBEDTLS_SSL_CLI_C)
    session->ticket = NULL;
#endif /* MBEDTLS_SSL_SESSION_TICKETS && MBEDTLS_SSL_CLI_C */

#if defined(MBEDTLS_X509_CRT_PARSE_C)
#if defined(MBEDTLS_SSL_KEEP_PEER_CERTIFICATE)
    /* Deserialize CRT from the end of the ticket. */
    if( 3 > (size_t)( end - p ) )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    cert_len = ( p[0] << 16 ) | ( p[1] << 8 ) | p[2];
    p += 3;

    if( cert_len != 0 )
    {
        int ret;

        if( cert_len > (size_t)( end - p ) )
            return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

        session->peer_cert = mbedtls_calloc( 1, sizeof( mbedtls_x509_crt ) );

        if( session->peer_cert == NULL )
            return( MBEDTLS_ERR_SSL_ALLOC_FAILED );

        mbedtls_x509_crt_init( session->peer_cert );

        if( ( ret = mbedtls_x509_crt_parse_der( session->peer_cert,
                                                p, cert_len ) ) != 0 )
        {
            mbedtls_x509_crt_free( session->peer_cert );
            mbedtls_free( session->peer_cert );
            session->peer_cert = NULL;
            return( ret );
        }

        p += cert_len;
    }
#else /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
    /* Deserialize CRT digest from the end of the ticket. */
    if( 1 > (size_t)( end - p ) )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    cert_digest_len = (size_t) p[0];
    p++;

    if( cert_digest_len != 0 )
    {
        if( cert_digest_len > (size_t)( end - p ) ||
            cert_digest_len != session->peer_cert_digest_len )
        {
            return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );
        }

        session->peer_cert_digest = mbedtls_calloc( 1, cert_digest_len );
        if( session->peer_cert_digest == NULL )
            return( MBEDTLS_ERR_SSL_ALLOC_FAILED );

        memcpy( session->peer_cert_digest, p, cert_digest_len );
        p += cert_digest_len;
    }
#endif /* MBEDTLS_SSL_KEEP_PEER_CERTIFICATE */
#endif /* MBEDTLS_X509_CRT_PARSE_C */

    if( p != end )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    return( 0 );
}

/*
 * Create session ticket, with the following structure:
 *
 *    struct {
 *        opaque key_name[4];
 *        opaque iv[12];
 *        opaque encrypted_state<0..2^16-1>;
 *        opaque tag[16];
 *    } ticket;
 *
 * The key_name, iv, and length of encrypted_state are the additional
 * authenticated data.
 */

int mbedtls_ssl_ticket_write( void *p_ticket,
                              const mbedtls_ssl_session *session,
                              unsigned char *start,
                              const unsigned char *end,
                              size_t *tlen,
                              uint32_t *ticket_lifetime )
{
    int ret;
    mbedtls_ssl_ticket_context *ctx = p_ticket;
    mbedtls_ssl_ticket_key *key;
    unsigned char *key_name = start;
    unsigned char *iv = start + TICKET_KEY_NAME_BYTES;
    unsigned char *state_len_bytes = iv + TICKET_IV_BYTES;
    unsigned char *state = state_len_bytes + TICKET_CRYPT_LEN_BYTES;
    unsigned char *tag;
    size_t clear_len, ciph_len;

    *tlen = 0;

    if( ctx == NULL || ctx->f_rng == NULL )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    /* We need at least 4 bytes for key_name, 12 for IV, 2 for len 16 for tag,
     * in addition to session itself, that will be checked when writing it. */
    if( end - start < TICKET_MIN_LEN )
        return( MBEDTLS_ERR_SSL_BUFFER_TOO_SMALL );

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    if( ( ret = ssl_ticket_update_keys( ctx ) ) != 0 )
        goto cleanup;

    key = &ctx->keys[ctx->active];

    *ticket_lifetime = ctx->ticket_lifetime;

    memcpy( key_name, key->name, TICKET_KEY_NAME_BYTES );

    if( ( ret = ctx->f_rng( ctx->p_rng, iv, TICKET_IV_BYTES ) ) != 0 )
        goto cleanup;

    /* Dump session state */
    if( ( ret = ssl_save_session( session,
                                  state, end - state, &clear_len ) ) != 0 ||
        (unsigned long) clear_len > 65535 )
    {
         goto cleanup;
    }
    state_len_bytes[0] = ( clear_len >> 8 ) & 0xff;
    state_len_bytes[1] = ( clear_len      ) & 0xff;

    /* Encrypt and authenticate */
    tag = state + clear_len;
    if( ( ret = mbedtls_cipher_auth_encrypt( &key->ctx,
                    iv, TICKET_IV_BYTES,
                    /* Additional data: key name, IV and length */
                    key_name, TICKET_ADD_DATA_LEN,
                    state, clear_len, state, &ciph_len,
                    tag, TICKET_AUTH_TAG_BYTES ) ) != 0 )
    {
        goto cleanup;
    }
    if( ciph_len != clear_len )
    {
        ret = MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        goto cleanup;
    }

    *tlen = TICKET_MIN_LEN + ciph_len;

cleanup:
#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

/*
 * Select key based on name
 */
static mbedtls_ssl_ticket_key *ssl_ticket_select_key(
        mbedtls_ssl_ticket_context *ctx,
        const unsigned char name[4] )
{
    unsigned char i;

    for( i = 0; i < sizeof( ctx->keys ) / sizeof( *ctx->keys ); i++ )
        if( memcmp( name, ctx->keys[i].name, 4 ) == 0 )
            return( &ctx->keys[i] );

    return( NULL );
}

/*
 * Load session ticket (see mbedtls_ssl_ticket_write for structure)
 */
int mbedtls_ssl_ticket_parse( void *p_ticket,
                              mbedtls_ssl_session *session,
                              unsigned char *buf,
                              size_t len )
{
    int ret;
    mbedtls_ssl_ticket_context *ctx = p_ticket;
    mbedtls_ssl_ticket_key *key;
    unsigned char *key_name = buf;
    unsigned char *iv = buf + TICKET_KEY_NAME_BYTES;
    unsigned char *enc_len_p = iv + TICKET_IV_BYTES;
    unsigned char *ticket = enc_len_p + TICKET_CRYPT_LEN_BYTES;
    unsigned char *tag;
    size_t enc_len, clear_len;

    if( ctx == NULL || ctx->f_rng == NULL )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

    if( len < TICKET_MIN_LEN )
        return( MBEDTLS_ERR_SSL_BAD_INPUT_DATA );

#if defined(MBEDTLS_THREADING_C)
    if( ( ret = mbedtls_mutex_lock( &ctx->mutex ) ) != 0 )
        return( ret );
#endif

    if( ( ret = ssl_ticket_update_keys( ctx ) ) != 0 )
        goto cleanup;

    enc_len = ( enc_len_p[0] << 8 ) | enc_len_p[1];
    tag = ticket + enc_len;

    if( len != TICKET_MIN_LEN + enc_len )
    {
        ret = MBEDTLS_ERR_SSL_BAD_INPUT_DATA;
        goto cleanup;
    }

    /* Select key */
    if( ( key = ssl_ticket_select_key( ctx, key_name ) ) == NULL )
    {
        /* We can't know for sure but this is a likely option unless we're
         * under attack - this is only informative anyway */
        ret = MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED;
        goto cleanup;
    }

    /* Decrypt and authenticate */
    if( ( ret = mbedtls_cipher_auth_decrypt( &key->ctx,
                    iv, TICKET_IV_BYTES,
                    /* Additional data: key name, IV and length */
                    key_name, TICKET_ADD_DATA_LEN,
                    ticket, enc_len,
                    ticket, &clear_len,
                    tag, TICKET_AUTH_TAG_BYTES ) ) != 0 )
    {
        if( ret == MBEDTLS_ERR_CIPHER_AUTH_FAILED )
            ret = MBEDTLS_ERR_SSL_INVALID_MAC;

        goto cleanup;
    }
    if( clear_len != enc_len )
    {
        ret = MBEDTLS_ERR_SSL_INTERNAL_ERROR;
        goto cleanup;
    }

    /* Actually load session */
    if( ( ret = ssl_load_session( session, ticket, clear_len ) ) != 0 )
        goto cleanup;

#if defined(MBEDTLS_HAVE_TIME)
    {
        /* Check for expiration */
        mbedtls_time_t current_time = mbedtls_time( NULL );

        if( current_time < session->start ||
            (uint32_t)( current_time - session->start ) > ctx->ticket_lifetime )
        {
            ret = MBEDTLS_ERR_SSL_SESSION_TICKET_EXPIRED;
            goto cleanup;
        }
    }
#endif

cleanup:
#if defined(MBEDTLS_THREADING_C)
    if( mbedtls_mutex_unlock( &ctx->mutex ) != 0 )
        return( MBEDTLS_ERR_THREADING_MUTEX_ERROR );
#endif

    return( ret );
}

/*
 * Free context
 */
void mbedtls_ssl_ticket_free( mbedtls_ssl_ticket_context *ctx )
{
    mbedtls_cipher_free( &ctx->keys[0].ctx );
    mbedtls_cipher_free( &ctx->keys[1].ctx );

#if defined(MBEDTLS_THREADING_C)
    mbedtls_mutex_free( &ctx->mutex );
#endif

    mbedtls_platform_zeroize( ctx, sizeof( mbedtls_ssl_ticket_context ) );
}

#endif /* MBEDTLS_SSL_TICKET_C */
