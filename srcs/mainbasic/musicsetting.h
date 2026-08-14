#ifndef MUSICSETTING_H
#define MUSICSETTING_H

#include <QStringList>

#include "baseform.h"
#include "statusbarform.h"
#include "soundintf.h"

namespace Ui {
class MusicSetting;
}

// 音乐目录路径       // TODO: 音乐文件不应该放到这个目录
#if OS_TYPE != 2
  #define MUSIC_DIR_PATH "/home/root/music"
#else
  #define MUSIC_DIR_PATH "/home/henry/Music"
#endif

//
class MusicSetting : public CBaseWidget
{
    Q_OBJECT
public:
    explicit MusicSetting(QWidget *parent = 0);
    ~MusicSetting();

signals:
#if (1 == OS_TYPE)
    void sendSIGNAL(enSysSignal _sys_signal);
#endif

protected Q_SLOTS:
    void slot_SoundIntf_MusicIndexChanged(int _index);

protected:
    void showEvent(QShowEvent *);
    void hideEvent(QHideEvent *);

    enPlaybackMode getPlayModeCfg();                    // 获取音乐的回放模式的配置值
    void setPlayModeCfg(enPlaybackMode _play_mode);     // 设置音乐的回放模式的配置值

    int getCurrentMusicCfg();                           // 获取当前音乐索引号的配置值
    void setCurrentMusicCfg(int _idx);                  // 设置当前音乐索引号的配置值

    int getVolumeCfg();                                 // 设置音量的配置值
    void setVolumeCfg(int _volume);                     // 设置音量的配置值

    void loadConfig();          // 载入配置到音乐接口（Object）
    void objToUi();             // 将 Object 的值设置到 UI
    void UiToObj();             // 将 UI 的值设置到 Object    /* 本窗体的设置即时生效，所以此函数的代码已分散到各个部件的修改事件，不需要调用此函数 */

    void updateTheme();         // 更新主题样式

    void updateLanguage();

    void getMusicListFromDir(QList<QUrl> &_list_musics);   // 检索音乐目录，得到音乐播放清单

private slots:
    void on_pushButton_Home_clicked();
    void on_pushButton_Back_clicked();
    void on_ckbVoice_clicked(bool _checked);
    void on_ckbSingleLoop_clicked(bool checked);
    void on_ckbListLoop_clicked(bool checked);
    void on_sliderVolume_sliderMoved(int position);
    void on_listWidgetMusics_clicked(const QModelIndex &index);
    void on_btnAddMusic_clicked();
    void on_btnDelMusic_clicked();
    void on_btnTestPromptSound_clicked();
    void on_btnTestVoice_clicked();
private:
    Ui::MusicSetting *ui;
};

#endif // MUSICSETTING_H
