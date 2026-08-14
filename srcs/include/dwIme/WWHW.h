#ifndef __WWHW_H__
#define __WWHW_H__

/* 手写识别模块头文件 */

typedef                int              HW_INT32;
typedef unsigned       int              HW_UINT32;

typedef                short            HW_INT16;
typedef unsigned       short            HW_UINT16;


#define WWHW_RANGE_NUMBER				0x1         /* 识别范围：数字     */
#define WWHW_RANGE_LOWER_CHAR			0x2         /* 识别范围：小写字母 */
#define WWHW_RANGE_UPPER_CHAR			0x4         /* 识别范围：大写字母 */
#define WWHW_RANGE_ASC_PUNCTUATION		0x8         /* 识别范围：半角标点符号 */

#define WWHW_RANGE_CN					0x8600      /* 识别范围：汉字 */
#define WWHW_RANGE_CHN_PUNCTUATION		0x800       /* 识别范围：中文标点符号 */
#define WWHW_RANGE_CONTROL_CHAR			0x2000      /* 识别范围：空格、回车以及删除等三个控制字符 */

#ifndef WWAPI
#ifdef WWIMPORT
#define WWAPI  __declspec(dllimport)
#else
#define WWAPI	extern
#endif
#endif

#ifdef __cplusplus
extern "C"{
#endif

enum WWHW_ERROR
{
        WWHW_OK             = 0x0,
        WWHW_ERR_NOT_INIT   = 0x100,
        WWHW_ERR_NULL_DATA,       /* 空数据 */
        WWHW_ERR_INVALID_DATA,    /* 数据格式错误 */
        WWHW_ERR_INVALID_PARAM,   /* 错误参数  */
    WWHW_ERR_COUNT
};

/***********************************************************************************************************
//
//  初始化引擎
//
//  data - 数据库开始地址， 如果是内置了数据库，这里转入 null 即可
//  param - 无用
//
//  返回：0 成功，非0失败
//
 ***********************************************************************************************************/
WWAPI int WWInitRecognition(const void * data,void* param);


/***********************************************************************************************************
//
// 关闭引擎
//
// 返回：0成功，非0失败
//
 ***********************************************************************************************************/
WWAPI int WWExitRecognition(void);


/***********************************************************************************************************
//
// 识别数
//
// pStrokeAry       (in)  字轨迹
// pResultStr       (out) 输出的结果 (Unicode )
// iCount           (in)  要求返回候选个数
// uMatchRange      (in)  识别范围
//
// return 返回结果的个数
//
 ***********************************************************************************************************/
WWAPI int WWRecognize(const short* pStrokeAry, unsigned short * pResultStr, int iCount, HW_UINT32 uMatchRange);

#ifdef __cplusplus
}
#endif

#endif
