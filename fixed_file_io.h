/* vi: set ts=4 sw=4 sts=4 tw=80 expandtab fenc=utf-8 ff=unix ft=c : */
/*
 * fixed_file_io.h
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
#ifndef _fixed_file_io
#define _fixed_file_io

#include <stdio.h>
#include <stddef.h>
#include <time.h>
#include <regex.h>
#include <limits.h>

#include "flf.h"

// ------------------------------------------------------
typedef enum { EDGE, CURRENT, SEQUENTIAL, SEARCH } ffi_loc_t;

// ------------------------------------------------------
typedef enum { PREV, NEXT, NONE } ffi_dir_t;

// ------------------------------------------------------
typedef enum { STATUS, DEBUG } ffi_msgline_t;

// ------------------------------------------------------
typedef void (*FUNC_msgout) (const ffi_msgline_t, const char *fmt, ...);

// ------------------------------------------------------
typedef enum { METHOD_REGEX, METHOD_HEX } ffi_method_t;

// ------------------------------------------------------
typedef struct {
    int len;
    unsigned char chr[MB_LEN_MAX + 1];
} mb_t;

// ------------------------------------------------------
typedef struct {
    /* ffi_open input parameter */
    char fn[256];               // data filepath
    size_t len;                 // fixed-record length
    FUNC_msgout msgout;
} ffi_init_t;

// ------------------------------------------------------
typedef struct {
    /* ffi_read input parameter */
    ffi_loc_t loc;              // search location ( NEXT or PREV )
    ffi_method_t method;        // search method ( REGEX or HEX )

    regex_t preg;

    size_t cmp_len;
    unsigned char hex[256];     // hex search

    char str[512];              // input chars

    /* ffi_read output parameter */
    flf_bool_t flg;             // search result
    char *hit_buf;              // search hit byte position
} ffi_search_t;

// ------------------------------------------------------------
typedef struct {
    ffi_init_t init;

    /* ffi_open output parameter */
    FILE *fp;                   // data file descriptor
    size_t cnt;                 // data file fixed-record all record count
    time_t mtime;               // mtime of fstat infomation
    off_t size;                 // byte of fstat infomation

    ffi_search_t search;

    /* ffi_read output parameter */
    size_t seq;                 // current fixed-recoed sequence. first record is 1.
    size_t read_byte;           // readed byte
    unsigned char *buf;         // read buffer
    unsigned char *ic_buf;      // after iconv buffer
    mb_t *mb_buf;               // multibyte char for display screen

    /* ffi output message */
    char msg[256];              // result messag of file-IO & search
} ffi_t;


// =====================================================================
// function
// =====================================================================
flf_bool_t ffi_open(const ffi_init_t, ffi_t *);

void ffi_close(ffi_t *);

flf_bool_t ffi_read_loc(ffi_t *, ffi_loc_t, ffi_dir_t);

flf_bool_t ffi_read_jump(ffi_t *, size_t);

flf_bool_t ffi_read_search(ffi_t *, ffi_dir_t);

flf_bool_t ffi_rewrite(ffi_t *);

void ffi_print_info(const ffi_t *);

#endif
