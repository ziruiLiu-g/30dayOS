#include "sheet.h"

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

int cons_newline(int cursor_y, struct Sheet *sheet);
void console_task(struct Sheet *sheet, unsigned int memtotal);

#endif // _CONSOLE_H_
