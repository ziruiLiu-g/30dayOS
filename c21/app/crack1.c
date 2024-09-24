void api_end(void);

void main(void) {
    *((char *) 0x00102600) = 0;
    api_end();
}