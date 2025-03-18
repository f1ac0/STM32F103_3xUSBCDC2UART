/*
 * cdc_uart.h
 *
 *  Created on: Mar 15, 2025
 *      Author: f1ac0
 */

#ifndef INC_CDC_UART_H_
#define INC_CDC_UART_H_

#define APP_RX_DATA_SIZE  64
#define APP_TX_DATA_SIZE  512

void cdc_task(void);
void led_blinking_task(void);

#endif /* INC_CDC_UART_H_ */
