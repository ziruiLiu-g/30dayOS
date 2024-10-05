#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include "sheet.h"

struct Console {
  struct Sheet *sheet;
  int cur_x, cur_y, cur_c;
  struct Timer *timer;
};

void console_task(struct Sheet *sheet, unsigned int memtotal);

void cons_putchar(struct Console *cons, int ch, char move);
void cons_newline(struct Console *cons);
void cons_run_cmd(char *cmdline, struct Console *cons, int *fat, unsigned int memtotal);
void cons_putstr(struct Console *cons, char *s);
void cons_putnstr(struct Console *cons, char *s, int n);

struct Task *open_cons_task(struct Sheet *sht, unsigned int memtotal);
struct Sheet *open_console(struct Shtctl *shtctl, unsigned int memtotal);
void close_cons_task(struct Task *task);
void close_console(struct Sheet *sht);

#endif // _CONSOLE_H_
