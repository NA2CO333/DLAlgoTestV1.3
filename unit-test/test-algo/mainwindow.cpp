#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <iostream>

#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>

#include "util-common.h"
#include "formmsgviewer.h"

#include "algo-invoker.h"
#include "algo.h"

#include "refractionstrategy.h"
#if APP_VER > 10309
#  include "appsetting.h"
#endif

//
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //
    appSetting::init("setting");

    //
    m_algoInvoker = new CAlgoInvoker();

    m_algoInvoker->m_algoIntf->setIsSaveImg(false);
    //
    for (int i = algoVer_Min; i <= algoVer_Max; i++) {
        ui->cbbDetectAlgoVer->addItem(enumToCaption_AlgoVer((enAlgoVer)i), i);
    }

    //QObject::connect(this, &CAlgoInvoker::sigCalcVisionFinished, this, &MainWindow::slotShowResult, Qt::DirectConnection);

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

void MainWindow::saveEdtVal(QLineEdit *_edt)
{
    // QLineEdit 值存取
    appSetting::setValue(_edt->objectName(), _edt->text());
}

void MainWindow::loadEdtVal(QLineEdit *_edt)
{
    _edt->setText(appSetting::value(_edt->objectName()).toString());
}

void MainWindow::saveCbbVal(QComboBox *_cbb)
{
    // QComboBox 值存取
    appSetting::setValue(_cbb->objectName(), _cbb->currentIndex());
}

void MainWindow::loadCbbVal(QComboBox *_cbb)
{
    _cbb->setCurrentIndex(appSetting::value(_cbb->objectName()).toInt());
}

void MainWindow::saveCkbVal(QCheckBox *_ckb)
{
    // QCheckBox 值存取
    appSetting::setValue(_ckb->objectName(), _ckb->isChecked());
}

void MainWindow::loadCkbVal(QCheckBox *_ckb)
{
    _ckb->setChecked(appSetting::value(_ckb->objectName()).toBool());
}

void MainWindow::saveRdbVal(QRadioButton *_rdb)
{
    // QRadioButton 值存取
    appSetting::setValue(_rdb->objectName(), _rdb->isChecked());
}

void MainWindow::loadRdbVal(QRadioButton *_rdb)
{
    _rdb->setChecked(appSetting::value(_rdb->objectName()).toBool());
}

//
void MainWindow::slotAboutToExit()
{
#ifndef RUN_IN_DESKTOP
    //system("echo 0 > /sys/class/backlight/pwm-backlight/brightness");
#endif

}

std::atomic_bool g_done{false};
/* 回调：结果一到就打印 */
void onResult(enCalcResultState st,
             const stVisionValue &v,
             const stVisionAbnormal &ab,
             const std::vector<stVisionValue> &visions )
{
    qDebug() << QString(__PRETTY_FUNCTION__);
    if (st != calcResultState_Succ){
        qDebug() << "!!! calc failed" << st;
    }else{
        qDebug() << "R:" << v.RSph << v.RCyl << v.RAx
                 << "L:" << v.LSph << v.LCyl << v.LAx
                 << "PD:" << v.PD;
    }
    for (const auto &vision : visions) {
        qDebug() << vision.toString(); // 使用toString方法输出
    }
    g_done = true;
}



void MainWindow::on_btnClose_clicked()
{
    QApplication::exit(EXIT_SUCCESS);
}

void MainWindow::on_btnSelFile_clicked()
{
    // 选择文件
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

void MainWindow::on_rbtnCalcNew_clicked()
{
    saveRdbVal(ui->rbtnCalcNew);
}

void MainWindow::on_rbtnCalcOld_clicked()
{
    saveRdbVal(ui->rbtnCalcOld);
}

void MainWindow::on_btnSelDir_clicked()
{
    // 选择文件夹
    QString dir_path = Util::selectDir(this, ui->edtDirPath->text());
    if (dir_path.length() > 0) {
        ui->edtDirPath->setText(dir_path);
    }
}

void MainWindow::on_btnDetectPupil_clicked()
{
    // 瞳孔检测
    saveEdtVal(ui->edtImgPath);
    saveCbbVal(ui->cbbDetectAlgoVer);

    QString file_path = ui->edtImgPath->text();
    if (file_path.length() > 0) {
        bool ret = m_algoInvoker->detectPupilOfImg(file_path, (enAlgoVer)ui->cbbDetectAlgoVer->currentIndex(), -1, ui->ckbCalcExposure->isChecked());
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
        m_algoInvoker->detectPupilOfImgsOfDir(dir_path, (enAlgoVer)ui->cbbDetectAlgoVer->currentIndex(), count_succ, count_fail);
        QMessageBox::information(this, "result", QString::asprintf("Succ: %d, Fail: %d", count_succ, count_fail), QMessageBox::Ok);
    } else {
        QMessageBox::warning(this, "error", "dir not exists!", QMessageBox::Ok);
    }
}

void MainWindow::on_btnCalcVision_clicked()
{
    // 计算屈光度
    saveEdtVal(ui->edtDirPath);
    saveCbbVal(ui->cbbDetectAlgoVer);

    stVisionValue vision;
    memset(&vision, 0, sizeof(stVisionValue));
    stVisionAbnormal vision_abnormal;
    memset(&vision_abnormal, 0, sizeof(stVisionAbnormal));

    QString dir_path = ui->edtDirPath->text();
    if (dir_path.length() > 0 && QFile::exists(dir_path)) {
        bool ret = m_algoInvoker->calcVisionOfImgs(dir_path,
                                                   (enAlgoVer)ui->cbbDetectAlgoVer->currentIndex(),
                                                   (enAlgoVer)(ui->rbtnCalcOld->isChecked() ? algoVer_2022_12 : algoVer_2022_12),
                                                   (ui->ckbIsSingleThread->isChecked()),vision,vision_abnormal);
        qDebug() << "Result:";
        qDebug() << "RSph: " << vision.RSph << " RCyl:" << vision.RCyl << " RAx:" << vision.RAx;
        qDebug() << "LSph: " << vision.LSph << " LCyl:" << vision.LCyl << " LAx:" << vision.LAx;
        QMessageBox::information(this, "result", ret ? "Succeeded" : "Failed", QMessageBox::Ok);
    } else {
        QMessageBox::warning(this, "error", "dir not exists!", QMessageBox::Ok);
    }
}

void MainWindow::on_btnCalcAverageGrey_clicked()
{
    //
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

    // 计算平均灰度
    m_algoInvoker->calcAverageGrey(dir_path);

    //
    QMessageBox::information(this, "notice", "finished", QMessageBox::Ok);
}

void MainWindow::on_btnCalcImgGrey_clicked()
{
    // 计算平均灰度
    QString file_path = ui->edtImgPath->text();
    if (file_path.length() > 0) {
        float gray = Util::getAverageGrayRough(file_path);
        QMessageBox::information(this, "result", QString::number(gray, 'f', 2), QMessageBox::Ok);
    }
}

void MainWindow::on_btnCalcContrast_clicked()
{
    // 计算对比度
    QString file_path = ui->edtImgPath->text();

    double mean, std_dev;
    bool succ = m_algoInvoker->calcContrast(file_path, mean, std_dev);
    if (succ) {
        QString msg = QString::number(mean, 'f', 2) + ", " + QString::number(std_dev, 'f', 2);
        qDebug() << "Contrast Info: " << msg;
        QMessageBox::information(this, "result", msg, QMessageBox::Ok);
    } else {
        QMessageBox::critical(this, "error", "failed");
    }
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


void MainWindow::on_btnTest2_clicked()
{
    QString file_path = ui->edtImgPath->text();

    if (!QFile::exists(file_path)) {
        QMessageBox::critical(this, "error", "文件不存在！");
        return;
    }

    //
    m_algoInvoker->test2(file_path);

}

void MainWindow::on_btnTest3_clicked()
{
    m_algoInvoker->test3();

}

void MainWindow::on_btnCalAll_clicked()
{
    double _prec=0.25;
    static QMutex mutex;
    mutex.lock();

//    QDir dir("/mnt/hgfs/vmware_work/testdata/dataset/新视筛1.5/0419/暗室/photo/");
//    QFile file("/mnt/hgfs/vmware_work/testdata/dataset/0419暗室计算结果.csv");

    QDir dir("/mnt/hgfs/vmware_work/testdata/dataset/新视筛1.5/0419/高照度/photo/");
    QFile file("/mnt/hgfs/vmware_work/testdata/dataset/0419高照度计算结果.csv");

//    QDir dir("/mnt/hgfs/vmware_work/testdata/dataset/视筛箱/photo_20230814_1731/photo/");
//    QFile file("/mnt/hgfs/vmware_work/testdata/dataset/视筛箱计算结果.csv");

    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QTextStream in(&file);
        in << QString("右眼球镜,右眼柱镜,右眼轴位,右眼瞳孔直径,左眼球镜,左眼柱镜,左眼轴位,左眼瞳孔直径,瞳距") << '\n';

        if(dir.exists())
        {
            dir.setFilter(QDir::Dirs|QDir::NoDotAndDotDot); /*设置dir的过滤模式,表示只遍历本文件夹内的子目录*/
            QFileInfoList fileList = dir.entryInfoList(); /*获取本文件夹内所有文件的信息*/
            for(int i=0;i<fileList.count();i++) /*遍历每个子目录*/
            {
                QFileInfo fileInfo = fileList[i];

                QString subDir=fileInfo.absoluteFilePath()+"/succ_pictures/";

                stVisionValue vision;
                memset(&vision, 0, sizeof(stVisionValue));
                stVisionAbnormal vision_abnormal;
                memset(&vision_abnormal, 0, sizeof(stVisionAbnormal));

                bool ret = m_algoInvoker->calcVisionOfImgs(subDir,
                                                       (enAlgoVer)ui->cbbDetectAlgoVer->currentIndex(),
                                                       (enAlgoVer)(ui->rbtnCalcOld->isChecked() ? algoVer_2022_12 : algoVer_2022_12),(ui->ckbIsSingleThread->isChecked()),vision,vision_abnormal);

                QString diopterInfo=QString("%1,%2,%3,%4,%5,%6,%7,%8,%9")
                        .arg(QString::number(Util::roundDouble(vision.RSph,_prec), 'f', 2)).arg(QString::number(Util::roundDouble(vision.RCyl,_prec), 'f', 2))
                        .arg(QString::number(vision.RAx, 'f', 2)).arg(QString::number(vision.RPs, 'f', 2))
                        .arg(QString::number(Util::roundDouble(vision.LSph,_prec), 'f', 2)).arg(QString::number(Util::roundDouble(vision.LCyl,_prec), 'f', 2))
                        .arg(QString::number(vision.LAx, 'f', 2)).arg(QString::number(vision.LPs, 'f', 2))
                        .arg(QString::number(vision.PD, 'f', 2));
                in << diopterInfo << '\n';
            }
        }

        file.close();
        QMessageBox::information(this, "result", "计算完成", QMessageBox::Ok);
    }
    mutex.unlock();
}

void MainWindow::on_chkHmode_stateChanged(int arg1)
{
    m_algoInvoker->setHmode(ui->chkHmode->isChecked());
}

void MainWindow::on_streamCalcBtn_clicked()
{
    /* 2. 准备根目录 */
    QString rootDirPath = ui->edtDirPath->text();
    QDir rootDir(rootDirPath);
    if (!rootDir.exists()) {
        qDebug() << "根目录不存在:" << rootDirPath;
        return;
    }

    // 获取所有数字文件夹并排序
    QStringList folderFilters;
    folderFilters << "[1-9]";  // 匹配1-9的文件夹

    rootDir.setNameFilters(folderFilters);
    rootDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);

    QStringList folderList = rootDir.entryList();
    folderList.sort();  // 按名称排序，确保顺序1,2,3,4,5

    if (folderList.isEmpty()) {
        qDebug() << "在路径" << rootDirPath << "中未找到数字文件夹";
        return;
    }

    qDebug() << "找到文件夹:" << folderList;

    m_algoInvoker->m_algoIntf->setVisionResultCallback(onResult);

    int sequenceNumber = 0;  // calcVisionBegin的序号参数

    // 遍历每个文件夹
    foreach (const QString &folderName, folderList) {
        QString folderPath = rootDir.absoluteFilePath(folderName);
        QDir currentDir(folderPath);

        if (!currentDir.exists()) {
            qDebug() << "文件夹不存在:" << folderPath;
            continue;
        }

        /* 3. 从当前文件夹加载 22 张图（保证顺序） */
        std::vector<QImage> imgs(23);   // 0 号弃用
        bool loadSuccess = true;

        for (int i = 1; i <= 22; ++i) {
            QString baseName = QString("temp%1").arg(i, 2, 10, QChar('0'));
            QString filePath;
            QImage img;

            // 优先尝试jpg，然后尝试bmp
            QString jpgFile = currentDir.filePath(baseName + ".jpg");
            QString bmpFile = currentDir.filePath(baseName + ".bmp");

            if (QFile::exists(jpgFile)) {
                filePath = jpgFile;
                img = QImage(jpgFile);
            } else if (QFile::exists(bmpFile)) {
                filePath = bmpFile;
                img = QImage(bmpFile);
            } else {
                qDebug() << "在文件夹" << folderName << "中文件不存在，尝试了:" << jpgFile << "和" << bmpFile;
                loadSuccess = false;
                break;
            }

            if (!img.isNull()) {
                imgs[i] = img.convertToFormat(QImage::Format_Grayscale8);
            } else {
                qDebug() << "在文件夹" << folderName << "中加载失败:" << filePath;
                loadSuccess = false;
                break;
            }
        }

        if (!loadSuccess) {
            qDebug() << "跳过文件夹:" << folderName;
            continue;
        }

        /* 4. 开始一次测量，序号参数递增 */
        m_algoInvoker->m_algoIntf->calcVisionBegin(ageRange_3_06_20_YEAR,
                             "2025-11-13-001",
                             singleDualEyeMode_Right,
                             sequenceNumber);
        sequenceNumber++;  // 下一个文件夹序号加1

        /* 5. 每隔 18 ms 喂一张图 */
        QElapsedTimer t;
        t.start();

        for (int i = 1; i <= 22; ++i) {
            if (!m_algoInvoker->m_algoIntf->appendImage(imgs[i].bits(),
                             imgs[i].width(),
                             imgs[i].height(),
                             i)) {
                qDebug() << "appendImage失败，在文件夹" << folderName << "第" << i << "张";
                break;
            }

            if (i < 22)  // 最后一张发完不需要再等
                QThread::msleep(18);
        }

        qDebug() << "文件夹" << folderName << ": 22 张图已发完，耗时" << t.elapsed() << "ms";

        // 可选：在文件夹之间添加延迟
        // if (folderName != folderList.last()) {
        //     QThread::msleep(100);
        // }
    }

    /* 6. 等待结果 */
    while (!g_done) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
    }

    qDebug() << "所有文件夹处理完成，共处理" << sequenceNumber << "个文件夹";
}

void MainWindow::on_refStrategyButton_clicked()
{
    runTestCases();
}

void MainWindow::on_stableTestBtn_clicked()
{
    runTestCases();
}

void MainWindow::on_calibBtn_clicked()
{
    const QString rootDirPath = "/mnt/hgfs/vmware_work/testdata/dataset/屈光测试集/模拟眼/1024/";
    const QString csvFilePath = "/mnt/hgfs/vmware_work/testdata/dataset/曲线标定.csv";
    const enAgeRange ageRange = enAgeRange::ageRange_4_20_100_YEAE;

    QDir rootDir(rootDirPath);
    if (!rootDir.exists()) {
        QMessageBox::warning(this, "错误", "根目录不存在：" + rootDirPath);
        return;
    }

    QFile csvFile(csvFilePath);
    if (!csvFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法打开 CSV 文件：" + csvFilePath);
        return;
    }

    QTextStream csvStream(&csvFile);
    csvStream << "Diopter(D),DSR0,DSR3,DSR6,DSR1,DSR4,DSR7,DSR2,DSR5,DSR8,DSR9,DSL0,DSL3,DSL6,DSL1,DSL4,DSL7,DSL2,DSL5,DSL8,DSL9\n";

    rootDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
    QFileInfoList dirList = rootDir.entryInfoList();

    // 自定义排序函数：按名称中的数字大小排序
    std::sort(dirList.begin(), dirList.end(), [](const QFileInfo &a, const QFileInfo &b) {
        QString nameA = a.fileName();
        QString nameB = b.fileName();

        // 提取数字部分（移除"D"字符）
        nameA.remove('D');
        nameB.remove('D');

        // 转换为数字进行比较
        return nameA.toDouble() < nameB.toDouble();
    });

    for (const QFileInfo &dirInfo : dirList) {
        QDir subDir(dirInfo.absoluteFilePath());
        subDir.setFilter(QDir::Dirs | QDir::NoDotAndDotDot);
        const QFileInfoList subDirList = subDir.entryInfoList(QDir::NoFilter, QDir::Name);

        QString diopter=dirInfo.fileName().left(dirInfo.fileName().indexOf('D'));
        for (const QFileInfo &subDirInfo : subDirList) {
            const QString photoDir = QDir::cleanPath(subDirInfo.absoluteFilePath() + "/succ_pictures/");

            Util::CScreenerImgsData imgsData;
            imgsData.imgsDir = photoDir;
            imgsData.isLoadPreImg = false;
            imgsData.fileType = Util::imgFileType_Bmp;
            imgsData.isNeedZeroth = false;
            imgsData.useSimulateImage = false;
            imgsData.setImgSize(IMG_WIDTH, IMG_HEIGHT);

            if (!imgsData.loadImgFiles(true)) {
                qWarning() << "加载图像失败：" << photoDir;
                continue;
            }

            std::vector<uchar*> resultByte;
            for (int i = 0; i < 23; ++i) {
                resultByte.push_back(imgsData.getImage(i));
            }

            double DSR[10] = {0};
            double DSL[10] = {0};
            const auto calcState = m_algoInvoker->m_algoIntf->performVisionPreprocessing(
                resultByte, ageRange, "", singleDualEyeMode_Right, DSL, DSR);

            if (calcState == calcResultState_Succ) {
                csvStream << diopter<< ",";
                csvStream << DSR[0] << "," << DSR[3] << "," << DSR[6] << ","
                          << DSR[1] << "," << DSR[4] << "," << DSR[7] << ","
                          << DSR[2] << "," << DSR[5] << "," << DSR[8] << ","
                          << DSR[9] << ",";
                csvStream << DSL[0] << "," << DSL[3] << "," << DSL[6] << ","
                          << DSL[1] << "," << DSL[4] << "," << DSL[7] << ","
                          << DSL[2] << "," << DSL[5] << "," << DSL[8] << ","
                          << DSL[9] << "\n";
            } else {
                qWarning() << "算法处理失败：" << photoDir;
            }
        }
    }

    csvFile.close();
    QMessageBox::information(this, "完成", "CSV 文件已生成：" + csvFilePath);
}
