#include "ui.h"
#include "util.h"
#include <ncurses.h>
#include <stdarg.h>
#include <string.h>

void
ui_label_init(struct ui_label *lb, int row, int col)
{
    lb->win = newwin(1, COLS, row, col);
}

void
ui_label_draw(struct ui_label *lb)
{
    if (!lb->redraw || lb->win == NULL)
        return;

    werase(lb->win);
    mvwprintw(lb->win, 0, 0, "%s", lb->str);
    wnoutrefresh(lb->win);
    lb->redraw = false;
}

/*
 * Apply bold and reverse attributes to label if "on", otherwise off.
 */
void
ui_label_highlight(struct ui_label *lb, bool on)
{
    if (lb->win == NULL)
        return;
    if (on)
        wattron(lb->win, A_REVERSE | A_BOLD);
    else
        wattroff(lb->win, A_REVERSE | A_BOLD);
    lb->redraw = true;
}

void
ui_label_setpos(struct ui_label *lb, int row, int col)
{
    if (lb->win == NULL)
        return;

    mvwin(lb->win, row, col);
}

void
ui_label_update(struct ui_label *lb, const char *fmt, ...)
{
    va_list ap;

    if (fmt == NULL)
        *lb->str = NUL;
    else
    {
        va_start(ap, fmt);
        vsnprintf(lb->str, sizeof(lb->str), fmt, ap);
        va_end(ap);
    }
    lb->redraw = true;
}

void
ui_label_clear(struct ui_label *lb)
{
    if (lb->win != NULL)
        delwin(lb->win);
}

/*
 * Initialize a new textbox at the given position on screen. Returns OK on
 * success and FAIL on failure.
 */
void
ui_textbox_init(
    struct ui_textbox *tb, int row, int col, const char *promptfmt, ...
)
{
    tb->win = newwin(1, COLS, row, col);

    va_list ap;

    va_start(ap, promptfmt);
    vsnprintf(tb->prompt, sizeof(tb->prompt), promptfmt, ap);
    va_end(ap);
    tb->promptlen = strlen(tb->prompt);

    tb->content[0] = NUL;
    tb->len = 0;
    tb->curpos = 0;
    tb->redraw = true;
}

/*
 * If textbox has been updated, then redraw it on the screen. Returns true if
 * did something.
 */
void
ui_textbox_draw(struct ui_textbox *tb)
{
    if (!tb->redraw || tb->win == NULL)
        return;

    werase(tb->win);
    if (tb->show == UI_TEXTBOX_SHOW_NORMAL)
        mvwprintw(tb->win, 0, 0, "%s%.*s", tb->prompt, tb->len, tb->content);
    else if (tb->show == UI_TEXTBOX_SHOW_ASTERISKS)
    {
        mvwprintw(tb->win, 0, 0, "%s", tb->prompt);
        for (int i = 0; i < tb->len; i++)
            waddch(tb->win, '*');
    }
    else
        mvwprintw(tb->win, 0, 0, "%s", tb->prompt);
    wnoutrefresh(tb->win);
    tb->redraw = false;
}

/*
 * Position cursor in the textbox
 */
void
ui_textbox_focus(struct ui_textbox *tb)
{
    if (tb->win == NULL)
        return;
    if (tb->show == UI_TEXTBOX_SHOW_INVIS)
        wmove(tb->win, 0, tb->promptlen);
    else
        wmove(tb->win, 0, tb->promptlen + tb->curpos);
    wrefresh(tb->win);
}

/*
 * Add text before cursor position in textbox
 */
void
ui_textbox_add(struct ui_textbox *tb, const char *text)
{
    int len = strlen(text);

    if (tb->len + len >= (int)sizeof(tb->content))
        return;

    // Shift text after cursor so it isn't overwritten
    if (tb->curpos < tb->len)
        memmove(
            tb->content + tb->curpos + len, tb->content + tb->curpos,
            tb->len - tb->curpos
        );
    memcpy(tb->content + tb->curpos, text, len);
    tb->len += len;
    tb->curpos += len;
    tb->redraw = true;
}

/*
 * Delete "n" characters before cursor position in textbox.
 */
void
ui_textbox_del(struct ui_textbox *tb, int n)
{
    if (tb->curpos == 0 || tb->len == 0)
        return;

    if (tb->len - n < 0)
        // Clamp value
        n = tb->len;

    memmove(
        tb->content + tb->curpos - n, tb->content + tb->curpos,
        tb->len - tb->curpos
    );
    tb->len -= n;
    tb->curpos -= n;
    tb->redraw = true;
}

/*
 * Move the cursor "n" cells in the textbox.
 */
void
ui_textbox_move(struct ui_textbox *tb, int n)
{
    tb->curpos += n;

    if (tb->curpos < 0)
        tb->curpos = 0;
    else if (tb->curpos > tb->len)
        tb->curpos = tb->len;
}

void
ui_textbox_clear(struct ui_textbox *tb)
{
    if (tb->win != NULL)
        delwin(tb->win);
}

static bool CURSOR_VISIBILITY = true;
static bool CURSOR_SLEEPING = false;

void
set_cursor(enum cursor_state state)
{
    if (state == CURSOR_UNSLEEP)
        CURSOR_SLEEPING = false;
    else if (state == CURSOR_SLEEP)
        CURSOR_SLEEPING = true;
    else if (state == CURSOR_ON)
        CURSOR_VISIBILITY = true;
    else if (state == CURSOR_OFF)
        CURSOR_VISIBILITY = false;

    if (!CURSOR_SLEEPING)
    {
        if (CURSOR_VISIBILITY)
            curs_set(1);
    }
    else
        curs_set(0);
}
