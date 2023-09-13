#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"
#include "pico/cyw43_arch.h"


// #define BAUD_RATE 115200
// #define BAUD_RATE 115200 * 8
#define BAUD_RATE 115200 * 8 * 8
#define DATA_BITS 8
#define STOP_BITS 1
#define PARITY    UART_PARITY_NONE

// carrier
// #define UART_ID uart1
// picow
#define UART_ID uart0

// carrier
// #define UART_TX_PIN 24
// #define UART_RX_PIN 25
// picow
#define UART_TX_PIN 16
#define UART_RX_PIN 17

// on carrier and picow
#define RS485_RX_EN 18
#define RS485_TX_EN 19

// only on carrier .. ignored on picow
#define POWER_PIN   12

// pico
// #define LED_PIN PICO_DEFAULT_LED_PIN
// picow
#define LED_PIN CYW43_WL_GPIO_LED_PIN



void DumpHex(const void* data, size_t size) {
	char ascii[17];
	size_t i, j;
	ascii[16] = '\0';
	for (i = 0; i < size; ++i) {
		printf("%02X ", ((unsigned char*)data)[i]);
		if (((unsigned char*)data)[i] >= ' ' && ((unsigned char*)data)[i] <= '~') {
			ascii[i % 16] = ((unsigned char*)data)[i];
		} else {
			ascii[i % 16] = '.';
		}
		if ((i+1) % 8 == 0 || i+1 == size) {
			printf(" ");
			if ((i+1) % 16 == 0) {
				printf("|  %s \n", ascii);
			} else if (i+1 == size) {
				ascii[(i+1) % 16] = '\0';
				if ((i+1) % 16 <= 8) {
					printf(" ");
				}
				for (j = (i+1) % 16; j < 16; ++j) {
					printf("   ");
				}
				printf("|  %s \n", ascii);
			}
		}
	}
}


static inline uint8_t uart_has_error(uart_inst_t *uart) {
    uint8_t status = (uart_get_hw(uart)->rsr & (UART_UARTRSR_BE_BITS | UART_UARTRSR_PE_BITS | UART_UARTRSR_FE_BITS));
    // uart_get_hw(uart)->rsr = UART_UARTRSR_BE_BITS | UART_UARTRSR_PE_BITS | UART_UARTRSR_FE_BITS;
    return status;
}


int main() {
    uart_init(UART_ID, BAUD_RATE);
    gpio_set_function(UART_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(UART_RX_PIN, GPIO_FUNC_UART);

    // do not supply power to the modules
    gpio_init(POWER_PIN);
    gpio_set_dir(POWER_PIN, GPIO_OUT);
    gpio_put(POWER_PIN, 0);

    // low = enabled
    gpio_init(RS485_RX_EN);
    gpio_set_dir(RS485_RX_EN, GPIO_OUT);
    gpio_put(RS485_RX_EN, 0);
    // gpio_put(RS485_RX_EN, 1);

    // low = disabled
    gpio_init(RS485_TX_EN);
    gpio_set_dir(RS485_TX_EN, GPIO_OUT);
    gpio_put(RS485_TX_EN, 0);
    // gpio_put(RS485_TX_EN, 1);

    // const uint LED_PIN = PICO_DEFAULT_LED_PIN;
    // gpio_init(LED_PIN);
    // gpio_set_dir(LED_PIN, GPIO_OUT);

    // picow, only for LED
    if (cyw43_arch_init()) {
        printf("Wi-Fi init failed");
        return -1;
    }

    stdio_init_all(); 

    int led_state = 0;
    // char tx1[] = "(11,42)\0";
    // char tx2[] = "(22,42)\0";
    char tx1[] = "(11,aaaa,bbbb,cccc,dddd,eeee,ffff,gggg,hhhh)\0";
    char tx2[] = "(22,iiii,jjjj,kkkk,llll,mmmm,nnnn,oooo,pppp)\0";
    char *tx;
    int slave_id = 0;
    char rx[64];
    int i;
    uint8_t rx_err;
    bool msg = true;
    absolute_time_t tx_t;
    tx_t = get_absolute_time();
    
    while (1) {
        while (uart_is_readable(UART_ID)) {
            rx[i] = uart_getc(UART_ID);
            rx_err = uart_has_error(UART_ID);
            // printf("m RX[%d] err %x : ", i, rx_err);
            // DumpHex(rx, i+1);
            if (rx_err) {
                i = 0;
            } else {
                if (rx[i] == ')') {
                    i++;
                    rx[i] = '\0';

                    // printf("m RX[%d]: ", i);
                    // DumpHex(rx, i);
                    // printf("m RX[%d]: %s\n", i, rx);
                    
                    i = 0;
                    if (rx[1] == '1' && rx[2] == '1') {
                        msg = true;
                    }
                    break;
                } else {
                    i++;
                }
            }
            if (i > 63) {
                printf("m err RX[%d]: ", i);
                DumpHex(rx, i);
                i = 0;
            }
        }
        
        // sleep_ms(1);
        // sleep_ms(2000);

        if ((absolute_time_diff_us(tx_t, get_absolute_time())) > 150) {
            msg = true;
        }

        // if (i == 0) {
        if (msg) {
            tx = (slave_id == 0) ? tx1 : tx2;
            tx = tx1;
            
            // printf("m TX: %s\n", tx);

            // disable rx, enable tx
            gpio_put(RS485_RX_EN, 1);
            gpio_put(RS485_TX_EN, 1);

            while (*tx) {
                uart_putc_raw(UART_ID, *tx++);
            }
            uart_tx_wait_blocking(UART_ID);

            // disable tx, enable rx (NOTE: must first disable TX otherwise junk is seen on RX)
            gpio_put(RS485_TX_EN, 0);
            gpio_put(RS485_RX_EN, 0);

            slave_id = 1 - slave_id;
            
            // printf("LED %d\n", led_state);
            // gpio_put(LED_PIN, led_state);
            // cyw43_arch_gpio_put(LED_PIN, led_state);
            led_state = 1 - led_state;
            msg = false;
            tx_t = get_absolute_time();
        }
    }
}
