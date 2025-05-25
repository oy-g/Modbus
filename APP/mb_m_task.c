/* Includes ------------------------------------------------------------------*/
#include "mb.h"
#include "mb_m.h"
#include "mb_m_stack.h"
#include "user_mb_app.h" // 添加头文件引用访问函数

// 换串口1
MB_M_StackTypeDef mbMasterStack = NEW_MB_M_StackTypeDef;

extern const USHORT usSCoilStart;
/* 线程安全的线圈读取写入函数声明 */
extern eMBErrorCode MB_SafeGetCoil(USHORT usCoilAddr, UCHAR *pucValue);
extern eMBErrorCode MB_SafeSetCoil(USHORT usCoilAddr, UCHAR ucValue);

void Mb_m_Task(void *argument)
{
    mbMasterStack.hardware.max485.phuart = &huart1;
    mbMasterStack.hardware.max485.dirPin = USART1_DIR_Pin;
    mbMasterStack.hardware.max485.dirPort = USART1_DIR_GPIO_Port;
    mbMasterStack.hardware.phtim = &htim3;
    mbMasterStack.hardware.uartIRQn = USART1_IRQn;
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

#include "mb.h"
#include "mb_m.h"
#include "mb_m_stack.h"

/**
 * 读取从站8个连续线圈
 * @param pStack     主站协议栈实例
 * @param slaveAddr  从站地址
 * @param coilAddr   起始线圈地址
 * @param coilValues 读取到的线圈值（每bit代表一个线圈，低位为起始线圈）
 * @param timeoutMs  超时时间
 * @return           Modbus主站请求错误码
 */
eMBMasterReqErrCode Read8CoilsFromSlave(
    pMB_M_StackTypeDef pStack,
    UCHAR slaveAddr,
    USHORT coilAddr,
    UCHAR *coilValues,   // 需保证至少1字节空间
    LONG timeoutMs
)
{
    eMBMasterReqErrCode reqStatus;
    UCHAR coilBuf[1] = {0}; // 只需1字节

    // 1. 发送读线圈请求（一次读8个线圈）
    reqStatus = eMBMasterReqReadCoils(
        pStack,
        slaveAddr,
        coilAddr,
        8,         // 读8个线圈
        timeoutMs
    );
    // if(reqStatus != MB_MRE_NO_ERR) {
    //     return reqStatus;
    // }

    // 2. 获取响应数据（同步方式，实际项目中一般协议栈自动调用回调并填充应用层缓冲区）
    if(eMBMasterRegCoilsCB(coilBuf, coilAddr + 1, 8, MB_REG_READ) == MB_ENOERR) {
        for(UCHAR i = 0; i < 8; i++) {
            coilValues[i] = (coilBuf[0] >> i) & 0x01; // 正确提取每一位
        }
        return MB_MRE_NO_ERR;
    } else {
        return MB_MRE_REV_DATA;
    }
}

eMBErrorCode eMBRegHoldingCB(UCHAR * pucRegBuffer, USHORT usAddress,
        USHORT usNRegs, eMBRegisterMode eMode);
void Mb_m_ComTask(void *argument)
{
    USHORT usCurrent = 0;
    BOOL xCoilStatus = FALSE;
    USHORT usRegAddr = 1;    // 保持寄存器地址
    USHORT usRegAddrOut = 100; // 输出寄存器地址
    UCHAR ucRegCount = 5;    // 保持寄存器数量
    UCHAR ucTimeoutMs = 100; // 超时时间
    USHORT usSlaveInputAddr = 1;  // 从机地址输入
    USHORT usSlaveOutputAddr = 2;  // 从机地址输出

    // 等待系统初始化完成
    osDelay(1000);

    for (;;)
    {
        // 针对输出从机
        // 循环遍历8个线圈
        for (usCurrent = 0; usCurrent < 8; usCurrent++)
        {
            // 使用安全函数获取线圈状态
            // 注意：Modbus线圈地址从1开始，而不是从0开始
            USHORT usCoilAddr = usSCoilStart + usCurrent + 1;
            xCoilStatus = GetCoilStatusSafe(usCoilAddr+ 99);

            // 向从机发送写单个线圈命令
            eMBMasterReqWriteCoil(
                &mbMasterStack,                // Modbus主机栈实例
                usSlaveOutputAddr,              // 从机地址
                usCoilAddr ,                    // 线圈地址
                xCoilStatus ? 0xFF00 : 0x0000, // 线圈值(ON/OFF)
                ucTimeoutMs                    // 超时时间
            );

            // 等待命令处理完成
            osDelay(50);
        }

        // 输出几个保持寄存器
        for(usCurrent = 0; usCurrent < ucRegCount; usCurrent++)
        {
            UCHAR ucBuf[2];
            eMBRegHoldingCB(ucBuf, usRegAddrOut + usCurrent +1 , 1, MB_REG_READ);
            // 向从机发送写寄存器命令
            eMBMasterReqWriteHoldingRegister(
                &mbMasterStack,                // Modbus主机栈实例
                usSlaveOutputAddr,              // 从机地址
                usRegAddrOut + usCurrent,      // 寄存器地址
                (ucBuf[0]<<8) | ucBuf[1],          // 寄存器值
                ucTimeoutMs                    // 超时时间
            );

            // 等待命令处理完成
            osDelay(50);
        }

        // 输出从机end


        // 针对输入从机
        // 读取从机保持寄存器以监控状态
        // 循环遍历8个线圈
        extern USHORT   usSDiscInStart;

        extern UCHAR    ucSDiscInBuf[];
        // ...existing code...
        UCHAR coilValues[8] = {1,1,1,1,0,0,0,0}; // 用于存储读取的线圈值
        Read8CoilsFromSlave(&mbMasterStack, usSlaveInputAddr, usSCoilStart, coilValues, ucTimeoutMs);

        // 清零目标位
        ucSDiscInBuf[usSDiscInStart+6] &= 0x01; // 保留bit0，清除bit1~bit7
        ucSDiscInBuf[usSDiscInStart+7] &= 0xFE; // 保留bit1~bit7，清除bit0

        // 设置bit1~bit7
        for(usCurrent = 0; usCurrent < 7; usCurrent++)
        {
            if(coilValues[usCurrent])
                ucSDiscInBuf[usSDiscInStart+6] |= (1 << (usCurrent + 1));
            else
                ucSDiscInBuf[usSDiscInStart+6] &= ~(1 << (usCurrent + 1));
        }

        // 设置bit0 of ucSDiscInBuf[usSDiscInStart+7]
        if(coilValues[7])
            ucSDiscInBuf[usSDiscInStart+7] |= 0x01;
        else
            ucSDiscInBuf[usSDiscInStart+7] &= ~0x01;
        // ...existing code...
        // ...existing code...
        // ...existing code...

        // 输入从机end


        // 合理的通信间隔
        osDelay(200);
    }
}