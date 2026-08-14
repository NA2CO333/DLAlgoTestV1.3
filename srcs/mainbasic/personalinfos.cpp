//未测编辑界面
#include "personalinfos.h"
#include "ui_personalinfos.h"

#include <QDebug>
#include <QString>
#include <QPixmap>
#include <QPainter>

#include "mysqlitepatients.h"
#include "windowsmanager.h"
#include "global.h"
#include "mainwindow.h"
#include "appsetting.h"
#include "util-app.h"
#include "utilui.h"
#include "data-intf-huayi.h"

using namespace DataTrans;

//
QString sexstring;
static QString lastPatSex = "M";

//
PersonalInfos::PersonalInfos(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::PersonalInfos)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    //
    //if(modeFlag_FromBarcode == m_modeFlag){
    //    MessageWin msg;
    //    msg.setContent("FromBarcode!");
    //    msg.show();
    //}
    //QObject::connect(this, SIGNAL(sigQueryPatientInfo(Client)), g_uploadThread, SLOT(slotQueryPatientInfo(Client)));

    //iniSetting = new QSettings("manylinks",QSettings::IniFormat);
    ui->pushButton_Home->setFlat(true);     //融入背景
    m_mysql = MySQLitePatients::getInstance();

    QObject::connect(this, &PersonalInfos::sigQueryPatientInfo, g_uploadThread, &UpLoadThread::slotQueryPatientInfo, Qt::QueuedConnection);
    QObject::connect(g_uploadThread, &UpLoadThread::requestClientInfoFeedback, this, &PersonalInfos::slot_uploadThread_ReceivedPatientInfo, Qt::QueuedConnection);
    QObject::connect(CDataIntfHuaYi::instance(), &CDataIntfHuaYi::sigReceivedPatientInfo, this, &PersonalInfos::slot_dataIntfHuaYi_ReceivedPatientInfo, Qt::QueuedConnection);

    ui->pushButton_back->setStyleSheet("border-radius:5px;padding:2px 4px;");
    ui->pushButton_test->setStyleSheet("border-radius:5px;padding:2px 4px;");

    //
    ui->rbtnSexNull->setVisible(false);
    ui->calendarBirthDate->setVisible(false);

    // 必填字段加上星号符
    Util::Ui::addAsteriskToSideOfWidget(ui->lblNum, false);
    Util::Ui::addAsteriskToSideOfWidget(ui->lblName, false);
    Util::Ui::addAsteriskToSideOfWidget(ui->lblSex, false);
    Util::Ui::addAsteriskToSideOfWidget(ui->lblBirthDate, false);

}

PersonalInfos::~PersonalInfos()
{
    delete ui;
    qDebug() << "PersonalInfos::~PersonalInfos()";
}

void PersonalInfos::showEvent(QShowEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 根据工作模式设置功能按钮可见性
    setButtonsEnabledByMode();

    // 更新语言
    //updateLanguage();

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 更新样式
    updateStyleSheet();

    // 若是扫码模式，将条码内容转到【业务实体对象】
    bool is_continue_show = true;               // 是否需要继续后续显示流程
    if (modeFlag_FromBarcode == m_modeFlag) {
        // 二维码内容解析（万灵通信协议）
        QString code_decoded;
        QString err_msg;
        enQrCodeType code_type = barcodeDataToEntity(m_barcodeData, code_decoded, m_patient, err_msg);
        bool is_code_valid = (code_type != qrCodeType_Unknown);
        PersonalInfos::m_isBarcodeValid = is_code_valid;
        if (is_code_valid) {
            is_continue_show = doAfterGetEntityFromBarcode(m_patient, code_type);
            if (!is_continue_show) {
                //m_patient.isBatch = true;     // TODO: 扫码时一律是批量筛查是否合理？
                // NOTE: 注意：云端的二维码，不能固定为批量筛查码，因为门诊的和学校的都有。见 DataTrans::batchClient2Patient()
            }
        } else {
            getWinManage()->showMsgWin(err_msg);
            is_continue_show = false;
        }
    }

    // 本窗体的实体对象的值确定后的处理
    if (is_continue_show) {
        doAfterGetEntity();
    }

    // 若是来自扫码，则支持继续扫码
    if (modeFlag_FromBarcode == m_modeFlag) {
        // 注册键盘侦听（用于扫码）
        globalService()->regKbReader(this);
    }

}

void PersonalInfos::doAfterGetEntity()
{
    // 将【业务实体对象】的数据显示到界面
    entityToUi(m_patient);

    // 标题刷新
    getWinManage()->updateWindowTitle(this, tr("被测者信息"));   // "TesteeInformation"

    // 根据是否批量筛查来设置编辑项的标题
    ui->lblClass->setText((m_patient.isBatch ? tr("班级：") : tr("籍贯：")));        // "Class: "  "NativePlace: "
    ui->lblWeChat->setText((m_patient.isBatch ? tr("微信号：") : tr("民族：")));     // "WeChat: "  "Nation: "

    /* 可见性、可编辑性等的设置 */

    // 若编号是从外部系统传入的，不允许编辑           // TODO: 这里应该是一律不允许修改编号？
    bool num_editable = !(false
                          || m_patient.isBatch                        // 批量名单的编号不允许编辑
                          || modeFlag_FromCommand == m_modeFlag     // 来自指令的编号不允许编辑
                          || modeFlag_FromBarcode == m_modeFlag     // 来自扫码的编号不允许编辑
                          );
    ui->edtNum->setReadOnly(!num_editable);
    ui->btnGenNum->setVisible(num_editable);

    // 若是筛查档案，不显示“微信号”
    ui->lblWeChat->setVisible(!m_patient.isBatch);
    ui->edtWeChat->setVisible(!m_patient.isBatch);

}

void PersonalInfos::hideEvent(QHideEvent *)
{
    // 如是是键盘的显示再隐藏期间，则跳过当前业务窗体的显示和隐藏事件      /* 注意：如果跳过了显示事件，也应当同时跳过隐藏事件，否则可能出现逻辑错误 */
    if (getWinManage()->getIsShowingKeyboard()) {
        return;
    }

    // 若是来自扫码，则取消扫码支持
    if (modeFlag_FromBarcode == m_modeFlag) {
        // 反注册键盘侦听（用于扫码）
        globalService()->unregKbReader(this);
    }

    // 扫码状态值复位
    m_barcodeData = "";

}

void PersonalInfos::updateStyleSheet()
{
    //
    QPalette palette;
    if(themeType_Black == getSysThemeType())
    {
        palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        ui->rbtnSexM->setStyleSheet("\
QRadioButton{color:rgb(204,204,204);}\
QRadioButton::indicator:unchecked {image:url(:/resource/gray.png);width:25px;height:25px;border-radius:10px;}\
QRadioButton::indicator:checked {image:url(:/resource/green.png);width:25px;height:25px;border-radius:10px;}"
                                          );
        ui->rbtnSexF->setStyleSheet("\
QRadioButton{color:rgb(204,204,204);} \
QRadioButton::indicator:unchecked {image:url(:/resource/gray.png);width:25px;height:25px;border-radius:10px;}\
QRadioButton::indicator:checked {image:url(:/resource/green.png);width:25px;height:25px;border-radius:10px;}"
                                            );

        QList<QLabel *> list_Label = findChildren<QLabel *>();          // TODO: 这样设置会覆盖被非窗体但是本窗体的子窗体的控件的样式！
        foreach(QLabel *p, list_Label){
            if (p->objectName().startsWith("label_") || p->objectName().startsWith("lbl")) {
                p->setStyleSheet("color:rgb(204,204,204);");
            }
        }

        QList<QLineEdit *> list_LineEdit = findChildren<QLineEdit *>();
        foreach(QLineEdit *p, list_LineEdit){
            p->setStyleSheet("QLineEdit{border-radius:5px; background-color:rgb(28,28,30); color:rgb(204,204,204);}");
        }

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_test->setIcon(QIcon(":/resource/black_theme/retest_b.png"));
        ui->pushButton_back->setIcon(QIcon(":/resource/black_theme/back_b.png"));

        ui->btnGenNum->setStyleSheet("QPushButton { background-color: rgb(51,56,62); color: rgb(204,204,204); border-radius: 5px; } ");
    }
    else
    {
        palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->rbtnSexM->setStyleSheet("QRadioButton{color:rgb(1,1,1);}\
                                           QRadioButton::indicator:unchecked {image:url(:/resource/gray.png);width:25px;height:25px;border-radius:10px;}\
                                           QRadioButton::indicator:checked {image:url(:/resource/green.png);width:25px;height:25px;border-radius:10px;}");
        ui->rbtnSexF->setStyleSheet("QRadioButton{color:rgb(1,1,1);}\
                                           QRadioButton::indicator:unchecked {image:url(:/resource/gray.png);width:25px;height:25px;border-radius:10px;}\
                                           QRadioButton::indicator:checked {image:url(:/resource/green.png);width:25px;height:25px;border-radius:10px;}");

        QList<QLabel *> list_Label = findChildren<QLabel *>();
        foreach(QLabel *p, list_Label){
            p->setStyleSheet("color:rgb(1,1,1);");
        }
        QList<QLineEdit *> list_LineEdit = findChildren<QLineEdit *>();
        foreach(QLineEdit *p, list_LineEdit){
            p->setStyleSheet("QLineEdit{border-radius:5px; background-color:rgb(227,227,232); color:rgb(1,1,1);}");
        }

        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_test->setIcon(QIcon(":/resource/white_theme/retest_w.png"));
        ui->pushButton_back->setIcon(QIcon(":/resource/white_theme/back_w.png"));

        ui->btnGenNum->setStyleSheet("");
    }
    this->setAutoFillBackground(true);
    this->setPalette(palette);
}

void PersonalInfos::updateLanguage(bool language)
{
    //
    if (language) {
        getWinManage()->updateWindowTitle(this, "被测者信息");

        ui->label_Home->setText("主页");
        ui->label_Save->setText("保存");
        ui->label_Remeasure->setText("测量");
        ui->label_Back->setText("返回");

        ui->lblNum->setText("编号：");
        ui->lblName->setText("姓名：");
        ui->lblSex->setText("性别：");
        ui->rbtnSexM->setText("男");
        ui->rbtnSexF->setText("女");
        ui->rbtnSexNull->setText("无");
        ui->lblBirthDate->setText("出生日期：");
        ui->label_year->setText("年");
        ui->label_month->setText("月");
        ui->label_date->setText("日");
        ui->lblPhone->setText("手机号：");
        ui->lblAddress->setText("地址：");

        ui->lblClass->setText((m_patient.isBatch ? "班级：" : "籍贯："));
        ui->lblWeChat->setText((m_patient.isBatch ? "微信号：" : "民族："));
    } else {
        getWinManage()->updateWindowTitle(this, "TesteeInformation");

        ui->label_Home->setText("Home");
        ui->label_Save->setText("Save");
        ui->label_Remeasure->setText("Measure");
        ui->label_Back->setText("Back");

        ui->lblNum->setText("Number: ");
        ui->lblName->setText("Name: ");
        ui->lblSex->setText("Sex: ");
        ui->rbtnSexM->setText("Male");
        ui->rbtnSexF->setText("Female");
        ui->rbtnSexNull->setText("None");
        ui->lblBirthDate->setText("BirthDate: ");
        ui->label_year->setText("Y");
        ui->label_month->setText("M");
        ui->label_date->setText("D");
        ui->lblPhone->setText("Phone: ");
        ui->lblAddress->setText("Address: ");

        ui->lblClass->setText((m_patient.isBatch ? "Class: " : "NativePlace: "));
        ui->lblWeChat->setText((m_patient.isBatch ? "WeChat: " : "Nation: "));
    }
}

void PersonalInfos::setButtonsEnabledByMode()
{
    //
    bool is_home_enabled = true;        // “主页”按钮
    bool is_save_enabled = true;        // “保存”按钮
    bool is_measure_enabled = true;     // “测量”按钮
    bool is_back_enabled = true;        // “返回”按钮
    m_savingLevelSelf = savingLevel_No;

    //
    switch (m_modeFlag) {
    case modeFlag_ViewOnly:             // “查看”模式：      “主页”、“保存”、“测量”按钮不可见
        is_home_enabled = false;
        is_save_enabled = false;
        is_measure_enabled = false;

        break;
    case modeFlag_New:                  // “新增”模式：      “主页”按钮不可见
        is_home_enabled = false;
        m_savingLevelSelf = savingLevel_Database;

        break;
    case modeFlag_EditToEntity:         // “编辑到本窗体的实体对象”模式：   只有“返回”按钮可见
        is_home_enabled = false;
        is_save_enabled = false;
        is_measure_enabled = false;
        m_savingLevelSelf = savingLevel_Entity;

        break;
    case modeFlag_EditAndSave:          // “编辑和保存”模式：   “主页”、“测量”按钮不可见
        is_home_enabled = false;
        is_measure_enabled = false;
        m_savingLevelSelf = savingLevel_Database;

        break;
    case modeFlag_EditAndTest:          // “编辑和测量”模式：   “主页”按钮不可见
        is_home_enabled = false;
        m_savingLevelSelf = savingLevel_Entity;

        break;
    case modeFlag_FromBarcode:          // “二维码”模式：     “保存”按钮不可见
        is_save_enabled = false;
        m_savingLevelSelf = savingLevel_Entity;

        break;
    case modeFlag_FromCommand:          // “外部指令”模式：    “保存”按钮不可见
        is_save_enabled = false;
        m_savingLevelSelf = savingLevel_Entity;

        break;
    default:

        break;
    }

    //
    ui->wgtHome->setVisible(is_home_enabled);
    ui->wgtSave->setVisible(is_save_enabled);
    ui->wgtMeasure->setVisible(is_measure_enabled);
    ui->wgtBack->setVisible(is_back_enabled);

}

enQrCodeType PersonalInfos::barcodeDataToEntity(QString _code_raw, QString &_code_decoded, CPatient &_pat, QString &_err_msg)
{
    enQrCodeType barcode_type = qrCodeType_Unknown;

    //
    _pat.reset();

    //
    _code_raw.replace("\r", "");
    _code_raw.replace("\n", "");

    // 条码内容反百分号编码，并按 UTF-8 编码转 QString
    QByteArray barcode_bytes = QByteArray::fromPercentEncoding(_code_raw.toLatin1());

    _code_decoded = QString::fromUtf8(barcode_bytes);

    // 检查是否有乱码          // TODO: 这种判断方法是否可靠？改用 Util::isChineseTextGarbled() ？
    bool is_garbeld = false;
    QByteArray bytes = _code_decoded.toUtf8();
    if (bytes.length() != barcode_bytes.length()) {
        QString err_msg = tr("扫码得到的内容包含乱码！\n请重新扫码。");   // "The content obtained by scanning contains garbled characters!\nPlease scan the barcode again. "
        getWinManage()->showMsgWin(err_msg);
        is_garbeld = true;
    }

    // 若有乱码，不管反序列化结果如何，都判定为条码非法
    if (is_garbeld) {
        return barcode_type;
    }

    // TODO: 从二维码内容到数据对象的过程，应放到上一层架构？

    // 若是 CSV 格式，且是万灵接口，得到最后的长度字段的值，并去掉最后的长度字段，使之与通用协议兼容
    /* 万灵云端的二维码字段（CSV）：
     * 门诊: 个人档案id,名称,性别,出生日期,班级,电话,微信,地址,身份证,学号,批次号,门诊号,字符串长度（最后一个字段之前的文字个数，不含最后一个逗号）
     * 学校: 个人档案id,名称,性别,出生日期,班级,电话,微信,地址,身份证,学号,批次号,字符串长度
     */
    // NOTE: 门诊和学校二维码的区分：门诊号是否为空，见 DataTrans::batchClient2Patient()
    // TODO: 待优化为《万灵门诊、学校筛查系统二维码（CSV格式）字段定义_20231114.docx》？
    int len = -1;
    //if (WinDataTrans::isManylinksDataIntf())     /* 先一律去掉，因为目前（202311）的协议只有万灵云端定义了批次号之后的字段，避免因未配置为万灵端口但实际上使用了万灵二维码而出问题。 */
    {
        int count_comma = _code_decoded.count(',');
        if (!_code_decoded.startsWith("{") && count_comma > 10) {
            int idx = _code_decoded.lastIndexOf(",");
            if (idx >= 0) {
                len = _code_decoded.mid(idx + 1).toInt();
                qDebug() << "csv len = " << len;
                // TODO: 校验 len

                _code_decoded.truncate(idx);
            }
        }
    }

    // 从条码内容获得【业务实体对象】的值
    if (_code_decoded.length() > 0) {     // 有条码内容，则数据应来自条码内容
        string err_str;
        ClientOfBatch batch_client;
        barcode_type = batch_client.FromBarcode(_code_decoded.toStdString(), err_str);
        bool is_succ_barcode = (barcode_type >= qrCodeType_JSON && barcode_type <= qrCodeType_Number);
        if (is_succ_barcode) {
            //
            QString num = QString::fromStdString(batch_client.Num);
            qDebug() << "get Num of barcode:" << num;

            if (num.length() > 0) {
                // 检查必有字段
                // TODO: ？


                //
                DataTrans::batchClient2Patient(batch_client, _pat);

                //
                _pat.barcodeData = _code_decoded;
            } else {
                _err_msg = tr("条码内容中的编号为空！");  // "Num in barcode content is empty!"

                barcode_type = qrCodeType_Unknown;
            }
        } else {
            _err_msg = tr("解析条码失败！") + QString("\n") + QString::fromStdString(err_str);    // "Parsing barcode failed!"
        }
    } else {
        _err_msg = tr("条码内容为空！");  // "The content of barcode is empty!"
    }

    //
    return barcode_type;
}

bool PersonalInfos::doAfterGetEntityFromBarcode(CPatient &_pat, const enQrCodeType _code_type)
{
    // NOTE: (2026-07-11)WindowsManagers::doOn_QrCode_ReceivedCode() 已确保 _code_type 只会是 qrCodeType_Number

    //
    if (qrCodeType_Number == _code_type) {
        // 若设置了“查询被测者信息”，则查询
        bool is_auto_get_info = appSetting::value("/data/autoGetInfo").toBool();
        if (is_auto_get_info) {
            // 查询
            emit sigQueryPatientInfo(_pat.patientid);
            showWaitForRequestWin();
            //return false;        // NOTE: 这里应返回 true，因为后面的 UI 刷新还是应执行
        } else {
            // 得到扫码后的受检者对象后的处理
            CPatient pat;
            pat.reset();
            pat.patientid = _pat.patientid;
            globalService()->doOn_QrCode_ReceivedPatient(pat, true, this);
            //return false;
        }
    }

    //
    //if (_code_type != qrCodeType_Unknown) {
    //    //
    //    QString num = (qrCodeType_Number == _code_type ? _code_decoded : _pat.patientid);
    //
    //    // 查询编号在本机数据库是否存在
    //    vector<CPatient> mypats = MySQLitePatients::getInstance()->findRecordByPatientid(num);
    //    bool exists = (mypats.size() > 0);
    //    if (exists) {       // 若编号已存在
    //        bool is_replace = false;
    //        if (_code_type != qrCodeType_Number) {    // 若条码含完整信息，则询问是否覆盖
    //            QString msg_sub = (mypats.at(0).isTest ? tr("，且已测量") : ""); // ", and has been measured"
    //            QString msg = tr("编号已存在%1！是否覆盖？").arg(msg_sub); // "Number already exists%1! Overwrite it?"
    //            is_replace = getWinManage()->showNoticeWin(msg);
    //            if (is_replace) {               // 若选择覆盖，则显示条码中的信息，并记下已有记录的 id
    //                _pat.id = mypats[0].id;     // 记下已有记录 id，使之后的保存替换已有记录
    //            } else {                        // 若选择不覆盖，则显示数据库的信息
    //                _pat.cloneFrom(mypats[0]);
    //            }
    //        } else {                    // 若条码仅含编号，则显示数据库的数据
    //            _pat.cloneFrom(mypats[0]);
    //        }
    //
    //        // 若已测量，且未覆盖，则转到结果页
    //        if (_pat.isTest && !is_replace) {
    //            QTimer::singleShot(200, this, [_pat]() {
    //                if (_pat.isTest) {
    //                    globalService()->getResultWin()->setPatient(_pat);
    //                    globalService()->getResultWin()->setIsNeedSave(false);
    //                    getWinManage()->showWindowByType(WIN_RESULT);
    //                }
    //            });
    //            return false;
    //        }
    //    } else {            // 若编号不存在
    //        if (_code_type == qrCodeType_Number) {    // 若条码仅含编号
    //            // 若设置了查询被测者信息，则查询
    //            bool is_auto_get_info = appSetting::value("/data/autoGetInfo").toBool();
    //            if (is_auto_get_info) {
    //                emit sigQueryPatientInfo(_pat.patientid);
    //                showWaitForRequestWin();
    //                return true;        // NOTE: 这里应返回 true，因为后面的 UI 刷新还是应执行
    //            }
    //        }
    //    }
    //}
    /* NOTE: (2026-07-11)
     * 1、去掉“扫码后，若是编号已存在且已测试，则跳转到结果页面”的流程（扫码的需求一般是启动测量）。
     * 2、查重的过程移到 WindowsManagers::doOn_QrCode_ReceivedPatient()。
     */

    //
    return true;
}

// 将数据从内存复制到界面
void PersonalInfos::entityToUi(const CPatient &_pat)
{
    // 编号
    ui->edtNum->setText(_pat.patientid);

    // 姓名
    ui->edtName->setText(_pat.patientname);

    // 性别
    if ("M" == _pat.patientsex) {
        ui->rbtnSexM->setChecked(true);
    } else if ("F" == _pat.patientsex) {
        ui->rbtnSexF->setChecked(true);
    } else {
        ui->rbtnSexNull->setChecked(true);
    }

    // 籍贯
    ui->edtClass->setText(_pat.patientstuclass);

    // 民族
    ui->edtWeChat->setText(_pat.patientWechat);

    // 出生日期（含年龄段）
    QString year_str, month_str, day_str;

    QDate birth_date = _pat.getBirthDate();
    if (!birth_date.isValid()) {
        // 若没有出生日期，而有年龄段，则根据年龄段虚构一个出生日期
        enAgeRange age_range = _pat.getAgeRange();
        if (age_range >= ageRange_Min && age_range <= ageRange_Max) {
            QDate date = CAgeRange::getBirthDateByAgeRange(age_range);
            if (date.isValid()) {
                birth_date = date;

                //
                QString msg = tr("出生日期为空，但年龄段为 %1。\n已自动选择了该年龄段内的一个日期为生日。"); // "BirthDate is empty, but AgeRange is %1. \nA date within this age group has been automatically selected as BirthDay."
                getWinManage()->showSuspensionPrompt(msg.arg(CAgeRange::getAgeRangeDesc(age_range)), 5000, this);
            }
        }
    }

    if (birth_date.isValid()) {
        year_str    = QString::number(birth_date.year(), 10);
        month_str   = QString::number(birth_date.month(), 10).rightJustified(2, '0');
        day_str     = QString::number(birth_date.day(), 10).rightJustified(2, '0');
    }

    ui->edtYear->setText(year_str);
    ui->edtMonth->setText(month_str);
    ui->edtDay->setText(day_str);

    // 电话
    ui->edtPhone->setText(_pat.patientPhone);

    // 地址
    ui->edtAddress->setText(_pat.patientAddress);

    //
    this->update();
}

void PersonalInfos::uiToEntity(CPatient &_pat)
{
    // 编号
    _pat.patientid = ui->edtNum->text();

    // 姓名
    _pat.patientname = ui->edtName->text();

    // 性别
    QString sex;
    if(ui->rbtnSexM->isChecked()) sex = "M";
    else if(ui->rbtnSexF->isChecked())   sex = "F";
    else    sex = "";

    _pat.patientsex = sex;

    // 籍贯
    _pat.patientstuclass = ui->edtClass->text();

    // 民族
    _pat.patientWechat = ui->edtWeChat->text();

    // 出生日期
    QString year_str = ui->edtYear->text();
    QString month_str = ui->edtMonth->text();
    QString day_str = ui->edtDay->text();

    QDate birth_date;
    bool succ_date = Util::strsToDate(year_str, month_str, day_str, birth_date);
    if (succ_date) {
        _pat.setBirthDate(birth_date);
    } else {
        _pat.setBirthDateStr("");
    }

    // 年龄段
    //if (succ_date) {
    //    enAgeRange age_range = CAgeRange::getAgeRangeFromBirthdate(birth_date);
    //    _pat.setAgeRange(age_range);
    //}

    // 电话
    _pat.patientPhone = ui->edtPhone->text();

    // 地址
    _pat.patientAddress = ui->edtAddress->text();

}

void PersonalInfos::showWaitForRequestWin()
{
    m_msgWin.setContent(tr("正在查询被测者信息..."));  // "Requesting info..."
    //m_msgWin.setButtonText(tr("手动选择"));   // "Manual select"
    m_msgWin.setButtonEnable(false);
    m_msgWin.show();      // NOTE: 这里不能用 exec()，否则阻塞当前过程可能导致逻辑错乱？
}

void PersonalInfos::hideWaitForRequestWin()
{
    m_msgWin.reject();
    m_msgWin.hide();
}

void PersonalInfos::slot_uploadThread_ReceivedPatientInfo(bool _is_succ, DataTrans::Client _client, QString _err_msg)
{
    //
    if (!m_msgWin.isVisible()) {
        return;
    }

    hideWaitForRequestWin();

    //
    if (_is_succ) {
        // 实体对象拷贝
        DataTrans::client2Patient(_client, m_patient);
        m_patient.isBatch = true;

        // 本窗体的实体对象的值确定后的处理
        //doAfterGetEntity();

        // 得到扫码后的受检者对象后的处理
        globalService()->doOn_QrCode_ReceivedPatient(m_patient, false, this);
    } else {
        QString text = tr("查询被测者信息失败：") + "\n" + _err_msg;    // "Request info failed:"
        getWinManage()->showMsgWin(text);
    }
}

void PersonalInfos::slot_dataIntfHuaYi_ReceivedPatientInfo(bool _is_succ, QString _err_msg, QDate _birthday, QString _business, QString _name, QString _pid, int _age)
{
    //
    if (!m_msgWin.isVisible()) {
        return;
    }

    hideWaitForRequestWin();

    //
    if (_is_succ) {
        // 实体对象拷贝
        CPatient pat {};
        pat.reset();

        if (_birthday.isValid()) {
            pat.setBirthDate(_birthday);
        } else {
            logWarning(QString("%1::%2(): 'birthdate' is invalid?! using age instead!").arg(S_CLASS_NAME).arg(__FUNCTION__));

            enAgeRange age_range = CAgeRange::fromAge(_age);
            if (age_range >= ageRange_Min && age_range <= ageRange_Max) {
                pat.setAgeRange(age_range);
            } else {
                logCritical(QString("%1::%2(): `age_range` from age is invalid?!").arg(S_CLASS_NAME).arg(__FUNCTION__));
                QString err_msg = tr("数据错误：生日和年龄都不合法！"); // "Data error: Birthday and age are both illegal!"
                getWinManage()->showMsgWin(err_msg);
                return;
            }
        }

        pat.patientname     = _name;
        pat.patientid       = _pid;

        Q_UNUSED(_business)

        pat.isBatch         = true;     // 定为批量筛查码

        // 得到扫码后的受检者对象后的处理
        globalService()->doOn_QrCode_ReceivedPatient(pat, false, this);
    } else {
        QString text = tr("查询被测者信息失败：") + "\n" + _err_msg;    // "Request info failed:"
        getWinManage()->showMsgWin(text);
    }
}

void PersonalInfos::setPatient(const CPatient &_pat)
{
    m_patient.cloneFrom(_pat);
}

CPatient &PersonalInfos::getPatient()
{
    return m_patient;
}

void PersonalInfos::slotPhysicButtonPressed()
{
    // 若本窗体不是当前窗体，则不处理此信号
    if (this != getWinManage()->getCurrentWin()) {
        return;
    }

    // 按下物理按键，等效于点击“测量”按钮
    if (ui->wgtMeasure->isVisible()) {
        applyAndStartMeasure();
    } else {
        // logDebug();
    }

}

bool PersonalInfos::checkIsDataChanged(const CPatient &_old_pat, const CPatient &_new_pat,
                                       bool *_is_modi_patientid, bool *_is_modi_patientname,
                                       bool *_is_modi_patientsex, bool *_is_modi_patientdate)
{
    //
    if (_is_modi_patientid  )   *_is_modi_patientid     = (_old_pat.patientid != _new_pat.patientid);
    if (_is_modi_patientname)   *_is_modi_patientname   = (_old_pat.patientname != _new_pat.patientname);
    if (_is_modi_patientsex )   *_is_modi_patientsex    = (_old_pat.patientsex != _new_pat.patientsex);
    if (_is_modi_patientdate)   *_is_modi_patientdate   = (_old_pat.getBirthDateStr() != _new_pat.getBirthDateStr());

    //
    QDate date_old = _old_pat.getBirthDate(), date_new = _new_pat.getBirthDate();

    if (_new_pat.patientid       != _old_pat.patientid         ) return true;
    if (_new_pat.patientname     != _old_pat.patientname       ) return true;
    if (_new_pat.patientsex      != _old_pat.patientsex        ) return true;
    if (date_new != date_old                                   ) return true;
    if (_new_pat.patientstuclass != _old_pat.patientstuclass   ) return true;
    if (_new_pat.patientWechat   != _old_pat.patientWechat     ) return true;
    if (_new_pat.patientPhone    != _old_pat.patientPhone      ) return true;
    if (_new_pat.patientAddress  != _old_pat.patientAddress    ) return true;

    return false;
}

void PersonalInfos::cloneEditingValues(const CPatient &_src_pat, CPatient &_dst_pat)
{
    _dst_pat.patientid          = _src_pat.patientid        ;
    _dst_pat.patientname        = _src_pat.patientname      ;
    _dst_pat.patientsex         = _src_pat.patientsex       ;
    _dst_pat.setBirthDate(_src_pat.getBirthDate());
    _dst_pat.patientstuclass    = _src_pat.patientstuclass  ;
    _dst_pat.patientWechat      = _src_pat.patientWechat    ;
    _dst_pat.patientPhone       = _src_pat.patientPhone     ;
    _dst_pat.patientAddress     = _src_pat.patientAddress   ;
}

bool PersonalInfos::checkUiValues(Ui::PersonalInfos *_ui, const CPatient &_origin_pat, PersonalInfos::enModeFlag _mode_flag, QString &_err_msg)
{
    /* 数据合法性检查逻辑：
     * 1、必填字段不为空，且为相应数据类型的有效值。
     * 2、若是外部指令传入，只需编号和年龄段有效。
     * 3、若是批量筛查（可能是U盘导入），则只需编号和年龄段有效即可。
     *
     * 上述逻辑的简化 ->
     * 对照 PersonalInfos::enModeFlag 的定义，除了 modeFlag_New 和 modeFlag_EditAndSave 两种模式需要检查所有必填字段外，
     * 其它模式都只需编号和年龄段，而 modeFlag_ViewOnly 则不需检查。
     *
     */

    //
    bool is_pass = false;
    _err_msg.clear();

    //
    /* 注意：若未设置 is_pass 为 true 却 break 了 do-while(false) 流程，则须确保设置了 _err_msg，否则后续的流程会弹出一个空的提示框。 */
    do {
        // 编号必填
        if (_ui->edtNum->text().length() == 0) {
            _err_msg = tr("被测者编号不可为空"); // "The Screen Number cannot be empty"
            break;
        }

        // 如果年份是 2 位数，自动补上前两位
        QString year_str = _ui->edtYear->text();
        if (year_str.length() == 2) {
            year_str = QString::number(QDate::currentDate().year()).left(2) + year_str;
            int year = year_str.toInt();
            if (year > QDate::currentDate().year()) {
                year -= 100;
            }
            _ui->edtYear->setText(QString::number(year));
        }

        // 检查出生日期是否已合法填写            // TODO: 若是扫码或其它非手动编辑方式进入的，允许不填出生日期，取默认年龄段，因为有的对接端采用“仅编号”的二维码，且没有自动查询
        bool is_birthdate_ok = false;
        QString err_msg_birthdate;
        do {
            //
            if (_ui->edtYear->text().length() == 0 || _ui->edtMonth->text().length() == 0 || _ui->edtDay->text().length() == 0) {
                err_msg_birthdate = tr("请填写出生日期！"); // "Please fill in the BirthDate!"
                break;
            }

            // 年份范围须合理（因为门诊页面显示 2 位数的年份，可能导致看不出问题）
            int year = _ui->edtYear->text().toInt();
            int curr_year = QDate::currentDate().year();
            if (year < curr_year - 100 || year > curr_year) {
                err_msg_birthdate = tr("出生年份超出范围！");    // "Birth year out of range!"
                break;
            }

            // 出生日期须是合法的日期
            QDate birth_date;
            bool succ_date = Util::strsToDate(_ui->edtYear->text(), _ui->edtMonth->text(), _ui->edtDay->text(), birth_date);
            if (!succ_date) {
                err_msg_birthdate = tr("出生日期不合法！"); // "The BirthDate is not valid!"
                break;
            }

            // 出生日期与已测年龄段不能冲突
            // TODO:

            //
            is_birthdate_ok = true;
        } while (false);

        // 出生日期必填（除非是外部传入或来自扫码，且已有年龄段）
        bool is_age_range_ok = (ageRange_Invalid != _origin_pat.getAgeRange())
                && (PersonalInfos::modeFlag_FromCommand == _mode_flag || PersonalInfos::modeFlag_FromBarcode == _mode_flag);
        if (!is_birthdate_ok && !is_age_range_ok) {
            _err_msg = err_msg_birthdate;
            break;
        }

        // 若数据是来自扫码或外部命令的新增操作，则必有字段只需编号和年龄段
        if (!((PersonalInfos::modeFlag_New == _mode_flag) || (PersonalInfos::modeFlag_EditAndSave == _mode_flag))) {
            is_pass = true;
            break;
        }

        // 姓名必填
        if (_ui->edtName->text().length() == 0) {
            _err_msg = tr("请填写姓名"); // "Please fill in the Name"
            break;
        }

        // 性别必填
        if ((!_ui->rbtnSexM->isChecked() && !_ui->rbtnSexF->isChecked())) {
            _err_msg = tr("请选择性别！");    // "Please select the Sex!"
            break;
        }

        // 电话必填
        //if (_ui->edtPhone->text().length() == 0) {
        //    _err_msg = (language ? "请填写电话号码" : "Please fill in the phone number");
        //    break;
        //}

        // 手机号须是 11 位数（中文语言时）
        QString phone = _ui->edtPhone->text();
        if (phone.length() > 0 && phone.length() != 11 && G_LANGUAGE_CHINESE == CGlobal::language) {
            _err_msg = tr("手机号须是 11 位数！");  // "Phone number must be 11 digits"
            break;
        }

        // 手机号须是数字
        bool is_phone_digit = true;
        for (int i = 0; i < phone.length(); i++) {
            if (!phone.at(i).isDigit()) {
                is_phone_digit = false;
                break;
            }
        }
        if (!is_phone_digit) {
            _err_msg = tr("手机号只能包含数字！");    // "Phone number can only contain digits"
            break;
        }

        //
        is_pass = true;

    } while (false);

    //
    return is_pass;
}

bool PersonalInfos::saveEnityToDB(CPatient &_pat_new, QString &_err_msg,
                                  const bool &_is_modi_patientid, const bool &_is_modi_patientname,
                                  const bool &_is_modi_patientsex, const bool &_is_modi_patientdate)
{
    // 若是批量筛查，且是新增的编号，则不允许编号重复
    if (_pat_new.isBatch && (0 == _pat_new.id) && (m_savingLevelSelf >= savingLevel_Database)) {           /* id 为 0 时，保存到数据库为插入操作 */
        std::vector<CPatient> pats = m_mysql->findRecordByPatientid(_pat_new.patientid);
        if (pats.size() > 0) {
            _err_msg = (tr("编号\"%1\"已存在！")).arg(_pat_new.patientid); // "Num '%1' is exsited!"
            return false;
        }
    }

    // 若已测量，且修改了编号，则将已生成的文件改名
    if (m_patient.isTest) {
        //if (_pat_new.patientid != m_patient.patientid) {
        //    // 编号修改了之后，预览图须改名
        //    QString img_path_old = UtilApp::getPreviewImgPath(m_patient);
        //    QString img_path_new = UtilApp::getPreviewImgPath(_pat_new);
        //
        //    QString cmd_rename_img = "mv " + img_path_old + " " + img_path_new;
        //    system(cmd_rename_img.toLatin1().data());
        //
        //    // 编号修改了之后，删掉旧的 pdf 文件（需要用到时会重新生成）
        //    QString pdf_path = UtilApp::getPdfFilePath(m_patient);
        //    QString cmd_rm_pdf = QString("rm ") + pdf_path;
        //    system(cmd_rm_pdf.toLatin1().constData());
        //}
    }

    // 被测者信息被修改后，应重新生成已保存的旧的 pdf 报告
    //std::vector<CPatient> pats = mysql->findRecordByPatientid(m_patient.patientid);       /* 这里需要用旧实体对象的编号来查询，因为编号可能变了 */
    //QString file_path_old, file_path_new;
    //for (auto &pat : pats) {
    //    // 若已存在 pdf 报告，则重新生成
    //    file_path_old = UtilApp::getPdfFilePath(pat);
    //    if (QFile::exists(file_path_old)) {
    //        // 删除旧文件
    //        QFile::remove(file_path_old);
    //
    //        // 创建新文件
    //        pat.patientid = _pat_new.patientid;                  /* 这里需要用新的实体对象的编号来生成新的文件名，因为编号可能变了 */
    //        file_path_new = UtilApp::getPdfFilePath(pat);
    //        Result::savePdfReport(pat);
    //    }
    //}     // NOTE: PDF 文件改为在需要时即时生成

    // 将当前内部业务对象保存到数据库
    /* 注意：因为当前被测者信息和测量结果未分表，所以修改记录时，需先查找出所有该编号的记录，然后整批修改。
     * 处理逻辑：
     * 1、在数据库中查找是否存在该编号，若存在，则查找出所有该编号的记录，然后整批修改，否则，插入新记录。
     * 2、避免在别的模块修改涉及 MySQLitePatients::getInfoForClinic() 函数的 sql 中的分组语句
     * （group by patientid, patientname, patientsex, patientdate）的字段，否则会在门诊记录界面产生多个编号。
     * 3、特别需要注意的是结果页面的修改功能，不能仅保存单条测量结果中的被测者信息，而是要将未插入数据库的记录先插入，
     * 然后再调用本函数统一修改被测者信息。
     */
    if (_pat_new.id > 0) {                   /* 若 id 大于 0，表示修改已有记录，须整批修改 */
        // 同步修改数据库中所有同编号的记录的被测者信息
        editTesteeInfoOfNumber(m_patient.patientid, _pat_new);  /* 这里的编号是打开页面时传入的，并不是从 ui 获取，所以即使用户修改了编号，这个仍是旧编号 */
    } else {
        // 将数据对象插入到数据库，并得到 id 值
        m_mysql->insertHistory(_pat_new, true);
    }

    //
    return true;
}

bool PersonalInfos::editTesteeInfoOfNumber(const QString &_number, const CPatient &_pat)
{
    MySQLitePatients *mysql = MySQLitePatients::getInstance();

    // 查找出所有该编号的记录，然后整批修改
    std::vector<CPatient> pats;
    pats = mysql->findRecordByPatientid(_number);

    for (size_t i = 0; i < pats.size(); i++) {
        cloneDataObjOfUiFields(_pat, pats[i]);
    }

    mysql->TableModify(pats);

    return true;
}

bool PersonalInfos::pinToTop(const QString &_number)
{
    // 查找出所有该编号的记录，然后整批修改
    MySQLitePatients *database = MySQLitePatients::getInstance();
    std::vector<CPatient> pats = database->findRecordByPatientid(_number);
    if (!pats.empty()) {
        bool is_batch = true;

        //
        QString creat_time = QDateTime::currentDateTime().toString(CPatient::dateTimeFormat());
        for (size_t i = 0; i < pats.size(); i++) {
            pats[i].creattime = creat_time;
            is_batch = pats.at(i).isBatch;
        }
        database->TableModify(pats);
    }

    //
    return true;
}

void PersonalInfos::doOnReceivedHuayiQrCode(const QByteArray &_line_bytes)
{
    //
    showWaitForRequestWin();

    // 二维码内容解析及处理
    QString err_msg;
    bool succ = CDataIntfHuaYi::instance()->sendPatienInfoQuery(_line_bytes, err_msg);
    if (succ) {
        //

    } else {
        //
        hideWaitForRequestWin();

        //
        getWinManage()->showMsgWin(err_msg);
    }
}

void PersonalInfos::cloneDataObjOfUiFields(const CPatient &_pat_src, CPatient &_pat_dst)
{
    // 将 UI 的值拷贝到当前内部数据对象
    _pat_dst.cloneBasicInfoFrom(_pat_src);
}

PersonalInfos *PersonalInfos::getPersonalInfoWin(enModeFlag _mode_flag, enPatientSource _patient_source,
                                                 const CPatient *_patient, const QString &_barcode_data)
{
    //
    PersonalInfos *person_info = getWinManage()->getWindow<PersonalInfos>(WIN_PER);
    if (person_info) {
        person_info->setModeFlag(_mode_flag);
        person_info->setPatientSource(_patient_source);

        person_info->m_barcodeData = _barcode_data;
        person_info->m_isBarcodeValid = false;

        if (_patient) {
            person_info->setPatient(*_patient);
        } else {
            CPatient patient;
            patient.reset();
            person_info->setPatient(patient);
        }
    } else {
#if (OS_TYPE == 2)
        getWinManage()->showSuspensionPrompt(tr("内部错误：获取“被测者信息”窗口失败")); // "Internal error: Failed to obtain 'Testee Information' window"
#endif
    }

    //
    return person_info;
}

bool PersonalInfos::showPersonalInfo(PersonalInfos::enModeFlag _mode_flag, enPatientSource _patient_source,
                                     const CPatient *_patient, const QString &_barcode_data)
{
    PersonalInfos *person_info = PersonalInfos::getPersonalInfoWin(_mode_flag, _patient_source, _patient, _barcode_data);
    if (person_info) {
        // 若窗口已显示，须先隐藏      // NOTE: 否则因为该界面的隐藏事件里会重置二维码内容，而 getWinManage()->showWindow() 显示窗口时会先隐藏，导致已设置的数据丢失
        if (person_info->isVisible()) {
            person_info->hide();
        }

        // 窗口的显示
        getWinManage()->showWindow(person_info);
        return true;
    } else {
        return false;
    }
}

void PersonalInfos::setModeFlag(PersonalInfos::enModeFlag _mode_flags)
{
    m_modeFlag = _mode_flags;
}

void PersonalInfos::on_pushButton_Home_clicked()
{
    // 返回之前的工作
    QString err_msg;
    bool is_finished = doBeforeGoBack(err_msg);
    if (!is_finished && err_msg.length() > 0) {
        getWinManage()->showMsgWin(err_msg);
        return;
    }

    // 返回主页
    getWinManage()->showWindowByType(WIN_HOME);
}

void PersonalInfos::on_pushButton_back_clicked()
{
    // 返回之前的工作
    QString err_msg;
    bool is_finished = doBeforeGoBack(err_msg);
    if (!is_finished && err_msg.length() > 0) {
        getWinManage()->showMsgWin(err_msg);
        return;
    }

    // 返回上一个页面
    if (!isDialogMode()) {
        //QWidget *win_last = getWinManage()->getLastWin();
        //Result *win_result = dynamic_cast<Result *>(win_last);
        //if (win_result) {
        //    win_result->reloadCurrentRecord();       // 重新载入当前记录
            getWinManage()->backToLastWidget();
        //} else {
        //    getWinManage()->showWindowByType(WIN_HOME);
#if (OS_TYPE == 2)
        //    getWinManage()->showSuspensionPrompt(language ? "内部错误：获取窗体失败" : "Internal error: Failed to retrieve form");
#endif
        //}
    } else {
        getWinManage()->hideDialog(this, (is_finished ? QDialog::Accepted : QDialog::Rejected));
    }
}

void PersonalInfos::on_pushButton_Save_clicked()
{
    // 保存数据（手动的）
    QString err_msg;
    bool is_saved = doSave(true, m_savingLevelSelf, err_msg);
    if (!is_saved && err_msg.length() > 0) {
        getWinManage()->showMsgWin(err_msg);
    }
}

void PersonalInfos::on_pushButton_test_clicked()
{
    // 若为扫码模式且条码内容非法，则警告并询问
    if (modeFlag_FromBarcode == m_modeFlag && (!PersonalInfos::m_isBarcodeValid)) {
        QString question = tr("条码内容出错，请重新扫码。\n是否坚持要测量？");   // "The barcode content is incorrect. Please scan the barcode again. \nDo you insist on testing?"
        bool is_continue = getWinManage()->showNoticeWin(question);
        if (!is_continue) {
            return;
        }
    }

    // 应用窗体的编辑并开启测量
    applyAndStartMeasure();
}

bool PersonalInfos::doBeforeGoBack(QString &_err_msg)
{
    // 非手动的执行保存动作（扫码和指令模式，返回时不需保存）
    enSavingLevel saving_level = ((modeFlag_FromBarcode == m_modeFlag || modeFlag_FromCommand == m_modeFlag) ? savingLevel_No : m_savingLevelSelf);
    bool is_saved = doSave(false, saving_level, _err_msg);

    //
    return is_saved;
}

bool PersonalInfos::doSave(const bool _is_manual, const enSavingLevel _saving_level, QString &_err_msg)
{
    /* 保存过程的流程;
     * “保存”按钮的保存过程：参数（手动的，本窗体的保存层级）。保存层级 < Entity ？返回 true；保存层级 >= Entity ？数据合法性检查 -> 保存到本窗体的 Entity；保存层级 >= DataBase ？保存到数据库。
     * “返回”按钮的保存过程：参数（非手动，本窗体的保存层级）。
     * “测量”按钮的保存过程：参数（非手动，保存到 Entity）。
     */

    /* 本函数的错误消息：若是返回 false，则放到 _err_msg 参数即可，若是返回 true，则须自己弹出。 */

    //
    bool is_finished = false;   // 是否已完成（未必是已保存，比如用户选择了不保存）
    do {
        // 若保存层级小于 Entiry，则返回
        if (_saving_level < savingLevel_Entity) {
            if (_is_manual) {           // 若是手动操作，则显示提示信息（这种情况下一般地程序不应显示“保存”按钮）
                getWinManage()->showSuspensionPrompt(tr("本窗体的数据不需要保存。"));   // "Data of this form not need to be saved."
            }
            is_finished = true;
            break;
        }

        // UI数值合法性检查
        QString err_msg_check;
        bool is_check_pass = checkUiValues(ui, m_patient, m_modeFlag, err_msg_check);

        // UI 到临时实体
        CPatient pat_new;
        pat_new.cloneFrom(m_patient);      /* 先从业务数据对象拷贝，然后再从 UI 拷贝，否则会丢失 isBatch 等非本窗体编辑的值 */     // TODO: 非本窗体编辑的值应另留一份，避免逻辑复杂化和易出错？
        uiToEntity(pat_new);

        // 是否在数据无修改时也要保存到数据库（若不需，则若数据无修改，则可立即返回）
        bool is_need_save_no_change = (_saving_level >= savingLevel_Database && m_patient.id == 0);

        // 检查数值是否已被用户修改
        bool is_modi_patientid = false, is_modi_patientname = false, is_modi_patientsex = false, is_modi_patientdate = false;
        bool is_changed = checkIsDataChanged(m_patient, pat_new, &is_modi_patientid, &is_modi_patientname,
                                             &is_modi_patientsex, &is_modi_patientdate);
        if (!is_changed && !is_need_save_no_change) {
            // 若是手动操作，则提示数据无修改
            if (_is_manual) {
                getWinManage()->showSuspensionPrompt(tr("数据未被修改")); // "Data not modified"
            }

            //
            is_finished = true;
            break;
        }

        // 询问是否保存
        if (!_is_manual && _saving_level >= savingLevel_Database) {
            bool is_confirmed_save = getWinManage()->showNoticeWin(tr("是否保存修改？"));  // "Want to save the modifications?"
            if (!is_confirmed_save) {
                is_finished = true;
                break;
            }
        }

        // 若合法性检查不通过，则返回
        if (!is_check_pass) {
            _err_msg = err_msg_check;
            break;
        }

        // 若是门诊自建档案（若非云端推送（以"C"开头），则认为是自建），且接口是万灵云端，且无手机号，则警告无法建档
        if ((!ui->edtNum->text().startsWith("C")) && WinDataTrans::isManylinksDataIntf() && ui->edtPhone->text().length() == 0) {
            QString msg = tr("没有手机号无法在万灵云端建档！");    // "Unable to create archive on ManylinksCloud without phone number."
            bool ret = getWinManage()->showNoticeWin(msg + tr("是否继续？"));    // "Do you want to continue?"
            if (!ret) {
                break;
            }
        }

        // 若保存层级小于保存到数据库，则同步到实体对象，然后返回  /* 若后续还要操作，则应到最后才同步到本窗体实体对象，因为保存到数据库的过程还需要用到它的旧值 */
        if (_saving_level < savingLevel_Database) {
            // 将最新数据应用到本窗体的实体对象
            m_patient.cloneFrom(pat_new);

            //
            is_finished = true;
            break;
        }

        // 保存到数据库
        QString err_msg_save;
        bool is_succ_save = saveEnityToDB(pat_new, err_msg_save, is_modi_patientid, is_modi_patientname,
                                          is_modi_patientsex, is_modi_patientdate);
        if (is_succ_save) {
            //
            getWinManage()->showMsgWin(tr("保存成功"), true, "OK", -1, true);    // "Save Succeeded"

        } else {
            _err_msg = tr("保存失败：") + err_msg_save;  // "Save failed: "
            break;
        }

        // 将最新数据应用到本窗体的实体对象     /* 最后才同步到本窗体实体对象，因为保存到数据库的过程还需要用到它的旧值 */
        m_patient.cloneFrom(pat_new);

        //
        is_finished = true;

    } while (false);

    //
    return is_finished;
}

bool PersonalInfos::doBeforeNextStep(QString &_err_msg)
{
    // 非手动的执行保存动作
    bool is_saved = doSave(false, savingLevel_Entity, _err_msg);

    //
    return is_saved;
}

void PersonalInfos::applyAndStartMeasure()
{
    // 测量之前需要做的工作
    QString err_msg;
    bool is_finished = doBeforeNextStep(err_msg);
    if (!is_finished && err_msg.length() > 0) {
        getWinManage()->showMsgWin(err_msg);
        return;
    }

    // 开启测量前的数据合法性检查
    QString err_msg_check;
    bool is_check_pass = checkUiValues(ui, m_patient, m_modeFlag, err_msg_check);
    if (!is_check_pass) {
        getWinManage()->showMsgWin(err_msg_check);
        return;
    }

    // 开启测量界面
    getWinManage()->openMeasureWin(m_patient, m_patientSource, this);
}

void PersonalInfos::keyPressEvent(QKeyEvent *)
{    //
    if (!this->isVisible()) {
        return;
    }

    //
    /*
    QDir dir("/media/cut");
    if(!dir.exists()){
        dir.mkdir("/media/cut");
    }

    QScreen *screen = QGuiApplication::primaryScreen();
    QPixmap pixmap = screen->grabWindow(0);
    QString filePathName = "/media/cut/cut-";
    filePathName += QTime::currentTime().toString(Qt::ISODate);
    filePathName += ".png";

    if(!pixmap.save(filePathName,"png"))
    {
        qDebug()<<"cut save png failed"<<endl;
    }
    */
}

void PersonalInfos::on_btnGenNum_clicked()
{
    // 若编号不为空，则检查限制
    if (m_patient.patientid.length() > 0) {
        // 若编号已上传，则警告
        bool is_uploaded = false;
        std::vector<CPatient> pats = m_mysql->findRecordByPatientid(m_patient.patientid);
        for (size_t i = 0; i < pats.size(); i++) {
            if (pats.at(i).isUploaded) {
                is_uploaded = true;
                break;
            }
        }
        if (is_uploaded) {
            bool ret = getWinManage()->showNoticeWin(tr("此编号的结果已上传！\n确定需要修改编号吗？")); // "Result data of this number has been uploaded. \nSure need to modify the number?"
            if (!ret) {
                return;
            }
        }

        // 来自万灵云端的编号不允许编辑
        if (m_patient.patientid.startsWith("C") /*&& WinDataTrans::isManylinksDataIntf()*/) {    // 如果以"C"开头，是来自万灵云端推送，不可编辑      // 这个不好维护？在数据库增加“来源”字段？
            getWinManage()->showMsgWin(tr("此编号来自云端，不可编辑")); // "This number comes from the cloud, cannot be edited"
            return;
        }
    }

    //
    QString num_new = CWinManage::getNewClinicNum();
    ui->edtNum->setText(num_new);

}
