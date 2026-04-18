#pragma once

#include "util.h"
#include <ncurses.h>

struct ui_label
{
    WINDOW *win;
    char    str[256];
    bool    redraw;
};

enum ui_textbox_show
{
    UI_TEXTBOX_SHOW_NORMAL,
    UI_TEXTBOX_SHOW_INVIS,
    UI_TEXTBOX_SHOW_ASTERISKS,
};

struct ui_textbox
{
    WINDOW *win;
    char    prompt[64];
    int     promptlen;
    char    content[256];
    int     len;
    int curpos; // Cursor position. If "curpos" == "len", then it is after the
                // last character.
    bool                 redraw;
    enum ui_textbox_show show; // Do not show content
};

enum cursor_state
{
    CURSOR_SLEEP,
    CURSOR_UNSLEEP,
    CURSOR_ON,
    CURSOR_OFF
};

void ui_label_init(struct ui_label *lb, int row, int col);
void ui_label_draw(struct ui_label *lb);
void ui_label_highlight(struct ui_label *lb, bool on);
void ui_label_setpos(struct ui_label *lb, int row, int col);
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
