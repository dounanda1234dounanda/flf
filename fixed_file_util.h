/* vi: set ts=4 sw=4 sts=4 tw=80 expandtab fenc=utf-8 ff=unix ft=c : */
/*
 * fixed_file_util.h
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
#ifndef _fixed_file_util
#define _fixed_file_util

#include <iconv.h>
#include "flf.h"
#include "fixed_file_io.h"

// =====================================================================
// encode
// =====================================================================
flf_bool_t encode_init(const char *);

flf_bool_t encode_append(const char *);

void encode_first();

void encode_next();

void encode_get_list(char *);

flf_bool_t encode_iconv( const char *, const size_t,
                               char *, const size_t);

// =====================================================================
// search
// =====================================================================
flf_bool_t search_rec_regex(ffi_t *);

flf_bool_t search_rec_hex(ffi_t *);

// =====================================================================
// convert
// =====================================================================
void char_conv(ffi_t *);

void mb_conv(ffi_t *);

// =====================================================================
// string
// =====================================================================
flf_bool_t str2posinum(char *, size_t *);

flf_bool_t str2hexnum(const char *, unsigned char *, size_t *);

char *flf_strncpy(char *, const char *, int);

unsigned int digit(const unsigned int);

// =====================================================================
// hex
// =====================================================================

typedef enum { FFI_LOWER, FFI_UPPER } ffi_hex_t;

unsigned char hexchar(const unsigned char, const ffi_hex_t);

flf_bool_t chg_hexchar(const int, unsigned char *, const ffi_hex_t);

#endif
