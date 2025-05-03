/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"
#include "mb_stack.h"

/* ----------------------- Defines ------------------------------------------*/
#define MB_PDU_FUNC_DIAG_SUBFUNC_OFF            (MB_PDU_DATA_OFF)
#define MB_PDU_FUNC_DIAG_SUBFUNC_LEN            (2)
#define MB_PDU_FUNC_DIAG_DATA_OFF               (MB_PDU_DATA_OFF + 2)
#define MB_PDU_FUNC_DIAG_DATA_LEN               (2)

/* 08命令的子功能码定义 */
#define MB_FUNC_DIAG_SUBFUNC_CLEAR_COUNTERS     (0x000A)  /* 10号子功能码：清除计数器和诊断寄存器 */
#define MB_FUNC_DIAG_SUBFUNC_BUS_MESSAGE_COUNT  (0x000E)  /* 14号子功能码：返回总线消息计数 */

/* 外部引用的消息计数器 */
extern USHORT usMBMessageCounters[2]; /* 假设第0个元素是总消息数，第1个元素是正确消息数 */

/* ----------------------- Static functions ---------------------------------*/
eMBException prveMBError2Exception(eMBErrorCode eErrorCode);

/* ----------------------- Start implementation -----------------------------*/

/**
 * 实现08诊断命令中的子功能码10和14
 * 
 * @param pucFrame 传入的Modbus帧数据
 * @param usLen 数据长度指针
 * @return 处理状态异常码
 */
eMBException
eMBFuncDiagnosticCounter(void * this, UCHAR * pucFrame, USHORT * usLen)
{
    UNUSED(this);
    eMBException    eStatus = MB_EX_NONE;
    USHORT          usSubFunction;
    USHORT          usDataValue;
    
    /* 检查最小长度 */
    if (*usLen >= (MB_PDU_FUNC_DIAG_SUBFUNC_LEN + MB_PDU_FUNC_DIAG_DATA_LEN + 1))
    {
        /* 获取子功能码 */
        usSubFunction = (USHORT)(pucFrame[MB_PDU_FUNC_DIAG_SUBFUNC_OFF] << 8);
        usSubFunction |= (USHORT)(pucFrame[MB_PDU_FUNC_DIAG_SUBFUNC_OFF + 1]);
        
        /* 获取数据值 */
        usDataValue = (USHORT)(pucFrame[MB_PDU_FUNC_DIAG_DATA_OFF] << 8);
        usDataValue |= (USHORT)(pucFrame[MB_PDU_FUNC_DIAG_DATA_OFF + 1]);
        
        switch (usSubFunction)
        {
            case MB_FUNC_DIAG_SUBFUNC_CLEAR_COUNTERS:
                /* 10号子功能码：清除计数器和诊断寄存器 */
                /* 清除所有计数器 */
                usMBMessageCounters[0] = 0;  /* 总消息数清零 */
                usMBMessageCounters[1] = 0;  /* 正确消息数清零 */
                
                /* 返回响应 */
                /* 响应格式与请求相同 */
                pucFrame[MB_PDU_FUNC_OFF] = MB_FUNC_DIAG_DIAGNOSTIC;
                /* 子功能码和数据部分保持不变 */
                *usLen = MB_PDU_FUNC_DIAG_DATA_OFF + MB_PDU_FUNC_DIAG_DATA_LEN;
                break;
                
            case MB_FUNC_DIAG_SUBFUNC_BUS_MESSAGE_COUNT:
                /* 14号子功能码：返回总线消息计数 */
                pucFrame[MB_PDU_FUNC_OFF] = MB_FUNC_DIAG_DIAGNOSTIC;
                /* 子功能码保持不变 */
                
                /* 设置响应数据为当前的总消息计数值 */
                pucFrame[MB_PDU_FUNC_DIAG_DATA_OFF] = (UCHAR)(usMBMessageCounters[0] >> 8);
                pucFrame[MB_PDU_FUNC_DIAG_DATA_OFF + 1] = (UCHAR)(usMBMessageCounters[0]);
                
                *usLen = MB_PDU_FUNC_DIAG_DATA_OFF + MB_PDU_FUNC_DIAG_DATA_LEN;
                break;
                
            default:
                /* 不支持的子功能码 */
                eStatus = MB_EX_ILLEGAL_FUNCTION;
                break;
        }
    }
    else
    {
        /* 请求长度不足 */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    
    return eStatus;
}