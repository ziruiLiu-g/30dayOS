  [BITS 32]

  GLOBAL api_putchar, api_putstr, api_end
  GLOBAL api_open_win, api_putstr_win, api_boxfill_win

api_putchar:
    MOV     EDX, 1
    MOV     AL, [ESP+4]
    INT     0x40
    RET

api_end:
    MOV     EDX, 4
    INT     0x40

api_putstr:                 ; void api_putstr(char *s);
    PUSH    EBX
    MOV     EDX, 2
    MOV     EBX, [ESP+8]    ; s
    INT     0x40
    POP     EBX
    RET

api_open_win:            ; int api_open_win(char *buf, int xsize, int ysize, int col_inv, char *title);
    PUSH    EDI
    PUSH    ESI
    PUSH    EBX
    MOV     EDX, 5
    MOV     EBX, [ESP+16] ; buf
    MOV     ESI, [ESP+20] ; xsize
    MOV     EDI, [ESP+24] ; ysize
    MOV     EAX, [ESP+28] ; col_inv
    MOV     ECX, [ESP+32] ; title
    INT     0x40
    POP     EBX
    POP     ESI
    POP     EDI
    RET

api_putstr_win:           ; void api_putstr_win(int win, int x, int y, int col, int len, char *str);
    PUSH    EDI
    PUSH    ESI
    PUSH    EBP
    PUSH    EBX
    MOV     EDX, 6
    MOV     EBX, [ESP+20] ; win
    MOV     ESI, [ESP+24] ; x
    MOV     EDI, [ESP+28] ; y
    MOV     EAX, [ESP+32] ; col
    MOV     ECX, [ESP+36] ; len
    MOV     EBP, [ESP+40] ; str
    INT     0x40
    POP     EBX
    POP     EBP
    POP     ESI
    POP     EDI
    RET

api_boxfill_win:        ; void api_boxfill_win(int win, int x0, int y0, int x1, int y1, int col);
    PUSH    EDI
    PUSH    ESI
    PUSH    EBP
    PUSH    EBX
    MOV     EDX, 7
    MOV     EBX, [ESP+20] ; win
    MOV     EAX, [ESP+24] ; x0
    MOV     ECX, [ESP+28] ; y0
    MOV     ESI, [ESP+32] ; x1
    MOV     EDI, [ESP+36] ; y1
    MOV     EBP, [ESP+40] ; col
    INT     0x40
    POP     EBX
    POP     EBP
    POP     ESI
    POP     EDI
    RET
