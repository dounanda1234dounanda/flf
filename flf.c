/* vi: set ts=4 sw=4 sts=4 tw=80 expandtab fenc=utf-8 ff=unix ft=c : */
/*
 * main.c
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

// **********************************************************************
//
//
// Fixed-Length Flat data file Editor
//
//
// **********************************************************************

#include <stdlib.h>
#include <string.h>
#include <curses.h>
#include <unistd.h>
#include <locale.h>
#include <langinfo.h>
#include <stdarg.h>

#include "fixed_file_util.h"
#include "fixed_file_io.h"

static const char *SRCFILE_ENCODE = "UTF-8";
static const char *welcome =
    "Fixed-Length-Flatfile View & Edit Ver.1.4 : 固定長flatfileの参照・更新ﾂｰﾙ";
static char welmsg[512] = "";

static const size_t DBGLINE = 0;
static const size_t SEQLINE = 1;

static const size_t DATALINE_START = 3;
static size_t DATALINE_WIDTH = 100;

static const size_t DATALINE_CNT = 5;   // all line + empty line
static const size_t DATALINE_MBCHAR = 0;
static const size_t DATALINE_GAGE = 1;
//static const size_t DATALINE_HEXUPPER = 2;
//static const size_t DATALINE_HEXLOWER = 3;

static flf_bool_t ALLOWEDIT_FLG = FLF_FALSE;
static flf_bool_t RECEDITING_FLG = FLF_FALSE;
static flf_bool_t EDITMODE_FLG = FLF_FALSE;

static WINDOW *win[10];
static int win_cnt = 0;

// *********************************************************************
void usage(const char *argv0)
// *********************************************************************
{
    fprintf(stderr,
            "Usage : %s [-e] [-w width] [-c fromcode[[,fromcode]...]] record-length file\n",
            argv0);
    exit(EXIT_FAILURE);
}

// *********************************************************************
void initialize_screen()
// *********************************************************************
{
    if (initscr() == NULL) {
        perror("*** ERROR *** initscr");
        exit(EXIT_FAILURE);
    }

    noecho();
    cbreak();
    keypad(stdscr, TRUE);

    //ESCDELAY = 100; // Solaris8 ERROR
}

// *********************************************************************
void display_message(ffi_msgline_t msgline, const char *fmt, ...)
// *********************************************************************
{
    char msg[512];
    va_list list;

    va_start(list, fmt);
    vsprintf(msg, fmt, list);
    va_end(list);

    if (msgline == STATUS)
        mvprintw(LINES - 1, 0, "%s", msg);

    if (msgline == DEBUG)
        mvprintw(DBGLINE, 0, "%s", msg);

    clrtoeol();
    refresh();
}

// *********************************************************************
void get_command(const int k, char *cmd_buf, const int n)
// *********************************************************************
{
    echo();
    mvprintw(LINES - 1, 0, "%c", k);
    clrtoeol();

    //getnstr (cmd_buf, n); // Solaris8 ERROR
    getstr(cmd_buf);

    noecho();
}

// *********************************************************************
void get_command_s(const char *s, char *cmd_buf, const int n)
// *********************************************************************
{
    echo();
    mvprintw(LINES - 1, 0, "%s", s);
    clrtoeol();

    //getnstr (cmd_buf, n); // Solaris8 ERROR
    getstr(cmd_buf);

    noecho();
}

// *********************************************************************
flf_bool_t get_answer(const char *msg)
// *********************************************************************
{
    int k;
    flf_bool_t ans = FLF_FALSE;

    while (1) {
        mvprintw(LINES - 1, 0, "%s(y/n):", msg);
        clrtoeol();
        k = getch();

        if (k == 'y' || k == 'Y') {
            ans = FLF_TRUE;
            break;
        } else if (k == 'n' || k == 'N') {
            ans = FLF_FALSE;
            break;
        }
    }

    return ans;
}

// *********************************************************************
void clear_search(ffi_t * ffi)
// *********************************************************************
{
    if (ffi->search.method == METHOD_REGEX) {
        regfree(&ffi->search.preg);
    }
    ffi->search.cmp_len = 0;
    memset(ffi->search.hex, '\0', sizeof(ffi->search.hex));
    flf_strncpy(ffi->search.str, "", sizeof(ffi->search.str));
}

// *********************************************************************
void dump_record(const ffi_t * ffi)
// *********************************************************************
{
    size_t bufidx = 0;
    unsigned int row;

    for (row = 0; row < win_cnt; row++) {
        delwin(win[row]);
    }
    for (row = 0;
         row < (ffi->read_byte + DATALINE_WIDTH - 1) / DATALINE_WIDTH; row++) {
        int col;
        int y = DATALINE_START + DATALINE_CNT * row;

        win[row] = newwin(4, DATALINE_WIDTH + 2, y + DATALINE_GAGE, 0);
        win_cnt = row + 1;

        if (EDITMODE_FLG)
            box(win[row], ACS_VLINE, ACS_HLINE);

        for (col = 0; col < DATALINE_WIDTH; col++) {
            if (ffi->read_byte == bufidx)
                break;

            if (ffi->search.hit_buf[bufidx] == '1') {
                attron(A_REVERSE);
                wattron(win[row], A_REVERSE);
            }

            if (((col + 1) / 10) % 2 == 1) {
                attron(A_BOLD);
                wattron(win[row], A_BOLD);
            }

            if ((col + 1) % 10 == 0)
                mvprintw(y + DATALINE_GAGE, col + 1, "%x", (col + 1) / 10);
            else if ((col + 1) % 5 == 0)
                mvprintw(y + DATALINE_GAGE, col + 1, "+");
            else
                mvprintw(y + DATALINE_GAGE, col + 1, "-");

            mvprintw(y + DATALINE_MBCHAR, col + 1, "%s",
                     ffi->mb_buf[bufidx].chr);

            mvwprintw(win[row], 1, col + 1, "%c",
                      hexchar(ffi->buf[bufidx], FFI_UPPER));

            mvwprintw(win[row], 2, col + 1, "%c",
                      hexchar(ffi->buf[bufidx], FFI_LOWER));

            bufidx++;
            attroff(A_BOLD);
            wattroff(win[row], A_BOLD);
            attroff(A_REVERSE);
            wattroff(win[row], A_REVERSE);

        }
        wrefresh(win[row]);
    }
    return;
}

// *********************************************************************
void display_screen(const ffi_t * ffi)
// *********************************************************************
{
    int x;
    char str[512];

    // screen all clear
    erase();

    display_message(DEBUG, "%s", welmsg);

    // ascreen message
    display_message(STATUS, "%s", ffi->msg);

    // screen header
    if (ffi->search.method == METHOD_REGEX) {
        mvprintw(SEQLINE, 0, "No:[%*d/%*d] search=[%s]", digit(ffi->cnt),
                 ffi->seq, digit(ffi->cnt), ffi->cnt, ffi->search.str);

    } else if (ffi->search.method == METHOD_HEX) {
        mvprintw(SEQLINE, 0, "No:[%*d/%*d] search-HEX=[%s]", digit(ffi->cnt),
                 ffi->seq, digit(ffi->cnt), ffi->cnt, ffi->search.str);

    } else {
        mvprintw(SEQLINE, 0, "No:[%*d/%*d]", digit(ffi->cnt),
                 ffi->seq, digit(ffi->cnt), ffi->cnt);
    }

    // screen detail
    dump_record(ffi);

    // screen footer
    attron(A_REVERSE);
    for (x = 0; x < COLS; x++)
        mvprintw(LINES - 2, x, " ");

    flf_strncpy(str, "[", sizeof(str));
    encode_get_list(str + strlen(str));
    sprintf(str + strlen(str), " -> %s]", nl_langinfo(CODESET));

    mvprintw(LINES - 2, 0, "%s byte:%d/%d",
             ffi->init.fn, ffi->read_byte, ffi->size);
    mvprintw(LINES - 2, COLS - strlen(str), "%s", str);
    attroff(A_REVERSE);

    // screen re-display
    refresh();
}

// ---------------------------------------------------------------------
typedef struct {
    int col;
    int row;
    int win_idx;
} flf_cur_pos_t;

// *********************************************************************
void move_cursor_prev(flf_cur_pos_t * cur_pos)
// *********************************************************************
{
    if (cur_pos->row == 2) {
        cur_pos->row = 1;
        return;
    }
    if (1 < cur_pos->col) {
        cur_pos->row = 2;
        cur_pos->col--;
        return;
    }
    if (0 < cur_pos->win_idx) {
        cur_pos->col = DATALINE_WIDTH;
        cur_pos->row = 2;
        cur_pos->win_idx--;
        return;
    }
}

// *********************************************************************
void move_cursor_next(flf_cur_pos_t * cur_pos)
// *********************************************************************
{
    if (cur_pos->row == 1) {
        cur_pos->row = 2;
        return;
    }
    if (cur_pos->col < DATALINE_WIDTH) {
        cur_pos->col++;
        cur_pos->row = 1;
        return;
    }
    if (cur_pos->win_idx < win_cnt - 1) {
        cur_pos->col = 1;
        cur_pos->row = 1;
        cur_pos->win_idx++;
        return;
    }
}

// *********************************************************************
void move_cursor(int k, flf_cur_pos_t * cur_pos)
// *********************************************************************
{
    switch (k) {
    case KEY_LEFT:
        if (1 < cur_pos->col)
            cur_pos->col--;
        break;
    case KEY_RIGHT:
        if (cur_pos->col < DATALINE_WIDTH)
            cur_pos->col++;
        break;
    case KEY_UP:
        if (cur_pos->row == 2)
            cur_pos->row = 1;
        else if (0 < cur_pos->win_idx) {
            cur_pos->win_idx--;
            cur_pos->row = 2;
        }
        break;
    case KEY_DOWN:
        if (cur_pos->row == 1)
            cur_pos->row = 2;
        else if (cur_pos->win_idx < win_cnt - 1) {
            cur_pos->win_idx++;
            cur_pos->row = 1;
        }
        break;
    }
}

// *********************************************************************
void edit_record(ffi_t * ffi)
// *********************************************************************
{
    flf_cur_pos_t cur_pos;

    cur_pos.col = 1;
    cur_pos.row = 1;
    cur_pos.win_idx = 0;

    EDITMODE_FLG = FLF_TRUE;
    display_screen(ffi);

    wmove(win[cur_pos.win_idx], cur_pos.row, cur_pos.col);
    wrefresh(win[cur_pos.win_idx]);
    RECEDITING_FLG = FLF_FALSE;

    while (1) {
        int k = getch();

        if (k == 27) {
            // ESCAPE key
            if (RECEDITING_FLG && get_answer("Record changed.save? ")) {
                ffi_rewrite(ffi);
            }
            break;

        } else if (k == KEY_LEFT || k == 'h') {
            move_cursor(KEY_LEFT, &cur_pos);
        } else if (k == KEY_RIGHT || k == 'l') {
            move_cursor(KEY_RIGHT, &cur_pos);
        } else if (k == KEY_UP || k == 'k') {
            move_cursor(KEY_UP, &cur_pos);
        } else if (k == KEY_DOWN || k == 'j') {
            move_cursor(KEY_DOWN, &cur_pos);

        } else if (k == KEY_STAB || k == KEY_BACKSPACE) {
            move_cursor_prev(&cur_pos);
        } else if (k == '\t' || k == ' ') {
            move_cursor_next(&cur_pos);

        } else if (k == 'w') {
            // output buffer
            if (RECEDITING_FLG) {
                ffi_rewrite(ffi);
                break;
            }

        } else {
            // data change
            size_t tmp = DATALINE_WIDTH * cur_pos.win_idx + cur_pos.col - 1;
            if (tmp < ffi->read_byte) {
                if (chg_hexchar
                    (k, &ffi->buf[tmp],
                     (cur_pos.row == 1 ? FFI_UPPER : FFI_LOWER))) {

                    RECEDITING_FLG = FLF_TRUE;
                    mvwprintw(win[cur_pos.win_idx], cur_pos.row, cur_pos.col,
                              "%c", k);
                    wrefresh(win[cur_pos.win_idx]);
                    move_cursor_next(&cur_pos);
                    sprintf(ffi->msg,
                            "Record changed. To save & return is 'w'key");
                }
            }
        }
        display_message(STATUS, "%s", ffi->msg);
        wmove(win[cur_pos.win_idx], cur_pos.row, cur_pos.col);
        wrefresh(win[cur_pos.win_idx]);
    }

    display_message(STATUS, "%s", ffi->msg);
    EDITMODE_FLG = FLF_FALSE;
}

// *********************************************************************
void analyze_arg(int argc, char *argv[], ffi_init_t * init)
// *********************************************************************
{
    int opt;

    flf_bool_t encode_flg = FLF_FALSE;

    if (argc < 1)
        usage("flf");

    while ((opt = getopt(argc, argv, "ec:w:")) != -1) {
        switch (opt) {
            char *tok;

        case 'e':
            ALLOWEDIT_FLG = FLF_TRUE;
            break;

        case 'c':
            tok = strtok(optarg, ",");
            while (tok != NULL) {
                if (!encode_append(tok)) {
                    perror("*** ERROR *** encode_append");
                    usage(argv[0]);
                }
                encode_flg = FLF_TRUE;

                tok = strtok(NULL, ",");
            }
            encode_first();
            break;

        case 'w':
            if (!str2posinum(optarg, &DATALINE_WIDTH)) {
                fprintf(stderr, "*** ERROR *** width not numeric[%s]\n",
                        optarg);
                usage(argv[0]);
            }
            if (DATALINE_WIDTH < 10 || 100 < DATALINE_WIDTH) {
                fprintf(stderr, "*** ERROR *** width must 10-100[%ld]\n",
                        DATALINE_WIDTH);
                usage(argv[0]);
            }
            break;
        }
    }

    if (argc - optind != 2)
        usage(argv[0]);

    if (!encode_flg) {
        if (!encode_append(SRCFILE_ENCODE)) {
            perror("*** ERROR *** encode_append");
            usage(argv[0]);
        }
    }

    printf("DATALINE_WIDTH=%ld\n", DATALINE_WIDTH);

    // record length
    if (!str2posinum(argv[optind], &init->len)) {
        fprintf(stderr,
                "*** ERROR *** record length is not positive number[%s]\n",
                argv[optind]);
        usage(argv[0]);
    }
    // filepath
    optind++;
    flf_strncpy(init->fn, argv[optind], sizeof(init->fn));

    // call back function
    init->msgout = &display_message;
}

// *********************************************************************
int main(int argc, char *argv[])
// *********************************************************************
{
    ffi_init_t init;
    ffi_t ffi;
    int prev_k = 0;
    ffi_dir_t dir = NONE;

    setlocale(LC_ALL, "");

    printf("MB_LEN_MAX=%d\n", MB_LEN_MAX);
#pragma GCC diagnostic ignored "-Wformat"
    printf("MB_CUR_MAX=%d\n", MB_CUR_MAX);
#pragma GCC diagnostic warning "-Wformat"

    if (!encode_init(SRCFILE_ENCODE)) {
        perror("*** ERROR *** encode_init");
        usage(argv[0]);
    }
    if (!encode_iconv(welcome, strlen(welcome), welmsg, sizeof(welmsg))) {
        perror("*** ERROR *** encode_iconv");
        usage(argv[0]);
    }
    encode_init("");

    analyze_arg(argc, argv, &init);

    ffi_open(init, &ffi);
    ffi_print_info(&ffi);

    initialize_screen();
    display_screen(&ffi);

    while (1) {
        int k = getch();

        if (k == 'q') {
            break;

        } else if (k == 'c') {
            encode_next();
            ffi_read_loc(&ffi, CURRENT, NONE);

        } else if (k == KEY_SLEFT || k == KEY_SR || (k == 'g' && prev_k == 'g'))
            ffi_read_loc(&ffi, EDGE, PREV);

        else if (k == KEY_LEFT || k == KEY_UP || k == 'h' || k == 'k')
            ffi_read_loc(&ffi, SEQUENTIAL, PREV);

        else if (k == KEY_RIGHT || k == KEY_DOWN || k == 'l' || k == 'j'
                 || k == ' ')
            ffi_read_loc(&ffi, SEQUENTIAL, NEXT);

        else if (k == KEY_SRIGHT || k == KEY_SF || k == 'G')
            ffi_read_loc(&ffi, EDGE, NEXT);

        else if (k == ':') {
            char cmd_buf[512];
            get_command(k, cmd_buf, sizeof(cmd_buf) - 1);

            if (strcmp(cmd_buf, "q") == 0)
                break;
            else {
                size_t num;
                if (!str2posinum(cmd_buf, &num)) {
                    sprintf(ffi.msg, "### numeric ERROR [%s]###\n", cmd_buf);
                } else {
                    ffi_read_jump(&ffi, num);
                }
            }

        } else if (k == 'b' || k == 'B') {
            char cmd_buf[512];
            get_command_s("search-HEX string:", cmd_buf, sizeof(cmd_buf) - 1);

            if (k == 'B')
                dir = PREV;
            if (k == 'b')
                dir = NEXT;

            clear_search(&ffi);

            if (!str2hexnum(cmd_buf, ffi.search.hex, &ffi.search.cmp_len)) {
                sprintf(ffi.msg, "### hex decimal ERROR [%s]###\n", cmd_buf);
                ffi.search.method = -1;
            } else {
                ffi.search.method = METHOD_HEX;
                flf_strncpy(ffi.search.str, cmd_buf, sizeof(ffi.search.str));
                ffi_read_loc(&ffi, CURRENT, NONE);
            }

        } else if (k == '/' || k == '?') {
            int rtn;
            char cmd_buf[512];
            get_command(k, cmd_buf, sizeof(cmd_buf) - 1);

            if (k == '?')
                dir = PREV;
            if (k == '/')
                dir = NEXT;

            clear_search(&ffi);

            if ((rtn = regcomp(&ffi.search.preg, cmd_buf, REG_EXTENDED)) != 0) {
                char errmsg[256];
                regerror(rtn, &ffi.search.preg, errmsg, sizeof(errmsg));
                sprintf(ffi.msg,
                        "*** ERROR *** regcomp [%s] %s\n", cmd_buf, errmsg);
                ffi.search.method = -1;
            } else {
                ffi.search.method = METHOD_REGEX;
                flf_strncpy(ffi.search.str, cmd_buf, sizeof(ffi.search.str));
                ffi_read_loc(&ffi, CURRENT, NONE);
            }

        } else if ((k == 'n' || k == 'N')
                   && (ffi.search.method == METHOD_HEX
                       || ffi.search.method == METHOD_REGEX)) {
            if (k == 'n')
                ffi_read_search(&ffi, dir);
            else {
                if (dir == NEXT)
                    ffi_read_search(&ffi, PREV);
                else if (dir == PREV)
                    ffi_read_search(&ffi, NEXT);
            }

        } else if (k == 'e') {
            if (!ALLOWEDIT_FLG) {
                sprintf(ffi.msg,
                        "*** ERROR *** read-only access mode, you must restart specify '-e' option");
            } else {
                edit_record(&ffi);
                ffi_read_loc(&ffi, CURRENT, NONE);
            }
        }

        prev_k = k;
        display_screen(&ffi);
    }

    endwin();
    fprintf(stderr, "\n");

    if (ffi.search.method == METHOD_REGEX) {
        regfree(&ffi.search.preg);
    }
    ffi_close(&ffi);

    return (EXIT_SUCCESS);
}
