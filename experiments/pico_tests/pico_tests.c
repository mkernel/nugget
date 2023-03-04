#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"


int main()
{
    stdio_init_all();
    char buf[32];
    char *current=buf;
    while(true) {
        int fetched = getchar_timeout_us(0);
        if(fetched != PICO_ERROR_TIMEOUT) {
            char input=(char)fetched;

            if(input=='\r' || input=='\n') {
                puts("");
                *current=0;
                current=buf;
                if(strcmp(current,"AT")==0) {
                    puts("OK - experiments v0.1");
                } else {
                    puts("invalid command: ");
                    puts(current);
                }
            } else {
                putchar(input);
                *current=input;
                current++;
            }
        }
    }

    return 0;
}
