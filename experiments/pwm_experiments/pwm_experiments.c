#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/pwm.h"



int main()
{
    stdio_init_all();
    int wrap;
    int level;

    char buf[32];
    char *current=buf;
    while(true) {
        int fetched = getchar_timeout_us(500);
        if(fetched != PICO_ERROR_TIMEOUT) {
            char input=(char)fetched;

            if(input=='\r' || input=='\n') {
                puts("");
                *current=0;
                current=buf;
                if(strcmp(current,"AT")==0) {
                    puts("OK - pwm experiments 0.1");
                } else if(strcmp(current,"ATREBOOT")==0) {
                    watchdog_enable(1,1);
                    while(true);
                } else if(strcmp(current,"ATENABLE")==0) {
                    int slice = pwm_gpio_to_slice_num(16);
                    int channel = pwm_gpio_to_channel(16);
                    gpio_set_function(16,GPIO_FUNC_PWM);
                    pwm_set_wrap(slice,wrap);
                    pwm_set_chan_level(slice,channel,level);
                    pwm_set_enabled(slice,true);
                    puts("OK");
                } else if(strcmp(current,"ATDISABLE")==0) {
                    int slice = pwm_gpio_to_slice_num(16);
                    pwm_set_enabled(slice,false);
                    gpio_set_function(16,GPIO_FUNC_NULL);
                    gpio_put(16,false);
                    puts("OK");
                } else if(current[0]=='A' && current[1]=='T' && current[2]=='+') {
                    //this is a variable access. its either read or write.
                    //the pattern is AT+{VARIABLE}? for reading and
                    //AT+{VARIABLE}={VALUE} for writing.
                    int idx=3;
                    bool read=false;
                    bool write=false;
                    while(current[idx]!='?' && current[idx]!='=') {
                        idx++;
                        if(current[idx]==0) break;
                        if(current[idx]=='?') read=true;
                        if(current[idx]=='=') write=true;
                    }
                    if(!read && !write) {
                        puts("ERR - Syntax");
                    } else {
                        char variable[32];
                        memcpy(variable,current+3,idx-3);
                        variable[idx-3]=0;
                        if(strcmp(variable,"WRAP")==0) {
                            if(write) {
                                char value[128];
                                strcpy(value,current+idx+1);
                                sscanf(value,"%d",&wrap);
                            } else {
                                printf("%d\n",wrap);
                            }
                        } else if(strcmp(variable,"LEVEL")==0) {
                            if(write) {
                                char value[128];
                                strcpy(value,current+idx+1);
                                sscanf(value,"%d",&level);
                            } else {
                                printf("%d\n",level);
                            }
                        } else {
                            printf("ERR - Unknown Variable %s\n",variable);
                        }
                    }
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
