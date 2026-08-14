#ifndef __spl_ime_eng_h_
#define __spl_ime_eng_h_


/** \enum InputMode
	\输入法模式
 */
enum InputMode
{
    IM_NONE = 0,            /* 无 */
    IM_PINYIN   = 0x01,     /* 拼音 */
    IM_STROKE,              /* 中文的 9键笔画输入法 */
    IM_FIXCODE,             /* 固定编码的输入法如：五笔 */
    IM_MYANMAR,             /* 缅甸 */
    IM_LATIN,               /* 单词 */
    IM_CHAR,                /* 字母 */
    IM_DIGITAL,             /* 数字 */
    IM_CUSTOM1,             /* 定制开发的输入法 */
    IM_CUSTOM2,             /* 定制开发的输入法 */
    IM_CUSTOM3,             /* 定制开发的输入法 */
    IM_CUSTOM4,             /* 定制开发的输入法 */
    IM_COUNT
};

/** \enum SPLanguage
    \ 语种
 */
enum SPLanguage
{
    SL_None = 0x0000,       /* 无 */
    SL_Afrikaans,           /* 南非语 */
    SL_Albanian,            /* 阿尔巴尼亚人 */
    SL_Arabic,              /* 阿拉伯语 */
    SL_Basque,              /* 巴斯克 */
    SL_Belarusian,          /* 白俄罗斯 */
    SL_Bulgarian,           /* 保加利亚语 */
    SL_Catalan,             /* 加泰罗尼亚 */
    SL_Chinese,             /* 简体(中文) */
    SL_Croatian,            /* 克罗地亚 */
    SL_Czech,               /* 捷克 */  /* 10 */
    SL_Danish,              /* 丹麦 */
    SL_Dutch,               /* 荷兰 */
    SL_English,             /* 英语 */
    SL_Estonian,            /* 爱沙尼亚语 */
    SL_Faeroese,            /* 法罗语 */
    SL_Farsi,               /* 波斯语 */
    SL_Finnish,             /* 芬兰 */
    SL_French,              /* 法国 */
    SL_Gaelic,              /* 盖尔 */
    SL_German,              /* 德国 */  /* 20 */
    SL_Greek,               /* 希腊 */
    SL_Hebrew,              /* 希伯来语 */
    SL_Hindi,               /* 印地文 */
    SL_Hungarian,           /* 匈牙利 */
    SL_Icelandic,           /* 冰岛 */
    SL_Indonesian,          /* 印度尼西亚 */
    SL_Italian,             /* 意大利 */
    SL_Japanese,            /* 日本 */
    SL_Korean,              /* 韩国 */
    SL_Latvian,             /* 拉脱维亚 */  /* 30 */
    SL_Lithuanian,          /* 立陶宛 */
    SL_Macedonian,          /* 马其顿 */
    SL_Malaysian,           /* 马来西亚 */
    SL_Maltese,             /* 马耳他人 */
    SL_Norwegian,           /* 挪威 */
    SL_Polish,              /* 波兰 */
    SL_Portuguese,          /* 葡萄牙 */
    SL_Romanian,            /* 罗马尼亚 */
    SL_Russian,             /* 俄罗斯 */	/*39*/
    SL_Serbian,             /* 塞尔维亚 */  /* 40 */
    SL_Slovak,              /* 斯洛伐克 */	/*41*/
    SL_Slovenian,           /* 斯洛文尼亚 */	/*42*/
    SL_Sorbian,             /* 索布 */		/*43*/
    SL_Spanish,             /* 西班牙语 */	/*44*/
    SL_Sutu,                /* 苏图 */
    SL_Swedish,             /* 瑞典 */
    SL_Thai,                /* 泰国 */
    SL_Tsonga,              /* 特松加 */
    SL_Tswana,              /* 茨瓦纳 */
    SL_Turkish,             /* 土耳其 */    /* 50 */
    SL_Ukrainian,           /* 乌克兰 */
    SL_Urdu,                /* 乌尔都语 */
    SL_Vietnamese,          /* 越南 */
    SL_Xhosa,               /* 科萨语 */
    SL_Yiddish,             /* 意第绪语 */
    SL_Zulu,                /* 祖鲁 */
    SL_Uighur,              /* 维吾尔 */
    SL_Tibetan,             /* 西藏 */
    SL_Swahili,             /* 斯瓦希里 */
    SL_Traditional,         /* 繁体(中文) */    /* 60 */
    SL_Hausa,               /* 豪萨 */
    SL_Tagalog,             /* 菲律宾语 */
    SL_Pashto,              /* 普什图语 */
    SL_Lao,                 /* 老挝 */
    SL_Khmer,               /* 高棉 */
    SL_Myanmar,             /* 缅甸 */      /* 66 */
    SL_Telugu,              /* 泰卢固语 */
    SL_Bengali,             /* 孟加拉 */
    SL_Marathi,             /* Marathi */
    SL_Kannada,             /* 卡纳达语 */
    SL_Malayalam,           /* 马来亚 */
    SL_Tamil,               /* 泰米尔人 */
    SL_Punjabi,             /* 旁遮普语 */
    SL_Gujarati,            /* 古加拉特 */
    SL_Oriya,               /* Oriya */
    SL_Assamese,            /* 阿萨姆 */
    SL_COUNT
};


/** 
	键盘类型
 */
enum KeyBoardType
{
    KBT_DIGIT = 0x01,
    KBT_QWERTY = 0x02,
    KBT_COUNT
};


/** \enum SplKeyType
    \brief key type defination
 */
enum SplKeyType
{
    SPKT_Down   = 0x0000,
    SPKT_Up,
    SPKT_Swypeing,          /* 滑行中 */
    SPKT_Stop,              /* 滑行停顿或结束 */
    SPKT_LongPress
};


/** \enum SplKey
    \brief key defination
 */
enum SplKey
{
/* char key */
    SPKEY_NULL                      = 0,

    SPKEY_Sharp                     = '#',      /* 0x23 */
    SPKEY_SEPARATOR                 = '\'',     /* 0x27 分隔符，拼音和9键笔画用 */
    SPKEY_Star                      = '*',      /* 0x2A */
    SPKEY_Dot                       = '.',      /* 0x2E */
    SPKEY_Cross                     = '-',      /* 日语里有用 */

    SPKEY_0                         = '0',      /* 0x30 */
    SPKEY_1                         = '1',      /* 0x31 */
    SPKEY_2                         = '2',      /* 0x32 */
    SPKEY_3                         = '3',      /* 0x33 */
    SPKEY_4                         = '4',      /* 0x34 */
    SPKEY_5                         = '5',	/* 0x35 */
    SPKEY_6                         = '6',	/* 0x36 */
    SPKEY_7                         = '7',      /* 0x37 */
    SPKEY_8                         = '8',      /* 0x38 */
    SPKEY_9                         = '9',      /* 0x39 */

    SPKEY_WILDCHAR                  = '?',      /* 0x3F 通配符，9键笔画用 */
    SPKEY_AT                        = '@',      /* 0x40 */

    SPKEY_A                         = 'a',      /* 0x61 */
    SPKEY_B                         = 'b',      /* 0x62 */
    SPKEY_C                         = 'c',      /* 0x63 */
    SPKEY_D                         = 'd',      /* 0x64 */
    SPKEY_E                         = 'e',      /* 0x65 */
    SPKEY_F                         = 'f',      /* 0x66 */
    SPKEY_G                         = 'g',      /* 0x67 */
    SPKEY_H                         = 'h',      /* 0x68 */
    SPKEY_I                         = 'i',      /* 0x69 */
    SPKEY_J                         = 'j',      /* 0x6A */
    SPKEY_K                         = 'k',      /* 0x6B */
    SPKEY_L                         = 'l',      /* 0x6C */
    SPKEY_M                         = 'm',      /* 0x6D */
    SPKEY_N                         = 'n',      /* 0x6E */
    SPKEY_O                         = 'o',      /* 0x6F */
    SPKEY_P                         = 'p',      /* 0x70 */
    SPKEY_Q                         = 'q',      /* 0x71 */
    SPKEY_R                         = 'r',      /* 0x72 */
    SPKEY_S                         = 's',      /* 0x73 */
    SPKEY_T                         = 't',      /* 0x74 */
    SPKEY_U                         = 'u',      /* 0x75 */
    SPKEY_V                         = 'v',      /* 0x76 */
    SPKEY_W                         = 'w',      /* 0x77 */
    SPKEY_X                         = 'x',      /* 0x78 */
    SPKEY_Y                         = 'y',      /* 0x79 */
    SPKEY_Z                         = 'z',      /* 0x7A */

    SPKEY_UA                         = 'A',
    SPKEY_UB                         = 'B',
    SPKEY_UC                         = 'C',
    SPKEY_UD                         = 'D',
    SPKEY_UE                         = 'E',
    SPKEY_UF                         = 'F',
    SPKEY_UG                         = 'G',
    SPKEY_UH                         = 'H',
    SPKEY_UI                         = 'I',
    SPKEY_UJ                         = 'J',
    SPKEY_UK                         = 'K',
    SPKEY_UL                         = 'L',
    SPKEY_UM                         = 'M',
    SPKEY_UN                         = 'N',
    SPKEY_UO                         = 'O',
    SPKEY_UP                         = 'P',
    SPKEY_UQ                         = 'Q',
    SPKEY_UR                         = 'R',
    SPKEY_US                         = 'S',
    SPKEY_UT                         = 'T',
    SPKEY_UU                         = 'U',
    SPKEY_UV                         = 'V',
    SPKEY_UW                         = 'W',
    SPKEY_UX                         = 'X',
    SPKEY_UY                         = 'Y',
    SPKEY_UZ                         = 'Z',

    MYEXT_1                         = '{',
    MYEXT_2                         = '}',
    MYEXT_3                         = '[',
    MYEXT_4                         = ']',
    MYEXT_5                         = '(',
    MYEXT_6                         = ')',
    MYEXT_7                         = '$',
    MYEXT_8                         = '&',
    MYEXT_9                         = '%',
    MYEXT_10                        = ';',
    MYEXT_11                        = '!',
    MYEXT_12                        = '^',
    MYEXT_13                        = '\"',
    MYEXT_14                        = ',',
    MYEXT_15                        = '|',

/* function key */
    SPKEY_FUNC_NULL                 = 0x80,
    SPKEY_Left                      = 0x81,
    SPKEY_Right                     = 0x82,
    SPKEY_Up                        = 0x83,
    SPKEY_Down                      = 0x84,
    SPKEY_OK                        = 0x85,
    SPKEY_Back                      = 0x86,
    SPKEY_Return                    = 0x87,

/* api key */
    SPKEY_API_NULL                  = 0x90,
    SPKEY_API_SHORTCUTS             = 0x91, /* 保留功能 */
    SPKEY_API_SWITCH_LT_LOWUP       = 0x92, /* 保留功能 */
    SPKEY_API_RESET                 = 0x93, /* 保留功能 */
    SPKEY_API_SYLLABLE              = 0x94, /* 选择音节 */
    SPKEY_API_ASSOCIATE             = 0x95, /* 联想功能开关 */
    SPKEY_API_AUTOCOMMIT            = 0x96, /* 自动上屏，暂时只适用于仓颉 */
    SPKEY_API_CORRECT               = 0x97, /* 容错功能 */
    SPKEY_API_LOOPASSOCIATE         = 0x98, /* 循环联想功能开关 */
    SPKEY_Num
} ;


/** 
    SIME internal return values definition
 */
enum SIMEReturn
{
    SMR_OK               = 0x00,    ///< OK, event handled
    SMR_Ignored          = 0x01,    ///< event ignored
    SMR_Failed           = 0x10,    ///< return errors of MMI internal begin with this value
    SMR_EngineNotInit,              ///< engine not initialized
    SMR_NoEngine,                   ///< no engine
    SMR_NullEvent,                  ///< pointer to event is null!
    SMR_InvalidEventType,           ///< invalid event type! see \ref EventType
    SMR_NullInitData,               ///< pointer to init data is null when event type is SplImeInit
    SMR_InvalidInputMode,           ///< invalid input mode, see \ref SplInputMode
    SMR_UnsupportedLanguageType,    ///< unsupported language type
    SMR_InvalidKeyType,             ///< invalid key type, see \ref SplKeyType
    SMR_InvalidKey,                 ///< invalid key, see \ref SplKey
    SMR_InvalidParam,               ///< 传入参数正确
    SMR_NUM
} ;



/* ----------------------------------------------------------------------------------- */
/* 初始化数据 */
/* ----------------------------------------------------------------------------------- */
typedef struct __tag_InitData
{
    void         *  imeData;
    void         *  udbData;        /* 调频数据库，必须是可写的一块内存 */
    unsigned int    udbDataSize;    /* 调频数据库内存大小 */
    void         *  asseData;       /* 单独的联想词库，有些用户会要求这样做，如：xfm， 注意这个库不能压缩 */
    unsigned char   reserved1;
    unsigned char   reserved2;
    unsigned char   reserved3;
    unsigned char   reserved4;
	
} SplInitData;


typedef int (*Spl_GetStringWidthAFunc) (const char *);
typedef int (*Spl_GetStringWidthWFunc) (const unsigned short *);

/* ----------------------------------------------------------------------------------- */
/* UI设置 */
/* ----------------------------------------------------------------------------------- */
typedef struct __tag_UIInfo
{
    Spl_GetStringWidthAFunc	fpGetStrWidthA;     //  获得char字符串宽度
    Spl_GetStringWidthWFunc	fpGetStrWidthW;     //  宽字符串的宽度
    unsigned int                candidateWidth;     // 候选栏宽度
    unsigned short              candMinSpacing;     // 每个候选之间的最小间隔
    unsigned short              candMaxCount;       // 最多输出多少个候选
}SplUIInfo;


/* ----------------------------------------------------------------------------------- */
/* 输出数据 */
/* ----------------------------------------------------------------------------------- */
typedef struct __tag_OutputInfo
{
    unsigned short      *       inputString;            /**< 记录用户的输入内容 */
    unsigned short      *       compString;             /**< 输入栏显示内容 */

    unsigned short      *       candidates[20];         /**< 候选字词 */
    unsigned short              candidatesNum;          /**< 候选的个数 */
    unsigned short              candidateIndex;         /**< 当前高亮的候选项索引 */

    unsigned char               isShowIMWin;            /**< 是否显示输入法窗口 */
    unsigned char               isSelectedCand;         /**< 当前是否进入候选选择状态 */
    unsigned char               isCanUpScreen;          /**< 在当前状态下是否可以上屏*/
    unsigned char		isAssociateMode;        /*< 是否处于中文联想模式    */

    unsigned char		isSwypeing;				/* 是否处于滑行状态 */
    unsigned char		padding;

    unsigned short              upscreenLen;            /**< isCanUpScreen为真时,存放需要上屏的字符个数 */
    unsigned short      *       upscreenStr;            /**< isCanUpScreen为真时,存放需要上屏的字符串 */

    unsigned char		isShowDownArrow;        /*< 判断候选页是否能下翻    */
    unsigned char		isShowUpArrow;          /*< 判断候选页是否能上翻    */
    unsigned char		isShowLeftArrow;        /*< 判断是否能左移选择候选  */
    unsigned char		isShowRightArrow;       /*< 判断是否能右移选择候选  */

    unsigned short      *       syllables[20];          /*< 音节        */
    unsigned short              syllableNum;            /*< 音节的个数  */
    unsigned short              syllableIndex;          /*< 拼音: 音节的索引，为0时表示当前选中数字区 */
} SplOutputInfo ;



/* ----------------------------------------------------------------------------------- */
/* 全局数据 */
/* ----------------------------------------------------------------------------------- */
typedef struct __tag_ImeGlobals
{
    unsigned short  language;           // 详看：SPLanguage
    unsigned char   inputMode;          // 详看：InputMode
    unsigned char   keyBoard;           // 详看：KeyBoardType
    SplInitData     initData;           // 引擎初始化参数
    SplUIInfo       uiInfo;             // UI参数设置
    SplOutputInfo   outputInfo;         // 引擎输出的结果
} SplImeGlobals;


#ifdef __cplusplus
extern "C" {
#endif

extern SplImeGlobals   g_SplImeGlobals;
extern enum SIMEReturn SplImeInit(void);
extern enum SIMEReturn SplImeDeinit(void);
extern enum SIMEReturn SplImeProcessKey(enum SplKey uKey, enum SplKeyType type, int param);


/*
 *  设置某按键输出的结果。因为在不同的手机里，有不同的操作习惯，
 *  比如：有些手机笔画模式下，是按7出符号的，有些是按#，更有些是按 0
 *  为了兼容这些不同的习惯，加了这一函数。
 *  用法：
 *  SplImeSetKeyCand(SPKEY_9, L":-D\0:-)\0;-)\0:-O\0:）\0:-P\0:-(\0-_-!\0\0");
 *
 *  以上代码的结果是：当用户单独按下“9”键的时候，会输出：:-D  :-)  ;-)  :-O  :）  :-P  :-(  -_-!
 *
 *  注意：cand 必须是全局的数据，因引擎内部不会copy，每个候选用 '\0' 结束，最后以两个 \0\0 结束
 *
 */
extern enum SIMEReturn SplImeSetKeyCand(enum SplKey uKey, const unsigned short *cand);


/*
 * 添加拼音模糊音，如 SplImeAddPYCorrect("zh", "z");
 *
 *	最多可以添加 20 个
 */
enum SIMEReturn SplImeAddPYCorrect(const char * py1, const char * py2);

unsigned int SplImePrepareUserDB(int inputMode);
enum SIMEReturn  SplImeCreateUserDB(int inputMode, void * db);

#ifdef __cplusplus
}
#endif

#endif

