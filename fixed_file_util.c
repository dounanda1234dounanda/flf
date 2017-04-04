/* vi: set ts=4 sw=4 sts=4 tw=80 expandtab fenc=utf-8 ff=unix ft=c : */
/*
 * fixed_file_util.c
 * Copyright (C) 2016 kazuhiro shimada <gekogeko5555@gmail.com>
 * 
 * flf is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * flf is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <string.h>
#include <stdlib.h>
#include <iconv.h>
#include <langinfo.h>
#include <math.h>

#include "fixed_file_util.h"

// =====================================================================
// encode
// =====================================================================

typedef struct {
    iconv_t icd;                // iconv descriptor
    char enc_fr[128];           // encode_from
    flf_bool_t add_shift;
} encode_t;

encode_t enc_tbl[5];

int enc_idx = 0;
int enc_cnt = 0;

// *********************************************************************
flf_bool_t encode_init(const char *encode)
// *********************************************************************
{
    int i;
    for (i = 0; i < enc_cnt; i++) {
        iconv_close(enc_tbl[i].icd);
    }

    enc_idx = 0;
    enc_cnt = 0;

    memset(enc_tbl, '\0', sizeof(enc_tbl));

    if (encode != NULL && strcmp(encode, "") != 0) {
        if (!encode_append(encode))
            return FLF_FALSE;
    }

    return FLF_TRUE;
}

// *********************************************************************
flf_bool_t encode_append(const char *encode)
// *********************************************************************
{
    int i;
    encode_t *p;

    if (!(enc_cnt < sizeof(enc_tbl) / sizeof(enc_tbl[0]))) {
        fprintf(stderr, "*** ERROR *** from encode MAX limit\n");
        return FLF_FALSE;
    }
    for (i = 0; i < enc_cnt; i++) {
        if (strcmp(encode, enc_tbl[i].enc_fr) == 0) {
            fprintf(stderr, "--- WARN --- duplicate fromencode[%s]\n",
                    enc_tbl[i].enc_fr);
            // do nothing
            return FLF_TRUE;
        }
    }

    p = &(enc_tbl[enc_cnt]);
    flf_strncpy(p->enc_fr, encode, sizeof(p->enc_fr));
    p->add_shift = FLF_FALSE;

    if (strstr(p->enc_fr, "@") != NULL) {
        char *r = NULL;
        p->add_shift = FLF_TRUE;
        strtok_r(p->enc_fr, "@", &r);
    }

    if ((p->icd = iconv_open(nl_langinfo(CODESET), p->enc_fr))
        == (iconv_t) - 1) {
        fprintf(stderr, "*** ERROR *** iconv_open[%s]->[%s]\n",
                p->enc_fr, nl_langinfo(CODESET));
        return FLF_FALSE;
    }

    enc_cnt++;
    enc_idx = enc_cnt - 1;

    return FLF_TRUE;
}

// *********************************************************************
void encode_first()
// *********************************************************************
{
    if (enc_cnt == 0)
        return;

    enc_idx = 0;
}

// *********************************************************************
void encode_next()
// *********************************************************************
{
    if (enc_cnt == 0)
        return;

    if (enc_idx < enc_cnt - 1)
        enc_idx++;
    else
        enc_idx = 0;
}

// *********************************************************************
void encode_get_list(char *list)
// *********************************************************************
{
    int i;

    strcpy(list, "");

    for (i = enc_idx; i < enc_cnt; i++) {
        if (list[0] != '\0')
            strcat(list, ",");
        strcat(list, enc_tbl[i].enc_fr);
        if (enc_tbl[i].add_shift)
            strcat(list, "@");

    }
    for (i = 0; i < enc_idx; i++) {
        if (list[0] != '\0')
            strcat(list, ",");
        strcat(list, enc_tbl[i].enc_fr);
        if (enc_tbl[i].add_shift)
            strcat(list, "@");
    }
}

// *********************************************************************
encode_t *encode_get_current()
// *********************************************************************
{
    if (enc_cnt == 0)
        return NULL;

    return &enc_tbl[enc_idx];
}

// *********************************************************************
flf_bool_t encode_iconv(const char *instr, const size_t instrlen,
                        char *otstr, const size_t otstrlen)
// *********************************************************************
{
    size_t inbytesleft = instrlen;
    size_t outbytesleft = otstrlen;

    char *in = (char *) instr;
    char *ot = otstr;
    iconv(encode_get_current()->icd, &in, &inbytesleft, &ot, &outbytesleft);
    if (inbytesleft != 0)
        return FLF_FALSE;

    return FLF_TRUE;
}

// =====================================================================
// search
// =====================================================================

// *********************************************************************
flf_bool_t search_rec_hex(ffi_t * ffi)
// *********************************************************************
{
    int i;
    for (i = 0; i < ffi->read_byte - ffi->search.cmp_len + 1; i++) {
        if (memcmp(&ffi->buf[i], ffi->search.hex, ffi->search.cmp_len)
            == 0) {
            memset(&ffi->search.hit_buf[i], '1', ffi->search.cmp_len);
            i += (ffi->search.cmp_len - 1);
        }
    }

    return FLF_TRUE;
}

// *********************************************************************
flf_bool_t search_rec_regex(ffi_t * ffi)
// *********************************************************************
{
    const char *s_start = (char *) ffi->ic_buf;
    char *h_start = (char *) ffi->search.hit_buf;

    while (1) {
        int i;
        regmatch_t pmatch;

        if (regexec(&ffi->search.preg, s_start, 1, &pmatch, 0)
            != 0)
            return FLF_TRUE;

        if (pmatch.rm_so == 0 && pmatch.rm_eo == 0)
            return FLF_TRUE;

        for (i = pmatch.rm_so; i < pmatch.rm_eo; i++)
            h_start[i] = '1';

        if ((long) ffi->read_byte <= pmatch.rm_eo + 1)
            return FLF_TRUE;

        s_start = s_start + pmatch.rm_eo;
        h_start = h_start + pmatch.rm_eo;
    }

    return FLF_TRUE;
}

// =====================================================================
// convert
// =====================================================================

// *********************************************************************
void char_conv(ffi_t * ffi)
// *********************************************************************
{
    size_t i;
    for (i = 0; i < ffi->read_byte; i++) {
        if (!encode_iconv((char *) &ffi->buf[i], 1,
                          (char *) &ffi->ic_buf[i], 1))
            ffi->ic_buf[i] = '?';

        else if (ffi->ic_buf[i] < 0X20 || 0X7E < ffi->ic_buf[i])
            ffi->ic_buf[i] = ' ';
    }
}

// *********************************************************************
flf_bool_t one_letter(mb_t * mb, char *str, size_t n)
// *********************************************************************
{
    size_t size;

    mb->len = 0;
    memset(mb->chr, '\0', sizeof(mb->chr));

    if (n < 1)
        return FLF_FALSE;

    if ((size = mblen(str, MB_CUR_MAX)) != n)
        return FLF_FALSE;

    mb->len = size;
    memcpy(mb->chr, str, mb->len);

    return FLF_TRUE;
}

// *********************************************************************
size_t insert_jis_shiftin(char *op, unsigned char *ip, size_t n)
// *********************************************************************
{
    const char JIS_SHIFT_IN[] = { 0x1B, 0x24, 0x42 };

    if (0x21 <= *ip && *ip <= 0x7e && 0x21 <= *(ip + 1) && *(ip + 1) <= 0x7e) {

        memset(op, '\0', n);
        memcpy(op, JIS_SHIFT_IN, sizeof(JIS_SHIFT_IN));
        memcpy(op + sizeof(JIS_SHIFT_IN), ip, 2);

        return sizeof(JIS_SHIFT_IN) + 2;
    }
    return 0;
}

// *********************************************************************
void mb_conv(ffi_t * ffi)
// *********************************************************************
{
    size_t bufidx = 0;
    while (bufidx < ffi->read_byte) {

        size_t inbytesleft = 0;
        flf_bool_t jis = FLF_FALSE;

        int trycnt;
        for (trycnt = MB_LEN_MAX; 0 < trycnt; trycnt--) {

            char ic[MB_LEN_MAX + 1];
            char *ip = NULL;

            char oc[MB_LEN_MAX + 1];
            char *op = oc;
            size_t outbytesleft = MB_LEN_MAX;

            if (encode_get_current()->add_shift
                && ((inbytesleft = insert_jis_shiftin(ic, &ffi->buf[bufidx],
                                                      sizeof(ic))
                    ) != 0)
                ) {
                ip = ic;
                jis = FLF_TRUE;

            } else {
                ip = (char *) &ffi->buf[bufidx];
                inbytesleft = trycnt;
                jis = FLF_FALSE;
            }

            memset(oc, '\0', sizeof(oc));
            iconv(encode_get_current()->icd,
                  &ip, &inbytesleft, &op, &outbytesleft);
            if (one_letter(&ffi->mb_buf[bufidx], oc, MB_LEN_MAX - outbytesleft))
                break;
        }
        if (trycnt == 0)
            bufidx++;
        else {
            if (jis)
                bufidx += 2;
            else
                bufidx += (trycnt - inbytesleft);
        }

    }
}

// =====================================================================
// string
// =====================================================================

// *********************************************************************
flf_bool_t str2posinum(char *str, size_t * num)
// *********************************************************************
{
    long rtn;
    char *end;

    *num = 0;
    rtn = strtol(str, &end, 10);

    if (*end != '\0')
        return FLF_FALSE;

    if (rtn < 0)
        return FLF_FALSE;

    *num = rtn;
    return FLF_TRUE;
}

// *********************************************************************
flf_bool_t str2hexnum(const char *str, unsigned char *hex, size_t * len)
// *********************************************************************
{
    int i;
    *len = 0;

    if (strlen(str) == 0 || strlen(str) % 2 != 0) {
        return FLF_FALSE;
    }

    for (i = 0; i < strlen(str); i += 2) {
        if (!chg_hexchar(str[i], &hex[i / 2], FFI_UPPER)) {
            return FLF_FALSE;
        }
        if (!chg_hexchar(str[i + 1], &hex[i / 2], FFI_LOWER)) {
            return FLF_FALSE;
        }
    }

    *len = strlen(str) / 2;
    return FLF_TRUE;
}

// *********************************************************************
char *flf_strncpy(char *dest, const char *src, int dest_size)
// *********************************************************************
{
    if (dest_size < 1)
        return NULL;

    dest[dest_size - 1] = '\0';
    return strncpy(dest, src, dest_size - 1);
}

// *********************************************************************
unsigned int digit(const unsigned int num)
// *********************************************************************
{
    if (num < 1) {
        return 1;
    }
    return (int) log10((double) num) + 1;
}

// =====================================================================
// hex
// =====================================================================

// *********************************************************************
unsigned char hexchar(const unsigned char in, const ffi_hex_t lu)
// *********************************************************************
{
    unsigned char c = 0;

    if (lu == FFI_LOWER) {
        c = in & 0x0F;
    }
    if (lu == FFI_UPPER) {
        c = in >> 4;
    }

    if (c <= 0X00 || c <= 0X09)
        return '0' + c;
    if (c <= 0X0A || c <= 0X0F)
        return 'A' + c - 0X0A;

    return '?';
}

// *********************************************************************
flf_bool_t chg_hexchar(const int k, unsigned char *c, const ffi_hex_t lu)
// *********************************************************************
{
    int hex;

    if ('0' <= k && k <= '9')
        hex = k - '0';
    else if ('a' <= k && k <= 'f')
        hex = k - 'a' + 0xA;
    else if ('A' <= k && k <= 'F')
        hex = k - 'A' + 0xA;
    else
        return FLF_FALSE;

    if (lu == FFI_LOWER) {
        *c = (*c & 0xF0) | hex;
    } else if (lu == FFI_UPPER) {
        *c = (*c & 0x0F) | (hex << 4);
    }

    return FLF_TRUE;
}
