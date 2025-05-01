/* Includes ------------------------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mb_m_stack.h"

MB_M_StackTypeDef mbMasterStack = NEW_MB_M_StackTypeDef;

void Mb_m_Task(void *argument)
{
    mbMasterStack.hardware.max485.phuart = &huart2;
    mbMasterStack.hardware.max485.dirPin = USART2_DIR_Pin;
    mbMasterStack.hardware.max485.dirPort = USART2_DIR_GPIO_Port;
    mbMasterStack.hardware.phtim = &htim3;
    mbMasterStack.hardware.uartIRQn = USART2_IRQn;
    mbMasterStack.hardware.timIRQn = TIM3_IRQn;
    eMBMasterInit(&mbMasterStack, MB_RTU, 2, 115200, MB_PAR_NONE);
    eMBMasterEnable(&mbMasterStack);
    while (1)
    {
        eMBMasterPoll(&mbMasterStack);
    }
}

// void Mb_m_ComTask(void *argument)
// {
//     for (;;)
//     {
//         eMBMasterReqReadHoldingRegister(&mbMasterStack, 1, 1, 1, 100);
//         osDelay(300);
//     }
// }

/* Includes ------------------------------------------------------------------*/

extern UCHAR ucSCoilBuf[];      // 外部线圈缓冲区
extern USHORT usSCoilStart;     // 线圈起始地址


// 获取指定位的线圈状态
static BOOL GetCoilStatus(UCHAR *pucCoilBuf, USHORT usCoilAddr)
{
    USHORT usCoilIndex = usCoilAddr - usSCoilStart;
    UCHAR ucByteIndex = usCoilIndex / 8;
    UCHAR ucBitIndex = usCoilIndex % 8;
    
    return (pucCoilBuf[ucByteIndex] & (1 << ucBitIndex)) ? TRUE : FALSE;
}

void Mb_m_ComTask(void *argument)
{
    USHORT usCurrentCoil = 0;
    BOOL xCoilStatus = FALSE;
    USHORT usRegAddr = 1;  // 保持寄存器地址
    UCHAR ucRegCount = 1;  // 保持寄存器数量
    UCHAR ucTimeoutMs = 100;  // 超时时间
    USHORT usSlaveAddr = 1;   // 从机地址
    
    // 等待系统初始化完成
    osDelay(1000);
    
    for (;;)
    {
        // 循环遍历8个线圈
        for (usCurrentCoil = 0; usCurrentCoil < 8; usCurrentCoil++)
        {
            // 获取线圈状态
            xCoilStatus = GetCoilStatus(ucSCoilBuf, usSCoilStart + usCurrentCoil);
            
            // 向从机发送写单个线圈命令
            eMBMasterReqWriteCoil(
                &mbMasterStack,   // Modbus主机栈实例
                usSlaveAddr,      // 从机地址
                usSCoilStart + usCurrentCoil, // 线圈地址
                xCoilStatus ? 0xFF00 : 0x0000, // 线圈值(ON/OFF)
                ucTimeoutMs       // 超时时间
            );
            
            // 等待命令处理完成
            osDelay(100);
        }
        
        // 读取从机保持寄存器以监控状态
        eMBMasterReqReadHoldingRegister(
            &mbMasterStack,
            usSlaveAddr,
            usRegAddr,
            ucRegCount,
            ucTimeoutMs
        );
        
        // 合理的通信间隔
        osDelay(200);
    }
}