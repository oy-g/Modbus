/* Includes ------------------------------------------------------------------*/
#include "mb.h"
#include "mb_stack.h"
#include "string.h"

MB_StackTypeDef mbStack = NEW_MB_StackTypeDef;

void MB_BaudrateTask(void * this);

ULONG MB_LoadBaudrateFromFlash(void);

void Mb_Task(void *argument)
{
    mbStack.hardware.max485.phuart = &huart1;
    mbStack.hardware.max485.dirPin = USART1_DIR_Pin;
    mbStack.hardware.max485.dirPort = USART1_DIR_GPIO_Port;
    mbStack.hardware.phtim = &htim4;
    mbStack.hardware.uartIRQn = USART1_IRQn;
    mbStack.hardware.timIRQn = TIM4_IRQn;
    

    // ULONG ulBaudrate = MB_LoadBaudrateFromFlash();
    ULONG ulBaudrate = 115200;
    eMBInit(&mbStack, MB_RTU, 0x01, 1, ulBaudrate, MB_PAR_NONE);
    eMBEnable(&mbStack);
    while (1)
    {
        eMBPoll(&mbStack);
        MB_BaudrateTask(&mbStack);
    }
}

#include "main.h"
extern UCHAR ucSCoilBuf[];
extern USHORT usSCoilStart;
typedef struct {
    uint16_t coilBit;     // 线圈位索引
    GPIO_TypeDef* port;   // GPIO端口
    uint16_t pin;         // GPIO引脚
} Coil_GPIO_Map;
// 定义线圈到GPIO的映射关系

static const Coil_GPIO_Map coilGpioMap[] = {
    {0, OU1_GPIO_Port, OU1_Pin}, // 第一个线圈映射到OU1
    {1, OU2_GPIO_Port, OU2_Pin}, // 第二个线圈映射到OU2
    {2, OU3_GPIO_Port, OU3_Pin}, // 第三个线圈映射到OU3
    {3, OU4_GPIO_Port, OU4_Pin}, // 第四个线圈映射到OU4
    {4, OU5_GPIO_Port, OU5_Pin}, // 第五个线圈映射到OU5
    {5, OU6_GPIO_Port, OU6_Pin}, // 第六个线圈映射到OU6
    {6, OU7_GPIO_Port, OU7_Pin}, // 第七个线圈映射到OU7
    {7, OU8_GPIO_Port, OU8_Pin}, // 第八个线圈映射到OU8
};

#define COIL_GPIO_MAP_SIZE (sizeof(coilGpioMap) / sizeof(coilGpioMap[0]))

// 从线圈缓冲区获取指定位的值
static uint8_t GetCoilBitValue(uint16_t bitIndex)
{
    uint16_t byteIndex = bitIndex / 8;
    uint8_t bitOffset = bitIndex % 8;
    return (ucSCoilBuf[byteIndex] & (1 << bitOffset)) ? 1 : 0;
}

void Mb_GpioTask(void *argument)
{
    // 初始化GPIO为输出模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    // 配置所有映射的GPIO
    for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
        GPIO_InitStruct.Pin = coilGpioMap[i].pin;
        HAL_GPIO_Init(coilGpioMap[i].port, &GPIO_InitStruct);
    }
    
    // 主循环，持续更新GPIO状态
    while(1)
    {
        // 根据线圈状态更新所有GPIO
        for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
            uint8_t coilState = GetCoilBitValue(coilGpioMap[i].coilBit);
            HAL_GPIO_WritePin(
                coilGpioMap[i].port,
                coilGpioMap[i].pin,
                coilState ? GPIO_PIN_SET : GPIO_PIN_RESET
            );
        }
        
        // 适当延时，避免过于频繁地访问共享数据
        osDelay(50); // 每50ms更新一次GPIO状态
    }
}
