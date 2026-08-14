#include "inputmethodintf.h"

#include <QApplication>
#include <QDebug>

//
CInputMethodIntf *CInputMethodIntf::obj = Q_NULLPTR;

CInputMethodIntf *CInputMethodIntf::instance()
{
    if (!obj) {
#ifdef USE_DWIME
        obj = new CInputMethodDwIme();
#else
        obj = new CInputMethodOpenSrc();
#endif
    }
    return obj;
}

CInputMethodIntf::CInputMethodIntf(QObject *parent) : QObject(parent)
{

}

CInputMethodIntf::~CInputMethodIntf()
{
    //close();
}

// (部分系统 wchar 是4字节的)
void CInputMethodIntf::UShortToWChar(const unsigned short *_src, wchar_t *_dst)
{
    while((*_dst++ = *_src++));
}
//
void CInputMethodIntf::WCharToUShort(const wchar_t *_src, unsigned short *_dst)
{
    while((*_dst++ = *_src++));
}

///=============================================================================================
///

#ifdef USE_DWIME

#include "dwIme/dwIme3rd.h"
#include "dwIme/WWHW.h"

CInputMethodDwIme::CInputMethodDwIme(QObject *parent) : CInputMethodIntf(parent)
{

}

CInputMethodDwIme::~CInputMethodDwIme()
{
    close();
}

int CInputMethodDwIme::openInputMethon(int _input_method_code)      // TODO: 改为枚举
{
    inputMethodCode = _input_method_code;

    int succ = -1;
    switch (inputMethodCode) {
    case 0:     // 拼音
        succ = DWIME3rd_OpenPY26();
        break;
    case 1:     // 手写
        // 手写引擎初始化
        succ = WWInitRecognition(NULL/*使用内置数据库*/, 0);

        SplImeProcessKey(SPKEY_API_ASSOCIATE, SPKT_Down, 1);    // 开启联想     // TODO:拼音输入法也要？

        break;
    case 2:     // 笔画
        succ = DWIME3rd_OpenSK();
        break;
    default:
        qWarning() << "_input_method_code error!";
        break;
    }

    return succ;
}

int CInputMethodDwIme::reset()
{
    return DWIME3rd_Reset();
}

void CInputMethodDwIme::close()
{
    WWExitRecognition();

}

int CInputMethodDwIme::inputKey(int _key)
{
    //if (1 != inputMethodCode)
    {
        return DWIME3rd_KeyPress((SplKey)_key);
    }
}

int CInputMethodDwIme::inputHandWrite(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate, uint _range)
{
    Q_UNUSED(_track_size)
    return WWRecognize(_track_data, _result, _max_candidate, _range);
}

// (部分系统 wchar 是4字节的)
static wchar_t *LDCharToWChar(wchar_t *dst, const LDChar *src)
{
    while((*dst++ = *src++));
    return dst;
}
//
static LDChar *WCharToLDChar(LDChar *dst, const wchar_t *src)
{
    while((*dst++ = *src++));
    return dst;
}

//
QString CInputMethodDwIme::getInputs()
{
    LDChar buffer[256];
    wchar_t trans[256];
    int cnt = 0;

    switch (inputMethodCode) {
    case 0:     // 拼音
        cnt = DWIME3rd_FormatPY(0, buffer, 256);
        if (cnt > 0) {
            LDCharToWChar(trans, buffer);
            return QString::fromWCharArray(trans);
        } else {
            return "";
        }
        break;
    case 1:     // 手写


        break;
    case 2:     // 笔画
        cnt = DWIME3rd_InputString(buffer, 256);
        if (cnt > 0)
        {
            LDCharToWChar(trans, buffer);
            return QString::fromWCharArray(trans);
        }
        break;
    default:
        break;
    }

    return "";
}

void CInputMethodDwIme::getCandidates(QStringList &_candidate_list)
{
    int count = DWIME3rd_ResultCount();  //查询当前候选结果的数量
    LDChar buffer[256];
    wchar_t cand[256];
    for(int i=0; i<count; i++)
    {
        DWIME3rd_ResultString(i, buffer, 256);  //从引擎里获取一个候选结果字符串
        LDCharToWChar(cand, buffer);
        QString sText = QString::fromWCharArray(cand);

        _candidate_list.append(sText);
    }
}

bool CInputMethodDwIme::selectCandidate(int _idx, QString _selected_word, QStringList *_new_candidates)
{
    DWIME3rd_SelectResult(_idx);

    if (1 == inputMethodCode) {
//        if (DWIME3rd_IsCanCommit())       // TODO: ???
//        {
//            // 输出
//            wchar_t out[256];
//            getCommitString(out);
//            ui->lineEdit_Input->setText(QString::fromWCharArray(out));
//            qDebug() << "Commit: " << QString::fromWCharArray(out) << endl;
//        }
    } else if (1 == inputMethodCode) {
        if (_selected_word.length() > 0) {
            wchar_t cand[256];
            LDChar buffer[256];
            _selected_word.toWCharArray(cand);
            WCharToLDChar(buffer,cand);
            DWIME3rd_HwAssociate(buffer);
        } else {
            qWarning() << "handwrite selected word should not be null!";
        }
    }

    bool is_associate = DWIME3rd_IsAsseMode();
    if (is_associate && !_new_candidates) {
        //
    }

    return is_associate;
}

#endif

#ifdef USE_GOOGLEPINYIN

#include <iostream>
#include <assert.h>

#include "googlepinyin/pinyinime.h"

#ifdef USE_ZINNIA
#  include "zinnia/zinnia.h"

//
zinnia::Recognizer *gRecognizer = Q_NULLPTR;
zinnia::Character *gCharacter = Q_NULLPTR;
#endif

#ifdef USE_WAGOMU
#  include "wagomu/wagomu.h"
#endif

//
CInputMethodOpenSrc::CInputMethodOpenSrc(QObject *parent) : CInputMethodIntf(parent)
{
#ifdef USE_ZINNIA
    gRecognizer = zinnia::Recognizer::create();
    QString model_file_path = qApp->applicationDirPath() + "/zinnia-model/handwriting-zh_CN.model";
    //QString model_file_path = qApp->applicationDirPath() + "/zinnia-model/handwriting-ja.model";
    if (!gRecognizer->open(model_file_path.toLocal8Bit().data())) {
        std::cerr << gRecognizer->what() << std::endl;
    }
    gCharacter = zinnia::Character::create();
#endif

#ifdef USE_DWHW
    CDwHw::initDwHw();
#endif
}

CInputMethodOpenSrc::~CInputMethodOpenSrc()
{
    close();

#ifdef USE_ZINNIA
    //
    delete gCharacter;
    gCharacter = nullptr;
    delete gRecognizer;
    gRecognizer = nullptr;
#endif

}

int CInputMethodOpenSrc::openInputMethon(int _input_method_code)
{
    Q_UNUSED(_input_method_code)

    if (isOpened) {
        qWarning() << "googlepinyin is opened!";
        reset();
        return 0;
    }

    QString app_dir(qApp->applicationDirPath() + "/googlepinyin-dict");
    bool succ = ime_pinyin::im_open_decoder(QString("%1/dict_pinyin.dat").arg(app_dir).toLocal8Bit().data(),
                                            QString("%1/dict_pinyin_user.dat").arg(app_dir).toLocal8Bit().data());
    if (!succ) {
        qCritical() << "Failed to open dictionary of googlepinyin!";
    }

#ifdef USE_DWHW
    CDwHw::openDwHw();
#endif

    //
    isOpened = true;
    reset();

    return 0;
}

int CInputMethodOpenSrc::reset()
{
    ime_pinyin::im_reset_search();

#ifdef USE_DWHW
    CDwHw::resetDwHw();
#endif

    inputBuff.clear();
    lastCandidateCount = 0;

    return 0;
}

void CInputMethodOpenSrc::close()
{
    ime_pinyin::im_close_decoder();

#ifdef USE_DWHW
    CDwHw::closeDwHw();
#endif

    //
    isOpened = false;
}

int CInputMethodOpenSrc::inputKey(int _key)
{
    char c = (char) _key;
    //bool is_add = (inputBuff.length() > 0);

    if (isValidChar(c)) {
        inputBuff += c;
    } else if (inputKey_Back == _key) {
        //is_add = false;
        inputBuff.remove(inputBuff.length() - 1, 1);
    } else {
        return lastCandidateCount;
    }

    //if (is_add)
    //{
    //    lastCandidateCount = (int)ime_pinyin::im_add_letter(c);     // TODO: why failed?
    //} else
    {
        lastCandidateCount = (int)ime_pinyin::im_search(inputBuff.toUtf8().data(), static_cast<size_t>(inputBuff.size()));
    }

    return lastCandidateCount;
}

#ifdef USE_ZINNIA
int CInputMethodOpenSrc::inputHandWrite_zinnia(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate)
{
    /* 笔划结束点：   (-1,0)
     * 加入笔迹结束点：(-1,-1));
     */

    //
    gCharacter->clear();

    int id = 0;
    int x, y;
    for (int i = 0; i < _track_size; i++) {
        x = _track_data[i * 2];
        y = _track_data[i * 2 + 1];

        if (x > 0 && y > 0) {
            gCharacter->add(id, x, y);
        } else {
            if (0 == i) {
                // TODO: ？
            }
        }

        if (-1 == x && 0 == y) {
            id++;
        }
    }

    zinnia::Result *result = gRecognizer->classify(*gCharacter, _max_candidate);
    if (!result) {
        std::cerr << gRecognizer->what() << std::endl;
        return -1;
    }
    QString candidate_str;
    int result_len = result->size();
    for (size_t i = 0; i < result->size(); ++i) {
        //std::cout << result->value(i) << "\t" << result->score(i) << std::endl;

        candidate_str += QString::fromUtf8(result->value(i));
    }
    //strcpy((char *)_result, (char *)candidate_str.utf16());
    int len2 = candidate_str.toWCharArray((wchar_t *)_result);
    assert(len2 == result_len);
    delete result;

    //
    return result_len;
}
#endif

int CInputMethodOpenSrc::inputHandWrite(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate, uint _range)
{
    Q_UNUSED(_range)

#ifdef USE_ZINNIA
    return inputHandWrite_zinnia(_track_data, _track_size, _result, _max_candidate);
#endif

#ifdef USE_DWHW
    return CDwHw::inputHandWrite_DwHw(_track_data, _track_size, _result, _max_candidate);
#endif

    return 0;
}

QString CInputMethodOpenSrc::getInputs()
{
    return inputBuff;
}

void CInputMethodOpenSrc::getCandidates(QStringList &_candidate_list)
{
    if (lastCandidateCount > 0) {
        ime_pinyin::char16 buf[256] = {0};
        for (int i = 0; i < lastCandidateCount; i++) {
            ime_pinyin::im_get_candidate(i, buf, 255);
            _candidate_list.append(QString::fromUtf16(buf));
        }
    }
}

bool CInputMethodOpenSrc::selectCandidate(int _idx, QStringList *_candidate_list)
{
    bool is_associate = false;      // TODO: GooglePinyin 好像没有联想？这里返回的是后面没有 fixed 的输入所对应的候选字词？

    //
    lastCandidateCount = ime_pinyin::im_choose((size_t)_idx);
    if (lastCandidateCount > 1) {
        //is_associate = true;

        Q_UNUSED(_candidate_list)
        //if (_candidate_list) {
        //    getCandidates(*_candidate_list);
        //}
    }

    //
    if (!is_associate) {
        reset();
    }

    //
    return is_associate;
}

void CInputMethodOpenSrc::setHandWriteSize(int _width, int _height)
{
    boardWidth = _width;
    boardHeight = _height;

#ifdef USE_ZINNIA
    //
    gCharacter->set_width(boardWidth);
    gCharacter->set_height(boardHeight);
#endif

}

bool CInputMethodOpenSrc::isValidChar(char _char)
{
    return isalnum(_char) || ('\'' == _char);
}

///=============================================================================
/// class CDwHw
///

#ifdef USE_DWHW

#include "DWIMECore_Dll.h"

#include <sys/time.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "DWIMECore/DWIMECore_Def.h"

// MAPKEY 、SECRETKEY， 由我们提供，保密内容不得泄露
//
int CDwHw::Linux_Set_appBinding()
{
    const char * MAPKEY = "rN2Xswu_yTupZdhNV_kIlui9";

    const unsigned short SECRETKEY[] = {0x0063, 0x006F, 0x006D, 0x002E, 0x0064, 0x0077, 0x0069,
                                        0x006D, 0x0065, 0x0063, 0x006F, 0x0072, 0x0065, 0x002E,
                                        0x006D, 0x0061, 0x006E, 0x0079, 0x006C, 0x0069, 0x006E,
                                        0x006B, 0x005F, 0x0069, 0x006D, 0x0065, 0};

    // 此函数代码不能修改
    const int mapSize = 128;

    int ret = -2;

    int fd = shm_open(MAPKEY, O_CREAT|O_RDWR, 0666);
    if (fd >= 0)
    {
        if (ftruncate(fd, mapSize) == 0)        // 申请空间大小
        {
            char * data = (char*)mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if (data != NULL)
            {
                memset(data, 0, mapSize);
                memcpy(data, SECRETKEY, sizeof(SECRETKEY));
                ret = DWIMECore_appBinding(NULL /*必须是空*/);
                munmap(data, mapSize);
            }
        }

        close(fd);
        shm_unlink(MAPKEY);
    }

    return ret;
}

//
void CDwHw::initDwHw()
{
    // 授权
    if (Linux_Set_appBinding() != 0)
    {
        const wchar_t Title[] = {0x63D0, 0x793A, 0};   // “提示”
        const wchar_t Content[] ={0x6388, 0x6743, 0x5931, 0x8D25, 0xFF01, 0};   // “授权失败！”

        qDebug() << "CDwHw::initDwHw() err: " << QString::fromWCharArray(Title) << " : " << QString::fromWCharArray(Content);
    } else {
        qDebug() << "CDwHw::initDwHw() authorization succeeded.";
    }
}

void CDwHw::openDwHw()
{
    DWIMECore_dataClear();
    DWIMECore_hwDeinit();

    // 加载实用版的数据
    qDebug() << "DwHw:: EngineVersion is write: " << (DWIMECore_getEngineVersion() == VERSION_PRACTICAL);

    QString data_path = qApp->applicationDirPath() + "/dwhw_practical_data/hw.data";
    qDebug() << "CDwHw::openDwHw(), data_path = " << data_path;

    DWError err = DWIMECore_dataMap(DWIME_DATA_HWDB, (unsigned short*)(data_path.toStdU16String().c_str()));
    if (DWRT_OK != err) {
        qDebug() << "CDwHw::openDwHw(), dataMap failed: err = " << err;
    }

    err = DWIMECore_hwInit(0);

    // 手写初始化，并设置识别范围
    if (DWRT_OK != err) {
        const wchar_t Title[] = {0x63D0, 0x793A, 0};   // “提示”
        const wchar_t Content[] ={0x624B, 0x5199, 0x521D, 0x59CB, 0x5316, 0x5931, 0x8D25, 0xFF01, 0};   // “手写初始化失败！”
        qDebug() << "CDwHw::openDwHw() err: " << QString::fromWCharArray(Title) << " : " << QString::fromWCharArray(Content);
        qDebug() << "Error = " << err;
    } else {
        qDebug() << "CDwHw::openDwHw(): open succeeded.";
    }

    DWIMECore_hwSetOption(DWHW_OPTION_NUMBER, 1);
    DWIMECore_hwSetOption(DWHW_OPTION_LOWER, 1);
    DWIMECore_hwSetOption(DWHW_OPTION_UPPER, 1);
    DWIMECore_hwSetOption(DWHW_OPTION_GB2312, 1);
}

void CDwHw::resetDwHw()
{
    DWIMECore_hwReset();
    qDebug() << "CDwHw::resetDwHw() called.";
}

int CDwHw::inputHandWrite_DwHw(const short *_track_data, const int _track_size, unsigned short *_result, const int _max_candidate)
{
    DWIMECore_hwSetOption(DWHW_OPTION_RESULT, _max_candidate);
    DWIMECore_hwRecognize(_track_data, _track_size);

    int count = DWIMECore_getCandCount();
    int i = 0;
    for(; i < count && i < _max_candidate; i++)
    {
        DWIMECore_getCandString(i, _result + i);
    }
    *(_result + i) = 0;

    return count;
}

void CDwHw::closeDwHw()
{
    DWIMECore_hwDeinit();
    qDebug() << "CDwHw::close() called.";
}

#endif

#endif
