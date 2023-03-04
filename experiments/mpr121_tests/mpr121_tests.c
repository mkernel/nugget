#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/binary_info.h"
#include "hardware/i2c.h"
#include "mpr121.h"

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

void i2cscan() {
    i2c_init(i2c_default, 100 * 1000);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);
    printf("\nI2C Bus Scan\n");
    printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

    for (int addr = 0; addr < (1 << 7); ++addr) {
        if (addr % 16 == 0) {
            printf("%02x ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        int ret;
        uint8_t rxdata;
        if (reserved_addr(addr))
            ret = PICO_ERROR_GENERIC;
        else
            ret = i2c_read_blocking(i2c_default, addr, &rxdata, 1, false);

        printf(ret < 0 ? "." : "@");
        printf(addr % 16 == 15 ? "\n" : "  ");
    }
    printf("Done.\n");

}

void mpr() {
    int mpr_addr=0x5A;
    int mpr_freq=100000;
    i2c_init(i2c_default, mpr_freq);
    gpio_set_function(PICO_DEFAULT_I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(PICO_DEFAULT_I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(PICO_DEFAULT_I2C_SDA_PIN);
    gpio_pull_up(PICO_DEFAULT_I2C_SCL_PIN);

    struct mpr121_sensor mpr121;
    mpr121_init(i2c_default, mpr_addr, &mpr121);
    mpr121_set_thresholds(12,
                          6, &mpr121);
    // Enable only n=1 electrode (electrode 0).
    mpr121_enable_electrodes(1, &mpr121);
    const uint8_t electrode = 0;
    bool is_touched;
    uint16_t baseline, filtered;

    while(1) {
        mpr121_is_touched(electrode, &is_touched, &mpr121);
        mpr121_filtered_data(electrode, &filtered, &mpr121);
        mpr121_baseline_value(electrode, &baseline, &mpr121);
        printf("%d %d %d\n", baseline, filtered, is_touched);
        sleep_ms(100);
    }
}


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
                    puts("OK - mpr tests v0.1");
                } else if(strcmp(current,"ATSCAN")==0) {
                    i2cscan();
                } else if(strcmp(current,"ATMPR")==0) {
                    mpr();
                }else {
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
