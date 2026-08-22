#include "usart.h"

#include <stddef.h>
#include <stdint.h>

#define BOARD_DEBUG_TX_TIMEOUT_MS 100U

int _write(int file, char *ptr, int len)
{
    (void)file;

    if ((ptr == NULL) || (len <= 0))
    {
        return 0;
    }

    if (HAL_UART_Transmit(
            &huart1,
            (uint8_t *)ptr,
            (uint16_t)len,
            BOARD_DEBUG_TX_TIMEOUT_MS) != HAL_OK)
    {
        return -1;
    }

    return len;
}