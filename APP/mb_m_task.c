/* Includes ------------------------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mb_m_stack.h"
#include "user_mb_app.h" // 添加头文件引用访问函数

MB_M_StackTypeDef mbMasterStack = NEW_MB_M_StackTypeDef;

extern const USHORT usSCoilStart;
/* 线程安全的线圈读取写入函数声明 */
extern eMBErrorCode MB_SafeGetCoil(USHORT usCoilAddr, UCHAR* pucValue);
extern eMBErrorCode MB_SafeSetCoil(USHORT usCoilAddr, UCHAR ucValue);

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

/* 使用安全函数获取线圈值 */
static BOOL GetCoilStatusSafe(USHORT usCoilAddr)
{
    UCHAR ucValue = 0;
    
    // 使用安全函数获取线圈值
    if (MB_SafeGetCoil(usCoilAddr, &ucValue) == MB_ENOERR)
    {
        return ucValue ? TRUE : FALSE;
    }
    
    // 如果发生错误，返回默认值FALSE
    return FALSE;
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
            // 使用安全函数获取线圈状态
            // 注意：Modbus线圈地址从1开始，而不是从0开始
            USHORT usCoilAddr = usSCoilStart + usCurrentCoil + 1;
            xCoilStatus = GetCoilStatusSafe(usCoilAddr);
            
            // 向从机发送写单个线圈命令
            eMBMasterReqWriteCoil(
                &mbMasterStack,   // Modbus主机栈实例
                usSlaveAddr,      // 从机地址
                usCoilAddr,       // 线圈地址(已加1处理)
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