/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"
#include "stm32f4xx_hal.h"
#include "main.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"
#include "mb_stack.h"

/* ----------------------- Defines ------------------------------------------*/
#define MB_PDU_FUNC_BAUDRATE_OFF           ( MB_PDU_DATA_OFF )
#define MB_PDU_FUNC_BAUDRATE_SIZE          ( 4 )

/* 支持的波特率列表 - STM32F407专用 */
#define MB_BAUDRATE_4800                   ( 4800UL )
#define MB_BAUDRATE_9600                   ( 9600UL )
#define MB_BAUDRATE_19200                  ( 19200UL )
#define MB_BAUDRATE_38400                  ( 38400UL )
#define MB_BAUDRATE_57600                  ( 57600UL )
#define MB_BAUDRATE_115200                 ( 115200UL )
#define MB_BAUDRATE_128000                 ( 128000UL )

/* Flash存储相关定义 - STM32F407专用 */
#define FLASH_USER_BAUDRATE_ADDR           (0x080E0000) /* Sector 11起始地址 */
#define FLASH_USER_BAUDRATE_SECTOR         FLASH_SECTOR_11

/* LED指示灯 */
#define BAUDRATE_CHANGE_LED                LED2_GPIO_Port
#define BAUDRATE_CHANGE_LED_PIN            LED2_Pin

/* ----------------------- Static functions ---------------------------------*/
eMBException    prveMBError2Exception( eMBErrorCode eErrorCode );
static BOOL     xMBIsValidBaudrate(ULONG ulBaudrate);
static void     vMBSetLED(BOOL xState);
void MB_SaveBaudrateToFlash(ULONG ulBaudrate);
/* ----------------------- Static variables ---------------------------------*/
static ULONG    ulCurrentBaudrate = 115200;  // 默认波特率
static ULONG    ulNewBaudrate = 0;
static BOOL     xBaudratePendingChange = FALSE;

/* ----------------------- Start implementation -----------------------------*/

#if MB_FUNC_CHANGE_BAUDRATE_ENABLED > 0

/**
 * 验证波特率值是否有效 - 针对STM32F407特定支持的波特率
 * @param ulBaudrate 待验证的波特率
 * @return TRUE 如果波特率有效，否则FALSE
 */
static BOOL xMBIsValidBaudrate(ULONG ulBaudrate)
{
    switch(ulBaudrate)
    {
        case MB_BAUDRATE_4800:
        case MB_BAUDRATE_9600:
        case MB_BAUDRATE_19200:
        case MB_BAUDRATE_38400:
        case MB_BAUDRATE_57600:
        case MB_BAUDRATE_115200:
        case MB_BAUDRATE_128000:
            return TRUE;
        default:
            return FALSE;
    }
}

/**
 * 波特率切换功能处理函数
 * @param this 协议栈实例
 * @param pucFrame PDU帧数据
 * @param usLen PDU数据长度指针
 * @return Modbus异常码
 */
eMBException eMBFuncChangeBaudrate(void *this, UCHAR *pucFrame, USHORT *usLen)
{
    UNUSED(this);
    ULONG           ulRequestedBaudrate = 0;
    UCHAR          *pucFrameCur;
    eMBException    eStatus = MB_EX_NONE;
    
    /* 检查PDU长度，应为功能码(1字节) + 波特率值(4字节) */
    if( *usLen == (MB_PDU_FUNC_BAUDRATE_SIZE + MB_PDU_SIZE_MIN) )
    {
        /* 从请求中提取波特率值 (ASCII格式解析) */
        BOOL isValidASCII = TRUE;

        /* 将ASCII格式的十进制数字转换为实际波特率值 */
        for (int i = 0; i < 4; i++) {
            UCHAR byte = pucFrame[MB_PDU_FUNC_BAUDRATE_OFF + i];
            /* 处理高4位 */
            UCHAR highNibble = (byte >> 4) & 0x0F;
            if (highNibble >= 0 && highNibble <= 9) {
                ulRequestedBaudrate = ulRequestedBaudrate * 10 + highNibble;
            } else {
                isValidASCII = FALSE;
                break;
            }
            
            /* 处理低4位 */
            UCHAR lowNibble = byte & 0x0F;
            if (lowNibble >= 0 && lowNibble <= 9) {
                ulRequestedBaudrate = ulRequestedBaudrate * 10 + lowNibble;
            } else {
                isValidASCII = FALSE;
                break;
            }
        }

        // /* 如果ASCII解析失败，尝试传统的二进制格式解析 */
        // if (!isValidASCII) {
        //     ulRequestedBaudrate = ((ULONG)pucFrame[MB_PDU_FUNC_BAUDRATE_OFF] << 24) |
        //                         ((ULONG)pucFrame[MB_PDU_FUNC_BAUDRATE_OFF + 1] << 16) |
        //                         ((ULONG)pucFrame[MB_PDU_FUNC_BAUDRATE_OFF + 2] << 8) |
        //                         ((ULONG)pucFrame[MB_PDU_FUNC_BAUDRATE_OFF + 3]);
        // }

        /* 验证波特率值是否有效 */
        if(ulRequestedBaudrate == 11111111)
        {
            /* 特殊波特率码: 11111111 - 执行系统重启 */
            
            /* 设置响应帧 */
            pucFrameCur = &pucFrame[MB_PDU_FUNC_OFF];
            *usLen = MB_PDU_FUNC_OFF;
            
            /* 功能码 */
            *pucFrameCur++ = MB_FUNC_CHANGE_BAUDRATE;
            *usLen += 1;
            
            /* 回传特殊重启码 */
            *pucFrameCur++ = 0x11;
            *pucFrameCur++ = 0x11;
            *pucFrameCur++ = 0x11;
            *pucFrameCur++ = 0x11;
            *usLen += 4;
            
            /* 将重启任务延迟执行，确保响应帧能够完全传输出去 */
            xBaudratePendingChange = TRUE;
            ulNewBaudrate = 0;  // 使用0作为特殊标志，表示需要重启
            
            /* 可选: 保存一个重启标志到Flash，确保在重启后可以检测到这是一次请求的重启 */
            //MB_SaveResetFlagToFlash(0xA5A5A5A5);
            
            return MB_EX_NONE;
        }
        else if(xMBIsValidBaudrate(ulRequestedBaudrate))
        {
            /* 设置响应帧 */
            pucFrameCur = &pucFrame[MB_PDU_FUNC_OFF];
            *usLen = MB_PDU_FUNC_OFF;
            
            /* 功能码 */
            *pucFrameCur++ = MB_FUNC_CHANGE_BAUDRATE;
            *usLen += 1;
            
            /* 回传波特率值 - ASCII格式 */
            ULONG tempBaudrate = ulRequestedBaudrate;
            UCHAR asciiBytes[4] = {0};
            
            /* 将波特率转换为ASCII格式，每字节包含2个十进制数字 */
            for (int i = 3; i >= 0; i--) {
                UCHAR lowDigit = tempBaudrate % 10;
                tempBaudrate /= 10;
                UCHAR highDigit = tempBaudrate % 10;
                tempBaudrate /= 10;
                
                asciiBytes[i] = (highDigit << 4) | lowDigit;
            }
            
            /* 将ASCII字节写入响应帧 */
            *pucFrameCur++ = asciiBytes[0];
            *pucFrameCur++ = asciiBytes[1];
            *pucFrameCur++ = asciiBytes[2];
            *pucFrameCur++ = asciiBytes[3];
            *usLen += 4;
            
            /* 保存新波特率，等待切换 */
            ulNewBaudrate = ulRequestedBaudrate;
            xBaudratePendingChange = TRUE;
                    
            /* 保存设置到Flash */
            MB_SaveBaudrateToFlash(ulNewBaudrate);
        }
        else
        {
            /* 波特率值无效 */
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
            //eStatus = ((ULONG)pucFrame[MB_PDU_FUNC_BAUDRATE_OFF] << 24);
        }
    }
    else
    {
        /* PDU长度错误 */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    
    return eStatus;
}


/**
 * 从Flash读取波特率设置 - STM32F407专用实现
 */
ULONG MB_LoadBaudrateFromFlash(void)
{
    ULONG ulSavedBaudrate = *(__IO ULONG*)(FLASH_USER_BAUDRATE_ADDR);
    
    /* 验证读取的值是否有效 */
    if(xMBIsValidBaudrate(ulSavedBaudrate))
    {
        ulCurrentBaudrate = ulSavedBaudrate;
        return ulSavedBaudrate;
    }
    else
    {
        /* 无效值，返回默认波特率 */
        return MB_BAUDRATE_115200;
    }
    
}

/**
 * 将波特率设置保存到Flash - STM32F407专用实现
 * @param ulBaudrate 要保存的波特率值
 */
void MB_SaveBaudrateToFlash(ULONG ulBaudrate)
{
    HAL_StatusTypeDef status;
    
    /* 解锁Flash */
    HAL_FLASH_Unlock();
    
    /* 清除所有挂起的标志 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    
    /* 擦除扇区 */
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError = 0;
    
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Sector = FLASH_USER_BAUDRATE_SECTOR;
    eraseInit.NbSectors = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    if(HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK)
    {
        /* 写入波特率值 (4字节) */
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                           FLASH_USER_BAUDRATE_ADDR, 
                           ulBaudrate) != HAL_OK)
        {
            /* 写入失败，点亮LED2指示错误 */
            //HAL_GPIO_WritePin(LED2_GPIO_Port, LED2_Pin, GPIO_PIN_SET);
        }
    }
    
    /* 锁定Flash */
    HAL_FLASH_Lock();
}


/**
 * 波特率变更/系统重启任务
 * 检查是否需要更改波特率或执行系统重启
 * @param this 协议栈实例
 */
void MB_BaudrateTask(void *this)
{
    pMB_StackTypeDef p = (pMB_StackTypeDef)this;
    
    if (xBaudratePendingChange == TRUE)
    {
        if (ulNewBaudrate == 0)
        {
            /* 特殊情况：执行系统重启 */
            
            /* 先等待一段时间，确保Modbus响应帧已发送完成 */
            HAL_Delay(200);
            
            /* 执行系统重启 */
            HAL_NVIC_SystemReset();
        }
    }
}

#endif /* MB_FUNC_CHANGE_BAUDRATE_ENABLED > 0 */