//视力标准界面
#include "eyesightstandard.h"
#include "ui_eyesightstandard.h"

#include <QPainter>
#include <QPixmap>
#include <QScroller>
#include <QScrollBar>

#include "windowsmanager.h"
#include "util-common.h"
#include "global.h"
#include "globalclass.h"

//
bool stVisionJudgementRst::isNormal() {
    if (!CGlobal::isReducedVersion)
        return !anisometropia && !astigmatism && !myopia && !hyperopia && !unequalInPupilSize && !verticalGaze && !nasalGaze && !bitemporalGaze && !gazeAsymmetry;
    else
        return !anisometropia && !astigmatism && !myopia && !hyperopia;
}

//
/*static*/ double g_aEyesightStandard[5][9] = {     // TODO: 此处改为 static，用到此变量的地方改用 eyesightstandard::standardCompare()
    {1.50, 2.25, 0.25, 3.50, 1.00, 8.00, 5.00, 8.00, 8.00},     // 6-12个月       /* struct stVisionJudgementRst 的前 9 个成员一一对应 */
    {1.00, 2.00, 0.00, 3.25, 1.00, 8.00, 5.00, 8.00, 8.00},     // 12-36个月
    {1.00, 2.00, -0.25, 3.00, 1.00, 8.00, 5.00, 8.00, 8.00},    // 3-6岁
    {1.00, 1.75, -1.00, 2.75, 1.00, 8.00, 5.00, 8.00, 8.00},    // 6-20岁
    {1.00, 1.75, -0.75, 1.75, 1.00, 8.00, 5.00, 8.00, 8.00},    // 20-100岁
};

/* 注意：右眼参数在前 */
// TODO: 浮点大小比较，除非须判断是否相等，否则一般可以不考虑精度问题？
void eyesightstandard::standardCompare(const CPatient &_patient, stVisionJudgementRst * _result_right, stVisionJudgementRst * _result_left)
{
    // TODO: 如果 QString 类型的值为 ""，则 toDouble 是 0，而此时 0 并非真实值，真实值是没有值，当作 0 来与标准值对比，则结果不对？

    int age_range = (int)_patient.getAgeRange();

    double r_vs = 0;
    double r_hs = 0;
    double r_sph = 0;
    double r_cyl = 0;
    if(_result_right)
    {
        r_vs = _patient.patientrightvs.toDouble();     // 右眼垂直凝视
        if((Util::compDouble(r_vs, g_aEyesightStandard[age_range][5]) >= 0 || Util::compDouble(r_vs, -g_aEyesightStandard[age_range][5]) <= 0) && !CGlobal::isReducedVersion)
            _result_right->verticalGaze = true;

        r_hs = _patient.patientrighths.toDouble();     // 右眼水平凝视
        if((Util::compDouble(r_hs, g_aEyesightStandard[age_range][7]) >= 0) && !CGlobal::isReducedVersion)
            _result_right->bitemporalGaze = true;
        if((Util::compDouble(r_hs, -g_aEyesightStandard[age_range][6]) <= 0) && !CGlobal::isReducedVersion)
            _result_right->nasalGaze = true;

        r_sph = _patient.patientrighteyesph.toDouble();            // TODO: 值为空时，不应当作 0 ？
        if(Util::compDouble(r_sph, g_aEyesightStandard[age_range][2]) <= 0)
            _result_right->myopia = true;
        if(Util::compDouble(r_sph, g_aEyesightStandard[age_range][3]) >= 0)
            _result_right->hyperopia = true;

        r_cyl = _patient.patientrighteyecyl.toDouble();
        if(qAbs(r_cyl) >= qAbs(g_aEyesightStandard[age_range][1]))
        {
            _result_right->astigmatism = true;
            if((!g_isHmMode && Util::compDouble(r_cyl, MAX_CYL_NORMAL) <= 0) || (g_isHmMode && Util::compDouble(r_cyl, MAX_CYL_HMMODE) <= 0))    // “散光 -4~-7.5，数据可能异常，建议核实”
                _result_right->dataAbnormal = true;
        }
    }

    double l_vs = 0;
    double l_hs = 0;
    double l_sph = 0;
    double l_cyl = 0;
    if(_result_left)
    {
        l_vs = _patient.patientleftvs.toDouble();      // 左眼垂直凝视
        if((Util::compDouble(l_vs, g_aEyesightStandard[age_range][5]) >= 0 || Util::compDouble(l_vs, -g_aEyesightStandard[age_range][5]) <= 0) && !CGlobal::isReducedVersion)
            _result_left->verticalGaze = true;

        l_hs = _patient.patientlefths.toDouble();      // 左眼水平凝视
        if((Util::compDouble(l_hs, g_aEyesightStandard[age_range][6]) >= 0) && !CGlobal::isReducedVersion)
            _result_left->nasalGaze = true;
        if((Util::compDouble(l_hs, -g_aEyesightStandard[age_range][7]) <= 0) && !CGlobal::isReducedVersion)
            _result_left->bitemporalGaze = true;

        l_sph = _patient.patientlefteyesph.toDouble();
        if(Util::compDouble(l_sph, g_aEyesightStandard[age_range][2]) <= 0)
            _result_left->myopia = true;
        if(Util::compDouble(l_sph, g_aEyesightStandard[age_range][3]) >= 0)
            _result_left->hyperopia = true;

        l_cyl = _patient.patientlefteyecyl.toDouble();
        if(qAbs(l_cyl) >= qAbs(g_aEyesightStandard[age_range][1]))
        {
            _result_left->astigmatism = true;
            if((!g_isHmMode && Util::compDouble(l_cyl, -4.0) <= 0) || (g_isHmMode && Util::compDouble(l_cyl, -7.50) <= 0))
                _result_left->dataAbnormal = true;
        }
    }

    if(_result_right && _result_left && (
                Util::compDouble(qAbs(l_cyl - r_cyl), g_aEyesightStandard[age_range][0]) >= 0 ||
                Util::compDouble(qAbs(l_sph - r_sph), g_aEyesightStandard[age_range][0]) >= 0
                ))
    {
        _result_right->anisometropia = true;
        _result_left->anisometropia = true;
    }

    if(_result_right && _result_left
            && Util::compDouble(qAbs(_patient.patientleftpd.toDouble() - _patient.patientrightpd.toDouble()), g_aEyesightStandard[age_range][4]) >= 0
            && !CGlobal::isReducedVersion
            )
    {
        _result_right->unequalInPupilSize = true;
        _result_left->unequalInPupilSize = true;
    }

    if(_result_right && _result_left && (
                //Util::doubleComp(qAbs(l_vs + r_vs), g_aEyesightStandard[age_range][8]) >= 0 ||
                Util::compDouble(qAbs(l_hs + r_hs), g_aEyesightStandard[age_range][8]) >= 0
                ) && !CGlobal::isReducedVersion)      // 取消垂直方向的凝视不对称判断，“垂直凝视不对称的人很少，而且判断方法应与水平方向不同”。（需求确认：梁健晖 2021-09-01）
    {
        _result_right->gazeAsymmetry = true;
        _result_left->gazeAsymmetry = true;
    }
}

eyesightstandard::eyesightstandard(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::eyesightstandard)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    //
    QScroller::grabGesture(ui->scrollArea->viewport(), QScroller::LeftMouseButtonGesture);

    //
    paraList = appSetting::value("/standard/para").toStringList();
    if (paraList.size() != 15) {
        setDefaultParams();
    }
    getStandardValue();     //更新获取本地保存的参数
    setEditLineValue();

}

void eyesightstandard::setDefaultParams()
{
    paraList.clear();

    paraList.push_back("2.25");
    paraList.push_back("2.0");
    paraList.push_back("2.0");
    paraList.push_back("1.75");
    paraList.push_back("1.75");
    paraList.push_back("0.25");
    paraList.push_back("0");
    paraList.push_back("-0.25");
    paraList.push_back("-1.0");
    paraList.push_back("-0.75");
    paraList.push_back("3.5");
    paraList.push_back("3.25");
    paraList.push_back("3.0");
    paraList.push_back("2.75");
    paraList.push_back("1.75");

    appSetting::setValue("/standard/para",paraList);
}

eyesightstandard::~eyesightstandard()
{
    delete ui;
}

void eyesightstandard::showEvent(QShowEvent *)
{
    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 语言
    //if (language) {
    //    ui->label_Home->setText("主页");
    //    ui->label_Save->setText("保存");
    //    ui->label_Recover->setText("恢复");
    //    ui->label_Back->setText("返回");
    //    ui->lblAgeRangeCaption->setText("年龄段");
    //} else {
    //    ui->label_Home->setText("Home");
    //    ui->label_Save->setText("Save");
    //    ui->label_Recover->setText("Restore");
    //    ui->label_Back->setText("Back");
    //    ui->lblAgeRangeCaption->setText("AgeRange");
    //}

    // 标题
    getWinManage()->updateWindowTitle(this, tr("视力标准"));    // "Eyesight standard"

    //
    ui->lblAgeRangeDesc_0->setText(CAgeRange::getAgeRangeDesc(ageRange_0_06_12_MONTH));
    ui->lblAgeRangeDesc_1->setText(CAgeRange::getAgeRangeDesc(ageRange_1_01_03_YEAR));
    ui->lblAgeRangeDesc_2->setText(CAgeRange::getAgeRangeDesc(ageRange_2_03_06_YEAR));
    ui->lblAgeRangeDesc_3->setText(CAgeRange::getAgeRangeDesc(ageRange_3_06_20_YEAR));
    ui->lblAgeRangeDesc_4->setText(CAgeRange::getAgeRangeDesc(ageRange_4_20_100_YEAE));

    // 图片载入
    static QString path_img_last;
    QString path_img_curr;
    if (themeType_Black == getSysThemeType()) {
        path_img_curr = QString(":/resource/black_theme/sight-standard_b_%1.png").arg(CGlobal::language);
    }
    else if (themeType_White == getSysThemeType()) {
        path_img_curr = QString(":/resource/white_theme/sight-standard_w_%1.png").arg(CGlobal::language);
    }
    static QPixmap pixmap_img;
    if (path_img_curr != path_img_last && path_img_curr.length() > 0) {
        pixmap_img.load(path_img_curr);
        ui->lblImg->setPixmap(pixmap_img);
        path_img_last = path_img_curr;
    }

    // 样式
    //QPalette palette;
    if (themeType_Black == getSysThemeType()) {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        this->setStyleSheet(QString()
                            //+ " QWidget#eyesightstandard { background-color: rgb(28,28,30); } "
                            + " QLabel { color:rgb(204,204,204); } "
                            + " QScrollArea { background-color: rgb(28,28,30); } "
                            //+ " QScrollArea { border-top: 2px solid rgb(235,46,50); border-bottom: 2px solid rgb(235,46,50); } "
                            //+ " QScrollArea QWidget { background-color: rgb(28,28,30); } "
                            );

        ui->scrollArea->viewport()->setStyleSheet(QString()
                                                  + " QWidget   { background-color: rgb(28,28,30); } "
                                                  + " QLineEdit { background-color: rgb(11,15,22); color: rgb(250,250,252); border: none; qproperty-alignment: AlignHCenter; } "
                                                  );
        // TODO: 为什么须直接设置 ui->scrollArea->viewport() 的背景，而在窗体的样式里设置无效（选择器：QScrollArea QWidget）？

        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/black_theme/save_b.png"));
        ui->pushButton_Recover->setIcon(QIcon(":/resource/black_theme/recover_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));

    } else {
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        //this->setStyleSheet(QString() + ;

        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Save->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Recover->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");
        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Save->setIcon(QIcon(":/resource/white_theme/save_w.png"));
        ui->pushButton_Recover->setIcon(QIcon(":/resource/white_theme/recover_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));

        //设置所有QLineEdit样式
        QList<QLineEdit *> list_LineEdit = findChildren<QLineEdit *>();
        foreach(QLineEdit *p,list_LineEdit)
        {
            QFont font;
            font.setPointSize(10);
            p->setFont(font);
            p->setStyleSheet("QLineEdit{border-radius:5px; background-color:rgb(245,245,255); color:rgb(1,1,1); qproperty-alignment:AlignHCenter;}");
        }
    }
    //this->setPalette(palette);

    // 设置滚动区域的宽度、高度与图片一致
    QRect rect_label(0, 0, pixmap_img.width(), pixmap_img.height());
    rect_label.setWidth(ui->scrollArea->width() - ui->scrollArea->verticalScrollBar()->width());
    ui->lblImg->setGeometry(rect_label);
    ui->scrollArea->setFrameShape(QFrame::NoFrame);
    ui->scrollArea->widget()->setGeometry(ui->lblImg->geometry());

}

void eyesightstandard::getStandardValue()
{
    paraList = appSetting::value("/standard/para").toStringList();
    qDebug()<<"-----getStandardValue:"<<paraList.size();

    QString strNum;
    strNum = paraList.at(0);
    g_aEyesightStandard[0][1] = strNum.toDouble();
    strNum = paraList.at(1);
    g_aEyesightStandard[1][1] = strNum.toDouble();
    strNum = paraList.at(2);
    g_aEyesightStandard[2][1] = strNum.toDouble();
    strNum = paraList.at(3);
    g_aEyesightStandard[3][1] = strNum.toDouble();
    strNum = paraList.at(4);
    g_aEyesightStandard[4][2] = strNum.toDouble();
    strNum = paraList.at(5);
    g_aEyesightStandard[0][2] = strNum.toDouble();
    strNum = paraList.at(6);
    g_aEyesightStandard[1][2] = strNum.toDouble();
    strNum = paraList.at(7);
    g_aEyesightStandard[2][2] = strNum.toDouble();
    strNum = paraList.at(8);
    g_aEyesightStandard[3][2] = strNum.toDouble();
    strNum = paraList.at(9);
    g_aEyesightStandard[4][2] = strNum.toDouble();
    strNum = paraList.at(10);
    g_aEyesightStandard[0][3] = strNum.toDouble();
    strNum = paraList.at(11);
    g_aEyesightStandard[1][3] = strNum.toDouble();
    strNum = paraList.at(12);
    g_aEyesightStandard[2][3] = strNum.toDouble();
    strNum = paraList.at(13);
    g_aEyesightStandard[3][3] = strNum.toDouble();
    strNum = paraList.at(14);
    g_aEyesightStandard[4][3] = strNum.toDouble();
    for(int i=0; i<15; i++)
        qDebug()<<"-----:"<<paraList.at(i);
}

//判断参数合法性
bool eyesightstandard::Correctness_of_judgment()
{
    if(ui->EditLine_1->text().toDouble()>=-5.0 && ui->EditLine_1->text().toDouble()<=10.0 && ui->EditLine_2->text().toDouble()>=-5.0 && ui->EditLine_2->text().toDouble()<=10.0 && ui->EditLine_3->text().toDouble()>=-5.0 && ui->EditLine_3->text().toDouble()<=10.0 &&\
       ui->EditLine_4->text().toDouble()>=-5.0 && ui->EditLine_4->text().toDouble()<=10.0 && ui->EditLine_5->text().toDouble()>=-5.0 && ui->EditLine_5->text().toDouble()<=10.0 && ui->EditLine_6->text().toDouble()>=-5.0 && ui->EditLine_6->text().toDouble()<=10.0 &&\
       ui->EditLine_7->text().toDouble()>=-5.0 && ui->EditLine_7->text().toDouble()<=10.0 && ui->EditLine_8->text().toDouble()>=-5.0 && ui->EditLine_8->text().toDouble()<=10.0 && ui->EditLine_9->text().toDouble()>=-5.0 && ui->EditLine_9->text().toDouble()<=10.0 &&\
       ui->EditLine_10->text().toDouble()>=-5.0 && ui->EditLine_10->text().toDouble()<=10.0 && ui->EditLine_11->text().toDouble()>=-5.0 && ui->EditLine_11->text().toDouble()<=10.0 && ui->EditLine_12->text().toDouble()>=-5.0 && ui->EditLine_12->text().toDouble()<=10.0 &&\
       ui->EditLine_13->text().toDouble()>=-5.0 && ui->EditLine_13->text().toDouble()<=10.0 && ui->EditLine_14->text().toDouble()>=-5.0 && ui->EditLine_14->text().toDouble()<=10.0 && ui->EditLine_15->text().toDouble()>=-5.0 && ui->EditLine_15->text().toDouble()<=10.0 &&\
       ui->EditLine_11->text().toDouble()-ui->EditLine_6->text().toDouble()>0 && ui->EditLine_12->text().toDouble()-ui->EditLine_7->text().toDouble()>0 && ui->EditLine_13->text().toDouble()-ui->EditLine_8->text().toDouble()>0 &&\
       ui->EditLine_14->text().toDouble()-ui->EditLine_9->text().toDouble()>0 && ui->EditLine_15->text().toDouble()-ui->EditLine_10->text().toDouble()>0)
    {
        return true;
    }
    else
        return false;
}

void eyesightstandard::setEditLineValue()   //更新EditLine
{
    //散光
    ui->EditLine_1->setText(paraList.at(0));    //6-12月
    ui->EditLine_2->setText(paraList.at(1));    //12-36月
    ui->EditLine_3->setText(paraList.at(2));    //3-6岁
    ui->EditLine_4->setText(paraList.at(3));    //6-20岁
    ui->EditLine_5->setText(paraList.at(4));    //20-100岁
    //近视
    ui->EditLine_6->setText(paraList.at(5));    //6-12月
    ui->EditLine_7->setText(paraList.at(6));    //12-36月
    ui->EditLine_8->setText(paraList.at(7));    //3-6岁
    ui->EditLine_9->setText(paraList.at(8));    //6-20岁
    ui->EditLine_10->setText(paraList.at(9));   //20-100岁
    //远视
    ui->EditLine_11->setText(paraList.at(10));  //6-12月
    ui->EditLine_12->setText(paraList.at(11));  //12-36月
    ui->EditLine_13->setText(paraList.at(12));  //3-6岁
    ui->EditLine_14->setText(paraList.at(13));  //6-20岁
    ui->EditLine_15->setText(paraList.at(14));  //20-100岁
}

void eyesightstandard::Save_prompt_dialog()
{
    QString text = tr("是否保存设置？");   // "Do you want to save Settings?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if(msg.exec() == QDialog::Accepted)
    {
        paraList.clear();
        paraList.push_back(ui->EditLine_1->text());
        paraList.push_back(ui->EditLine_2->text());
        paraList.push_back(ui->EditLine_3->text());
        paraList.push_back(ui->EditLine_4->text());
        paraList.push_back(ui->EditLine_5->text());
        paraList.push_back(ui->EditLine_6->text());
        paraList.push_back(ui->EditLine_7->text());
        paraList.push_back(ui->EditLine_8->text());
        paraList.push_back(ui->EditLine_9->text());
        paraList.push_back(ui->EditLine_10->text());
        paraList.push_back(ui->EditLine_11->text());
        paraList.push_back(ui->EditLine_12->text());
        paraList.push_back(ui->EditLine_13->text());
        paraList.push_back(ui->EditLine_14->text());
        paraList.push_back(ui->EditLine_15->text());
        appSetting::setValue("/standard/para",paraList);
    }
}

void eyesightstandard::on_pushButton_Home_clicked()
{
    getStandardValue();     //更新获取本地保存的参数
    if(paraList.at(0)==ui->EditLine_1->text() && paraList.at(1)==ui->EditLine_2->text() && paraList.at(2)==ui->EditLine_3->text() && paraList.at(3)==ui->EditLine_4->text() && paraList.at(4)==ui->EditLine_5->text() && paraList.at(5)==ui->EditLine_6->text() &&\
       paraList.at(6)==ui->EditLine_7->text() && paraList.at(7)==ui->EditLine_8->text() && paraList.at(8)==ui->EditLine_9->text() && paraList.at(9)==ui->EditLine_10->text() && paraList.at(10)==ui->EditLine_11->text() && paraList.at(11)==ui->EditLine_12->text() &&\
       paraList.at(12)==ui->EditLine_13->text() && paraList.at(13)==ui->EditLine_14->text() && paraList.at(14)==ui->EditLine_15->text())
    {
        getWinManage()->showWindowByType(WIN_HOME);
    }
    else
    {
        if(Correctness_of_judgment())
        {
            Save_prompt_dialog();
            getStandardValue();     //更新获取本地保存的参数
            setEditLineValue();
            getWinManage()->showWindowByType(WIN_HOME);
        }
        else
        {
            QString text = tr("标准范围:-5.0~10.0\n近视值应小于远视值!");    // "Standard range: -5.0~10.0\nThe myopia value should be less than the hyperopia value!"
            NoticeWin msg;
            msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
            msg.setContent(text);
            if(msg.exec() == QDialog::Accepted)
            {
                setEditLineValue();
                getWinManage()->showWindowByType(WIN_HOME);
            }
        }
    }
}

void eyesightstandard::on_pushButton_Save_clicked()
{
    if(!Correctness_of_judgment())
    {
        QString text = tr("标准范围:-5.0~10.0\n近视值应小于远视值!");    // "Standard range: -5.0~10.0\nThe myopia value should be less than the hyperopia value!"
        NoticeWin msg;
        msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
        msg.setContent(text);
        if(msg.exec() == QDialog::Accepted){}
        return;
    }
    Save_prompt_dialog();
}

void eyesightstandard::on_pushButton_Back_clicked()
{
    //this->hide();
    getStandardValue();     //更新获取本地保存的参数
    if(paraList.at(0)==ui->EditLine_1->text() && paraList.at(1)==ui->EditLine_2->text() && paraList.at(2)==ui->EditLine_3->text() && paraList.at(3)==ui->EditLine_4->text() && paraList.at(4)==ui->EditLine_5->text() && paraList.at(5)==ui->EditLine_6->text() &&\
       paraList.at(6)==ui->EditLine_7->text() && paraList.at(7)==ui->EditLine_8->text() && paraList.at(8)==ui->EditLine_9->text() && paraList.at(9)==ui->EditLine_10->text() && paraList.at(10)==ui->EditLine_11->text() && paraList.at(11)==ui->EditLine_12->text() &&\
       paraList.at(12)==ui->EditLine_13->text() && paraList.at(13)==ui->EditLine_14->text() && paraList.at(14)==ui->EditLine_15->text())
    {
        getWinManage()->backToLastWidget();
    }
    else
    {
        if(Correctness_of_judgment())
        {
            Save_prompt_dialog();
            getStandardValue();     //更新获取本地保存的参数
            setEditLineValue();
            getWinManage()->backToLastWidget();
        }
        else
        {
            QString text = tr("标准范围:-5.0~10.0\n近视值应小于远视值!");    // "Standard range: -5.0~10.0\nThe myopia value should be less than the hyperopia value!"
            NoticeWin msg;
            msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
            msg.setContent(text);
            if(msg.exec() == QDialog::Accepted)
            {
                setEditLineValue();
                getWinManage()->backToLastWidget();
            }
        }
    }
}

//一键恢复
void eyesightstandard::on_pushButton_Recover_clicked()
{
    QString text = tr("是否恢复默认参数？"); // "Do you want to restore the default parameters?"
    NoticeWin msg;
    msg.setWindowModality(Qt::ApplicationModal);        //阻塞msg以外的所以窗体
    msg.setContent(text);
    if(msg.exec() == QDialog::Accepted)
    {
        setDefaultParams();
        getStandardValue();     //更新获取本地保存的参数
        setEditLineValue();
    }
}
