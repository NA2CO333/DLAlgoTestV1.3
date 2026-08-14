//数据导出处理
#include "threadmodel.h"

#include <QTableWidget>
#include <QDebug>
#include <QModelIndex>
#include <QTableWidgetItem>
#include <QScrollBar>
#include <QMessageBox>
#include <QDebug>
#include <QTextCodec>
#include <QDateTime>

#include "mainwindow.h"
#include "mysqlitepatients.h"
#include "winclinic.h"
#include "progresswindow.h"
#include "windowsmanager.h"
#include "result.h"
#include "global.h"
#include "util-common.h"
#include "util-app.h"

#include <string>
#include <vector>
#include <iostream>

#include "QtXlsx"

using namespace std;

using namespace DataTrans;

//
ThreadModel::ThreadModel(QObject *parent) : QObject(parent)
{
    mysql = MySQLitePatients::getInstance();
}

ThreadModel:: ~ThreadModel()
{
    qDebug() << "~ThreadModel()";
}

QString ThreadModel::getCellName(int row,int col)   // Excel 的行名规则是 A~Z, AA~AZ, BA~BZ, ..., ZA~ZZ
{
    if (col < 0 || col >= 26 * 26 || row < 0 || row >= 65535)
            return QString();

    QString name;
    if(col / 26)
    {
        name.append('A' + col / 26 - 1);
    }
    name.append('A' + col % 26);
    name.append(QString::number(row + 1));

    return name;
}

//QString ThreadModel::getGender(QString data)
//{
//    if (data == "男" || data == "male")
//        return "M";
//    else if(data == "女" || data == "female")
//        return "F";
//    else
//        return "";
//}

QString ThreadModel::mergeGradeAndClass(const QString &_grade, const QString &_class)
{
    return ((_grade.length() > 0 ? _grade + GRADE_CLASS_SEPARATOR : "") + _class);
}

void ThreadModel::splitGradeAndClass(const QString &_grade_and_class, QString &_grade, QString &_class)
{
    int idx_class = _grade_and_class.indexOf(GRADE_CLASS_SEPARATOR);
    _grade = (idx_class >= 0 ? _grade_and_class.mid(0, idx_class) : "");
    _class = (idx_class >= 0 ? _grade_and_class.mid(idx_class + 1) : _grade_and_class);
}

// 读取 Excel 表
/**
 * 需导入的字段：编号, 姓名, 性别, 出生日期，电话号码, 班级, 微信, 常住地址
 *      出生日期，别名：生日
 *      身份证号，别名：身份证
 *      常住地址，别名：住址
 *
 * 读取“年级”字段，但并入“班级”字段中。
 *
 * 字段名的最大字节数：30（在UTF-8编码中，若是中文，约10个字符，若是英文，约30个字符）。此处的字节数，是指存储字符串所需的字节数，而不是字符个数。
 * 字段名行可以位于表格第一行，也可以位于表格第二行，如果位于第二行，则第一行第一列单元格文本字节数须大于字段名的最大字节数。
 *
 * 日期格式兼容："yyyy-MM-dd", "yyyyMMdd", "yyyy/MM/dd", "yyyy.MM.dd"（见 Util::formatDateStr() -> Util::strToDate()）。
 */
bool ThreadModel::readExcel(QString _path, std::vector<CPatient> &_list_pats, QString _process_msg)
{
    logDebug("ThreadModel::readExcel() path=\""+ _path + "\"", CGlobal::LOG_DATATRANS);

    //QTextCodec *codec_old = QTextCodec::codecForLocale();
    //QTextCodec::setCodecForLocale(QTextCodec::codecForName("UTF8"));

    QXlsx::Document *xlsx = Q_NULLPTR;
    try {
        xlsx = new QXlsx::Document(_path);
    } catch (...) {
        logCritical(QString::asprintf("ThreadModel::readExcel(), unknow exception:\n %s", strerror(errno)), CGlobal::LOG_DATATRANS);
        emit sigWarningMsg("Open xlsx file failed!");
        return false;
    }

    QFile xlsxFile(_path);
    if(xlsx == NULL || !xlsxFile.exists())
    {
        QString file_name = _path.mid(_path.lastIndexOf(QDir::separator()) + 1);
        emit sigWarningMsg(tr("打开文件“%1”失败！").arg(file_name));   // "Open file \"%1\" failed!"
        return false;
    }

    int col_count = xlsx->dimension().columnCount();
    int row_count = xlsx->dimension().rowCount();

    //
    emit progressSig(_process_msg, 0);

    // 确定表头行号
    /* 注意：QXlsx 里的行和列号是从1开始的，而不是从0开始 */
    const int MAX_FIELD_NAME_BYTE_SIZE = 30;        // 字段名最大字节数
    int row_head = 1;
    if (xlsx->read(1, 1).toString().toUtf8().length() > MAX_FIELD_NAME_BYTE_SIZE)
        row_head = 2;

    // 各字段所对应的表格列号
    const int FIELD_COUNT = 9;      // 需读入的字段的列数
    int field_cols[FIELD_COUNT] = {-1, -1, -1, -1, -1, -1, -1, -1, -1};
    QString field_names[]       = {"编号", "姓名", "性别", "出生日期", "电话号码", "班级", "微信", "常住地址", "年级"};

    // 读入表头各字段名
    QStringList list_header_values;     // 表头值列表
    QString head_value;
    for (int i = 1; i <= col_count; i++) {
        head_value = xlsx->read(row_head, i).toString();
        if (head_value.length() > 0)
            list_header_values.append(head_value);
        else
            break;
    }

    // 表头字段名列数检查
    const int MIN_COL_COUNT = 2;       // 必有字段为“编号”和“出生日期” 2 个
    if (list_header_values.count() < MIN_COL_COUNT) {
        emit sigWarningMsg(tr("格式错误：读取表头失败！")); // "Format error: failed to read table header!"
        return false;
    }

    // 查找获得各字段在表格中的列索引
    int idx_header_values;
    for (int i = 0; i < FIELD_COUNT; i++) {
        idx_header_values = list_header_values.indexOf(field_names[i]);
        if (idx_header_values >= 0) {
            field_cols[i] = idx_header_values;
        }
    }

    // 兼容字段名的查找
    if (field_cols[3] < 0)      // 出生日期，别名：生日
        field_cols[3] = list_header_values.indexOf("生日");
    if (field_cols[7] < 0)      // 常住地址，别名：住址
        field_cols[7] = list_header_values.indexOf("住址");

    // 表头必有字段检查
    if (field_cols[0] < 0) {
        emit sigWarningMsg(tr("格式错误：未找到“编号”列！"));   // "Format error: Can't find \"Number\" column!"
        return false;
    }
    if (field_cols[3] < 0) {
        emit sigWarningMsg(tr("格式错误：未找到“出生日期”列！")); // "Format error: Can't find \"Name\" column!"
        return false;
    }

    // 逐行读入解析
    QString field_values[FIELD_COUNT];
    int col;
    QString value;
    bool is_valid;
    QRegExp reg_exp("^[0-9A-Za-z\\-_]+$");
    for(int row = row_head + 1; row <= row_count; row++)
    {
        is_valid = true;

        // 读入各字段值
        for (int i = 0; i < FIELD_COUNT; i++) {
            if (field_cols[i] < 0)
                continue;

            col = field_cols[i] + 1;
            value = "";
            QXlsx::Cell *cell = xlsx->cellAt(row, col);     // 单元格无内容时，QXlsx::Cell 可能不为 Null，也可能为 Null。
            if (cell) {
                if (3 == i) {       // “出生日期”字段，需做日期解析
                    if (cell->isDateTime()) {                               // TODO: 被 LibreOffice 保存过后，日期单元格被识别为 QXlsx::Cell::NumberType？
                        value = cell->dateTime().toString("yyyy-MM-dd");
                    } else {
                        value = cell->value().toString();
                        value = Util::formatDateStr(value, "yyyy-MM-dd");
                    }
                } else {
                    value = cell->value().toString();
                }
            }
            field_values[i] = value;
        }

        // “编号”字段不可为空
        if (field_values[0].length() == 0 || field_values[3] == 0) {
            is_valid = false;
            logDebug("data valid: Num can't be null!", CGlobal::LOG_DATATRANS);
        }

        // 编号只能是英文字母或数字或"-"、"_"
        if (reg_exp.indexIn(field_values[0]) < 0) {
            is_valid = false;
            logDebug("data valid: Num format error!", CGlobal::LOG_DATATRANS);
        }

        // “出生日期”不可为空
        if (field_values[3].length() == 0) {
            is_valid = false;
            logDebug("data valid: BirthDate can't be null!", CGlobal::LOG_DATATRANS);
        }

        //
        if (is_valid)
        {
            CPatient pat;

            pat.patientid       = field_values[0];
            pat.patientname     = field_values[1];

            pat.setSexFromDisc(field_values[2]);

            pat.setBirthDate(Util::strToDate(field_values[3]));
            //pat.setAgeRange(CAgeRange::getAgeRangeFromBirthdateStr(pat.getBirthDateStr()));

            pat.patientPhone    = field_values[4];
            pat.patientstuclass = ThreadModel::mergeGradeAndClass(field_values[8], field_values[5]);
            pat.patientWechat   = field_values[6];
            pat.patientAddress  = field_values[7];

            pat.isTest          = false;
            pat.isBatch         = true;
            pat.isNeedUpload    = false;
            pat.isUploaded      = false;

            _list_pats.push_back(pat);
        } else {
            logDebug(QString::asprintf("data valid, row %d is ignored!", row), CGlobal::LOG_DATATRANS);
        }

        int percent = ((float)row / row_count) * 100;
        emit progressSig(_process_msg, percent);

        QTime _time = QTime::currentTime().addMSecs(1);
        while(QTime::currentTime() < _time)
            QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
    }

    if (xlsx) {
        delete xlsx;
        xlsx = nullptr;
    }

    //
    //QTextCodec::setCodecForLocale(codec_old);

    //
    return true;
}

void ThreadModel::slotExportData(QVector<int> _selected_ids, QString _udisk_path, bool _is_batch, bool _is_sheet, bool _is_pdf, bool _is_template)     //门诊记录导出数据
{
    logDebug(QString::asprintf("ThreadModel::slotExportData(_udisk_path='%s', _is_batch=%s, _result=%s, _pdf=%s, _template=%s)",
                                   _udisk_path.toUtf8().data(), Util::bool2str(_is_batch), Util::bool2str(_is_sheet), Util::bool2str(_is_pdf), Util::bool2str(_is_template)
                                   ));

    // add by wim -- mount udisk
    //int ret_mount = system("mount /dev/sda1 -t vfat " + _udisk_path.toLatin1());  // TODO: 这个貌似多余的？这个路径已经挂载了？历史遗留的垃圾代码？ 2021-05-18
    //qDebug() << "mount ret_mount:" << ret_mount;

    // copy vector
    const double totalSize = _selected_ids.size();

    std::vector<CPatient> pats;
    pats = mysql->findRecordByIdList(_selected_ids);

    if(_is_sheet && totalSize > 0)
    {
        QString text = tr("导出结果数据");    // "Exporting result data"
        emit progressSig(text,0);

        // 构造 Excel 文档数据
        QXlsx::Document xlsx;

        QXlsx::Format cell_format;
        cell_format.setBorderColor(Qt::black);
        cell_format.setBorderStyle(QXlsx::Format::BorderThin);
        cell_format.setVerticalAlignment(QXlsx::Format::AlignVCenter);

        // do
        {
            QString header_str;
            //if (!CGlobal::isReducedVersion)      // 裁减版，屏蔽瞳孔尺寸、眼位、上睑下垂     // 不作处理？因为本来就没数据？否则可能以后若需要导入，则导致两个版本导出的数据不兼容？   2021-05-20
                header_str = tr("编号,姓名,年龄段,性别,出生日期,右眼球镜,右眼柱镜,右眼轴位,右眼等效球镜,右眼瞳孔直径,右眼水平固视,右眼垂直固视,右眼上睑下垂,左眼球镜,左眼柱镜,左眼轴位,左眼等效球镜,左眼瞳孔直径,左眼水平固视,左眼垂直固视,左眼上睑下垂,瞳距,屈光判断,年级,班级,测量时间,是否已测,是否批量,联系电话,微信号,住址");
                // "No,Name,AgeRange,Sex,BirthDate,SPH_R,CYL_R,Axis_R,SE_R,PD_R,HoriGaze_R,VertGaze_R,Ptosis_R,SPH_L,CYL_L,Axis_L,SE_L,PD_L,HoriGaze_L,VertGaze_L,Ptosis_L,ppd,summary,Grade,class,time,istest,isbatch,phone,wechat,address"
            //else
            //    header_str = tr(xx"编号,姓名,年龄段,性别,出生日期,右眼球镜,右眼柱镜,右眼轴位,左眼球镜,左眼柱镜,左眼轴位,瞳距,屈光判断,年级,班级,测量时间,是否已测,是否批量,联系电话,微信号,住址");

            if (visionNotation_None != CGlobal::visionNotation.getValue()) {
                header_str += tr(",右眼参考视力,左眼参考视力"); // ",Ref.Vision_R,Ref.Vision_L"
            }

            QStringList list_header = header_str.split(',');
            cell_format.setFontBold(true);
            cell_format.setHorizontalAlignment(QXlsx::Format::AlignHCenter);
            for (int i = 0; i < list_header.count(); i++) {
                xlsx.write(1, i + 1, list_header[i], cell_format);
            }

            //
            cell_format.setFontBold(false);
            cell_format.setHorizontalAlignment(QXlsx::Format::AlignLeft);

            CPatient patient;
            QStringList list_value;
            bool is_test;
            bool is_has_right;
            bool is_has_left;
            for (size_t row = 0; row < pats.size(); row++) {
                //
                patient = pats.at(row);
                is_test = patient.isTest;

                // 裁减版处理
                Result::reduceResult(patient);

                //
                is_has_right = false;
                is_has_left = false;
                if (is_test) {
                    Result::judgeSingleDualEyeMode(patient, &is_has_right, &is_has_left);
                }

                //
                QString num = patient.patientid;
                QString name = patient.patientname;
                QString age_range_desc = CAgeRange::getAgeRangeDesc(patient.getAgeRange());
                QString sex = patient.getSexDisc();
                QString birthdate = patient.getBirthDateStr();

                QString rsph    = (is_has_right ? patient.patientrighteyesph : "");
                QString rcyl    = (is_has_right ? patient.patientrighteyecyl : "");
                QString raxis   = (is_has_right ? patient.patientrighteyeax  : "");
                QString rse     = (is_has_right ? patient.patientrightse     : "");
                QString rpd     = (is_has_right ? patient.patientrightpd     : "");
                QString rhfix   = (is_has_right ? patient.patientrighths     : "");
                QString rvfix   = (is_has_right ? patient.patientrightvs     : "");
                QString rptosis = "";    // 右眼上睑下垂
                if (is_has_right && !CGlobal::isReducedVersion) {    // 裁减版，屏蔽上睑下垂
                    rptosis = (patient.patientrightptosis ? tr("是") : tr("否"));      // "Yes", "No"
                }

                QString lsph    = (is_has_left ? patient.patientlefteyesph  : "");
                QString lcyl    = (is_has_left ? patient.patientlefteyecyl  : "");
                QString laxis   = (is_has_left ? patient.patientlefteyeax   : "");
                QString lse     = (is_has_left ? patient.patientleftse      : "");
                QString lpd     = (is_has_left ? patient.patientleftpd      : "");
                QString lhfix   = (is_has_left ? patient.patientlefths      : "");
                QString lvfix   = (is_has_left ? patient.patientleftvs      : "");
                QString lptosis = "";    // 左眼上睑下垂
                if (is_has_left && !CGlobal::isReducedVersion) {    // 裁减版，屏蔽上睑下垂
                    lptosis = (patient.patientleftptosis ? tr("是") : tr("否"));      // "Yes", "No"
                }

                QString ppd = (is_test ? patient.patientpd : "");

                QString summary;
                if (is_test) {
                    stVisionJudgementRst right_comp, left_comp;
                    Result::getVisionJudgementRst(patient, is_has_right, is_has_left, right_comp, left_comp);
                    summary = Result::getVisionJudgementDesc(patient, is_has_right, is_has_left, right_comp, left_comp).toStr();
                    logDebug("ThreadModel::slotExportData(): summary = " + summary);
                }

                QString nianji, banji;
                ThreadModel::splitGradeAndClass(patient.patientstuclass, nianji, banji);

                QString testtime = patient.patienttesttime;
                QString istest_str = Util::boolToYesNo(patient.isTest);
                QString isbatch = Util::boolToYesNo(patient.isBatch);
                QString phone = patient.patientPhone;
                QString wechat = patient.patientWechat;
                QString address = patient.patientAddress;

                // 若柱镜度为 0（精度修整后），则柱镜、轴位都显示为空（包括结果界面、小票、导出、数据上传）
                if (is_has_right && Util::compDouble(rcyl.toDouble(), 0) == 0) {
                    rcyl = "0";     // NOTE: (2026-07-15)导出 Excel 时保留柱镜度的0值（有客户需要，崔继友）
                    raxis = "";
                }
                if (is_has_left && Util::compDouble(lcyl.toDouble(), 0) == 0) {
                    lcyl = "0";     // NOTE: (2026-07-15)导出 Excel 时保留柱镜度的0值（有客户需要，崔继友）
                    laxis = "";
                }

                // 写单元格
                list_value.clear();

                list_value.append(num);
                list_value.append(name);
                list_value.append(age_range_desc);
                list_value.append(sex);
                list_value.append(birthdate);
                list_value.append(rsph);
                list_value.append(rcyl);
                list_value.append(raxis);
                list_value.append(rse);
                list_value.append(rpd);
                list_value.append(rhfix);
                list_value.append(rvfix);
                list_value.append(rptosis);
                list_value.append(lsph);
                list_value.append(lcyl);
                list_value.append(laxis);
                list_value.append(lse);
                list_value.append(lpd);
                list_value.append(lhfix);
                list_value.append(lvfix);
                list_value.append(lptosis);
                list_value.append(ppd);
                list_value.append(summary);
                list_value.append(nianji);
                list_value.append(banji);
                list_value.append(testtime);
                list_value.append(istest_str);
                list_value.append(isbatch);
                list_value.append(phone);
                list_value.append(wechat);
                list_value.append(address);

                if (visionNotation_None != CGlobal::visionNotation.getValue()) {
                    list_value.append(is_has_right ? CAlgoInvoker::diopterToVision(rsph, rcyl, CGlobal::visionNotation.getValue()) : "");
                    list_value.append(is_has_left ? CAlgoInvoker::diopterToVision(lsph, lcyl, CGlobal::visionNotation.getValue()) : "");
                }

                for (int idx_val = 0; idx_val < list_value.count(); idx_val++) {
                    xlsx.currentWorksheet()->writeString(row + 2, idx_val + 1, list_value[idx_val], cell_format);
                }

                // 设置列宽
                if (0 == row) {
                    double w_default, w_header, w_content, w;
                    w_default = 8.3;
                    for (int idx_col = 0; idx_col < list_header.count(); idx_col++) {
                        w_header = list_header[idx_col].length() * 2 + 0.8;
                        w_content = Util::calcTextCharWidth(list_value[idx_col]) + 0.8;
                        w = Util::max3(w_default, w_header, w_content);
                        xlsx.setColumnWidth(idx_col + 1, w);
                    }
                }

                //
                int pNum = (row + 1)/totalSize * 100;
                emit progressSig(text,pNum);
                QTime _time = QTime::currentTime().addMSecs(1);
                while(QTime::currentTime() < _time)
                    QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
            }

//          qDebug()<<"slotExportData+++++++++++++++++++++";
            //            emit mov();
            //            emit sigProcessEnd();
        }
        //while (false);

        // 确定文件名
        QString file_name = CGlobal::devNum
                + (_is_batch ? "_Screen_" : "_Clinic_")
                + QDateTime::currentDateTime().toString("yyyyMMdd");   // TODO: 文件名不支持中文，待解决
        QString file_name_ext = ".xlsx";
        QString file_path;
        int count_file_repeat = 0;
        do {
            file_path = _udisk_path + QDir::separator() + file_name + (count_file_repeat > 0 ? QString("(%1)").arg(count_file_repeat) : "") + file_name_ext;
            count_file_repeat++;
        } while (QFile::exists(file_path));
        qDebug() << "file_path:" << file_path;

        // 保存数据到文件
        bool save_succ =  xlsx.saveAs(file_path);
        if (!save_succ) {
            //
        }
    }

    //
    if(_is_pdf)
    {
        QString text = tr("导出pdf报告");    // "Exporting pdf file"
        emit progressSig(text,0);

        // 如果目标目录不存在，则进行创建
        QString target_dir_path = QString("%1/PdfReports").arg(_udisk_path);
        QDir targetDir(target_dir_path);
        if(!targetDir.exists())
        {
            if (!targetDir.mkdir(targetDir.absolutePath())) {
                emit sigProcessEnd();
                return;
            }
        }

        // 逐个报表拷贝
        int num_cp = 0;
        int count = pats.size();
        for (int i = 0; i < count; i++) {
            CPatient &pat = pats.at(i);

            if (!pat.isTest) {
                continue;
            }

            // 得到源 pdf 的路径
            QString file_path = Result::savePdfReport(pat);

            // 文件拷贝
            QString img_dir_name = pat.getImgDirName();
            QString target_path = target_dir_path + QDir::separator() + img_dir_name + ".pdf";
            QFile::copy(file_path, target_path);

            // 进度信息
            int percent = (++num_cp) / totalSize * 100;
            emit progressSig(text, percent);

            QTime _time = QTime::currentTime().addMSecs(1);
            while(QTime::currentTime() < _time)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
    }

    //
    if(_is_template)
    {
        QString cmd = "cp /usr/patients.xlsx " + _udisk_path + QDir::separator();
        system(cmd.toLatin1().data());
    }

    // 导出图片
    if (saveImage) {
        QString dir_path_dst = QString("%1/photo_%2").arg(_udisk_path).arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmm"));
        QDir dir_dst(dir_path_dst);
        if (!dir_dst.exists()) {
            if (!dir_dst.mkdir(dir_dst.absolutePath())) {
                emit sigProcessEnd();
                return;
            }
        }

        // 拷贝全部 log
        copyDir(QDir("/media/log"),     QDir(dir_path_dst + "/log"));                 // TODO: 这些路径应该用常量定义
        copyDir(QDir("/media/algoLog"), QDir(dir_path_dst + "/algoLog"));

        //
        //copyDir(QDir("/media/pdfPreviewImg"), QDir(target + "/pdfPreviewImg"));

        // 拷贝选定的编号对应的图片
        QString src_parent_dir_path = QString::fromStdString(CAlgoIntf::getImageDirPath());         // 源图片文件的母文件夹路径

        QString img_dir_name;
        int count = pats.size();
        for(int i = 0; i < count; i++) {
            CPatient &pat = pats.at(i);
            img_dir_name = pat.getImgDirName();

            copyDir(QDir((src_parent_dir_path + "/%1").arg(img_dir_name)), QDir((dir_path_dst + "/photo/%1").arg(img_dir_name)));

            // 进度信息
            // TODO:

        }
    }

    //
    QString text = "writing";
    emit progressSig(text,0);
    QTime _time = QTime::currentTime().addMSecs(1);
    while(QTime::currentTime() < _time)
        QCoreApplication::processEvents(QEventLoop::AllEvents, 1);

    bool ret_sync = Util::CUDisk::sync();
    logDebug(QString::asprintf("sync executed, ret_sync = %s", Util::bool2str(ret_sync)));

    // add by wim -- umount udisk
    //int ret_umount = Util::CUDisk::umount();
    //qDebug() << "umount ret_umount = " << ret_umount;

    //
    emit progressSig("end",100);
    emit sigProcessEnd();
}

void ThreadModel::copyDir(QDir _dir_src, QDir _dir_dst)
{
    if(!_dir_src.exists())
    {
        qDebug() << "dir_src:" << _dir_src.absolutePath() << "doesn't exists!";
        return;
    }
    if(!_dir_dst.exists())
    {
        if(_dir_dst.mkpath(_dir_dst.absolutePath()))
        {
            qDebug() << "create dir_tar:" << _dir_dst.absolutePath();
        } else {
            qDebug() << "create dir_tar failed! path = " << _dir_dst.absolutePath();
        }
    }

    QFileInfoList fileList = _dir_src.entryInfoList();
    QString text = tr("导出图像");  // "Exporting image"
    double totalSize = fileList.size();
    emit progressSig(text,0);
    int cpNum = 1;
    foreach(QFileInfo fileInfo, fileList)
    {
        if (fileInfo.fileName() == "." || fileInfo.fileName() == "..")
            continue;

        if (fileInfo.isDir())
        {
            QDir dir_sub_src(fileInfo.absoluteFilePath());
            QDir dir_sub_dst(_dir_dst.path() + QString("/") + fileInfo.fileName());
            qDebug() << "copy srcDir:" << dir_sub_src.absolutePath();
            qDebug() << "to tarDir:" << dir_sub_dst.absolutePath();
            if(!dir_sub_dst.exists())
            {
                if(_dir_dst.mkpath(dir_sub_dst.absolutePath()))
                {
                    qDebug() << "create dir_sub_tar:" << dir_sub_dst.absolutePath();
                } else {
                    qDebug() << "create dir_sub_tar failed! path = " << dir_sub_dst.absolutePath();
                }
            }
            copyDir(dir_sub_src, dir_sub_dst);
        }

        if (fileInfo.fileName().endsWith("txt")
                || fileInfo.fileName().endsWith("bmp")
                || fileInfo.fileName().endsWith("png")
                || fileInfo.fileName().endsWith("jpg")
                )
        {
            QString srcFile = fileInfo.absoluteFilePath();
            QString tarFile = _dir_dst.absolutePath() + QString('/') + fileInfo.fileName();

            if(QFile(tarFile).exists())
            {
                if(_dir_dst.remove(fileInfo.fileName()))
                    qDebug() << "cover file:" << tarFile;
            }

            if(!QFile::copy(srcFile, tarFile))
            {
                qDebug() << "copy file error! src: " << srcFile << " -> tar: " << tarFile;
            }

            int pNum = (cpNum++)/totalSize * 100;
            emit progressSig(text,pNum);

            qDebug()<<"cp :"<<cpNum;
            QTime _time = QTime::currentTime().addMSecs(1);
            while(QTime::currentTime() < _time)
                QCoreApplication::processEvents(QEventLoop::AllEvents, 1);
        }
    }

}

/*
void ThreadModel::slotImportBatch(QString _udisk_path)
{
    QString udiskPath = QString("%1/").arg(_udisk_path);
    // add by wim -- mount udisk
    int ret = system("mount /dev/sda1 -t vfat " + udiskPath.toLatin1());
    qDebug() << "mount ret:" << ret;

    //QTextCodec *codec_old = QTextCodec::codecForLocale();
    //QTextCodec::setCodecForLocale(QTextCodec::codecForName("GBK"));

    QString male = QString("男");
    QString female = QString("女");

    QString url = QString("%1/patients.csv").arg(_udisk_path);
    qDebug() << "import path :" << url;
    QFile file(url);
    int fileLines = 0;

    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))
    {
        qDebug() << "file open failed:" << url;
        emit warnSig(...);
    }
    else
    {
        vector<CPatient>  veclist;
        QStringList list;
        list.clear();
        QTextStream in(&file);

        qDebug() << "start reading csv file";
        qDebug() << "read fist line:" << in.readLine(); //skip first line

        int lineCount = 0;
        while(!in.atEnd())
        {
            in.readLine();
            lineCount++;
        }
        in.seek(0);
        in.readLine();//skip first line
        ProgressWindow progress;
        progress.setContext("正在导入");
        progress.setProgress(0);
        progress.show();

        while(!in.atEnd())
        {
            fileLines++;
            QString fileLine = in.readLine(); //从第一行读取至下一行
            qDebug() << "readline:" << fileLine;
            list.clear();
            list = fileLine.split(",");

            qDebug() << "QStringlist:";
            QList<QString>::Iterator it = list.begin();
            for(; it != list.end(); it++)
            {
                qDebug() << *it;
            }
            qDebug() << endl;

//                  QVariant tempValue = list.at(16);
//                  bool tempFinished = tempValue.toBool();
//                  QVariant tempText = list.at(17);

//                  bool tempEnd = tempText.toBool();
//                  qDebug()<<list.at(0)<<"sex ="<<sex;
//                  qDebug()<<"male="<<male;
//                  qDebug()<<list.at(0)<<"sex.toLatin1() ="<<QString::fromStdString(sex);
//                  qDebug()<<list.at(0)<<"QString::fromLocal8Bit(sex) ="<<QString::fromLocal8Bit(sex);

//                  CPatient pats(list.at(0),list.at(1),list.at(2), sex,list.at(4),list.at(5),list.at(6),list.at(7),
//                  list.at(8),list.at(9),list.at(10),list.at(11),list.at(12),list.at(13),list.at(14),list.at(15),tempFinished,tempEnd,list.at(18),list.at(19));

//                  QString::fromUtf8(name);

            CPatient pats;
            //num
            if(list.size() >= 1)
            {
                QString id = list.at(0);
                if(id != "")
                    pats.patientid = list.at(0);
                else
                    continue;
            }
            else
                continue;

            //name
            if(list.size() >= 2)
            {
                QString name = list.at(1);
                pats.patientname = name;
            }
            else
                pats.patientname = "";

            //age
            if(list.size() >= 3)
            {
                QString str = list.at(2);
                int age = str.toInt();
                qDebug() << pats.patientid << "=" << age;
                if(age <= 1)
                    pats.patientagerange = QString("0");
                else if(age > 1 && age <= 3)
                    pats.patientagerange = QString("1");
                else if(age > 3 && age <= 6)
                    pats.patientagerange = QString("2");
                else if(age > 6 && age <= 20)
                    pats.patientagerange = QString("3");
                else if(age > 20)
                    pats.patientagerange = QString("4");
            }

            //sex
            if(list.size() >= 4)
            {
                QString sex = list.at(3);
                if (sex == male || sex == "male")
                {
                    sex = "M";
                    qDebug() << list.at(0) << "sex = M";
                }
                else if(sex == female || sex == "female")
                {
                    sex = "F";
                    qDebug() << list.at(0) << "sex = F";

                }
                else
                    sex = "";

                pats.setSexFromDisc(list.at(3));
            }
            //class
            if(list.size() >= 5)
                pats.patientstuclass = list.at(4);
            //birthdate
            if(list.size() >= 6)
            {
                QString date = list.at(5);
                if(date != "")
                {
                    date.insert(4, "-");
                    date.insert(7, "-");
                    pats.patientdate = date;
                }
            }
            qDebug() << "--id:" << pats.patientid << ",ageRange:" << pats.patientagerange << ",date:" << pats.patientdate;
            //phone
            if(list.size() >= 7)
                pats.patientPhone = list.at(6);
            //wechat
            if(list.size() >= 8)
                pats.patientWechat = list.at(7);
            //address
            if(list.size() >= 9)
                pats.patientAddress = list.at(8);

            pats.isTest = false;
            pats.isBatch = true;
            pats.isNeedUpload = false;
            pats.isUploaded = false;


            veclist.push_back(pats);
            if  (veclist.size() % 20 == 0)
            {
                mysql->TableBatchAdd(veclist);
                veclist.clear();
                emit mov();
                emit sigProcessEnd();
            }
            if(fileLines * 100 / lineCount != (fileLines - 1) * 100 / lineCount)
            {
                progress.hide();
                progress.setProgress(fileLines * 100 / lineCount);
                progress.show();
            }
        }
        mysql->TableBatchAdd(veclist);
    }

    ret = system("umount " + udiskPath.toLatin1());
    qDebug() << "umount ret = " << ret;

    emit mov();
    emit sigProcessEnd();

    //
    //QTextCodec::setCodecForLocale(codec_old);
}
*/

void ThreadModel::slotImportBatch(QStringList _file_list)
{
    //QString udiskPath = QString("%1/").arg(_udisk_path);
    // add by wim -- mount udisk
    //int ret = system("mount /dev/sda1 -t vfat " + udiskPath.toLatin1());
    //qDebug() << "mount ret:" << ret;

    int count_repeated = 0;

    emit sigEnableViewObject(false);
    vector<CPatient> pat_list;
    for (int i = 0; i < _file_list.length(); i++) {
        pat_list.clear();
        QString process_msg = tr("正在读取数据"); // "Reading data"
        process_msg = (process_msg + "(%1 / %2)").arg(i + 1).arg(_file_list.count());
        bool succ = readExcel(_file_list.at(i), pat_list, process_msg);
        if(!succ){
            Util::CUDisk::sync();
//            Util::CUDisk::CUDisk::umount();;
            emit progressSig("end",100);
            emit sigProcessEnd();
            emit sigEnableViewObject(true);
            return;
        }
        mysql->TableBatchAdd(pat_list, &count_repeated);
    }

    Util::CUDisk::sync();
//    Util::CUDisk::CUDisk::umount();;
    emit progressSig("end",100);

    if (count_repeated > 0) {
        getWinManage()->showSuspensionPrompt(tr("已过滤编号重复的记录 %1 条！").arg(count_repeated));   // "Filtered %1 records with duplicate numbers"
    }

    emit sigRefresh();
    emit sigProcessEnd();
    emit sigEnableViewObject(true);
}


// add by wim
void ThreadModel::showProgress(int msecs)
{
    ProgressWindow progress;

    int i;
    for(i = 0; i <= msecs / 50; i++)
    {
        progress.setProgress(i * 100 * 50 / msecs);
        progress.show();
        QThread::msleep(50);
        progress.hide();
    }
    progress.close();

//    MessageWin mess(NULL);
//    if(language)
//        mess.setContent("导出成功");
//    else
//        mess.setContent("Export success");
//    mess.setButtonEnable(false);
//    mess.show();
//    QThread::sleep(1);
//    mess.close();
}
