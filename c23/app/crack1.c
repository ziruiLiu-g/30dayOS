void api_end(void);

int main(void) {
    *((char *) 0x00102600) = 0;
    api_end();

    return 0;
}