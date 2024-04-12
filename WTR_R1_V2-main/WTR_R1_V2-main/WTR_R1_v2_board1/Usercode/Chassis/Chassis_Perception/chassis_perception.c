#include "chassis_perception.h"

/**
 * @brief   码盘定位系统初始化
 */
void Chassis_Perception_Init(void)
{
    HAL_UART_Receive_DMA(&OPS_UART_HANDLE, ops_buffer, 28);
    __HAL_UART_ENABLE_IT(&OPS_UART_HANDLE, UART_IT_IDLE);
}