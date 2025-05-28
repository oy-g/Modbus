/* Includes ------------------------------------------------------------------*/
#include "mb.h"
#include "mb_stack.h"

MB_StackTypeDef mbStack = NEW_MB_StackTypeDef;

void Mb_Task(void *argument)
{
    mbStack.hardware.max485.phuart = &huart1;
    mbStack.hardware.max485.dirPin = USART1_DIR_Pin;
    mbStack.hardware.max485.dirPort = USART1_DIR_GPIO_Port;
    mbStack.hardware.phtim = &htim4;
    mbStack.hardware.uartIRQn = USART1_IRQn;
    mbStack.hardware.timIRQn = TIM4_IRQn;
    eMBInit(&mbStack, MB_RTU, 0x01, 1, 115200, MB_PAR_NONE);
    eMBEnable(&mbStack);
    while (1)
    {
        eMBPoll(&mbStack);
    }
}

// 输出
// #include "main.h"
// extern UCHAR ucSCoilBuf[];
// extern USHORT usSCoilStart;
// typedef struct {
//     uint16_t coilBit;     // 线圈位索引
//     GPIO_TypeDef* port;   // GPIO端口
//     uint16_t pin;         // GPIO引脚
// } Coil_GPIO_Map;
// // 定义线圈到GPIO的映射关系

// static const Coil_GPIO_Map coilGpioMap[] = {
//     {0, OU1_GPIO_Port, OU1_Pin}, // 第一个线圈映射到OU1
//     {1, OU2_GPIO_Port, OU2_Pin}, // 第二个线圈映射到OU2
//     {2, OU3_GPIO_Port, OU3_Pin}, // 第三个线圈映射到OU3
//     {3, OU4_GPIO_Port, OU4_Pin}  // 第四个线圈映射到OU4
// };

// #define COIL_GPIO_MAP_SIZE (sizeof(coilGpioMap) / sizeof(coilGpioMap[0]))

// // 从线圈缓冲区获取指定位的值
// static uint8_t GetCoilBitValue(uint16_t bitIndex)
// {
//     uint16_t byteIndex = bitIndex / 8;
//     uint8_t bitOffset = bitIndex % 8;
//     return (ucSCoilBuf[byteIndex] & (1 << bitOffset)) ? 1 : 0;
// }

// void Mb_GpioTask(void *argument)
// {
//     // 初始化GPIO为输出模式
//     GPIO_InitTypeDef GPIO_InitStruct = {0};
    
//     GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
//     GPIO_InitStruct.Pull = GPIO_NOPULL;
//     GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
//     // 配置所有映射的GPIO
//     for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
//         GPIO_InitStruct.Pin = coilGpioMap[i].pin;
//         HAL_GPIO_Init(coilGpioMap[i].port, &GPIO_InitStruct);
//     }
    
//     // 主循环，持续更新GPIO状态
//     while(1)
//     {
//         // 根据线圈状态更新所有GPIO
//         for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
//             uint8_t coilState = GetCoilBitValue(coilGpioMap[i].coilBit);
//             HAL_GPIO_WritePin(
//                 coilGpioMap[i].port,
//                 coilGpioMap[i].pin,
//                 coilState ? GPIO_PIN_SET : GPIO_PIN_RESET
//             );
//         }
        
//         // 适当延时，避免过于频繁地访问共享数据
//         osDelay(50); // 每50ms更新一次GPIO状态
//     }
// }


// 输入
// ...existing code...
#include "main.h"
extern UCHAR ucSCoilBuf[];
extern USHORT usSCoilStart;
typedef struct {
    uint16_t coilBit;     // 线圈位索引
    GPIO_TypeDef* port;   // GPIO端口
    uint16_t pin;         // GPIO引脚
} Coil_GPIO_Map;
// 定义GPIO输入到线圈的映射关系

static const Coil_GPIO_Map coilGpioMap[] = {
    {0, OU1_GPIO_Port, OU1_Pin}, // OU1输入映射到第一个线圈
    {1, OU2_GPIO_Port, OU2_Pin}, // OU2输入映射到第二个线圈
    {2, OU3_GPIO_Port, OU3_Pin}, // OU3输入映射到第三个线圈
    {3, OU4_GPIO_Port, OU4_Pin}  // OU4输入映射到第四个线圈
};

#define COIL_GPIO_MAP_SIZE (sizeof(coilGpioMap) / sizeof(coilGpioMap[0]))

// 从线圈缓冲区获取指定位的值
static uint8_t GetCoilBitValue(uint16_t bitIndex)
{
    uint16_t byteIndex = bitIndex / 8;
    uint8_t bitOffset = bitIndex % 8;
    return (ucSCoilBuf[byteIndex] & (1 << bitOffset)) ? 1 : 0;
}

// 设置线圈缓冲区中指定位的值
static void SetCoilBitValue(uint16_t bitIndex, uint8_t value)
{
    uint16_t byteIndex = bitIndex / 8;
    uint8_t bitOffset = bitIndex % 8;
    
    if (value)
        ucSCoilBuf[byteIndex] |= (1 << bitOffset);   // 设置位
    else
        ucSCoilBuf[byteIndex] &= ~(1 << bitOffset);  // 清除位
}

void Mb_GpioTask(void *argument)
{
    // 初始化GPIO为输入模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLDOWN;  // 使用下拉电阻，确保不连接时为低电平
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
    // 配置所有映射的GPIO为输入
    for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
        GPIO_InitStruct.Pin = coilGpioMap[i].pin;
        HAL_GPIO_Init(coilGpioMap[i].port, &GPIO_InitStruct);
    }
    
    // 主循环，持续读取GPIO状态并更新线圈值
    while(1)
    {
        // 读取所有GPIO状态并更新对应的线圈值
        for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
            uint8_t gpioState = HAL_GPIO_ReadPin(coilGpioMap[i].port, coilGpioMap[i].pin);
            SetCoilBitValue(coilGpioMap[i].coilBit, gpioState);
        }
        
        // 适当延时，避免过于频繁地访问硬件
        osDelay(50); // 每50ms读取一次GPIO状态
    }
}