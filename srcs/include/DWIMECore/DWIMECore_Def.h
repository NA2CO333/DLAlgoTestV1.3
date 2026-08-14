#ifndef DWIMECORE_DEF_H
#define DWIMECORE_DEF_H


// ------------------------------
// 大版本定义
// ------------------------------
typedef enum __DWVersion
{
    VERSION_UNKNOW              = 0,
    VERSION_PRACTICAL           = 1,  // 实用版
    VERSION_PROFESSIONAL        = 3,  // 专业版
    VERSION_ENTERPRISE          = 5,  // 企业版

}DWVersion;
typedef char __check_dwversion[sizeof(DWVersion) == 4 ? 1 : -1];


// ------------------------------
// 布尔类型
// ------------------------------
typedef enum __DWBool
{
    dw_false = 0x0000,
    dw_true  = 0x0001
}DWBool;
typedef char __check_dwbool[sizeof(DWBool) == 4 ? 1 : -1];


// ------------------------------
// enum InputMode，输入模式
// ------------------------------
typedef enum __DWInputMode
{
    DWIM_NONE			        = 0,
    DWIM_PINYIN                 = 1,    // 拼音
    DWIM_STROKE                 = 2,    // 笔画
    DWIM_WUBI                   = 3,    // 五笔
    DWIM_CANGJIE                = 4,    // 颉仓
    DWIM_SUCHENG                = 5,    // 速成
    DWIM_ZHUYIN                 = 6,    // 注音
    DWIM_KOPINYIN               = 7,    // 韩语拼音
    DWIM_JPROMAJI               = 8,    // 日语罗马字
    DWIM_JPKANA                 = 9,    // 日文假名
    DWIM_MYANMAR                = 10,   // 藏系如，缅文
    DWIM_LATIN                  = 11,   // 拉丁系：英文，法文，德文等等
}DWInputMode;
typedef char __check_dwinputmode[sizeof(DWInputMode) == 4 ? 1 : -1];


// ------------------------------
// enum DWKBType，键盘类型
// ------------------------------
typedef enum __DWKBType
{
    DWKBT_NONE                  = 0,
    DWKBT_DIGIT                 = 9,   // 数字键盘（用0~9输入）
    DWKBT_QWERTY                = 26,  // 全键盘
}DWKBType;
typedef char __check_dwkbtype[sizeof(DWKBType) == 4 ? 1 : -1];


// ------------------------------
// enum DWKeyStatus 按键状态
// ------------------------------
typedef enum __DWKeyStatus
{
    DWKS_Down                   = 0x0000,
    DWKS_Up                     = 0x0001,
    DWKS_Swypeing               = 0x0002,
    DWKS_Stop                   = 0x0003,
}DWKeyStatus;
typedef char __check_dwkeystatus[sizeof(DWKeyStatus) == 4 ? 1 : -1];


// ------------------------------
// enum DWLanguage, 语种
// ------------------------------
typedef enum __DWLanguage
{
    DWL_None                     = 0,   /* 无 */
    DWL_Afrikaans                = 1,   /* 南非语 */
    DWL_Albanian                 = 2,   /* 阿尔巴尼亚人 */
    DWL_Arabic                   = 3,   /* 阿拉伯语 */
    DWL_Basque                   = 4,   /* 巴斯克 */
    DWL_Belarusian               = 5,   /* 白俄罗斯 */
    DWL_Bulgarian                = 6,   /* 保加利亚语 */
    DWL_Catalan                  = 7,   /* 加泰罗尼亚 */
    DWL_Chinese                  = 8,   /* 简体(中文) */
    DWL_Croatian                 = 9,   /* 克罗地亚 */
    DWL_Czech                    = 10,  /* 捷克 */
    DWL_Danish                   = 11,  /* 丹麦 */
    DWL_Dutch                    = 12,  /* 荷兰 */
    DWL_English                  = 13,  /* 英语 */
    DWL_Estonian                 = 14,  /* 爱沙尼亚语 */
    DWL_Faeroese                 = 15,  /* 法罗语 */
    DWL_Farsi                    = 16,  /* 波斯语 */
    DWL_Finnish                  = 17,  /* 芬兰 */
    DWL_French                   = 18,  /* 法国 */
    DWL_Gaelic                   = 19,  /* 盖尔 */
    DWL_German                   = 20,  /* 德国 */
    DWL_Greek                    = 21,  /* 希腊 */
    DWL_Hebrew                   = 22,  /* 希伯来语 */
    DWL_Hindi                    = 23,  /* 印地文 */
    DWL_Hungarian                = 24,  /* 匈牙利 */
    DWL_Icelandic                = 25,  /* 冰岛 */
    DWL_Indonesian               = 26,  /* 印度尼西亚 */
    DWL_Italian                  = 27,  /* 意大利 */
    DWL_Japanese                 = 28,  /* 日本 */
    DWL_Korean                   = 29,  /* 韩国 */
    DWL_Latvian                  = 30,  /* 拉脱维亚 */
    DWL_Lithuanian               = 31,  /* 立陶宛 */
    DWL_Macedonian               = 32,  /* 马其顿 */
    DWL_Malaysian                = 33,  /* 马来西亚 */
    DWL_Maltese                  = 34,  /* 马耳他人 */
    DWL_Norwegian                = 35,  /* 挪威 */
    DWL_Polish                   = 36,  /* 波兰 */
    DWL_Portuguese               = 37,  /* 葡萄牙 */
    DWL_Romanian                 = 38,  /* 罗马尼亚 */
    DWL_Russian                  = 39,  /* 俄罗斯 */
    DWL_Serbian                  = 40,  /* 塞尔维亚 */
    DWL_Slovak                   = 41,  /* 斯洛伐克 */
    DWL_Slovenian                = 42,  /* 斯洛文尼亚 */
    DWL_Sorbian                  = 43,  /* 索布 */
    DWL_Spanish                  = 44,  /* 西班牙语 */
    DWL_Sutu                     = 45,  /* 苏图 */
    DWL_Swedish                  = 46,  /* 瑞典 */
    DWL_Thai                     = 47,  /* 泰国 */
    DWL_Tsonga                   = 48,  /* 特松加 */
    DWL_Tswana                   = 49,  /* 茨瓦纳 */
    DWL_Turkish                  = 50,  /* 土耳其 */
    DWL_Ukrainian                = 51,  /* 乌克兰 */
    DWL_Urdu                     = 52,  /* 乌尔都语 */
    DWL_Vietnamese               = 53,  /* 越南 */
    DWL_Xhosa                    = 54,  /* 科萨语 */
    DWL_Yiddish                  = 55,  /* 意第绪语 */
    DWL_Zulu                     = 56,  /* 祖鲁 */
    DWL_Uighur                   = 57,  /* 维吾尔 */
    DWL_Tibetan                  = 58,  /* 西藏 */
    DWL_Swahili                  = 59,  /* 斯瓦希里 */
    DWL_Traditional              = 60,  /* 繁体(中文) */
    DWL_Hausa                    = 61,  /* 豪萨 */
    DWL_Tagalog                  = 62,  /* 菲律宾语 */
    DWL_Pashto                   = 63,  /* 普什图语 */
    DWL_Lao                      = 64,  /* 老挝 */
    DWL_Khmer                    = 65,  /* 高棉 */
    DWL_Myanmar                  = 66,  /* 缅甸 */
    DWL_Telugu                   = 67,  /* 泰卢固语 */
    DWL_Bengali                  = 68,  /* 孟加拉 */
    DWL_Marathi                  = 69,  /* Marathi */
    DWL_Kannada                  = 70,  /* 卡纳达语 */
    DWL_Malayalam                = 71,  /* 马来亚 */
    DWL_Tamil                    = 72,  /* 泰米尔人 */
    DWL_Punjabi                  = 73,  /* 旁遮普语 */
    DWL_Gujarati                 = 74,  /* 古加拉特 */
    DWL_Oriya                    = 75,  /* 奥里亚 */
    DWL_Assamese                 = 76,  /* 阿萨姆 */
}DWLanguage;
typedef char __check_dwlanguage[sizeof(DWLanguage) == 4 ? 1 : -1];


// ------------------------------
// enum DWKey 按键定义
// ------------------------------
typedef enum __DWKey
{
    DWKEY_Back                  = 0x86,
    DWKEY_A                     = 'a',
    DWKEY_B                     = 'b',
    DWKEY_C                     = 'c',
    DWKEY_D                     = 'd',
    DWKEY_E                     = 'e',
    DWKEY_F                     = 'f',
    DWKEY_G                     = 'g',
    DWKEY_H                     = 'h',
    DWKEY_I                     = 'i',
    DWKEY_J                     = 'j',
    DWKEY_K                     = 'k',
    DWKEY_L                     = 'l',
    DWKEY_M                     = 'm',
    DWKEY_N                     = 'n',
    DWKEY_O                     = 'o',
    DWKEY_P                     = 'p',
    DWKEY_Q                     = 'q',
    DWKEY_R                     = 'r',
    DWKEY_S                     = 's',
    DWKEY_T                     = 't',
    DWKEY_U                     = 'u',
    DWKEY_V                     = 'v',
    DWKEY_W                     = 'w',
    DWKEY_X                     = 'x',
    DWKEY_Y                     = 'y',
    DWKEY_Z                     = 'z',
}DWKey;
typedef char __check_dwkey[sizeof(DWKey) == 4 ? 1 : -1];

// ------------------------------
// DWIMECore_hwSetOption(int option, int value)
// 手写功能设置项定义，即 option 参数
// ------------------------------
typedef enum __DWHWOption
{
    DWHW_OPTION_RESULT          = 0,   // 返回结果的数量，默认为10
    DWHW_OPTION_NUMBER          = 1,   // 识别范围: 数字
    DWHW_OPTION_LOWER           = 2,   // 识别范围: 小字字母
    DWHW_OPTION_UPPER           = 3,   // 识别范围: 大写字母
    DWHW_OPTION_ENSYMB          = 4,   // 识别范围: 英文符号
    DWHW_OPTION_CNSYMB          = 5,   // 识别范围: 中文符号
    DWHW_OPTION_GB2312          = 6,   // 识别范围: 中文简体
    DWHW_OPTION_BIG5            = 7,   // 识别范围: 中文繁体
    DWHW_OPTION_GBK             = 8,   // 识别范围: GBK字符集
    DWHW_OPTION_GB18030         = 9,   // 识别范围: GB18030字符集
    DWHW_OPTION_EXTB            = 10,  // 识别范围: ExtB字符集 (未来扩展用，目前不支持)
    DWHW_OPTION_MODE            = 11,  // 识别模式：0单字、1行写、2叠写、3多模式自动区分
    DWHW_OPTION_SCREEN          = 12,  // 设置屏幕尺寸, 参数为: (((int)width) & 0x0000ffff) | ((((int)height) << 16) & 0xffff0000)
}DWHWOption;


// ------------------------------
// 手写识别模式
// ------------------------------
typedef enum __DWHWMode
{
    DWHW_MODE_SINGLE = 0,       // 单字
    DWHW_MODE_LINE = 1,         // 行写
    DWHW_MODE_OVERLAP = 2,      // 叠写
    DWHW_MODE_AUTO = 3,         // 自动区分
    
}DWHWMode;



// ------------------------------
// DWIMECore_setPYFuzzy(int fuzzy)
// 模糊音定义，即 fuzzy 参数
// ------------------------------
typedef enum __DWFuzzy
{
    // 声母 7 个
    DWIME_FUZZY_C_CH            = 0X00000001,
    DWIME_FUZZY_S_SH		    = 0X00000002,
    DWIME_FUZZY_Z_ZH		    = 0X00000004,
    DWIME_FUZZY_K_G		        = 0X00000008,
    DWIME_FUZZY_F_H		        = 0X00000010,
    DWIME_FUZZY_N_L		        = 0X00000020,
    DWIME_FUZZY_L_R		        = 0X00000040,
    // 韵母 10 个
    DWIME_FUZZY_AN_ANG	        = 0X00000080,
    DWIME_FUZZY_EN_ENG	        = 0X00000100,
    DWIME_FUZZY_IN_ING	        = 0X00000200,
    DWIME_FUZZY_IAN_IANG	    = 0X00000400,
    DWIME_FUZZY_UAN_UANG	    = 0X00000800,
    DWIME_FUZZY_AN_AI		    = 0X00001000,
    DWIME_FUZZY_UN_ONG	        = 0X00002000,
    DWIME_FUZZY_IE_UE	        = 0X00004000,
    DWIME_FUZZY_UN_IONG         = 0X00008000,
    DWIME_FUZZY_ENG_ONG         = 0X00010000,
    // 自定义模糊音
    DWIME_FUZZY_CUSTOM1         = 0X00020000,
    DWIME_FUZZY_CUSTOM2         = 0X00040000,
    DWIME_FUZZY_CUSTOM3         = 0X00080000,
    DWIME_FUZZY_CUSTOM4         = 0X00100000,
    DWIME_FUZZY_CUSTOM5         = 0X00200000,
}DWFuzzy;


// ------------------------------
// DWIMECore_setOption(int option, int value)
// 输入法功能设置定义，即 option参数
// ------------------------------
typedef enum __DWOption
{
    DWIME_OPTIONS_ASSOCIATE     = 1,       /* 联想开关                                                                    */
    DWIME_OPTIONS_LOOPASSOCIATE = 2,       /* 循环联想开关                                                                */
    DWIME_OPTIONS_SENTENCE      = 3,       /* 自动组句开关                                                                */
    DWIME_OPTIONS_AUTOCOMMIT    = 4,       /* 自动输出开关，仅：五笔，速成，仓颉适用                                      */
    DWIME_OPTIONS_AUTOBACK      = 5,       /* 自动退格开关，输入一个键无结果时，自动回退一个键,仅:五笔,速成,仓颉适用      */
    DWIME_OPTIONS_USERDB        = 6,       /* 记忆功能开关，修改后， ★★要重新初始化引擎，并且要init前设置 ★★          */
    DWIME_OPTIONS_CORRECT       = 7,       /* 纠错功能开关，仅：拼音、英文可用，同 DWIMECore_setCorrect                   */
    DWIME_OPTIONS_CONTACT       = 8,       /* 搜索联系人，前提是已经导入联系人到记忆里                                    */
    DWIME_OPTIONS_WBMIXPY       = 9,       /* 五笔模式里同时搜索拼音                                                      */
    DWIME_OPTIONS_SHOWWBCODE    = 10,      /* 五笔模式里，用拼音打出来的字显示五笔编码，方便五笔学习                      */
    DWIME_OPTIONS_SHOWWBHINT    = 11,      /* 五笔模式里，显示编码提示                                                    */
    DWIME_OPTIONS_SHOWGB18030   = 12,      /* 显示 GB18030 的候选字（仅笔画）                                             */
    DWIME_OPTIONS_SHOWEXTB      = 13,      /* 显示 EXTB 的候选字（仅笔画）                                                */
    DWIME_OPTIONS_SHOWEMOJI     = 14,      /* 显示表情 (仅：拼音、笔画、五笔生效)                                         */
    DWIME_OPTIONS_SHUANGPIN     = 15,      /* 开启双拼音功能，value = 1 ~ n，表示双拼方案，0就是关闭功能                  */
    DWIME_OPTIONS_ENMIX         = 16,      /* 开启拼音可以混合英文的功能，1 开启，0关闭                                   */
    DWIME_OPTIONS_PUNCASSE      = 17,      /* 标点联想功能是否开启，1 开启，0关闭                                         */
    DWIME_OPTIONS_FUZZY         = 18,      /* 设置模糊功能，同 DWIMECore_setPYFuzzy                                       */
}DWOption;


// ------------------------------
// DWIMECore_userDBGetAttr(int index, int attrType)
// 记忆数据属性定义，即 attrType 参数
// ------------------------------
typedef enum __DWUserAttr
{
    DWIME_USER_ATTR_FREQ        = 0x71657266,     // freq, 查询调频值
    DWIME_USER_ATTR_MODTIME     = 0x656d6974,     // time, 保留：查询最后修改时间
    DWIME_USER_ATTR_ISNEW       = 0x3F77656E,     // new?, 保留：查询是不是新造的词
    DWIME_USER_ATTR_PHRLEN      = 0x006E656C,     // len, 保留：查询词长度
}DWUserAttr;


//--------------------------------
// 记忆数据类型
//--------------------------------
typedef enum __DWUserDBType
{
    DWIME_USERDB_TYPE_CURRENT   = 0x0,              // 当前正在使用的数据，如当前是拼音，那当前的记忆数据就是拼音数据。
    DWIME_USERDB_TYPE_PINYIN    = 0x00007970,       // py, 拼音记忆数据
    DWIME_USERDB_TYPE_STROKES   = 0x00006b73,       // sk, 笔画记忆数据
    DWIME_USERDB_TYPE_ENGLISH   = 0x00006e65,       // en, 英文记忆数据
    DWIME_USERDB_TYPE_BIGRAM    = 0x6d726762,       // bgrm, 二元记忆数据
    DWIME_USERDB_TYPE_CONTACTS  = 0x746e6f63,       // cont, 导入的联系人数据（如果要删除联系人的记忆就要用到）
    
}DWUserDBType;


//--------------------------------
// 候选属性种类定义 
//--------------------------------
typedef enum __DWCandAttr
{
    DWIME_CAND_ATTR_TYPE        = 0x65707974,       // type, DWIMECore_getCandAttr 的attrType 参数，用于查询候选词是什么类型
}DWCandAttr;

//--------------------------------
// 候选类型
//--------------------------------
typedef enum __DWCandType
{
    DWIME_CAND_TYPE_SENTENCE    = 0x746e6573,       // sent, 智能组句
    DWIME_CAND_TYPE_FULLWORD    = 0x6c6c7566,       // full, 完整词
    DWIME_CAND_TYPE_PARTWORD    = 0x74726170,       // part, 部分词
    DWIME_CAND_TYPE_ASSE        = 0x65737361,       // asse, 联想
    DWIME_CAND_TYPE_SIZI        = 0x697a6973,       // sizi, 单字
    DWIME_CAND_TYPE_FUZZY       = 0x797a7566,       // fuzy, 模糊音
    DWIME_CAND_TYPE_CORRECTED   = 0x72726f63,       // corr, 智能纠错
    DWIME_CAND_TYPE_CELL        = 0x6c6c6563,       // cell, 自细胞词
    DWIME_CAND_TYPE_CONTACT     = 0x746e6f63,       // cont, 联系人
    DWIME_CAND_TYPE_CLOUD       = 0x756f6c63,       // clou, 云端推荐
}DWCandType;

// ------------------------------
// 数据类型定义
// ------------------------------
typedef enum __DWDataType
{
    // 手写识别的数据
    DWIME_DATA_HWDB             = 0,
    
    // V1 引擎的数据
    DWIME_V1_DATA_PY            = 1,
    DWIME_V1_DATA_SK            = 2,
    DWIME_V1_DATA_FIX           = 3,
    DWIME_V1_DATA_LT            = 4,
    DWIME_V1_DATA_ASSE          = 5,
    DWIME_V1_DATA_USER          = 6,
    DWIME_V1_DATA_EXT           = 7,
    
    
    // V3 引擎的数据
    DWIME_V3_DATA_MAIN          = 1,
    DWIME_V3_DATA_PYTR          = 2,
    DWIME_V3_DATA_SKTR          = 3,
    DWIME_V3_DATA_FIXTR         = 4,
    DWIME_V3_DATA_PYWL          = 5,
    DWIME_V3_DATA_ASSE          = 6,
    DWIME_V3_DATA_LATIN         = 7,
    DWIME_V3_DATA_USER          = 8,
    DWIME_V3_DATA_EXT           = 9,
    DWIME_V3_DATA_CELL01        = 21,    // 细胞数据开始id
    DWIME_V3_DATA_CELLEND       = 40,    // 细胞数据结束id，共20个
    
    // V5 引擎的数据
    DWIME_V5_DATA_MAIN          = 1,
    DWIME_V5_DATA_PYTR          = 2,
    DWIME_V5_DATA_PYSL          = 3,
    DWIME_V5_DATA_FIXTR         = 4,
    DWIME_V5_DATA_SKTR          = 5,
    DWIME_V5_DATA_LATIN         = 6,
    DWIME_V5_DATA_USER          = 7,
    DWIME_V5_DATA_CELL01        = 31,    // 细胞数据开始id
    DWIME_V5_DATA_CELLEND       = 60,    // 细胞数据结束id，共30个
}DWDataType;


// 错误码定义
typedef enum __DWError
{
    DWRT_OK                      =  0,   // 操作成功
    DWRT_IGNORED                 =  1,   // 操作被忽略
    DWRT_FAILED	                 =  2,   // 操作失败
    DWRT_ENGINE_NOTINIT          =  3,   // 引擎没有初始化
    DWRT_ENGINE_NULL             =  4,   // 引擎为空
    DWRT_INVALID_KEY             =  5,   // 键值错误
    DWRT_INVALID_INPUTMODE       =  6,   // 输入模式错误
    DWRT_INVALID_PARAM           =  7,   // 传入参数正确
    DWRT_INVALID_DATABASE        =  8,   // 数据库错误
    DWRT_INVALID_USERDB          =  9,   // 记忆数据库错误
    DWRT_INVALID_INDEX           =  10,  // 索引错误
    DWRT_NOTENOUGH_MEMORY        =  11,  // 内存不足
}DWError;
typedef char __check_dwerror[sizeof(DWError) == 4 ? 1 : -1];

#endif  // DWIMECORE_DEF_H 
 