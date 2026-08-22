#include "ethernet_rtos.h"

#include <stddef.h>

#include "cmsis_os2.h"
#include "ethernet_driver.h"

#define ETHERNET_RX_EVENT_FLAG  (1UL << 0)

static uint8_t g_rx_frame[ETHERNET_FRAME_BUFFER_SIZE];
static osThreadId_t g_rx_task_handle;
static volatile bool g_ready;
static EthernetRtosRxFrameHandler g_rx_frame_handler;
static void *g_rx_frame_handler_context;

/**
 * @brief  Ethernet Driver RX complete ISR 事件处理。
 */
static void EthernetRtos_OnRxEvent(void *context)
{
    osThreadId_t task_handle = g_rx_task_handle;

    (void)context;

    if (task_handle != NULL)
    {
        (void)osThreadFlagsSet(task_handle, ETHERNET_RX_EVENT_FLAG);
    }
}

/**
 * @brief  设置任务上下文中的完整 Frame 处理函数。
 *
 * @details
 * 建议在 MAC/DMA Start 前完成设置。frame 指针只在 handler 调用期间有效，
 * handler 返回后不得继续持有该指针。
 */
void EthernetRtos_SetRxFrameHandler(EthernetRtosRxFrameHandler handler, void *context)
{
    g_rx_frame_handler_context = context;
    g_rx_frame_handler = handler;
}

/**
 * @brief  查询 RX Task 是否已经建立 ISR notification 绑定。
 */
bool EthernetRtos_IsReady(void)
{
    return g_ready;
}

/**
 * @brief  CMSIS-RTOS2 Ethernet RX Task 入口。
 *
 * @details
 * 本函数不创建 Task。Task 的 priority、stack 和 allocation 由应用负责配置。
 */
void EthernetRtos_RxTask(void *argument)
{
    (void)argument;

    g_rx_task_handle = osThreadGetId();
    EthernetDriver_SetRxEventHandler(EthernetRtos_OnRxEvent, NULL);
    g_ready = true;

    for (;;)
    {
        uint32_t flags = osThreadFlagsWait(ETHERNET_RX_EVENT_FLAG, osFlagsWaitAny, osWaitForever);

        if ((flags & osFlagsError) != 0U)
        {
            continue;
        }

        for (;;)
        {
            uint16_t frame_length = 0U;
            EthernetRxResult result = EthernetDriver_Receive(g_rx_frame, sizeof(g_rx_frame), &frame_length);

            if (result == ETHERNET_RX_NONE)
            {
                break;
            }

            if (result == ETHERNET_RX_ERROR)
            {
                break;
            }

            if (g_rx_frame_handler != NULL)
            {
                g_rx_frame_handler(g_rx_frame, frame_length, g_rx_frame_handler_context);
            }
        }
    }
}
