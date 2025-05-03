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
#define MB_PDU_FUNC_CHANGE_T35_OFF           (MB_PDU_DATA_OFF)
#define MB_PDU_FUNC_CHANGE_T35_SIZE          (4)

/* Flash存储相关定义 - STM32F407专用 */
#define FLASH_USER_T35_ADDR                 (0x080E1000) /* Sector 12起始地址 */
#define FLASH_USER_T35_SECTOR               FLASH_SECTOR_11
#define DEFAULT_T35_VALUE                   (35)    /* 默认T3.5值 */
#define MIN_T35_VALUE                       (1)     /* 最小T3.5值 */
#define MAX_T35_VALUE                       (99999999) /* 最大T3.5值 */

/* ----------------------- Static variables ---------------------------------*/
static BOOL xT35PendingChange = FALSE;

/* ----------------------- Static functions ---------------------------------*/
eMBException prveMBError2Exception(eMBErrorCode eErrorCode);

/**
 * 将ASCII格式的数字转换为无符号长整型
 * @param pucBuffer ASCII格式数据缓冲区
 * @param ulLength 缓冲区长度
 * @return 转换后的数值
 */
static ULONG prvucASCIIToUlong(UCHAR *pucBuffer, ULONG ulLength)
{
    ULONG ulResult = 0;
    UCHAR ucDigit;
    
    for (ULONG i = 0; i < ulLength; i++)
    {
        /* 每个字节包含两个十进制数字 */
        /* 高4位 */
        ucDigit = (pucBuffer[i] >> 4) & 0x0F;
        if (ucDigit <= 9)
        {
            ulResult = ulResult * 10 + ucDigit;
        }
        
        /* 低4位 */
        ucDigit = pucBuffer[i] & 0x0F;
        if (ucDigit <= 9)
        {
            ulResult = ulResult * 10 + ucDigit;
        }
    }
    
    return ulResult;
}

/**
 * 将无符号长整型转换为ASCII格式
 * @param ulValue 要转换的数值
 * @param pucBuffer 输出缓冲区
 * @param ulLength 缓冲区长度
 * @return 实际写入的字节数
 */
static UCHAR prvulongToASCII(ULONG ulValue, UCHAR *pucBuffer, ULONG ulLength)
{
    UCHAR ucBytes[4] = {0};
    ULONG ulTemp = ulValue;
    
    /* 将数值转换为ASCII格式，每字节包含2个十进制数字 */
    for (int i = 3; i >= 0; i--)
    {
        UCHAR lowDigit = ulTemp % 10;
        ulTemp /= 10;
        UCHAR highDigit = ulTemp % 10;
        ulTemp /= 10;
        
        ucBytes[i] = (highDigit << 4) | lowDigit;
    }
    
    /* 复制到目标缓冲区 */
    for (ULONG i = 0; i < ulLength && i < 4; i++)
    {
        pucBuffer[i] = ucBytes[i];
    }
    
    return 4; /* 固定返回4字节 */
}

/**
 * 从Flash读取T35值 - STM32F407专用实现
 * @return 读取到的T35值，如果无效则返回默认值
 */
ULONG MB_LoadT35FromFlash(void)
{
    ULONG ulSavedT35 = *(__IO ULONG*)(FLASH_USER_T35_ADDR);
    
    /* 验证读取的值是否在合理范围内 */
    if(ulSavedT35 >= MIN_T35_VALUE && ulSavedT35 <= MAX_T35_VALUE)
    {
        return ulSavedT35;
    }
    else
    {
        /* 无效值，返回默认T35值 */
        return DEFAULT_T35_VALUE;
    }
}

/**
 * 将T35值保存到Flash - STM32F407专用实现
 * @param ulValue 要保存的T35值
 * @return 操作是否成功
 */
BOOL MB_SaveT35ToFlash(ULONG ulValue)
{
    BOOL bSuccess = FALSE;
    
    /* 解锁Flash */
    HAL_FLASH_Unlock();
    
    /* 清除所有挂起的标志 */
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | 
                          FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    
    /* 擦除扇区 */
    FLASH_EraseInitTypeDef eraseInit;
    uint32_t sectorError = 0;
    
    eraseInit.TypeErase = FLASH_TYPEERASE_SECTORS;
    eraseInit.Sector = FLASH_USER_T35_SECTOR;
    eraseInit.NbSectors = 1;
    eraseInit.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    
    if(HAL_FLASHEx_Erase(&eraseInit, &sectorError) == HAL_OK)
    {
        /* 写入T35值 (4字节) */
        if(HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
                            FLASH_USER_T35_ADDR, 
                            ulValue) == HAL_OK)
        {
            bSuccess = TRUE;
        }
    }
    
    /* 锁定Flash */
    HAL_FLASH_Lock();
    
    return bSuccess;
}

/**
 * 获取当前T3.5定时器值
 * @return 当前T3.5定时器值
 */
ULONG ulMBGetT35Value(void)
{
    /* 直接从Flash读取T35值 */
    return MB_LoadT35FromFlash();
}

/**
 * 设置T3.5定时器值 - 直接写入Flash
 * @param ulNewValue 新的T3.5值
 * @return 操作是否成功
 */
BOOL vMBSetT35Value(ULONG ulNewValue)
{
    if(ulNewValue >= MIN_T35_VALUE && ulNewValue <= MAX_T35_VALUE)
    {
        /* 直接写入Flash */
        return MB_SaveT35ToFlash(ulNewValue);
    }
    return FALSE; /* 无效值 */

}

/* ----------------------- Start implementation -----------------------------*/
#if MB_FUNC_CHANGE_T35_ENABLED > 0

/**
 * T3.5定时器值更改功能处理函数
 * @param pucFrame 传入的Modbus帧数据
 * @param usLen 数据长度指针
 * @return 处理状态异常码
 */
eMBException
eMBFuncChangeT35(void * this, UCHAR * pucFrame, USHORT * usLen)
{
    UNUSED(this);
    eMBException    eStatus = MB_EX_NONE;
    UCHAR          *pucFrameCur;
    
    /* 检查PDU长度是否符合要求 */
    if (*usLen == (MB_PDU_FUNC_CHANGE_T35_SIZE + 1))
    {
        /* 从请求中读取T3.5值 */
        ULONG ulRequestedT35 = prvucASCIIToUlong(pucFrame+1, 4);
        
        /* 验证T35值是否在合理范围内 */
        if (ulRequestedT35 >= MIN_T35_VALUE && ulRequestedT35 <= MAX_T35_VALUE)
        {
            /* 标记为待更新 - 实际更新将在MB_T35UpdateTask中执行 */
            xT35PendingChange = TRUE;
            
            /* 构建响应帧 */
            pucFrameCur = &pucFrame[MB_PDU_FUNC_OFF];
            *usLen = MB_PDU_FUNC_OFF;
            
            /* 功能码 */
            *pucFrameCur++ = MB_FUNC_CHANGE_T35;
            *usLen += 1;
            
            /* 回传T3.5值 - ASCII格式 */
            UCHAR ucByteCount = prvulongToASCII(ulRequestedT35, pucFrameCur, 4);
            *usLen += ucByteCount;
            
            // /* 将新值暂存在临时缓冲区，用于异步更新 */
            // HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, 
            //                  FLASH_USER_T35_ADDR + 4, /* 使用T35地址+4的位置暂存 */
            //                  ulRequestedT35);
            while(!vMBSetT35Value(ulRequestedT35));
            /* 先等待一段时间，确保Modbus响应帧已发送完成 */
            HAL_Delay(200);

            /* 执行系统重启 */
            HAL_NVIC_SystemReset();

            // 后面改到重启需求里去
        }
        else
        {
            /* T35值超出范围 */
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
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
 * T35更新任务 - 在主循环中定期调用
 * 检查是否需要更新T35值，如果需要则执行更新
 */
// void MB_T35UpdateTask(void)
// {
//     if (xT35PendingChange == TRUE)
//     {
//         /* 从临时缓冲区读取待更新的值 */
//         ULONG ulPendingValue = *(__IO ULONG*)(FLASH_USER_T35_ADDR + 4);
        
//         /* 写入到正式位置 */
//         if (MB_SaveT35ToFlash(ulPendingValue))
//         {
//             /* 清除标志 */
//             xT35PendingChange = FALSE;
//         }
//     }
// }

#endif /* MB_FUNC_CHANGE_T35_ENABLED > 0 */