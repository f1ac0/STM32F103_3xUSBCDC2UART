/*
 * cdc_uart.c
 *
 *  Created on: Mar 15, 2025
 *      Author: f1ac0
 */

#include "cdc_uart.h"
#include "main.h"
#include "tusb.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;

// Received data over USB are stored in this buffer
uint8_t UartTxBufferFS[CFG_TUD_CDC][APP_RX_DATA_SIZE];

// Data to send over USB CDC are stored in this buffer
uint8_t UartRxBufferFS[CFG_TUD_CDC][2]; //2 bytes for the case of 9 bits

uint8_t UartRxActive[CFG_TUD_CDC]; //Track when UART is ready to receive data (inactive when buffer is full)
uint8_t CDC_Active = 0; //Track when CDC device are mounted

// Blink pattern
// - 2000 ms : device not mounted
// - on      : device mounted
// - 500 ms  : device is suspended
enum {
  BLINK_NOT_MOUNTED = 2000,
  BLINK_MOUNTED = 0,
  BLINK_SUSPENDED = 500,
  BLINK_ACTIVE = 100,
};

static uint32_t blink_interval_ms = BLINK_NOT_MOUNTED;

static UART_HandleTypeDef* UART_HandleFromItf(uint8_t itf) { return((itf==0)?&huart1:((itf==1)?&huart2:&huart3)); }
static uint8_t ItfFromUART_Handle(UART_HandleTypeDef* uart) { return((uart==&huart1)?0:((uart==&huart2)?1:2)); }


// Invoked when device is mounted
void tud_mount_cb(void) {
  CDC_Active = 1;
  blink_interval_ms = BLINK_MOUNTED;
  UartRxActive[0] = 0;
  UartRxActive[1] = 0;
  UartRxActive[2] = 0;
}

// Invoked when device is unmounted
void tud_umount_cb(void) {
  blink_interval_ms = BLINK_NOT_MOUNTED;
  if(HAL_UART_Abort(&huart1) != HAL_OK)
    Error_Handler(); // Initialization Error
  if(HAL_UART_Abort(&huart2) != HAL_OK)
    Error_Handler(); // Initialization Error
  if(HAL_UART_Abort(&huart3) != HAL_OK)
    Error_Handler(); // Initialization Error
  CDC_Active = 0;

  //DeInitialize the UART peripheral
//  if(HAL_UART_DeInit(&UartHandle) != HAL_OK)
//    Error_Handler();
}


//--------------------------------------------------------------------+
// USB CDC
//--------------------------------------------------------------------+

void cdc_task(void) {
  if(CDC_Active){
    tud_cdc_tx_complete_cb(0);
    tud_cdc_tx_complete_cb(1);
    tud_cdc_tx_complete_cb(2);
    tud_cdc_rx_cb(0);
    tud_cdc_rx_cb(1);
    tud_cdc_rx_cb(2);
  }
}

/**
  * @brief  CDC callback at receive packet to send it through the UART
  * @param  itf: USB interface
  * @retval None
  */
void tud_cdc_rx_cb(uint8_t itf) {
  UART_HandleTypeDef* uart = UART_HandleFromItf(itf);

  if (tud_cdc_n_available(itf) && uart->gState == HAL_UART_STATE_READY){// && HAL_UART_GetState(uart)==HAL_UART_STATE_READY) { //check only UART TX state for full duplex
    uint32_t count = tud_cdc_n_read(itf, UartTxBufferFS[itf], sizeof(UartTxBufferFS[itf]));
    HAL_UART_Transmit_DMA(uart, UartTxBufferFS[itf], count);
  }
}

/**
  * @brief  CDC callback at end of transmit packet to send the next one
  * @param  itf: USB interface
  * @retval None
  */
void tud_cdc_tx_complete_cb(uint8_t itf) {
  tud_cdc_n_write_flush(itf); //send the next packet right away

  if(UartRxActive[itf]==0 && tud_cdc_n_write_available(itf)>0) { //resume transfer
    UART_HandleTypeDef* uart = UART_HandleFromItf(itf);
    HAL_UART_Receive_IT(uart, (uint8_t *)(UartRxBufferFS[itf]), 1);
  }
}

// Invoked when cdc when line state changed e.g connected/disconnected
// Use to reset to DFU when disconnect with 1200 bps
void tud_cdc_line_state_cb(uint8_t instance, bool dtr, bool rts) {
  (void)rts;
/*
  // DTR = false is counted as disconnected
  if (!dtr) {
    // touch1200 only with first CDC instance (Serial)
    if (instance == 0) {
      cdc_line_coding_t coding;
      tud_cdc_get_line_coding(&coding);
      if (coding.bit_rate == 1200) {
        if (board_reset_to_bootloader) {
          board_reset_to_bootloader();
        }
      }
    }
  }*/
}

/**
  * @brief  CDC callback when line configuration changes
  * @param  itf: USB interface, p_line_coding: new configuration
  * @retval None
  */
void tud_cdc_line_coding_cb(uint8_t itf, cdc_line_coding_t const* p_line_coding) {

  UART_HandleTypeDef* uart = UART_HandleFromItf(itf);

  if(HAL_UART_Abort(uart) != HAL_OK)
      Error_Handler(); // Initialization Error
  //if(HAL_UART_DeInit(uart) != HAL_OK) //Dont deinit otherwise it will send a bad char over the line
  //    Error_Handler(); // Initialization Error

  //set the Stop bit
  switch (p_line_coding->stop_bits)
  {
  case 0:
    uart->Init.StopBits = UART_STOPBITS_1;
    break;
  case 2:
    uart->Init.StopBits = UART_STOPBITS_2;
    break;
  default :
    uart->Init.StopBits = UART_STOPBITS_1;
    break;
  }

  //set the parity bit
  switch (p_line_coding->parity)
  {
  case 0:
    uart->Init.Parity = UART_PARITY_NONE;
    break;
  case 1:
    uart->Init.Parity = UART_PARITY_ODD;
    break;
  case 2:
    uart->Init.Parity = UART_PARITY_EVEN;
    break;
  default :
    uart->Init.Parity = UART_PARITY_NONE;
    break;
  }

  //set the data type : only 8bits and 9bits is supported
  switch (p_line_coding->data_bits)
  {
  case 0x07:
    //With this configuration a parity (Even or Odd) must be set
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    break;
  case 0x08:
    if(uart->Init.Parity == UART_PARITY_NONE)
      uart->Init.WordLength = UART_WORDLENGTH_8B;
    else
      uart->Init.WordLength = UART_WORDLENGTH_9B;
    break;
  default :
    uart->Init.WordLength = UART_WORDLENGTH_8B;
    break;
  }

  uart->Init.BaudRate     = p_line_coding->bit_rate;
  uart->Init.HwFlowCtl    = UART_HWCONTROL_NONE;
  uart->Init.Mode         = UART_MODE_TX_RX;
  uart->Init.OverSampling = UART_OVERSAMPLING_16;

  if(HAL_UART_Init(uart) != HAL_OK)
    Error_Handler(); // Initialization Error

  //Signal that we need to start reception again
  UartRxActive[itf]=0;

}

//--------------------------------------------------------------------+
// UART EVENTS
//--------------------------------------------------------------------+

/**
  * @brief  Tx Transfer completed callback
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  //Initiate next USB packet transfer once UART completes transfer (transmitting data over Tx line)
  uint8_t itf = ItfFromUART_Handle(huart);
  tud_cdc_rx_cb(itf);

  blink_interval_ms=BLINK_ACTIVE;
}

/**
  * @brief  Rx Transfer completed callback
  * @param  huart: UART handle
  * @retval None
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t itf = ItfFromUART_Handle(huart);
  tud_cdc_n_write_char(itf, UartRxBufferFS[itf][0]);
  if(tud_cdc_n_write_available(itf)>0)
    HAL_UART_Receive_IT(huart, (uint8_t *)(UartRxBufferFS[itf]), 1);
  else
    UartRxActive[itf] = 0;

  blink_interval_ms=BLINK_ACTIVE;
}

/**
  * @brief  UART error callbacks
  * @param  UartHandle: UART handle
  * @retval None
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *UartHandle)
{
  //Transfer error occurred in reception and/or transmission process
  Error_Handler();
}


//--------------------------------------------------------------------+
// BLINKING TASK
//--------------------------------------------------------------------+
void led_blinking_task(void) {
  static uint32_t start_ms = 0;
  static uint32_t prev_state = 0;

  if(blink_interval_ms == 0){ //always on
    if(prev_state != 0)
      HAL_GPIO_WritePin(LED0_GPIO_Port,LED0_Pin,GPIO_PIN_RESET);
  } else if(blink_interval_ms==BLINK_ACTIVE) {
    if(prev_state!=BLINK_ACTIVE) {
      HAL_GPIO_WritePin(LED0_GPIO_Port,LED0_Pin,GPIO_PIN_SET);
      start_ms=HAL_GetTick()+blink_interval_ms;
    } else if(HAL_GetTick()>start_ms) {
      HAL_GPIO_WritePin(LED0_GPIO_Port,LED0_Pin,GPIO_PIN_RESET);
      blink_interval_ms=BLINK_MOUNTED;
    }
  } else {
    if(blink_interval_ms != prev_state) {
      HAL_GPIO_TogglePin(LED0_GPIO_Port,LED0_Pin);
      start_ms=HAL_GetTick();
    } else {
      // Blink every interval ms
      if (HAL_GetTick() - start_ms < blink_interval_ms) return; // not enough time
      start_ms += blink_interval_ms;
      HAL_GPIO_TogglePin(LED0_GPIO_Port,LED0_Pin);
    }
  }
  prev_state = blink_interval_ms;
}
