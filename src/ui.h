#pragma once

#include "util.h"
#include <ncurses.h>

#define UI_LABEL_SIZE 256
#define UI_TEXTBOX_SIZE 256
#define UI_TEXTBOX_PROMPTSIZE 64
#define UI_SELBOX_PROMPTSIZE 64

struct ui_label
{
    WINDOW *win;
    char    str[UI_LABEL_SIZE];
    bool    redraw;
};

struct ui_textbox
{
    WINDOW *win;
    char    prompt[UI_TEXTBOX_PROMPTSIZE];
    int     promptlen;
    char    content[UI_TEXTBOX_SIZE];
    int     len;
    int curpos; // Cursor position. If "curpos" == "len", then it is after the
                // last character.
    bool redraw;
};

enum cursor_state
{
    CURSOR_SLEEP,
    CURSOR_UNSLEEP,
    CURSOR_ON,
    CURSOR_OFF
};

void ui_label_init(
    struct ui_label *lb, int row, int col, const char *fmt, ...
) PRINTFLIKE(4, 5);
void ui_label_draw(struct ui_label *lb);
void ui_label_highlight(struct ui_label *lb, bool on);
void
ui_label_update(struct ui_label *lb, const char *fmt, ...) PRINTFLIKE(2, 3);
void ui_label_clear(struct ui_label *lb);

void ui_textbox_init(
    struct ui_textbox *tb, int row, int col, const char *promptfmt, ...
) PRINTFLIKE(4, 5);
void ui_textbox_draw(struct ui_textbox *tb);
void ui_textbox_focus(struct ui_textbox *tb);
void ui_textbox_add(struct ui_textbox *tb, const char *text);
void ui_textbox_del(struct ui_textbox *tb, int n);
void ui_textbox_move(struct ui_textbox *tb, int n);
void ui_textbox_clear(struct ui_textbox *tb);

void set_cursor(enum cursor_state state);

