#include <stdio.h>

#include "memory.h"
#include "sheet.h"

struct Shtctl *shtctl_init(struct MemMan *memman, unsigned char *vram,
                           int xsize, int ysize) {
  struct Shtctl *ctl =
      (struct Shtctl *)memman_alloc_4k(memman, sizeof(struct Shtctl));

  if (!ctl) {
    return NULL;
  }

  ctl->map = (unsigned char *) memman_alloc_4k(memman, xsize * ysize);
  if (ctl->map == 0) {
    memman_free_4k(memman, (int) ctl, sizeof (struct Shtctl));
    return NULL;
  }
  ctl->vram = vram;
  ctl->xsize = xsize;
  ctl->ysize = ysize;
  ctl->top = -1; // no sheets for now

  for (int i = 0; i < MAX_SHEETS; i++) {
    ctl->sheets0[i].flags = 0;
    ctl->sheets0[i].ctl = ctl;
  }

  return ctl;
}

struct Sheet *sheet_alloc(struct Shtctl *ctl) {
  struct Sheet *sht;
  for (int i = 0; i < MAX_SHEETS; i++) {
    if (ctl->sheets0[i].flags == 0) {
      sht = &ctl->sheets0[i];
      sht->flags = SHEET_USE;
      sht->height = -1;
      sht->task = 0;
      return sht;
    }
  }

  return 0;
}

void sheet_setbuf(struct Sheet *sht, unsigned char *buf, int xsize, int ysize,
                  int col_inv) {
  sht->buf = buf;
  sht->bxsize = xsize;
  sht->bysize = ysize;
  sht->col_inv = col_inv;
}

void sheet_updown(struct Sheet *sht, int height) {
  struct Shtctl *ctl = sht->ctl;
  int h, old = sht->height;

  if (height > ctl->top + 1) {
    height = ctl->top + 1;
  }
  if (height < -1) {
    height = -1;
  }

  sht->height = height;

  // rearrange
  if (old > height) {
    if (height >= 0) {
      for (h = old; h > height; h--) {
        ctl->sheets[h] = ctl->sheets[h - 1];
        ctl->sheets[h]->height = h;
      }
      ctl->sheets[height] = sht;
      sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize,
                     sht->vy0 + sht->bysize, height + 1);
      sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize,
                     sht->vy0 + sht->bysize, height + 1, old);
    } else {
      if (ctl->top > old) {
        for (h = old; h < ctl->top; h++) {
          ctl->sheets[h] = ctl->sheets[h + 1];
          ctl->sheets[h]->height = h;
        }
      }
      ctl->top--;
    }
    sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize,
                     sht->vy0 + sht->bysize, 0);
    sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize,
                     sht->vy0 + sht->bysize, 0, old - 1);
  } else if (old < height) {
    if (old >= 0) {
      for (h = old; h < height; h++) {
        ctl->sheets[h] = ctl->sheets[h + 1];
        ctl->sheets[h]->height = h;
      }
      ctl->sheets[height] = sht;
    } else {
      for (h = ctl->top; h >= height; h--) {
        ctl->sheets[h + 1] = ctl->sheets[h];
        ctl->sheets[h + 1]->height = h + 1;
      }
      ctl->sheets[height] = sht;
      ctl->top++;
    }
    sheet_refreshmap(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize,
                     sht->vy0 + sht->bysize, height);
    sheet_refreshsub(ctl, sht->vx0, sht->vy0, sht->vx0 + sht->bxsize,
                     sht->vy0 + sht->bysize, height, height);
  }
}

void sheet_refreshsub(struct Shtctl *ctl, int vx0, int vy0, int vx1, int vy1, int h0, int h1) {
  unsigned char *buf, *vram = ctl->vram, *map = ctl->map;
  struct Sheet *sht;
  int bx, by, vx, vy;

  if (vx0 < 0) {
    vx0 = 0;
  }
  if (vy0 < 0) {
    vy0 = 0;
  }
  if (vx1 > ctl->xsize) {
    vx1 = ctl->xsize;
  }
  if (vy1 > ctl->ysize) {
    vy1 = ctl->ysize;
  }

  for (int h = h0; h <= h1; h++) {
    sht = ctl->sheets[h];
    buf = sht->buf;
    unsigned char sid = sht - ctl->sheets0;

    // 使用vx0 ~ vy1，对bx0 ~ by1进行倒推
    int bx0 = vx0 - sht->vx0;
    int by0 = vy0 - sht->vy0;
    int bx1 = vx1 - sht->vx0;
    int by1 = vy1 - sht->vy0;

    if (bx0 < 0) {
      bx0 = 0;
    }
    if (by0 < 0) {
      by0 = 0;
    }
    if (bx1 > sht->bxsize) {
      bx1 = sht->bxsize;
    }
    if (by1 > sht->bysize) {
      by1 = sht->bysize;
    }

    if (!(sht->vx0 & 3)) {
      int i = (bx0 + 3) / 4;
      int i1 = bx1 / 4;
      i1 = i1 - i;
      int sid4 = sid | sid4 << 8 | sid4 << 16 | sid4 << 24;
      for (by = by0; by < by1; by++) {
        vy = sht->vy0 + by;
        for (bx = bx0; bx < bx1; bx++) {
          vx = sht->vx0 + bx;
          if (map[vy * ctl->xsize + vx] == sid) {
            vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
          }
        }
        vx = sht->vx0 + bx;
        int *p = (int *)&map[vy * ctl->xsize + vx];
        int *q = (int *)&vram[vy * ctl->xsize + vx];
        int *r = (int *)&buf[by * sht->bxsize + bx];
        for (i = 0; i < i1; i++) {
          if (p[i] == sid4) {
            q[i] = r[i];
          } else {
            int bx2 = bx + i * 4;
            vx = sht->vx0 + bx2;
            if (map[vy * ctl->xsize + vx + 0] == sid) {
              vram[vy * ctl->xsize + vx + 0] = buf[by * sht->bxsize + bx2 + 0];
            }
            if (map[vy * ctl->xsize + vx + 1] == sid) {
              vram[vy * ctl->xsize + vx + 1] = buf[by * sht->bxsize + bx2 + 1];
            }
            if (map[vy * ctl->xsize + vx + 2] == sid) {
              vram[vy * ctl->xsize + vx + 2] = buf[by * sht->bxsize + bx2 + 2];
            }
            if (map[vy * ctl->xsize + vx + 3] == sid) {
              vram[vy * ctl->xsize + vx + 3] = buf[by * sht->bxsize + bx2 + 3];
            }
          }
        }

        for (bx += i1 * 4; bx < bx1; bx++) {
          vx = sht->vx0 + bx;
          if (map[vy * ctl->xsize + vx] == sid) {
            vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
          }
        }
      }
    } else {
      for (by = by0; by < by1; by++) {
        vy = sht->vy0 + by;
        for (bx = bx0; bx < bx1; bx++) {
          vx = sht->vx0 + bx;
          if (map[vy * ctl->xsize + vx] == sid) {
            vram[vy * ctl->xsize + vx] = buf[by * sht->bxsize + bx];
          }
        }
      }
    }
  }
}

void sheet_refresh(struct Sheet *sht, int bx0, int by0,
                   int bx1, int by1) {
  if (sht->height >= 0) {
    // 如果正在显示，则按新图层的信息刷新画面
    sheet_refreshsub(sht->ctl, sht->vx0 + bx0, sht->vy0 + by0, sht->vx0 + bx1,
                     sht->vy0 + by1, sht->height, sht->height);
  }
}

void sheet_refreshmap(struct Shtctl *ctl, int vx0, int vy0, int vx1, int vy1, int h0) {
  int h, bx, by, vx, vy, bx0, by0, bx1, by1;
  int sid4, *p;

  unsigned char *buf, sid, *map = ctl->map;
  struct Sheet *sht;

  if (vx0 < 0) {vx0 = 0;}
  if (vy0 < 0) {vy0 = 0;}
  if (vx1 > ctl->xsize) {vx1 = ctl->xsize;}
  if (vy1 > ctl->ysize) {vy1 = ctl->ysize;}

  for (int h = h0; h <= ctl->top; h++) {
    sht = ctl->sheets[h];
    sid = sht - ctl->sheets0;
    buf = sht->buf;
    // 使用vx0 ~ vy1，对bx0 ~ by1进行倒推
    int bx0 = vx0 - sht->vx0;
    int by0 = vy0 - sht->vy0;
    int bx1 = vx1 - sht->vx0;
    int by1 = vy1 - sht->vy0;

    if (bx0 < 0) {
      bx0 = 0;
    }
    if (by0 < 0) {
      by0 = 0;
    }
    if (bx1 > sht->bxsize) {
      bx1 = sht->bxsize;
    }
    if (by1 > sht->bysize) {
      by1 = sht->bysize;
    }

    if (sht->col_inv == -1) {
      if ((sht->vx0 & 3) == 0 && (bx0 & 3) == 0 && (bx1 & 3) == 0) {
        bx1 = (bx1 - bx0) / 4; // move times
        sid4 = sid | sid << 8 | sid << 16 | sid << 24;
        for (int by = by0; by < by1; by++) {
          int vy = sht->vy0 + by;
          int vx = sht->vx0 + bx0;
          p = (int *) &map[vy * ctl->xsize + vx];
          for (int bx = 0; bx < bx1; bx++) {
            p[bx] = sid4;
          }
        }
      } else {
        for (int by = by0; by < by1; by++) {
          int vy = sht->vy0 + by;
          for (int bx = bx0; bx < bx1; bx++) {
            int vx = sht->vx0 + bx;
            map[vy * ctl->xsize + vx] = sid;
          }
        }
      }
    } else {
      for (int by = by0; by < by1; by++) {
        int vy = sht->vy0 + by;
        for (int bx = bx0; bx < bx1; bx++) {
          int vx = sht->vx0 + bx;
          if (buf[by * sht->bxsize + bx] != sht->col_inv) {
            map[vy * ctl->xsize + vx] = sid;
          }
        }
      }
    }
  }
}

void sheet_slide(struct Sheet *sht, int vx0, int vy0) {
    int old_vx0 = sht->vx0, old_vy0 = sht->vy0;
    sht->vx0 = vx0;
    sht->vy0 = vy0;
    if (sht->height >= 0) {
        sheet_refreshmap(sht->ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0);
        sheet_refreshmap(sht->ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height);
        sheet_refreshsub(sht->ctl, old_vx0, old_vy0, old_vx0 + sht->bxsize, old_vy0 + sht->bysize, 0, sht->height - 1);
        sheet_refreshsub(sht->ctl, vx0, vy0, vx0 + sht->bxsize, vy0 + sht->bysize, sht->height, sht->height);
    }

    return;
}

void sheet_free(struct Sheet *sht) {
  if (sht->height >= 0) {
    sheet_updown(sht, -1);
  }

  sht->flags = 0;
  return;
}