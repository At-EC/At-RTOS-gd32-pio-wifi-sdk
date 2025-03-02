/*!
    \file    mqtt_ssl_config.c
    \brief   MQTT ssl config for GD32W51x WiFi SDK

    \version 2023-6-12, V1.0.3, firmware for GD32W51x
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

#include "lwip/apps/mqtt.h"
#ifdef LWIP_SSL_MQTT
#include "lwip/apps/mqtt_ssl_config.h"
#include "lwip/apps/mqtt_priv.h"
#include "altcp_tls.h"

static const char ca[] =
"-----BEGIN CERTIFICATE-----\r\n" \
"MIIDdTCCAl2gAwIBAgILBAAAAAABFUtaw5QwDQYJKoZIhvcNAQEFBQAwVzELMAkG\r\n" \
"A1UEBhMCQkUxGTAXBgNVBAoTEEdsb2JhbFNpZ24gbnYtc2ExEDAOBgNVBAsTB1Jv\r\n" \
"b3QgQ0ExGzAZBgNVBAMTEkdsb2JhbFNpZ24gUm9vdCBDQTAeFw05ODA5MDExMjAw\r\n" \
"MDBaFw0yODAxMjgxMjAwMDBaMFcxCzAJBgNVBAYTAkJFMRkwFwYDVQQKExBHbG9i\r\n" \
"YWxTaWduIG52LXNhMRAwDgYDVQQLEwdSb290IENBMRswGQYDVQQDExJHbG9iYWxT\r\n" \
"aWduIFJvb3QgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDaDuaZ\r\n" \
"jc6j40+Kfvvxi4Mla+pIH/EqsLmVEQS98GPR4mdmzxzdzxtIK+6NiY6arymAZavp\r\n" \
"xy0Sy6scTHAHoT0KMM0VjU/43dSMUBUc71DuxC73/OlS8pF94G3VNTCOXkNz8kHp\r\n" \
"1Wrjsok6Vjk4bwY8iGlbKk3Fp1S4bInMm/k8yuX9ifUSPJJ4ltbcdG6TRGHRjcdG\r\n" \
"snUOhugZitVtbNV4FpWi6cgKOOvyJBNPc1STE4U6G7weNLWLBYy5d4ux2x8gkasJ\r\n" \
"U26Qzns3dLlwR5EiUWMWea6xrkEmCMgZK9FGqkjWZCrXgzT/LCrBbBlDSgeF59N8\r\n" \
"9iFo7+ryUp9/k5DPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNVHRMBAf8E\r\n" \
"BTADAQH/MB0GA1UdDgQWBBRge2YaRQ2XyolQL30EzTSo//z9SzANBgkqhkiG9w0B\r\n" \
"AQUFAAOCAQEA1nPnfE920I2/7LqivjTFKDK1fPxsnCwrvQmeU79rXqoRSLblCKOz\r\n" \
"yj1hTdNGCbM+w6DjY1Ub8rrvrTnhQ7k4o+YviiY776BQVvnGCv04zcQLcFGUl5gE\r\n" \
"38NflNUVyRRBnMRddWQVDf9VMOyGj/8N7yy5Y0b2qvzfvGn9LhJIZJrglfCm7ymP\r\n" \
"AbEVtQwdpf5pLGkkeB6zpxxxYu7KyJesF12KwvhHhm4qxFYxldBniYUr+WymXUad\r\n" \
"DKqC5JlR3XC321Y9YeRq4VzW9v493kHMB65jUr9TU/Qr6cf9tveCX4XSQRjbgbME\r\n" \
"HMUfpIBvFSDJ3gyICh3WZlXi/EjJKSZp4A==\r\n" \
"-----END CERTIFICATE-----\r\n";

static const char client_crt[] =
"-----BEGIN CERTIFICATE-----\r\n" \
"MIIEmzCCA4OgAwIBAgIDHz8HMA0GCSqGSIb3DQEBCwUAMG4xCzAJBgNVBAYTAkNO\r\n" \
"MSMwIQYDVQQDDBpvbmxpbmUuaW90ZGV2aWNlLmJhaWR1LmNvbTEOMAwGA1UECgwF\r\n" \
"QkFJRFUxDDAKBgNVBAsMA0JDRTEcMBoGCSqGSIb3DQEJARYNaW90QGJhaWR1LmNv\r\n" \
"bTAeFw0yMzEwMTAwMjI0MjdaFw0zMzEwMDcwMjI0MjdaMEMxDjAMBgNVBAoMBUJh\r\n" \
"aWR1MQswCQYDVQQGEwJDTjEWMBQGA1UEAwwNYWZzdW16eS9mZWlnZTEMMAoGA1UE\r\n" \
"CwwDQkNFMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuAjdVrOTOLdB\r\n" \
"/rytGQzCmoC1edQiRlKVo43j4ymfma9/s4zDYOxjTlZ8TBQDT/Z4rzX3Tk/B84RS\r\n" \
"tHq/p0LVirdJsbINVjihMbJjtYtp8pecnH/MeY/Uj2IZ4TAz2K6xOsURe1Qo1CbV\r\n" \
"f5EkOXixegm6XSIZtAriYxMFSe9YtENT3feqCWvHP14nYQUdZEn+XfquBDrLrvo0\r\n" \
"rOmpV5HULE8Iq944JcGdNT5F2DMiHsfVXzYhdTDFekFtMuMZgZMpMweKoqclqkLC\r\n" \
"xCRrkyxTXe561hganjd+4biV4zYjywFXR62B39nAjLdB+I7z7lx73FuOnrir/ZUV\r\n" \
"vSJwAyipFQIDAQABo4IBazCCAWcwHQYDVR0OBBYEFPWrL+4RumdfIJQdF8X/reDU\r\n" \
"4SJmMAwGA1UdEwEB/wQCMAAwHwYDVR0jBBgwFoAUs+sTxDBPAMKn6xUOzNgrJ3YT\r\n" \
"ZFcwgaMGA1UdHwSBmzCBmDCBlaCBkqCBj4aBjGh0dHA6Ly9wa2lpb3YuYmFpZHVi\r\n" \
"Y2UuY29tL3YxL3BraS9jcmw/Y21kPWNybCZmb3JtYXQ9UEVNJmlzc3Vlcj1DPUNO\r\n" \
"LENOPW9ubGluZS5pb3RkZXZpY2UuYmFpZHUuY29tLEVNQUlMQUREUkVTUz1pb3RA\r\n" \
"YmFpZHUuY29tLE89QkFJRFUsT1U9QkNFMEIGCCsGAQUFBwEBBDYwNDAyBggrBgEF\r\n" \
"BQcwAYYmaHR0cDovL3BraWlvdi5iYWlkdWJjZS5jb20vdjEvcGtpL29jc3AwDgYD\r\n" \
"VR0PAQH/BAQDAgP4MB0GA1UdJQQWMBQGCCsGAQUFBwMCBggrBgEFBQcDBDANBgkq\r\n" \
"hkiG9w0BAQsFAAOCAQEAT8RG/U7fDKnNxZeuYoNEAIJmvEZjbNbEmJh/bB+y7/tY\r\n" \
"2nPW1moX40i2jL2Gbn0+5bvE845uXnyOUSe04HJ5PbaDB2MuNPbqgZAa+zPie5AD\r\n" \
"jvLgBNur5roV6n+viKv4uKRJIluheJTEvcAAvwY366j7T3zYSBHjuEMd3WjAZ8FF\r\n" \
"UNQo17chTQurWRo87EBS/SOOcfyEeKyo+1qEU+JW4f/GO6gE+mlLEbsU7byFRJYl\r\n" \
"KxLcfdvOfbd5PisolrkqDhol62xHfvOlqBaCowtNXbRGpRBHmVs6NHOCjajgHkNP\r\n" \
"i0dWZAfVwbinMvfflog9F1hjOCp/8VhCt6xXCKma3A==\r\n" \
"-----END CERTIFICATE-----\r\n";

static const char client_private_key[] =
"-----BEGIN RSA PRIVATE KEY-----\r\n" \
"MIIEpAIBAAKCAQEAuAjdVrOTOLdB/rytGQzCmoC1edQiRlKVo43j4ymfma9/s4zD\r\n" \
"YOxjTlZ8TBQDT/Z4rzX3Tk/B84RStHq/p0LVirdJsbINVjihMbJjtYtp8pecnH/M\r\n" \
"eY/Uj2IZ4TAz2K6xOsURe1Qo1CbVf5EkOXixegm6XSIZtAriYxMFSe9YtENT3feq\r\n" \
"CWvHP14nYQUdZEn+XfquBDrLrvo0rOmpV5HULE8Iq944JcGdNT5F2DMiHsfVXzYh\r\n" \
"dTDFekFtMuMZgZMpMweKoqclqkLCxCRrkyxTXe561hganjd+4biV4zYjywFXR62B\r\n" \
"39nAjLdB+I7z7lx73FuOnrir/ZUVvSJwAyipFQIDAQABAoIBAG2kcmIGSK7gl4Px\r\n" \
"2tryvDoadoQnu2fUKeywS1X6ZWjFozpQodJr41o3soQM5FBOkmYoq7dPU0kGy9NU\r\n" \
"0jwPWHP1cQVaBBIbWQXntvnhHnT2mMqwZR0DcOsf0jVUZ38vzM0rvZeRc2W54TbI\r\n" \
"PSG0Y2BGzW5RM6kNVwUZvuvmmh2CfaG0EwcsPe5N65jKtp6yQsb9iclJY/70CTFv\r\n" \
"5v7YyjLRBGdKu668PX7h5FjgHnEu1RHm3CsQQVkrldjCXAw37wv0yWAJ0eIXdG5z\r\n" \
"QKlXlrHFoTrv8CnqI+EDw+D2ABC+rEYPT4eukDnuuXiLe4rCVFrJjGP6CgLMu0bn\r\n" \
"nuMdg+ECgYEA/Ln64tRkYnzy8zCTKTTMX1LutnVlmENZmrpqHIh35eBjc0URxa7K\r\n" \
"aZ0DU4U+TN/qw1sqrhjDfk7EalD/OdmZcgyVnwzVcyq/oBphb6qBZXG7sqzcUkjT\r\n" \
"+L5EjiupzP1UjadA9r7cJxEs35b11885TowHwfV0q5fdx1SB1R5FL20CgYEAumsb\r\n" \
"rKAVDoY3tZ+svLAz74/1dA37iqjRvdxh5gaDEjjwCmT2PsbFthavA/TDGX2WkPcY\r\n" \
"lTnfAvWdGvthbr+wyQsvG2IvZARVFd2Ka4euh1ffzxMSQmQZgjJuQtbN01pi8qBe\r\n" \
"IohQge9PRkPZ9BRLMoaEi9BH4GVbbCbLOVJJz0kCgYEA8/Qa6CaZmDCA3JuBEn+y\r\n" \
"4DlP0LTWEvrAXgmgMFbTVgUaOsTreOVW4kf8U/0EvHRRS66PLmsdGqmyE+aH2DHi\r\n" \
"WyMmstdSm88iFswgTghKy7/TrZALRSqj4zLXPl2LlSLdIbfXj9eA7/02UcaJHX8d\r\n" \
"FGM7gdEMhC8emDFM+oozwqkCgYBLBqB0sVjIJ6x0JHdY2XGNkNqwgpAiFh+T0gZ0\r\n" \
"lVpbEx9Ij6mrSkR4LFjztqZus+TNIyV5qXjfsAoyuclU0UFNKHslAjcggb3ctvTm\r\n" \
"ogzT28HvjTVkEvVROQq3S8w6q732+CPqsgH0yWOWW+h0G1bIVusxefnzjddlh5dy\r\n" \
"0nNyQQKBgQDVN0yLWO6d0HUF+wLR6djvjF95zxtwSj12a75/bCxTDhVpDUJ8BBFW\r\n" \
"u/bfDYlHy/eRWYu+LYMWU+hKMjrxvnYsIgUnQx/yjaqGdmqBBYf+zBRR2K7M8X+N\r\n" \
"vx/QmG29Y5900d4UyVhV3VsycH8hE3iu0EGnLIg7xaFqtLaZ4XBuEg==\r\n" \
"-----END RSA PRIVATE KEY-----\r\n";

int mqtt_ssl_CA_cfg(mqtt_client_t *client, u8_t mutual_auth)
{
    if (mutual_auth) {
        client->tls_config = altcp_tls_create_config_client_2wayauth(
                                (u8_t *)ca, sizeof(ca),
                                (u8_t *)client_private_key, sizeof(client_private_key), NULL, 0,
                                (u8_t *)client_crt, sizeof(client_crt));
    } else {
        client->tls_config = altcp_tls_create_config_client((u8_t *)ca, sizeof(ca));
    }

    LWIP_ERROR("tls config failed!", (client->tls_config != NULL), return -1);
    return 0;
}

void mqtt_ssl_CA_free(mqtt_client_t *client)
{
    altcp_tls_free_config(client->tls_config);
    return;
}

#endif /*LWIP_SSL_MQTT*/