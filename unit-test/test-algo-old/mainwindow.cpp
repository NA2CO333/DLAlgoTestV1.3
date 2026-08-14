#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <iostream>

#include <QFileDialog>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTextStream>

#include "opencv2/core/core.hpp"
#include "opencv2/opencv.hpp"
//#include "opencv2/highgui/highgui.hpp"
#include "opencv2/imgproc/imgproc.hpp"

#include "util-common.h"
#include "formmsgviewer.h"

#include "algoobj.h"

#include "visionmeasure.h"

//
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    appSetting::init("setting");

    mVisionMeasure = new CVisionMeasure();

//    Mat image = cv::imread("/root/work/screener-unit-test/bin/satur_204551514_ae-10973_d-944_g-0_p-0_expo-N.bmp");
//    //Mat image = cv::imread("/root/debug/img_gauss.bmp");
//    //imshow("SoureImage",image);
//    cvtColor(image,image,CV_RGB2GRAY);
//    Mat imageOutput;
//    Mat imageOtsu;
//    int thresholdValue=OtsuAlgThreshold(image);
//    qDebug()<<"类间方差为： "<<thresholdValue<<endl;
//    cv::threshold(image,imageOutput,thresholdValue,255,CV_THRESH_BINARY);
//    cv::threshold(image,imageOtsu,0,255,CV_THRESH_OTSU); //Opencv Otsu算法

//    cv::imwrite("/root/debug/OutputImage.bmp", imageOutput);
//    cv::imwrite("/root/debug/OpencvOtsu.bmp", imageOtsu);

    for (int i = algoVerAll_Min; i <= algoVerAll_Max; i++) {
        ui->cbbDetectAlgoVer->addItem(CGlobal::pupilAlgoVerDesc[i], i);
    }

}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showEvent(QShowEvent *)
{
    loadEdtVal(ui->edtImgPath);
    loadEdtVal(ui->edtDirPath);
    loadCbbVal(ui->cbbDetectAlgoVer);
    loadRdbVal(ui->rbtnCalcNew);
    loadRdbVal(ui->rbtnCalcOld);
}

// QLineEdit 值存取
void MainWindow::saveEdtVal(QLineEdit *_edt)
{
    setting->setValue(_edt->objectName(), _edt->text());
}

void MainWindow::loadEdtVal(QLineEdit *_edt)
{
    _edt->setText(setting->value(_edt->objectName()).toString());
}

// QComboBox 值存取
void MainWindow::saveCbbVal(QComboBox *_cbb)
{
    setting->setValue(_cbb->objectName(), _cbb->currentIndex());
}

void MainWindow::loadCbbVal(QComboBox *_cbb)
{
    _cbb->setCurrentIndex(setting->value(_cbb->objectName()).toInt());
}

// QCheckBox 值存取
void MainWindow::saveCkbVal(QCheckBox *_ckb)
{
    setting->setValue(_ckb->objectName(), _ckb->isChecked());
}

void MainWindow::loadCkbVal(QCheckBox *_ckb)
{
    _ckb->setChecked(setting->value(_ckb->objectName()).toBool());
}

// QRadioButton 值存取
void MainWindow::saveRdbVal(QRadioButton *_rdb)
{
    setting->setValue(_rdb->objectName(), _rdb->isChecked());
}

void MainWindow::loadRdbVal(QRadioButton *_rdb)
{
    _rdb->setChecked(setting->value(_rdb->objectName()).toBool());
}

//
void MainWindow::slotAboutToExit()
{
#ifndef RUN_IN_DESKTOP
    //system("echo 0 > /sys/class/backlight/pwm-backlight/brightness");
#endif

}

// 选择文件
void MainWindow::on_btnSelPath_clicked()
{
    QString path = ui->edtImgPath->text();
    if (path.length() > 0) {
        int idx = path.lastIndexOf(QDir::separator());
        if (idx >= 0) {
            path = path.left(idx);
        }
    }
    QStringList file_list = Util::selectFile(this, "BMP, PNG, JPG, .csv, .txt (*.bmp *.png *.jpg *.csv *.txt)", false, path);
    if (file_list.size() > 0) {
        ui->edtImgPath->setText(file_list[0]);
    }
}

void MainWindow::on_btnDetectPupil_clicked()
{
    // 瞳孔检测
    saveEdtVal(ui->edtImgPath);
    saveCbbVal(ui->cbbDetectAlgoVer);

    QString file_path = ui->edtImgPath->text();
    if (file_path.length() > 0) {
        bool ret = mVisionMeasure->detectPupilOfImg(file_path, (enAlgoVerAll)ui->cbbDetectAlgoVer->currentIndex(), -1, ui->ckbCalcExposure->isChecked());
        QMessageBox::information(this, "result", ret ? "Succeeded" : "Failed", QMessageBox::Ok);
    }
}

void MainWindow::on_btnDetectPupilAll_clicked()
{
    // 检测文件夹里所有图片的瞳孔
    saveEdtVal(ui->edtDirPath);
    saveCbbVal(ui->cbbDetectAlgoVer);

    QString dir_path = ui->edtDirPath->text();
    if (dir_path.length() > 0 && QFile::exists(dir_path)) {
        int count_succ, count_fail;
        mVisionMeasure->detectPupilOfImgsOfDir(dir_path, (enAlgoVerAll)ui->cbbDetectAlgoVer->currentIndex(), count_succ, count_fail);
        QMessageBox::information(this, "result", QString::asprintf("Succ: %d, Fail: %d", count_succ, count_fail), QMessageBox::Ok);
    } else {
        QMessageBox::warning(this, "error", "dir not exists!", QMessageBox::Ok);
    }
}

void MainWindow::on_btnSelDir_clicked()
{
    // 选择文件夹
    QString dir_path = Util::selectDir(this, ui->edtDirPath->text());
    if (dir_path.length() > 0) {
        ui->edtDirPath->setText(dir_path);
    }
}

void MainWindow::on_btnCalcVision_clicked()
{
    // 计算屈光度
    saveEdtVal(ui->edtDirPath);
    saveCbbVal(ui->cbbDetectAlgoVer);

    QString dir_path = ui->edtDirPath->text();
    if (dir_path.length() > 0 && QFile::exists(dir_path)) {
        bool ret = mVisionMeasure->calcVisionOfImgs(dir_path,
                                                   (enAlgoVerAll)ui->cbbDetectAlgoVer->currentIndex(),
                                                   (enAlgoVerAll)(ui->rbtnCalcOld->isChecked() ? algoVerAll_2019 : algoVerAll_2021_07),
                                                   (ui->ckbIsSingleThread->isChecked())
                                                   );
        QMessageBox::information(this, "result", ret ? "Succeeded" : "Failed", QMessageBox::Ok);
    } else {
        QMessageBox::warning(this, "error", "dir not exists!", QMessageBox::Ok);
    }
}

void MainWindow::on_btnTest_clicked()
{
    // Test
    QString file_path = ui->edtImgPath->text();
    if (!QFile::exists(file_path))
        return;

    IplImage *img = cvLoadImage(file_path.toLatin1().data(), CV_LOAD_IMAGE_GRAYSCALE);
    CvRect rect = cvRect(10, 10, 50, 50);
    cvSetImageROI(img, rect);
    CvRect rect_roi = cvGetImageROI(img);
    //cvSaveImage("/root/debug/test_roi.bmp", img);
    cvReleaseImage(&img);

}

void MainWindow::on_btnCalcAverageGrey_clicked()
{
    // Calc Average Grey
    QString dir_path = ui->edtDirPath->text();
    if (dir_path.length() == 0) {
        QMessageBox::warning(this, "error", "set dir path first", QMessageBox::Ok);
        return;
    }

    QDir dir(dir_path);
    if (!dir.exists()) {
        QMessageBox::warning(this, "error", "dir not exists", QMessageBox::Ok);
        return;
    }

    // connect database
    QString db_file_path = dir_path + QDir::separator() + "patients.db";
    if (!QFile::exists(db_file_path)) {
        QMessageBox::warning(this, "error", "database not found", QMessageBox::Ok);
        return;
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(db_file_path);
    db.open();
    QString sql_str = "select patientid, patienttesttime, creattime from HistroyPatients where patientid = ?";
    //QString sql_str = "select patientid, patienttesttime, creattime from HistroyPatients where patienttesttime is not null order by patienttesttime asc";

    // open output file     // TODO: 把灰度值写入数据库更方便统计分析？
    QFile file(dir_path + QDir::separator() + "AverageGrey.csv");
    file.open(QFile::WriteOnly | QFile::Truncate);
    QTextStream stream(&file);

    stream << "MeasureTime,CreationTime,Number,AverageGrey\r\n";

    // 遍历所有存图文件夹
    dir_path = dir_path + QDir::separator() + "media" + QDir::separator() + "photo";
    dir.setPath(dir_path);
    QFileInfoList dir_info_list = dir.entryInfoList(QDir::Dirs);
    QFileInfoList file_info_list;
    QString dir_name = "";
    QString dir_sub_path = "";
    QDir dir_sub;
    QString measure_time = "";
    QString create_time = "";
    //int record_size;
    QString file_name = "";
    QString file_path = "";
    float average_grey = 0;

    for (QFileInfo dir_info : dir_info_list) {
        if (!dir_info.isDir())
            continue;

        dir_name = dir_info.fileName();
        if (dir_name == "." || dir_name == "..")
            continue;

        // 查询测量时间
        measure_time = "";
        create_time = "";
        QSqlQuery sql_query;
        sql_query.prepare(sql_str);
        sql_query.addBindValue(dir_name);
        if (!sql_query.exec())
            continue;
        //record_size = sql_query.size();
        //if (record_size <= 0) // TODO: 无法得到 size 值？
        //    continue;
        if (!sql_query.first())
            continue;
        measure_time = sql_query.value(1).toString().mid(11);
        create_time = sql_query.value(2).toString().mid(11);

        // 选择图像文件
        file_name = "";
        dir_sub_path = dir_path + QDir::separator() + dir_name;
        dir_sub.setPath(dir_sub_path);
        file_info_list = dir_sub.entryInfoList(QDir::Files);
        foreach (QFileInfo file_info, file_info_list) {
            if (file_info.fileName().endsWith(".bmp")) {
                file_name = file_info.fileName();
                break;
            }
        }
        if (file_name.length() == 0)
            continue;

        // 计算平均灰度
        file_path = dir_sub_path + QDir::separator() + file_name;
        cv::Mat img = cv::imread(file_path.toStdString(), cv::IMREAD_GRAYSCALE);
        average_grey = Util::getAverageGrayRough(img);

        //
        stream << measure_time << "," << create_time << "," << dir_name << "," << QString::number(average_grey, 'g', 2) << "\r\n";
    }

    //
    stream.flush();
    file.flush();
    file.close();

    //
    db.close();

    //
    QMessageBox::information(this, "notice", "finished", QMessageBox::Ok);
}

// Led Posi Detect
void MainWindow::on_btnLedPosiDetect_clicked()
{
    QString dir_path = ui->edtDirPath->text();
    if (dir_path.length() > 0) {
        bool ret = mVisionMeasure->ledPosiDetect(dir_path, (enAlgoVerAll)ui->cbbDetectAlgoVer->currentIndex(), (enAlgoVerAll)(ui->rbtnCalcOld->isChecked() ? algoVerAll_2019 : algoVerAll_2021_07));
        QMessageBox::information(this, "result", ret ? "Succeeded" : "Failed", QMessageBox::Ok);
    }
}

// Calc Average Gray
void MainWindow::on_btnCalcImgGrey_clicked()
{
    QString file_path = ui->edtImgPath->text();
    if (file_path.length() > 0) {
        float gray = Util::getAverageGrayRough(file_path);
        QMessageBox::information(this, "result", QString::number(gray, 'f', 2), QMessageBox::Ok);
    }
}

// 计算对比度
void MainWindow::on_btnCalcContrast_clicked()
{
    QString file_path = ui->edtImgPath->text();
    if (file_path.length() > 0) {
        cv::Mat mat = cv::imread(file_path.toLocal8Bit().data(), cv::IMREAD_GRAYSCALE);

        double mean, std_dev;
        CAlgoObj::calcMeanStdDev(mat, mean, std_dev);
        QString msg = QString::number(mean, 'f', 2) + ", " + QString::number(std_dev, 'f', 2);
        qDebug() << "Contrast Info: " << msg;
        QMessageBox::information(this, "result", msg, QMessageBox::Ok);
    }
}

// 设置对比度
void MainWindow::on_btnSetContrast_clicked()
{

}

// 由采样数据集找出清晰度最高时的距离值
void calcDistOfMaxClarity()
{
    const int len_arr = 152;

    QVector<struct stDistClarity> dataList;
    //dataList.reserve(len_arr);

    //std::cout << "init data" << std::endl;
/*
    dataList.append({0.22, 1449});
    dataList.append({0.22, 1450});
    dataList.append({0.22, 1461});
    dataList.append({0.22, 1439});
    dataList.append({0.22, 1440});
    dataList.append({0.22, 1448});
    dataList.append({0.22, 1440});
    dataList.append({0.22, 1439});
    dataList.append({0.22, 1440});
    dataList.append({0.21, 1447});
    dataList.append({0.22, 1456});
    dataList.append({0.21, 1440});
    dataList.append({0.26, 1450});
    dataList.append({0.24, 1463});
    dataList.append({0.22, 1030});
    dataList.append({0.21, 1038});
    dataList.append({0.22, 1041});
    dataList.append({0.21, 1052});
    dataList.append({0.21, 1071});
    dataList.append({0.21, 1064});
    dataList.append({0.21, 1052});
    dataList.append({0.21, 1045});
    dataList.append({0.21, 1055});
    dataList.append({0.22, 1072});
    dataList.append({0.21, 1084});
    dataList.append({0.21, 1084});
    dataList.append({0.22, 1095});
    dataList.append({0.21, 1105});
    dataList.append({0.21, 1110});
    dataList.append({0.21, 1117});
    dataList.append({0.21, 1125});
    dataList.append({0.21, 1132});
    dataList.append({0.21, 1136});
    dataList.append({0.21, 1135});
    dataList.append({0.20, 1140});
    dataList.append({0.20, 1144});
    dataList.append({0.20, 1158});
    dataList.append({0.20, 1164});
    dataList.append({0.20, 1168});
    dataList.append({0.20, 1174});
    dataList.append({0.21, 1176});
    dataList.append({0.21, 1171});
    dataList.append({0.21, 1183});
    dataList.append({0.21, 1581});
    dataList.append({0.20, 1190});
    dataList.append({0.20, 1195});
    dataList.append({0.21, 1193});
    dataList.append({0.20, 1201});
    dataList.append({0.21, 1197});
    dataList.append({0.21, 1192});
    dataList.append({0.21, 1201});
    dataList.append({0.20, 1200});
    dataList.append({0.20, 1204});
    dataList.append({0.20, 1191});
    dataList.append({0.20, 1207});
    dataList.append({0.20, 1201});
    dataList.append({0.20, 1203});
    dataList.append({0.20, 1212});
    dataList.append({0.21, 1219});
    dataList.append({0.20, 1215});
    dataList.append({0.20, 1210});
    dataList.append({0.20, 1193});
    dataList.append({0.21, 1187});
    dataList.append({0.20, 1169});
    dataList.append({0.21, 1158});
    dataList.append({0.20, 1147});
    dataList.append({0.20, 1146});
    dataList.append({0.21, 1129});
    dataList.append({0.21, 1116});
    dataList.append({0.21, 1107});
    dataList.append({0.21, 1098});
    dataList.append({0.22, 1084});
    dataList.append({0.22, 1087});
    dataList.append({0.22, 1070});
    dataList.append({0.21, 1067});
    dataList.append({0.22, 1063});
    dataList.append({0.21, 1055});
    dataList.append({0.22, 1049});
    dataList.append({0.22, 1046});
    dataList.append({0.22, 1041});
    dataList.append({0.22, 1047});
    dataList.append({0.22, 1041});
    dataList.append({0.22, 1032});
    dataList.append({0.23, 1018});
    dataList.append({0.22, 1024});
    dataList.append({0.24, 1010});
    dataList.append({0.24, 1003});
    dataList.append({0.24,  995});
    dataList.append({0.24,  994});
    dataList.append({0.25,  998});
    dataList.append({0.26,  989});
    dataList.append({0.25,  992});
    dataList.append({0.24,  988});
    dataList.append({0.24,  992});
    dataList.append({0.25,  981});
    dataList.append({0.25,  984});
    dataList.append({0.26,  978});
    dataList.append({0.27,  978});
    dataList.append({0.27,  975});
    dataList.append({0.28,  974});
    dataList.append({0.26,  966});
    dataList.append({0.24,  966});
    dataList.append({0.26,  954});
    dataList.append({0.25,  957});
    dataList.append({0.24,  955});
    dataList.append({0.23,  946});
    dataList.append({0.23,  942});
    dataList.append({0.23,  943});
    dataList.append({0.22,  931});
    dataList.append({0.22,  932});
    dataList.append({0.22,  925});
    dataList.append({0.20,  917});
    dataList.append({0.21,  912});
    dataList.append({0.21,  907});
    dataList.append({0.22,  902});
    dataList.append({0.21,  900});
    dataList.append({0.21,  889});
    dataList.append({0.21,  889});
    dataList.append({0.21,  877});
    dataList.append({0.20,  871});
    dataList.append({0.21,  881});
    dataList.append({0.21,  889});
    dataList.append({0.21,  907});
    dataList.append({0.22,  899});
    dataList.append({0.22,  913});
    dataList.append({0.22,  914});
    dataList.append({0.22,  924});
    dataList.append({0.22,  930});
    dataList.append({0.22,  928});
    dataList.append({0.22,  941});
    dataList.append({0.24,  954});
    dataList.append({0.26,  955});
    dataList.append({0.28,  963});
    dataList.append({0.27,  975});
    dataList.append({0.26,  973});
    dataList.append({0.27,  974});
    dataList.append({0.26,  973});
    dataList.append({0.25,  980});
    dataList.append({0.27,  985});
    dataList.append({0.26,  981});
    dataList.append({0.27,  983});
    dataList.append({0.26,  987});
    dataList.append({0.26,  987});
    dataList.append({0.26,  993});
    dataList.append({0.25,  991});
    dataList.append({0.26,  987});
    dataList.append({0.25,  985});
    dataList.append({0.23,  988});
    dataList.append({0.22,  979});
    dataList.append({0.24,  986});
    dataList.append({0.24,  984});
    dataList.append({0.24,  984});
*/

/*
    dataList.append(stDistClarity({0.22, 1449}));
    dataList.append(stDistClarity({0.22, 1450}));
    dataList.append(stDistClarity({0.22, 1461}));
    dataList.append(stDistClarity({0.22, 1439}));
    dataList.append(stDistClarity({0.22, 1440}));
    dataList.append(stDistClarity({0.22, 1448}));
    dataList.append(stDistClarity({0.22, 1440}));
    dataList.append(stDistClarity({0.22, 1439}));
    dataList.append(stDistClarity({0.22, 1440}));
    dataList.append(stDistClarity({0.21, 1447}));
    dataList.append(stDistClarity({0.22, 1456}));
    dataList.append(stDistClarity({0.21, 1440}));
    dataList.append(stDistClarity({0.26, 1450}));
    dataList.append(stDistClarity({0.24, 1463}));
    dataList.append(stDistClarity({0.22, 1030}));
    dataList.append(stDistClarity({0.21, 1038}));
    dataList.append(stDistClarity({0.22, 1041}));
    dataList.append(stDistClarity({0.21, 1052}));
    dataList.append(stDistClarity({0.21, 1071}));
    dataList.append(stDistClarity({0.21, 1064}));
    dataList.append(stDistClarity({0.21, 1052}));
    dataList.append(stDistClarity({0.21, 1045}));
    dataList.append(stDistClarity({0.21, 1055}));
    dataList.append(stDistClarity({0.22, 1072}));
    dataList.append(stDistClarity({0.21, 1084}));
    dataList.append(stDistClarity({0.21, 1084}));
    dataList.append(stDistClarity({0.22, 1095}));
    dataList.append(stDistClarity({0.21, 1105}));
    dataList.append(stDistClarity({0.21, 1110}));
    dataList.append(stDistClarity({0.21, 1117}));
    dataList.append(stDistClarity({0.21, 1125}));
    dataList.append(stDistClarity({0.21, 1132}));
    dataList.append(stDistClarity({0.21, 1136}));
    dataList.append(stDistClarity({0.21, 1135}));
    dataList.append(stDistClarity({0.20, 1140}));
    dataList.append(stDistClarity({0.20, 1144}));
    dataList.append(stDistClarity({0.20, 1158}));
    dataList.append(stDistClarity({0.20, 1164}));
    dataList.append(stDistClarity({0.20, 1168}));
    dataList.append(stDistClarity({0.20, 1174}));
    dataList.append(stDistClarity({0.21, 1176}));
    dataList.append(stDistClarity({0.21, 1171}));
    dataList.append(stDistClarity({0.21, 1183}));
    dataList.append(stDistClarity({0.21, 1581}));
    dataList.append(stDistClarity({0.20, 1190}));
    dataList.append(stDistClarity({0.20, 1195}));
    dataList.append(stDistClarity({0.21, 1193}));
    dataList.append(stDistClarity({0.20, 1201}));
    dataList.append(stDistClarity({0.21, 1197}));
    dataList.append(stDistClarity({0.21, 1192}));
    dataList.append(stDistClarity({0.21, 1201}));
    dataList.append(stDistClarity({0.20, 1200}));
    dataList.append(stDistClarity({0.20, 1204}));
    dataList.append(stDistClarity({0.20, 1191}));
    dataList.append(stDistClarity({0.20, 1207}));
    dataList.append(stDistClarity({0.20, 1201}));
    dataList.append(stDistClarity({0.20, 1203}));
    dataList.append(stDistClarity({0.20, 1212}));
    dataList.append(stDistClarity({0.21, 1219}));
    dataList.append(stDistClarity({0.20, 1215}));
    dataList.append(stDistClarity({0.20, 1210}));
    dataList.append(stDistClarity({0.20, 1193}));
    dataList.append(stDistClarity({0.21, 1187}));
    dataList.append(stDistClarity({0.20, 1169}));
    dataList.append(stDistClarity({0.21, 1158}));
    dataList.append(stDistClarity({0.20, 1147}));
    dataList.append(stDistClarity({0.20, 1146}));
    dataList.append(stDistClarity({0.21, 1129}));
    dataList.append(stDistClarity({0.21, 1116}));
    dataList.append(stDistClarity({0.21, 1107}));
    dataList.append(stDistClarity({0.21, 1098}));
    dataList.append(stDistClarity({0.22, 1084}));
    dataList.append(stDistClarity({0.22, 1087}));
    dataList.append(stDistClarity({0.22, 1070}));
    dataList.append(stDistClarity({0.21, 1067}));
    dataList.append(stDistClarity({0.22, 1063}));
    dataList.append(stDistClarity({0.21, 1055}));
    dataList.append(stDistClarity({0.22, 1049}));
    dataList.append(stDistClarity({0.22, 1046}));
    dataList.append(stDistClarity({0.22, 1041}));
    dataList.append(stDistClarity({0.22, 1047}));
    dataList.append(stDistClarity({0.22, 1041}));
    dataList.append(stDistClarity({0.22, 1032}));
    dataList.append(stDistClarity({0.23, 1018}));
    dataList.append(stDistClarity({0.22, 1024}));
    dataList.append(stDistClarity({0.24, 1010}));
    dataList.append(stDistClarity({0.24, 1003}));
    dataList.append(stDistClarity({0.24,  995}));
    dataList.append(stDistClarity({0.24,  994}));
    dataList.append(stDistClarity({0.25,  998}));
    dataList.append(stDistClarity({0.26,  989}));
    dataList.append(stDistClarity({0.25,  992}));
    dataList.append(stDistClarity({0.24,  988}));
    dataList.append(stDistClarity({0.24,  992}));
    dataList.append(stDistClarity({0.25,  981}));
    dataList.append(stDistClarity({0.25,  984}));
    dataList.append(stDistClarity({0.26,  978}));
    dataList.append(stDistClarity({0.27,  978}));
    dataList.append(stDistClarity({0.27,  975}));
    dataList.append(stDistClarity({0.28,  974}));
    dataList.append(stDistClarity({0.26,  966}));
    dataList.append(stDistClarity({0.24,  966}));
    dataList.append(stDistClarity({0.26,  954}));
    dataList.append(stDistClarity({0.25,  957}));
    dataList.append(stDistClarity({0.24,  955}));
    dataList.append(stDistClarity({0.23,  946}));
    dataList.append(stDistClarity({0.23,  942}));
    dataList.append(stDistClarity({0.23,  943}));
    dataList.append(stDistClarity({0.22,  931}));
    dataList.append(stDistClarity({0.22,  932}));
    dataList.append(stDistClarity({0.22,  925}));
    dataList.append(stDistClarity({0.20,  917}));
    dataList.append(stDistClarity({0.21,  912}));
    dataList.append(stDistClarity({0.21,  907}));
    dataList.append(stDistClarity({0.22,  902}));
    dataList.append(stDistClarity({0.21,  900}));
    dataList.append(stDistClarity({0.21,  889}));
    dataList.append(stDistClarity({0.21,  889}));
    dataList.append(stDistClarity({0.21,  877}));
    dataList.append(stDistClarity({0.20,  871}));
    dataList.append(stDistClarity({0.21,  881}));
    dataList.append(stDistClarity({0.21,  889}));
    dataList.append(stDistClarity({0.21,  907}));
    dataList.append(stDistClarity({0.22,  899}));
    dataList.append(stDistClarity({0.22,  913}));
    dataList.append(stDistClarity({0.22,  914}));
    dataList.append(stDistClarity({0.22,  924}));
    dataList.append(stDistClarity({0.22,  930}));
    dataList.append(stDistClarity({0.22,  928}));
    dataList.append(stDistClarity({0.22,  941}));
    dataList.append(stDistClarity({0.24,  954}));
    dataList.append(stDistClarity({0.26,  955}));
    dataList.append(stDistClarity({0.28,  963}));
    dataList.append(stDistClarity({0.27,  975}));
    dataList.append(stDistClarity({0.26,  973}));
    dataList.append(stDistClarity({0.27,  974}));
    dataList.append(stDistClarity({0.26,  973}));
    dataList.append(stDistClarity({0.25,  980}));
    dataList.append(stDistClarity({0.27,  985}));
    dataList.append(stDistClarity({0.26,  981}));
    dataList.append(stDistClarity({0.27,  983}));
    dataList.append(stDistClarity({0.26,  987}));
    dataList.append(stDistClarity({0.26,  987}));
    dataList.append(stDistClarity({0.26,  993}));
    dataList.append(stDistClarity({0.25,  991}));
    dataList.append(stDistClarity({0.26,  987}));
    dataList.append(stDistClarity({0.25,  985}));
    dataList.append(stDistClarity({0.23,  988}));
    dataList.append(stDistClarity({0.22,  979}));
    dataList.append(stDistClarity({0.24,  986}));
    dataList.append(stDistClarity({0.24,  984}));
    dataList.append(stDistClarity({0.24,  984}));
*/

/*
    struct stDistClarity dataList[len_arr] = {
        {0.22, 1449},
        {0.22, 1450},
        {0.22, 1461},
        {0.22, 1439},
        {0.22, 1440},
        {0.22, 1448},
        {0.22, 1440},
        {0.22, 1439},
        {0.22, 1440},
        {0.21, 1447},
        {0.22, 1456},
        {0.21, 1440},
        {0.26, 1450},
        {0.24, 1463},
        {0.22, 1030},
        {0.21, 1038},
        {0.22, 1041},
        {0.21, 1052},
        {0.21, 1071},
        {0.21, 1064},
        {0.21, 1052},
        {0.21, 1045},
        {0.21, 1055},
        {0.22, 1072},
        {0.21, 1084},
        {0.21, 1084},
        {0.22, 1095},
        {0.21, 1105},
        {0.21, 1110},
        {0.21, 1117},
        {0.21, 1125},
        {0.21, 1132},
        {0.21, 1136},
        {0.21, 1135},
        {0.20, 1140},
        {0.20, 1144},
        {0.20, 1158},
        {0.20, 1164},
        {0.20, 1168},
        {0.20, 1174},
        {0.21, 1176},
        {0.21, 1171},
        {0.21, 1183},
        {0.21, 1581},
        {0.20, 1190},
        {0.20, 1195},
        {0.21, 1193},
        {0.20, 1201},
        {0.21, 1197},
        {0.21, 1192},
        {0.21, 1201},
        {0.20, 1200},
        {0.20, 1204},
        {0.20, 1191},
        {0.20, 1207},
        {0.20, 1201},
        {0.20, 1203},
        {0.20, 1212},
        {0.21, 1219},
        {0.20, 1215},
        {0.20, 1210},
        {0.20, 1193},
        {0.21, 1187},
        {0.20, 1169},
        {0.21, 1158},
        {0.20, 1147},
        {0.20, 1146},
        {0.21, 1129},
        {0.21, 1116},
        {0.21, 1107},
        {0.21, 1098},
        {0.22, 1084},
        {0.22, 1087},
        {0.22, 1070},
        {0.21, 1067},
        {0.22, 1063},
        {0.21, 1055},
        {0.22, 1049},
        {0.22, 1046},
        {0.22, 1041},
        {0.22, 1047},
        {0.22, 1041},
        {0.22, 1032},
        {0.23, 1018},
        {0.22, 1024},
        {0.24, 1010},
        {0.24, 1003},
        {0.24,  995},
        {0.24,  994},
        {0.25,  998},
        {0.26,  989},
        {0.25,  992},
        {0.24,  988},
        {0.24,  992},
        {0.25,  981},
        {0.25,  984},
        {0.26,  978},
        {0.27,  978},
        {0.27,  975},
        {0.28,  974},
        {0.26,  966},
        {0.24,  966},
        {0.26,  954},
        {0.25,  957},
        {0.24,  955},
        {0.23,  946},
        {0.23,  942},
        {0.23,  943},
        {0.22,  931},
        {0.22,  932},
        {0.22,  925},
        {0.20,  917},
        {0.21,  912},
        {0.21,  907},
        {0.22,  902},
        {0.21,  900},
        {0.21,  889},
        {0.21,  889},
        {0.21,  877},
        {0.20,  871},
        {0.21,  881},
        {0.21,  889},
        {0.21,  907},
        {0.22,  899},
        {0.22,  913},
        {0.22,  914},
        {0.22,  924},
        {0.22,  930},
        {0.22,  928},
        {0.22,  941},
        {0.24,  954},
        {0.26,  955},
        {0.28,  963},
        {0.27,  975},
        {0.26,  973},
        {0.27,  974},
        {0.26,  973},
        {0.25,  980},
        {0.27,  985},
        {0.26,  981},
        {0.27,  983},
        {0.26,  987},
        {0.26,  987},
        {0.26,  993},
        {0.25,  991},
        {0.26,  987},
        {0.25,  985},
        {0.23,  988},
        {0.22,  979},
        {0.24,  986},
        {0.24,  984},
        {0.24,  984}
    };
*/



}

// 计算距离数据的波峰
void MainWindow::on_btnCalcDistDataPeak_clicked()
{
/*
    calcDistOfMaxClarity();
    return;
*/

    QString file_path = ui->edtImgPath->text();
    if (file_path.length() == 0) {
        QMessageBox::information(this, "error", "请先选择文件。", QMessageBox::Ok);
    } else if (!QFile::exists(file_path)) {
        QMessageBox::information(this, "error", "文件不存在！", QMessageBox::Ok);
    } else {
        QFile file(file_path);
        file.open(QFile::ReadOnly);
        QByteArray text = file.readAll();
        file.close();

        CDistCalibration dist_calibration;
        //dist_calibration.reset();
        dist_calibration.setIsStarted(true);

        //qDebug() << "load from file";
        QString line;
        int pos_curr = 0;
        int pos;
        int p_c;
        float clarity;
        int dist;
        do {
            pos = text.indexOf("\r\n", pos_curr);
            if (pos >= 0) {
                line = text.mid(pos_curr, pos - pos_curr);
                //qDebug() << line;

                p_c = line.indexOf(",");
                if (p_c > 0) {
                    clarity = line.mid(0, p_c).toDouble();
                    dist = line.mid(p_c + 1).toInt();
                    //qDebug() << "clarity:" << clarity << ", dist:" << dist;

                    dist_calibration.putDistance(dist, clarity);
                }

                //
                pos_curr = pos + 2;
            }
        } while (pos >= 0);

        dist_calibration.setIsStarted(false);
        int dist_best = dist_calibration.getDistOfMaxClarity();
        QMessageBox::information(this, "result", QString::number(dist_best, 10), QMessageBox::Ok);
    }
}

void MainWindow::on_btnClose_clicked()
{
    QApplication::exit(EXIT_SUCCESS);
}

void MainWindow::on_btnTest2_clicked()
{
    QString file_path = ui->edtImgPath->text();

    if (!QFile::exists(file_path)) {
        QMessageBox::critical(this, "error", "文件不存在！");
        return;
    }

    IplImage *img = cvLoadImage(file_path.toLocal8Bit().data(), CV_LOAD_IMAGE_GRAYSCALE);
    stPupilInfo pupil_info_r, pupil_info_l;
    //memset(&pupil_info_r, 0, sizeof(stPupilInfo));
    //memset(&pupil_info_l, 0, sizeof(stPupilInfo));
    int thresh_val, tag;

    bool succ = false;
    //succ = CAlgoObj::detectPupil_2(img, pupil_info_r, pupil_info_l, thresh_val, tag);
    //succ = CAlgoObj::detectPupil_3(img, pupil_info_r, pupil_info_l, thresh_val, tag);
    // TODO: 这是旧算法代码定义的

    qDebug() << succ;
}

void MainWindow::on_btnFilterLog_clicked()
{
    QString key_word = ui->edtFilterKeyWord->text();
    QString file_path = ui->edtImgPath->text();

    if (key_word.length() == 0) {
        QMessageBox::critical(this, "error", "关键词不能为空！");
        return;
    }
    if (!QFile::exists(file_path)) {
        QMessageBox::critical(this, "error", "文件不存在！");
        return;
    }

    QFile file(file_path);
    file.open(QIODevice::ReadOnly);
    QTextStream text_stream(&file);
    text_stream.setCodec("UTF-8");
    QStringList list_msg;
    QString line_str;
    do {
        line_str = text_stream.readLine();
        if (line_str.contains(key_word)) {
            qDebug() << line_str;
            list_msg.append(line_str);
        }
    } while (!text_stream.atEnd());

    //
    //QMessageBox::information(this, "tips", "finished");

    //
    getMsgViewer()->setText(list_msg);
    getMsgViewer()->show();

}

void MainWindow::on_btnTest3_clicked()
{
    cv::Mat mat_11 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/11.png");
    double mean_11, std_dev_11;
    CAlgoObj::calcMeanStdDev(mat_11, mean_11, std_dev_11);

    cv::Mat mat_12 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/12.png");
    double mean_12, std_dev_12;
    CAlgoObj::calcMeanStdDev(mat_12, mean_12, std_dev_12);

    cv::Mat mat_21 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/21.png");
    double mean_21, std_dev_21;
    CAlgoObj::calcMeanStdDev(mat_21, mean_21, std_dev_21);

    cv::Mat mat_22 = cv::imread("/mnt/hgfs/VMWareShare/screener_docs/Test/高亮度测试/22.png");
    double mean_22, std_dev_22;
    CAlgoObj::calcMeanStdDev(mat_22, mean_22, std_dev_22);

    qDebug() << "mean_11: " << mean_11 << ", mean_12: " << mean_12 << ", mean_21: " << mean_21 << ", mean_22: " << mean_22 << "\r\n";

}

void MainWindow::on_rbtnCalcNew_clicked()
{
    saveRdbVal(ui->rbtnCalcNew);
}

void MainWindow::on_rbtnCalcOld_clicked()
{
    saveRdbVal(ui->rbtnCalcOld);
}

void MainWindow::on_btnTest_processPic0_clicked()
{
    saveEdtVal(ui->edtImgPath);
    saveCbbVal(ui->cbbDetectAlgoVer);

    QString file_path = ui->edtImgPath->text();
    if (file_path.length() > 0) {
        bool ret = mVisionMeasure->test_Runtask_processPic0(file_path, -1, (enAlgoVerAll)ui->cbbDetectAlgoVer->currentIndex());
        QMessageBox::information(this, "result", ret ? "Succeeded" : "Failed", QMessageBox::Ok);
    }
}

void MainWindow::on_btnCalcVisionNew_clicked()
{

}
