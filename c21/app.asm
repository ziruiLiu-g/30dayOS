    [BITS 32]
    GLOBAL start_app

start_app:                  ; void start_app(int eip, int cs, int esp, int ds, int *tss_esp0);
    PUSHAD                  ; store all 32bits reg
    MOV     EAX, [ESP+36]   ; eip for app
    MOV     ECX, [ESP+40]   ; cs
    MOV     EDX, [ESP+44]   ; esp
    MOV     EBX, [ESP+48]   ; ds/SS
    MOV     EBP, [ESP+52]   ; tss.esp
    MOV     [EBP], ESP      ; store os's esp to the tss.esp0(address)
    MOV     [EBP+4], SS
    MOV     ES, BX
    MOV     DS, BX
    MOV     FS, BX
    MOV     GS, BX

    OR      ECX, 3
    OR      EBX, 3
    PUSH    EBX
    PUSH    EDX
    PUSH    ECX
    PUSH    EAX
    RETF