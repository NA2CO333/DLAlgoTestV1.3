//音乐界面
#include "musicsetting.h"
#include "ui_musicsetting.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileInfoList>
#include <QListView>
#include <QStringListModel>
#include <QSettings>

#include "messagewin.h"
#include "windowsmanager.h"
#include "global.h"
#include "musicsetting.h"

//#ifdef SURPORT_FRAME_BUFFER
//#include <linux/mxcfb.h>
//#endif

// 缺少音乐文件时的音乐资源
#define DEFAULT_MUSIC_RES "qrc:/resource/music/lowBattery.mp3"

// 音乐文件名（不含扩展名）转为供用户查看的名称
QString musicFileNameToViewName(QString _file_name)
{
    QString view_name = _file_name;
    if (G_LANGUAGE_CHINESE == CGlobal::language) {
        if (_file_name == "cheerful")
            view_name = "欢快";
        else if (_file_name == "bird")
            view_name = "鸟叫声1";
        else if (_file_name == "bird2")
            view_name = "鸟叫声2";
        else if (_file_name == "croak")
            view_name = "青蛙叫声";
        else if (_file_name == "rhythm")
            view_name = "节奏";
        else if (_file_name == "water")
            view_name = "水流声";
        else if (_file_name == "scorpion")
            view_name = "蝈蝈叫声";
        else
            view_name = _file_name;
    }
    return view_name;
}

//
MusicSetting::MusicSetting(QWidget *parent) :
    CBaseWidget(parent),
    ui(new Ui::MusicSetting)
{
    ui->setupUi(this);

    isShowStatusBar = true;

    this->setGeometry(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

    ui->sliderVolume->setTracking(true);    // 移动滑块时同时改变 value，使 sliderMoved() 事件得到的 position 等于当前 value

    //
    if (!g_SoundIntf) {
        logWarning(QString(__PRETTY_FUNCTION__) + ": soundIntf is not created!", CGlobal::LOG_SYS);
        g_SoundIntf = CSoundIntf::getInstance();
    }

    // 载入配置
    loadConfig();

}

MusicSetting::~MusicSetting()
{
    delete ui;
}

void MusicSetting::slot_SoundIntf_MusicIndexChanged(int _index)
{
    // 切换表格当前行
    ui->listWidgetMusics->setCurrentRow(_index);
}

enPlaybackMode MusicSetting::getPlayModeCfg()
{
    int cfg = appSetting::value("/tool/playMode").toInt();
    enPlaybackMode mode = playbackMode_SingleLoop;
    if (cfg >= playbackMode_Min && cfg <= playbackMode_ListLoop) {
        mode = (enPlaybackMode)cfg;
    } else {
        logWarning(QString::asprintf("MusicSetting::getPlayModeCfg() : value %d is not valid!", (int)cfg), CGlobal::LOG_SYS);
    }
    return mode;
}

void MusicSetting::setPlayModeCfg(enPlaybackMode _play_mode)
{
    enPlaybackMode play_mode = playbackMode_SingleLoop;
    if (_play_mode >= playbackMode_Min && _play_mode <= playbackMode_ListLoop) {
        play_mode = _play_mode;
    } else {
        logWarning(QString::asprintf("MusicSetting::setPlayModeCfg() : value %d is not valid!", (int)_play_mode), CGlobal::LOG_SYS);
    }
    appSetting::setValue("/tool/playMode", (int)play_mode);
}

int MusicSetting::getCurrentMusicCfg()
{
    return appSetting::value("/tool/currentMusicIndex").toInt();
}

void MusicSetting::setCurrentMusicCfg(int _idx)
{
    appSetting::setValue("/tool/currentMusicIndex", _idx);
}

void MusicSetting::setVolumeCfg(int _volume)
{
    appSetting::setValue("/tool/volume", _volume);
}

int MusicSetting::getVolumeCfg()
{
    return appSetting::value("/tool/volume").toInt();
}

void MusicSetting::loadConfig()
{
    // 回放模式
    enPlaybackMode play_mode = this->getPlayModeCfg();
    g_SoundIntf->setPlayMode(play_mode);

    // 音量
    int volume = this->getVolumeCfg();
    g_SoundIntf->setVolume(volume);

    // 播放列表
    QList<QUrl> list_musics;
    getMusicListFromDir(list_musics);
    for (int i = 0; i < list_musics.size(); i++) {
        g_SoundIntf->addMusicUrl(list_musics[i]);
    }

    // 当前曲目
    int curr_idx = this->getCurrentMusicCfg();
    bool is_succ = g_SoundIntf->setCurrentMusicIndex(curr_idx);
    if (!is_succ) {
        this->setCurrentMusicCfg(curr_idx);
    }
}

// 检索音乐目录，得到音乐播放清单
void MusicSetting::getMusicListFromDir(QList<QUrl> &_list_musics)
{
    _list_musics.clear();

    QDir dir_music(MUSIC_DIR_PATH);
    if (dir_music.exists()) {
        QFileInfoList file_info_list = dir_music.entryInfoList();
        QFileInfo file_info;
        QString file_path;
        for (int i = 0; i < file_info_list.size(); i++) {
            file_info = file_info_list[i];
            if (file_info.fileName().endsWith("mp3")) {
                file_path = file_info.absoluteFilePath();

                logDebug(QString("MusicSetting::getMusicListFromDir(): add music \"%1\" to intf").arg(file_path), CGlobal::LOG_SYS);
                _list_musics.append(QUrl::fromLocalFile(file_path));
            }
        }
    }

    if (_list_musics.size() == 0) {
        _list_musics.append(QUrl(DEFAULT_MUSIC_RES));
    }
}

void MusicSetting::objToUi()
{
    // 回放模式
    ui->ckbSingleLoop->setChecked(playbackMode_SingleLoop == g_SoundIntf->getPlayMode());
    ui->ckbListLoop->setChecked(playbackMode_ListLoop == g_SoundIntf->getPlayMode());

    // 进度条
    ui->sliderVolume->setValue(g_SoundIntf->getVolume());

    // 播放列表
    QList<QUrl> list_url;
    g_SoundIntf->getMusicUrlList(list_url);

    ui->listWidgetMusics->clear();
    QUrl url_music;
    QString file_path;
    QString str_temp;
    QString file_name;
    QString view_name;
    for (int i = 0; i < list_url.size(); i++) {
        url_music = list_url[i];
        file_path = (url_music.isLocalFile() ? url_music.toLocalFile() : url_music.toString());
        logDebug(QString("MusicSetting::objToUi(): path of music %1 is \"%2\"").arg(i).arg(file_path), CGlobal::LOG_SYS);

        Util::separateFilePath(file_path, str_temp, file_name);
        Util::separateFileName(file_name, file_name, str_temp);

        view_name = musicFileNameToViewName(file_name);
        ui->listWidgetMusics->addItem(view_name);
    }

    // 当前曲目
    int curr_idx = g_SoundIntf->getCurrentMusicIndex();
    ui->listWidgetMusics->setCurrentRow(curr_idx);

    // 语音提示
    ui->ckbVoice->setChecked(CGlobal::isVoicePrompt);
}

void MusicSetting::UiToObj()
{

}

void MusicSetting::showEvent(QShowEvent *)
{
    // 事件连接
    QObject::connect(g_SoundIntf, &CSoundIntf::sigMusicIndexChanged, this, &MusicSetting::slot_SoundIntf_MusicIndexChanged);

    // 隐藏测试功能
    ui->btnAddMusic->setVisible(CGlobal::isDebugMode);
    ui->btnDelMusic->setVisible(CGlobal::isDebugMode);
    ui->btnTestPromptSound->setVisible(CGlobal::isDebugMode);
    ui->btnTestVoice->setVisible(CGlobal::isDebugMode);

#if (1 == OS_TYPE)
    // 打开声卡使能
    emit sendSIGNAL(sysSignal_MusicOn);        // TODO: 把这个封装到声音接口？
#endif

    // 重新翻译界面（应放到最前，避免此过程的 UI 文本处理过程被覆盖）
    ui->retranslateUi(this);

    // 更新 UI 部件的值
    objToUi();

    // 开始播放
    //g_SoundIntf->playMusics();

    // 更新主题
    updateTheme();

    // 更新语言
    updateLanguage();

}

void MusicSetting::hideEvent(QHideEvent *)
{
    // 停止音乐播放
    g_SoundIntf->stop();

    // 事件断连
    QObject::disconnect(g_SoundIntf, &CSoundIntf::sigMusicIndexChanged, this, &MusicSetting::slot_SoundIntf_MusicIndexChanged);

#if (1 == OS_TYPE)
    // 关闭声卡使能
    emit sendSIGNAL(sysSignal_MusicOff);
#endif

    //
    CGlobal::saveConfs();
}

void MusicSetting::updateLanguage()
{
    // 更新语言
    //if (language) {
    //    ui->label_context->setText("选择一首背景音乐");
    //    ui->btnAddMusic->setText("添加音乐");
    //    ui->ckbSingleLoop->setText("单曲循环");
    //    ui->ckbListLoop->setText("列表循环");
    //    ui->label_context_3->setText("设置音量:");
    //    ui->label_Home->setText("主页");
    //    ui->label_Back->setText("返回");
    //} else {
    //    ui->label_context->setText("Select a background music");
    //    ui->btnAddMusic->setText("Add music");
    //    ui->ckbSingleLoop->setText("Single loop");
    //    ui->ckbListLoop->setText("List loop");
    //    ui->label_context_3->setText("Set volume:");
    //    ui->label_Home->setText("Home");
    //    ui->label_Back->setText("Back");
    //}

    // 更新标题
    getWinManage()->updateWindowTitle(this, tr("音乐"));  // "Music"


}

void MusicSetting::updateTheme()        // TODO: 通过 QSS 文件实现样式设置
{
    // 更新主题
    //QPalette palette;
    if(themeType_Black == getSysThemeType()){
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/black_theme/blackground_color_b.png"));     //设置主题

        ui->listWidgetMusics->setStyleSheet("QListWidget{background-color:rgb(51,56,62); color:rgb(255,255,255);}");
        ui->btnAddMusic->setStyleSheet("QPushButton{border-radius:5px; background-color:rgb(51,56,62); color:rgb(204,204,204);}");

        //设置所有 QCheckBox 样式
        QList<QCheckBox *> list_CheckBox = findChildren<QCheckBox *>();
        foreach (QCheckBox *p, list_CheckBox) {
            p->setStyleSheet("QCheckBox {background-color: transparent; color:rgb(204,204,204);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} QCheckBox::indicator:unchecked{image:url(:/resource/unchecked.png);} ");
        }

        // 设置 QLabel 的样式
        ui->label_context->setStyleSheet("color:rgb(255,255,255);");
        ui->label_context_3->setStyleSheet("color:rgb(152,151,156);");
        ui->label_Home->setStyleSheet("color:rgb(204,204,204);");
        ui->label_Back->setStyleSheet("color:rgb(204,204,204);");

        //
        ui->pushButton_Home->setIcon(QIcon(":/resource/black_theme/home_b.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/black_theme/back_b.png"));
    }
    else{
        //palette.setBrush(this->backgroundRole(), QPixmap(":/resource/white_theme/whiteground_color_w.png"));

        ui->listWidgetMusics->setStyleSheet("QListWidget{background-color:rgb(249,249,251); color:rgb(1,1,1);}");
        ui->btnAddMusic->setStyleSheet("QPushButton{border-radius:5px; background-color:rgb(226,226,231); color:rgb(1,1,1);}");

        //设置所有 QCheckBox 的样式
        QList<QCheckBox *> list_CheckBox = findChildren<QCheckBox *>();
        foreach (QCheckBox *p, list_CheckBox) {
            p->setStyleSheet("QCheckBox {background-color:rgb(242,242,247); color:rgb(1,1,1);} QCheckBox::indicator {width: 20px; height: 20px;} QCheckBox::indicator:checked {image: url(:/resource/checked.png);} ");
        }

        //设置 QLabel 样式
        ui->label_context->setStyleSheet("color:rgb(1,1,1);");
        ui->label_context_3->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Home->setStyleSheet("color:rgb(1,1,1);");
        ui->label_Back->setStyleSheet("color:rgb(1,1,1);");

        //
        ui->pushButton_Home->setIcon(QIcon(":/resource/white_theme/home_w.png"));
        ui->pushButton_Back->setIcon(QIcon(":/resource/white_theme/back_w.png"));
    }
    //this->setPalette(palette);
}

void MusicSetting::on_pushButton_Back_clicked()
{
    //UiToObj();

    getWinManage()->backToLastWidget();
}

void MusicSetting::on_pushButton_Home_clicked()
{
    //UiToObj();

    getWinManage()->showWindowByType(WIN_HOME);
}

void MusicSetting::on_ckbVoice_clicked(bool _checked)
{
    CGlobal::isVoicePrompt = _checked;
}

void MusicSetting::on_ckbSingleLoop_clicked(bool checked)
{
    ui->ckbListLoop->setChecked(!checked);

    enPlaybackMode play_mode = (checked ? playbackMode_SingleLoop : playbackMode_ListLoop);
    this->setPlayModeCfg(play_mode);
    g_SoundIntf->setPlayMode(play_mode);
}

void MusicSetting::on_ckbListLoop_clicked(bool checked)
{
    ui->ckbSingleLoop->setChecked(!checked);

    enPlaybackMode play_mode = (checked ? playbackMode_ListLoop : playbackMode_SingleLoop);
    this->setPlayModeCfg(play_mode);
    g_SoundIntf->setPlayMode(play_mode);
}

void MusicSetting::on_sliderVolume_sliderMoved(int position)
{
    qDebug() << "===== current volume: " << position;
    this->setVolumeCfg(position);
    g_SoundIntf->setVolume(position);

    // 若未播放，则开始播放
    if (!g_SoundIntf->isPlaying()) {
        g_SoundIntf->playMusics();
    }
}

void MusicSetting::on_listWidgetMusics_clicked(const QModelIndex &index)
{
    int curr_row = index.row();
    this->setCurrentMusicCfg(curr_row);
    g_SoundIntf->setCurrentMusicIndex(curr_row);

    // 若未播放，则开始播放
    if (!g_SoundIntf->isPlaying()) {
        g_SoundIntf->playMusics();
    }
}

// 从 U 盘添加音乐
void MusicSetting::on_btnAddMusic_clicked()
{
    // 若目标目录不存在，则创建
    QDir dir_dst(MUSIC_DIR_PATH);
    if (!dir_dst.exists()) {
        dir_dst.mkpath(dir_dst.absolutePath());
    }

    // 从 U 盘固定目录查找添加音乐
    QString disk_path = Util::CUDisk::getPath();
    if (!QFile::exists(disk_path)) {
        getWinManage()->showSuspensionPrompt(tr("未检测到 U 盘"));   // "U Disk not found"
        return;
    }

    QDir dir_src(QString("%1/music").arg(disk_path));
    if (dir_src.exists()) {
        int count_added = 0;
        int count_same = 0;

        QFileInfoList list_file_info = dir_src.entryInfoList();
        if (list_file_info.size() > 2) {
            // 得到现有播放清单
            QStringList list_url_str;
            g_SoundIntf->getMusicUrlStrList(list_url_str);

            // 查找目录中的音乐文件，若现有清单中不存在，则添加
            QFileInfo file_info;
            QString file_path_dest;
            QString url_str;
            for (int i = 0; i < list_file_info.size(); i++) {
                file_info = list_file_info[i];
                if (file_info.isFile() && file_info.fileName().endsWith("mp3")) {
                    file_path_dest = QString(MUSIC_DIR_PATH) + QDir::separator() + file_info.fileName();

                    // 检查是否重复
                    url_str = QUrl::fromLocalFile(file_path_dest).toString();
                    if (list_url_str.contains(url_str)) {
                        count_same++;
                        continue;
                    }

                    // 拷贝文件到内存存储
                    if (!QFile::copy(file_info.absoluteFilePath(), file_path_dest)) {
                        qDebug() << "copy music file failed";
                        continue;
                    }

                    // 添加到 UI 列表
                    ui->listWidgetMusics->addItem(musicFileNameToViewName(file_info.fileName().section('.',0,0)));

                    // 添加到声音接口
                    g_SoundIntf->addMusicUrl(QUrl::fromLocalFile(file_path_dest));

                    //
                    count_added++;
                }
            }
        }

        //
        if (!(0 == count_added && 0 == count_same)) {
            QString msg = QString(tr("添加了 %1 首音乐")).arg(count_added);   // "Added %1 musics"
            if (count_same > 0) {
                msg += QString(tr("，%1 首重复")).arg(count_same);  // ", %1 file repeated"
            }
            getWinManage()->showSuspensionPrompt(msg);
        } else {
            getWinManage()->showSuspensionPrompt(tr("未发现音乐文件"));    // "music file not found"
        }
    } else {
        getWinManage()->showSuspensionPrompt(tr("音乐文件夹（[U盘]/music/）不存在"));  // "music direction([UDisk]/music/) not exists"
    }
}

// 删除音乐
void MusicSetting::on_btnDelMusic_clicked()
{
    int row = ui->listWidgetMusics->currentRow();
    if (row >= 0) {
        QUrl url;
        if (g_SoundIntf->getMusicUrl(row, url)) {
            if (url.isLocalFile()) {
                // 在 UI 中删除
                QListWidgetItem *item = ui->listWidgetMusics->takeItem(row);
                if (item) {
                    delete item;
                    item = nullptr;
                }

                // 在接口中删除
                g_SoundIntf->removeMusic(row);

                // 删除文件
                QString file_path = url.toLocalFile();
                QFile file(file_path);
                if (file.exists()) {
                    if (!file.remove()) {
                        getWinManage()->showSuspensionPrompt("err: failed to remove file!");
                    }
                } else {
                    getWinManage()->showSuspensionPrompt("logic err: file not exists!");
                }
            } else {
                getWinManage()->showSuspensionPrompt("logic err: not local file!");
            }
        } else {
            getWinManage()->showSuspensionPrompt("logic err: failed to get url!");
        }
    }
}

void MusicSetting::on_btnTestPromptSound_clicked()
{
    // 测试警告声
    static int idx = 0;
    if (0 == idx) {
        g_SoundIntf->playPrompt(soundPrompt_LowBattery);
        idx++;
    } else if (1 == idx) {
        g_SoundIntf->playPrompt(soundPrompt_Shutdown);
        idx = 0;
    }
}

void MusicSetting::on_btnTestVoice_clicked()
{
    g_SoundIntf->playVoice(enVoicePrompt::FocusOnLight);
}
