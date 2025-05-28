/* Includes ------------------------------------------------------------------*/
#include "mb.h"
#include "mb_stack.h"
#include "string.h"

#define PWM_HOLDING_REG_START 100 // PWM寄存器起始地址

MB_StackTypeDef mbStack = NEW_MB_StackTypeDef;

void MB_BaudrateTask(void * this);

/* 线程安全的线圈读取写入函数 */
eMBErrorCode MB_SafeGetCoil(USHORT usCoilAddr, UCHAR* pucValue);
eMBErrorCode MB_SafeSetCoil(USHORT usCoilAddr, UCHAR ucValue);

eMBErrorCode MB_SafeGetHoldingReg(USHORT usRegAddr, USHORT* pusValue);
eMBErrorCode MB_SafeSetHoldingReg(USHORT usRegAddr, USHORT usValue);

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
    eMBInit(&mbStack, MB_RTU, 0x02, 1, ulBaudrate, MB_PAR_NONE);
    eMBEnable(&mbStack);
    while (1)
    {
        eMBPoll(&mbStack);
        MB_BaudrateTask(&mbStack);
    }
}

#include "main.h"
//extern UCHAR ucSCoilBuf[];
extern const USHORT usSCoilStart;


typedef struct {
    uint16_t coilBit;     // 线圈位索引
    GPIO_TypeDef* port;   // GPIO端口
    uint16_t pin;         // GPIO引脚
} Coil_GPIO_Map;
// 定义线圈到GPIO的映射关系

static const Coil_GPIO_Map coilGpioMap[] = {
    {1, OU1_GPIO_Port, OU1_Pin}, // 第一个线圈映射到OU1
    {2, OU2_GPIO_Port, OU2_Pin}, // 第二个线圈映射到OU2
    {3, OU3_GPIO_Port, OU3_Pin}, // 第三个线圈映射到OU3
    {4, OU4_GPIO_Port, OU4_Pin}, // 第四个线圈映射到OU4
};
#define COIL_GPIO_MAP_SIZE (sizeof(coilGpioMap) / sizeof(coilGpioMap[0]))

typedef struct {
    uint16_t coilBit;     // 线圈位索引
    unsigned int channel;   // PWM channel
} Coil_PWM_Map;
// 定义线圈到GPIO的映射关系
static const Coil_PWM_Map coilPWMMap[] = {
    {5, TIM_CHANNEL_4}, // 第五个线圈映射到
    {6, TIM_CHANNEL_3}, // 第六个线圈映射到
    {7, TIM_CHANNEL_2}, // 第七个线圈映射到
    {8, TIM_CHANNEL_1}, // 第八个线圈映射到
};
#define COIL_PWM_MAP_SIZE (sizeof(coilPWMMap) / sizeof(coilPWMMap[0]))

// 这里可以根据需要添加更多的线圈到GPIO的映射关系

/* 替换原来直接访问缓冲区的函数 */

// 使用安全函数获取线圈值
static uint8_t GetCoilBitValue(uint16_t bitIndex)
{
    UCHAR ucValue = 0;
    // 注意：Modbus地址从1开始，而不是从0开始
    USHORT usAddress = usSCoilStart + bitIndex+1 ; 
    
    // 使用安全函数获取线圈值
    if (MB_SafeGetCoil(usAddress, &ucValue) == MB_ENOERR)
    {
        return ucValue ? 1 : 0;
    }
    
    // 如果发生错误，返回默认值0
    return 0;
}

// 使用安全函数设置线圈值
static void SetCoilBitValue(uint16_t bitIndex, uint8_t value)
{
    // 注意：Modbus地址从1开始，而不是从0开始
    USHORT usAddress = usSCoilStart + bitIndex+1 ;
    
    // 使用安全函数设置线圈值
    MB_SafeSetCoil(usAddress, value ? 1 : 0);
    
    // 这里不需要检查返回值，因为原函数没有返回值
    // 但在实际应用中，建议检查返回值并处理可能的错误
}

eMBErrorCode eMBRegHoldingCB(UCHAR * pucRegBuffer, USHORT usAddress,
    USHORT usNRegs, eMBRegisterMode eMode);
void Mb_GpioTask(void *argument)
{
    // 初始化GPIO为输出模式
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    BOOL PWM_MODE = FALSE; // 是否使用PWM模式，默认关闭
    HAL_TIM_Base_Start(&htim2);
    

    // 配置所有映射的GPIO
    for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
        GPIO_InitStruct.Pin = coilGpioMap[i].pin;
        HAL_GPIO_Init(coilGpioMap[i].port, &GPIO_InitStruct);
    }
    for (uint8_t i = 0; i < COIL_PWM_MAP_SIZE; i++) {
        HAL_TIM_PWM_Start(&htim3, coilPWMMap[i].channel);
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

        UCHAR ucBuf[2];
        USHORT Mode_Value = 0;
        /* 调用标准回调函数读取保持寄存器值 */
        eMBRegHoldingCB(ucBuf, PWM_HOLDING_REG_START+2, 1, MB_REG_READ);
        Mode_Value = (ucBuf[0] << 8) | ucBuf[1];
        PWM_MODE = Mode_Value > 0 ? TRUE : FALSE;
        
        
        if(!PWM_MODE)
        //if(FALSE)
        {
            for (uint8_t i = 0; i < COIL_PWM_MAP_SIZE; i++) {
                uint8_t coilState = GetCoilBitValue(coilPWMMap[i].coilBit);
                __HAL_TIM_SET_COMPARE(&htim3,
                    coilPWMMap[i].channel, 
                    coilState ? htim3.Init.Period  : 0
                    );
            }
        }
        else
        {
            for (uint8_t i = 0; i < COIL_PWM_MAP_SIZE; i++) {
                UCHAR ucBuf[2];
                /* 调用标准回调函数读取保持寄存器值 */
                eMBRegHoldingCB(ucBuf, PWM_HOLDING_REG_START + i + 3, 1, MB_REG_READ);
                uint8_t duty = ucBuf[0];      // 占空比（0~100）
                uint8_t pulse_count = ucBuf[1]; // 输出脉冲数量
            
                // 设置PWM占空比
                __HAL_TIM_SET_COMPARE(&htim3,
                    coilPWMMap[i].channel, 
                    (duty <= 100) ? ((htim3.Init.Period + 1) * duty) / 100 : 0
                );
            
                // TODO: 根据 pulse_count 实现脉冲输出逻辑
                // 例如：你可以在此处调用一个函数，控制输出指定数量的脉冲
                // Set_Output_Pulse_Count(coilPWMMap[i].channel, pulse_count);
            }
            // ...existing code...
            
        }
        // 适当延时
        osDelay(10); // 每10ms更新一次GPIO状态
    }
}

// void Mb_GpioTask(void *argument)
// {
//     // 初始化GPIO为输入模式
//     GPIO_InitTypeDef GPIO_InitStruct = {0};
    
//     GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
//     GPIO_InitStruct.Pull = GPIO_PULLDOWN;  // 使用下拉电阻，确保不连接时为低电平
//     GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    
//     // 配置所有映射的GPIO为输入
//     for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
//         GPIO_InitStruct.Pin = coilGpioMap[i].pin;
//         HAL_GPIO_Init(coilGpioMap[i].port, &GPIO_InitStruct);
//     }
    
//     // 主循环，持续读取GPIO状态并更新线圈值
//     while(1)
//     {
//         // 读取所有GPIO状态并更新对应的线圈值
//         for (uint8_t i = 0; i < COIL_GPIO_MAP_SIZE; i++) {
//             uint8_t gpioState = HAL_GPIO_ReadPin(coilGpioMap[i].port, coilGpioMap[i].pin);
//             SetCoilBitValue(coilGpioMap[i].coilBit, gpioState);
//         }
        
//         // 适当延时，避免过于频繁地访问硬件
//         osDelay(50); // 每50ms读取一次GPIO状态
//     }
// }