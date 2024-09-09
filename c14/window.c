#include "window.h"
#include "graphic.h"

void make_window8(unsigned char *buf, int xsize, int ysize, char *title) {
    static char closebtn[14][16] = {
        "OOOOOOOOOOOOOOO@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQQQ@@QQQQQ$@",
        "OQQQQQ@@@@QQQQ$@",
        "OQQQQ@@QQ@@QQQ$@",
        "OQQQ@@QQQQ@@QQ$@",
        "OQQQQQQQQQQQQQ$@",
        "OQQQQQQQQQQQQQ$@",
        "O$$$$$$$$$$$$$$@",
        "@@@@@@@@@@@@@@@@"
    };

    int x, y;
    char c;

    box_fill8(buf, xsize, COL8_C6C6C6, 0, 0, xsize - 1, 0);
    box_fill8(buf, xsize, COL8_FFFFFF, 1, 1, xsize - 2, 1);
    box_fill8(buf, xsize, COL8_C6C6C6, 0, 0, 0, ysize - 1);
    box_fill8(buf, xsize, COL8_FFFFFF, 1, 1, 1, ysize - 2);
    box_fill8(buf, xsize, COL8_848484, xsize - 2, 1, xsize - 2, ysize - 2);
    box_fill8(buf, xsize, COL8_000000, xsize - 1, 0, xsize - 1, ysize - 1);
    box_fill8(buf, xsize, COL8_C6C6C6, 2, 2, xsize - 3, ysize - 3);
    box_fill8(buf, xsize, COL8_000084, 3, 3, xsize - 4, 20);
    box_fill8(buf, xsize, COL8_848484, 1, ysize - 2, xsize - 2, ysize - 2);
    box_fill8(buf, xsize, COL8_000000, 0, ysize - 1, xsize - 1, ysize - 1);
    put_fonts8_asc(buf, xsize, 24, 4, COL8_FFFFFF, title);

    for (int y = 0; y < 14; y++) {
        for (int x = 0; x < 16; x++) {
            char c = closebtn[y][x];
            switch (c) {
            case '@':
                c = COL8_000000;
                break;
            case '$':
                c = COL8_848484;
                break;
            case 'Q':
                c = COL8_C6C6C6;
                break;
            default:
                c = COL8_FFFFFF;
                break;
            }
            buf[(5 + y) * xsize + (xsize - 21 + x)] = c;
        }
    }
}

void make_textbox8(struct Sheet *sht, int x0, int y0, int sx, int sy, int c) {
  int x1 = x0 + sx, y1 = y0 + sy;

  box_fill8(sht->buf, sht->bxsize, COL8_848484, x0 - 2, y0 - 3, x1 + 1, y0 - 3);
  box_fill8(sht->buf, sht->bxsize, COL8_848484, x0 - 3, y0 - 3, x0 - 3, y1 + 1);
  box_fill8(sht->buf, sht->bxsize, COL8_FFFFFF, x0 - 3, y1 + 2, x1 + 1, y1 + 2);
  box_fill8(sht->buf, sht->bxsize, COL8_FFFFFF, x1 + 2, y0 - 3, x1 + 2, y1 + 2);
  box_fill8(sht->buf, sht->bxsize, COL8_000000, x0 - 1, y0 - 2, x1 + 0, y0 - 2);
  box_fill8(sht->buf, sht->bxsize, COL8_000000, x0 - 2, y0 - 2, x0 - 2, y1 + 0);
  box_fill8(sht->buf, sht->bxsize, COL8_C6C6C6, x0 - 2, y1 + 1, x1 + 1, y1 + 1);
  box_fill8(sht->buf, sht->bxsize, COL8_C6C6C6, x1 + 1, y0 - 2, x1 + 0, y1 + 1);
  box_fill8(sht->buf, sht->bxsize, c, x0 - 1, y0 - 1, x1 + 0, y1 + 0);
}
