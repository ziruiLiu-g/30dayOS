#include <stdio.h>

#include "api.h"
#include "console.h"
#include "graphic.h"
#include "task.h"
#include "sheet.h"
#include "window.h"

int *hrb_api(int edi, int esi, int ebp, int esp, int ebx, int edx, int ecx,
             int eax) {
  struct Console *cons = (struct Console *)*((int *) 0x0fec);
  int ds_base = *((int *) 0xfe8);
  struct Task *task = task_now();
  struct Shtctl *shtctl = (struct Shtctl *)*((int *)0x0fe4);
  struct Sheet *sht;

  int *reg = &eax + 1;

  if (edx == 1) {
    cons_putchar(cons, eax & 0xff, 1);
  } else if (edx == 2) {
    cons_putstr(cons, (char *)ebx + ds_base);
  } else if (edx == 3) {
    cons_putnstr(cons, (char *)ebx + ds_base, ecx);
  } else if (edx == 4) {
    return &(task->tss.esp0);
  } else if (edx == 5) {
    sht = sheet_alloc(shtctl);
    sheet_setbuf(sht, (char *) ebx + ds_base, esi, edi, eax);
    make_window8((char *) ebx + ds_base, esi, edi, (char *) ecx + ds_base, 0);
    sheet_slide(sht, 100, 50);
    sheet_updown(sht, 3);
    reg[7] = (int) sht;
  } else if (edx == 6) {  // print font on window
    sht = (struct Sheet *) ebx;
    put_fonts8_asc(sht->buf, sht->bxsize, esi, edi, eax, (char *) ebp + ds_base);
    sheet_refresh(sht, esi, edi, esi + ecx * 8, edi + 16);
  } else if (edx == 7) {  // draw block
    sht = (struct Sheet *) ebx;
    box_fill8(sht->buf, sht->bxsize, ebp, eax, ecx, esi, edi);
    sheet_refresh(sht, eax, ecx, esi + 1, edi + 1);
  }

  return 0;
}
