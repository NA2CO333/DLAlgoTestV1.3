#ifndef CINPUTMETHODINTF_H
#define CINPUTMETHODINTF_H

//
#include <QObject>

typedef unsigned short char16;

enum enInputKey
{
/* char key */
        inputKey_NULL						= 0,

        inputKey_Sharp						= '#',      /* 0x23 */
    inputKey_SEPARATOR                 = '\'',     /* 0x27 分隔符，拼音和9键笔画用 */
        inputKey_Star						= '*',		/* 0x2A */
        inputKey_Dot                       = '.',      /* 0x2E */
        inputKey_Cross                     = '-',      /* 日语里有用 */

    inputKey_0 						= '0',      /* 0x30 */
        inputKey_1							= '1',      /* 0x31 */
        inputKey_2							= '2',      /* 0x32 */
        inputKey_3							= '3',      /* 0x33 */
        inputKey_4							= '4',      /* 0x34 */
        inputKey_5							= '5',	    /* 0x35 */
        inputKey_6							= '6',	    /* 0x36 */
        inputKey_7							= '7',      /* 0x37 */
        inputKey_8							= '8',      /* 0x38 */
        inputKey_9							= '9',      /* 0x39 */

    inputKey_WILDCHAR                  = '?',      /* 0x3F 通配符，9键笔画用 */
        inputKey_AT						= '@',      /* 0x40 */

    inputKey_A                         = 'a',      /* 0x61 */
    inputKey_B                         = 'b',      /* 0x62 */
    inputKey_C                         = 'c',      /* 0x63 */
    inputKey_D                         = 'd',      /* 0x64 */
    inputKey_E                         = 'e',      /* 0x65 */
    inputKey_F                         = 'f',      /* 0x66 */
    inputKey_G                         = 'g',      /* 0x67 */
    inputKey_H                         = 'h',      /* 0x68 */
    inputKey_I                         = 'i',      /* 0x69 */
    inputKey_J                         = 'j',      /* 0x6A */
    inputKey_K                         = 'k',      /* 0x6B */
    inputKey_L                         = 'l',      /* 0x6C */
    inputKey_M                         = 'm',      /* 0x6D */
    inputKey_N                         = 'n',      /* 0x6E */
    inputKey_O                         = 'o',      /* 0x6F */
    inputKey_P                         = 'p',      /* 0x70 */
    inputKey_Q                         = 'q',      /* 0x71 */
    inputKey_R                         = 'r',      /* 0x72 */
    inputKey_S                         = 's',      /* 0x73 */
    inputKey_T                         = 't',      /* 0x74 */
    inputKey_U                         = 'u',      /* 0x75 */
    inputKey_V                         = 'v',      /* 0x76 */
    inputKey_W                         = 'w',      /* 0x77 */
    inputKey_X                         = 'x',      /* 0x78 */
    inputKey_Y                         = 'y',      /* 0x79 */
    inputKey_Z                         = 'z',      /* 0x7A */

    inputKey_UA                         = 'A',
    inputKey_UB                         = 'B',
    inputKey_UC                         = 'C',
    inputKey_UD                         = 'D',
    inputKey_UE                         = 'E',
    inputKey_UF                         = 'F',
    inputKey_UG                         = 'G',
    inputKey_UH                         = 'H',
    inputKey_UI                         = 'I',
    inputKey_UJ                         = 'J',
    inputKey_UK                         = 'K',
    inputKey_UL                         = 'L',
    inputKey_UM                         = 'M',
    inputKey_UN                         = 'N',
    inputKey_UO                         = 'O',
    inputKey_UP                         = 'P',
    inputKey_UQ                         = 'Q',
    inputKey_UR                         = 'R',
    inputKey_US                         = 'S',
    inputKey_UT                         = 'T',
    inputKey_UU                         = 'U',
    inputKey_UV                         = 'V',
    inputKey_UW                         = 'W',
    inputKey_UX                         = 'X',
    inputKey_UY                         = 'Y',
    inputKey_UZ                         = 'Z',

//    MYEXT_1                         = '{',
//    MYEXT_2                         = '}',
//    MYEXT_3                         = '[',
//    MYEXT_4                         = ']',
//    MYEXT_5                         = '(',
//    MYEXT_6                         = ')',
//    MYEXT_7                         = '$',
//    MYEXT_8                         = '&',
//    MYEXT_9                         = '%',
//    MYEXT_10                        = ';',
//    MYEXT_11                        = '!',
//    MYEXT_12                        = '^',
//    MYEXT_13                        = '\"',
//    MYEXT_14                        = ',',
//    MYEXT_15                        = '|',

/* function key */
        inputKey_FUNC_NULL					= 0x80,
        inputKey_Left                      = 0x81,
        inputKey_Right                     = 0x82,
        inputKey_Up                        = 0x83,
        inputKey_Down                      = 0x84,
        inputKey_OK                        = 0x85,
        inputKey_Back                      = 0x86,
        inputKey_Return                    = 0x87,

/* api key */
    inputKey_API_NULL                  = 0x90,
    inputKey_API_SHORTCUTS             = 0x91, /* 保留功能 */
    inputKey_API_SWITCH_LT_LOWUP       = 0x92, /* 保留功能 */
    inputKey_API_RESET                 = 0x93, /* 保留功能 */
        inputKey_API_SYLLABLE				= 0x94, /* 选择音节 */
        inputKey_API_ASSOCIATE             = 0x95, /* 联想功能开关 */
        inputKey_API_AUTOCOMMIT            = 0x96, /* 自动上屏，暂时只适用于仓颉 */
        inputKey_API_CORRECT				= 0x97, /* 容错功能 */
        inputKey_API_LOOPASSOCIATE         = 0x98, /* 循环联想功能开关 */

    inputKey_Num

};

// 输入法接口
class CInputMethodIntf : public QObject
{
    Q_OBJECT

public:
    static CInputMethodIntf *instance();

    // 打开指定的输入法
    virtual int openInputMethon(int _input_method_code) = 0;
    // 重置输入法
    virtual int reset() = 0;
    // 关闭输入法
    virtual void close() = 0;
    // 传入一个键值
    virtual int inputKey(int _key) = 0;
    // 传入手写轨迹，并得到候选字词
    virtual int inputHandWrite(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate, const uint _range) = 0;
    // 查询已输入的字符
    virtual QString getInputs() = 0;
    // 获取候选字词列表
    virtual void getCandidates(QStringList& _candidate_list) = 0;
    // 选择候选字词，返回是否联想状态
    virtual bool selectCandidate(int _idx, QStringList *_candidate_list = 0) = 0;
    // 设置手写板尺寸
    virtual void setHandWriteSize(int _width, int _height) = 0;

    //
    static void UShortToWChar(const unsigned short *_src, wchar_t *_dst);
    static void WCharToUShort(const wchar_t *_src, unsigned short *_dst);

protected:
    explicit CInputMethodIntf(QObject *parent=0);
    ~CInputMethodIntf();

    static CInputMethodIntf *obj;

};

#ifdef USE_DWIME

// “deIme” 输入法库
class CInputMethodDwIme : public CInputMethodIntf
{
    Q_OBJECT
public:
    // 打开指定的输入法
    int openInputMethon(int _input_method_code) override;
    // 重置输入法
    int reset();
    // 关闭输入法
    virtual void close();
    // 传入一个键值
    int inputKey(int _key);
    // 传入手写轨迹，并得到候选字词
    int inputHandWrite(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate, uint _range);
    // 查询已输入的字符
    QString getInputs();
    // 获取候选字词列表
    void getCandidates(QStringList& _candidate_list);
    // 选择候选字词，返回是否联想状态
    bool selectCandidate(int _idx, QString _selected_word = "", QStringList *_new_candidates = 0);

protected:
    explicit CInputMethodDwIme(QObject *parent=0);
    ~CInputMethodDwIme();
    friend class CInputMethodIntf;

    int inputMethodCode = -1;       // 输入法种类，与 class VirtualKeyboard 里的定义相同：0-拼音，1-手写，2-笔画

};

#endif

#ifdef USE_GOOGLEPINYIN

// “开源输入法”库（googlepinyin + ?handwrite）
class CInputMethodOpenSrc : public CInputMethodIntf     // TODO: 把 OpenSrc 里的各种输入法拆开，包括 googlepinyin、zinnia、wagomu，支持独立配置是否编译
{
    Q_OBJECT
public:
    // 打开指定的输入法
    int openInputMethon(int _input_method_code) override;
    // 重置输入法
    int reset() override;
    // 关闭输入法
    void close() override;
    // 传入一个键值
    int inputKey(int _key) override;
    // 传入手写轨迹，并得到候选字词
    int inputHandWrite(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate, uint _range) override;
    // 查询已输入的字符
    QString getInputs() override;
    // 获取候选字词列表
    void getCandidates(QStringList& _candidate_list) override;
    // 选择候选字词，返回是否联想状态
    bool selectCandidate(int _idx, QStringList *_candidate_list = 0) override;
    // 设置手写板尺寸
    virtual void setHandWriteSize(int _width, int _height) override;

protected:
    explicit CInputMethodOpenSrc(QObject *parent=0);
    ~CInputMethodOpenSrc();
    friend class CInputMethodIntf;

    bool isOpened = false;
    QString inputBuff;
    int lastCandidateCount = 0;

    int boardWidth = 400;
    int boardHeight = 400;

    bool isValidChar(char _char);

#ifdef USE_ZINNIA
    int inputHandWrite_zinnia(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate);
#endif

};

#endif

#ifdef USE_DWHW

// “多文”手写引擎 API 的调用封装
class CDwHw
{
public:
    static void initDwHw();
    static void openDwHw();
    static void resetDwHw();
    static int inputHandWrite_DwHw(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate);
    static void closeDwHw();
protected:
    static int Linux_Set_appBinding();
};

#endif

#endif // CINPUTMETHODINTF_H
