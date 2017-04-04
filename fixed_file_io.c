/* vi: set ts=4 sw=4 sts=4 tw=80 expandtab fenc=utf-8 ff=unix ft=c : */
/*
 * fixed_file_io.c
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
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "fixed_file_io.h"
#include "fixed_file_util.h"

// *********************************************************************
flf_bool_t ffi_open(const ffi_init_t init, ffi_t * ffi)
// *********************************************************************
{
    struct stat stat_buf;

    ffi->init = init;

    ffi->fp = NULL;
    ffi->mtime = 0;
    ffi->cnt = 0;
    ffi->size = 0;

    ffi->seq = 0;
    ffi->read_byte = 0;
    ffi->buf = NULL;
    ffi->ic_buf = NULL;
    ffi->mb_buf = NULL;

    ffi->search.method = -1;
    ffi->search.hit_buf = NULL;
    memset(ffi->search.str, 0, sizeof(ffi->search.str));
    ffi->search.flg = FLF_FALSE;

    memset(ffi->msg, '\0', sizeof(ffi->msg));

    // check file exists
    if ((ffi->fp = fopen(ffi->init.fn, "rb")) == NULL) {
        perror("***ERROR*** fopen");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }
    ffi_close(ffi);

    // check file exists
    if ((ffi->fp = fopen(ffi->init.fn, "rb+")) == NULL) {
        perror("***ERROR*** fopen");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    if (fstat(fileno(ffi->fp), &stat_buf) != 0) {
        perror("***ERROR*** fstat");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    ffi->size = stat_buf.st_size;
    ffi->mtime = stat_buf.st_mtime;

    if (ffi->init.len <= 0 || 99999 < ffi->init.len) {
        fprintf(stderr, "***ERROR*** reclen range must 1-99999 .\n");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    if ((ffi->buf =
         (unsigned char *) calloc((ffi->init.len + MB_LEN_MAX),
                                  sizeof(ffi->buf[0]))) == NULL) {
        perror("***ERROR*** calloc1");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    if ((ffi->ic_buf =
         (unsigned char *) calloc((ffi->init.len + 1),
                                  sizeof(ffi->ic_buf[0]))) == NULL) {
        perror("***ERROR*** calloc2");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    if ((ffi->mb_buf =
         (mb_t *) calloc((ffi->init.len + 1),
                         sizeof(ffi->mb_buf[0]))) == NULL) {
        perror("***ERROR*** calloc4");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    if ((ffi->search.hit_buf =
         (char *) calloc((ffi->init.len + 1),
                         sizeof(ffi->search.hit_buf[0]))) == NULL) {
        perror("***ERROR*** calloc5");
        ffi_close(ffi);
        exit(EXIT_FAILURE);
    }

    ffi->cnt = ffi->size / ffi->init.len;
    if (ffi->size % ffi->init.len != 0)
        ffi->cnt++;

    return ffi_read_jump(ffi, 1);
}

// *********************************************************************
flf_bool_t read_seq(ffi_t * ffi, const size_t seq, flf_bool_t mb_conv_flg)
// *********************************************************************
{
    strcpy(ffi->msg, "");
    ffi->search.flg = FLF_FALSE;
    memset(ffi->ic_buf, 0, sizeof(ffi->ic_buf[0]) * ffi->init.len);
    memset(ffi->search.hit_buf,
           '0', sizeof(ffi->search.hit_buf[0]) * ffi->init.len);
    memset(ffi->mb_buf, 0, sizeof(ffi->mb_buf[0]) * ffi->init.len);

    if (ffi->size % ffi->init.len != 0)
        sprintf(ffi->msg,
                "***ERROR*** Number of bytes is not a multiple of the record length.\n");

    if (fseek(ffi->fp, (seq - 1) * (ffi->init.len), SEEK_SET) != 0) {
        perror("***ERROR*** fseek");
        return FLF_FALSE;
    }

    ffi->seq = seq;
    ffi->read_byte =
        fread(ffi->buf, sizeof(ffi->buf[0]), ffi->init.len, ffi->fp);
    if (ferror(ffi->fp)) {
        perror("***ERROR*** fread");
        return FLF_FALSE;
    }

    char_conv(ffi);
    if (mb_conv_flg)
        mb_conv(ffi);

    if (ffi->read_byte != ffi->init.len) {
#pragma GCC diagnostic ignored "-Wformat"
        sprintf(ffi->msg,
                "***ERROR*** Reading of the fixed-length record.len=%d but read_byte=%d\n",
                ffi->init.len, ffi->read_byte);
#pragma GCC diagnostic warning "-Wformat"
        return FLF_FALSE;
    }

    if (ffi->search.method == METHOD_HEX || ffi->search.method == METHOD_REGEX) {
        char *rtn;

        if (ffi->search.method == METHOD_HEX) {
            if (!search_rec_hex(ffi))
                return FLF_FALSE;
        }
        if (ffi->search.method == METHOD_REGEX) {
            if (!search_rec_regex(ffi))
                return FLF_FALSE;
        }
        // search result msg
        if ((rtn = strchr(ffi->search.hit_buf, '1')) == NULL) {
            ffi->search.flg = FLF_FALSE;
            sprintf(ffi->msg, "### search miss current record ###\n");
        } else {
            ffi->search.flg = FLF_TRUE;
            sprintf(ffi->msg,
                    "!!! search hit !!! (position:%d)\n",
                    (int) (rtn - ffi->search.hit_buf + 1));
        }
    }

    return FLF_TRUE;
}

// *********************************************************************
flf_bool_t ffi_read_loc(ffi_t * ffi, ffi_loc_t loc, ffi_dir_t dir)
// *********************************************************************
{
    if (loc == EDGE) {
        if (dir == PREV)
            return read_seq(ffi, 1, FLF_TRUE);
        else if (dir == NEXT)
            return read_seq(ffi, ffi->size / ffi->init.len, FLF_TRUE);

    } else if (loc == CURRENT) {
        return read_seq(ffi, ffi->seq, FLF_TRUE);

    } else if (loc == SEQUENTIAL || loc == SEARCH) {

        flf_bool_t mb_conv_flg = FLF_TRUE;
        if (loc == SEARCH)
            mb_conv_flg = FLF_FALSE;

        if (dir == NEXT) {
            if (ffi->seq < ffi->cnt)
                return read_seq(ffi, ffi->seq + 1, mb_conv_flg);
            else {
                sprintf(ffi->msg, "### WARNING no data later ###\n");
                return FLF_FALSE;
            }
        } else if (dir == PREV) {
            if (1 < ffi->seq)
                return read_seq(ffi, ffi->seq - 1, mb_conv_flg);
            else {
                sprintf(ffi->msg, "### WARNING no data earlier ###\n");
                return FLF_FALSE;
            }
        }
    }

    return FLF_FALSE;
}

// *********************************************************************
flf_bool_t ffi_read_jump(ffi_t * ffi, size_t jump_row)
// *********************************************************************
{
    if (jump_row < 1) {
        return ffi_read_loc(ffi, EDGE, PREV);
    } else {
#pragma GCC diagnostic ignored "-Wsign-compare"
        if (ffi->cnt < jump_row)
            return ffi_read_loc(ffi, EDGE, NEXT);
#pragma GCC diagnostic warning "-Wsign-compare"
    }

    ffi->seq = jump_row;
    return ffi_read_loc(ffi, CURRENT, NONE);
}

// *********************************************************************
flf_bool_t ffi_read_search(ffi_t * ffi, ffi_dir_t dir)
// *********************************************************************
{
    size_t cur_seq = ffi->seq;

    do {
        if (!ffi_read_loc(ffi, SEARCH, dir))
            return FLF_FALSE;

        if (ffi->seq % 1000 == 0)
            ffi->init.msgout(STATUS, "Searching... [%*d/%*d]",
                             digit(ffi->cnt), ffi->seq, digit(ffi->cnt),
                             ffi->cnt);
    }
    while (1 < ffi->seq
           && ffi->seq < ffi->size / ffi->init.len && !ffi->search.flg);

    if (ffi->search.flg) {
        // search hit
        return ffi_read_loc(ffi, CURRENT, NONE);
    } else {
        ffi->seq = cur_seq;
        if (!ffi_read_loc(ffi, CURRENT, NONE))
            return FLF_FALSE;
        sprintf(ffi->msg, "### search miss other record ###\n");
    }

    return FLF_TRUE;
}

// *********************************************************************
flf_bool_t ffi_rewrite(ffi_t * ffi)
// *********************************************************************
{
    size_t seq = ffi->seq;
    size_t write_size;

    if (fseek(ffi->fp, (seq - 1) * (ffi->init.len), SEEK_SET) != 0) {
        perror("***ERROR*** fseek");
        return FLF_FALSE;
    }

    write_size = fwrite(ffi->buf, sizeof(ffi->buf[0]), ffi->read_byte, ffi->fp);
    if (write_size != ffi->read_byte) {
        perror("***ERROR*** fread");
#pragma GCC diagnostic ignored "-Wformat"
        sprintf(ffi->msg, "***ERROR*** fwrite write_size=%d\n", write_size);
#pragma GCC diagnostic warning "-Wformat"
        return FLF_FALSE;
    }
    ffi->seq = seq;
    if (!ffi_read_loc(ffi, CURRENT, NONE))
        return FLF_FALSE;

    return FLF_TRUE;
}

// *********************************************************************
void ffi_close(ffi_t * ffi)
// *********************************************************************
{
    if (ffi->fp != NULL) {
        if (fclose(ffi->fp) == EOF)
            perror("***ERROR*** fclose");
    }

    free(ffi->buf);
    ffi->buf = NULL;

    free(ffi->ic_buf);
    ffi->ic_buf = NULL;

    free(ffi->search.hit_buf);
    ffi->search.hit_buf = NULL;

    free(ffi->mb_buf);
    ffi->mb_buf = NULL;
}

// *********************************************************************
void ffi_print_info(const ffi_t * ffi)
// *********************************************************************
{
    fprintf(stderr, "=== file infomation =======================\n");
#pragma GCC diagnostic ignored "-Wformat"

    fprintf(stderr,
            "  filename:%s len:%lu size:%lu cnt:%lu\n",
            ffi->init.fn, ffi->init.len, ffi->size, ffi->cnt);

    fprintf(stderr, "  seq:%u read_size:%u mtime:%s", ffi->seq, ffi->read_byte,
            ctime(&(ffi->mtime))
        );

#pragma GCC diagnostic warning "-Wformat"
    fprintf(stderr, "===========================================\n");
}
