#ifndef CSOUNDINTF_H
#define CSOUNDINTF_H

#include <QObject>
#include <QMediaPlayer>
#include <QMediaPlaylist>
#include <QStringList>

// 提示音
enum enSoundPrompt {
    soundPrompt_LowBattery  = 0,    // 低电量
    soundPrompt_Shutdown,           // 关机

    soundPrompt_Min = soundPrompt_LowBattery,
    soundPrompt_Max = soundPrompt_Shutdown,
};

// 语音提示
enum class enVoicePrompt {
    MeasurementStarting     ,       // 开始测量
    FocusOnLight            ,       // 注视固视灯
    PupilNotDetected        ,       // 瞳孔检测失败
    MeasurementTimeout      ,       // 测量超时
    MeasurementFinished     ,       // 结束测量

    Min = MeasurementStarting   ,
    Max = MeasurementFinished   ,
};
const char *voicePromptToFileName(enVoicePrompt _voice);     // 获取语音提示枚举值对应的文件名

// 回放模式
enum enPlaybackMode {
    playbackMode_SingleLoop = 0,
    playbackMode_ListLoop,

    playbackMode_Min = playbackMode_SingleLoop,
    playbackMode_Max = playbackMode_ListLoop,
};

// 默认警告音音量
#define DEFAULT_VOLUME_PROMPT   80

// 默认音乐音量
#define DEFAULT_VOLUME_MUSIC    50

// 声音接口
/****
 * 因为 Buildroot + qt5.14 平台只支持音频设备独占模式，所以只能有一个播放器，所以通过切换播放清单的方式实现切换音乐和警告音两种声音类型。
 * // TODO: QMediaPlayer 能释放对底层的占用，从而可以同时用两个播放器吗？
 */
class CSoundIntf : public QObject
{
    Q_OBJECT
public:
    ~CSoundIntf();

    static CSoundIntf *getInstance();
    static void releaseInstance();

    void playMusics();                                      // 播放音乐
    void playVoice(enVoicePrompt _voice);                   // 播放语音提示
    void playPrompt(enSoundPrompt _sound);                  // 播放提示音

    void stop();                                            // 停止播放
    bool isPlaying();                                       // 是否正在播放

    void setVolume(int _volume);                            // 设置音量
    int getVolume();                                        // 获取音量

    void clearMusicList();                                  // 清除音乐播放清单
    int getMusicListSize();                                 // 获取音乐播放清单的媒体数量
    void addMusicUrl(QUrl _url);                            // 添加音乐（@param _url: URL，例： QUrl::fromLocalFile("/absolute/file/path"); QUrl("qrc:/resource/file/path"); ）
    void getMusicUrlList(QList<QUrl> &_list_url);           // 获取音乐 URL 列表
    void getMusicUrlStrList(QStringList &_list_url_str);    // 获取音乐 URL str 列表
    bool getMusicUrl(int _idx, QUrl &_url);                 // 获取指定索引号的音乐 URL
    bool removeMusic(int _idx);                             // 移除指定索引号的音乐

    int getCurrentMusicIndex();                             // 获得音乐播放列表当前音乐索引
    bool setCurrentMusicIndex(int &_idx);                   // 设置音乐播放列表当前音乐索引

    enPlaybackMode getPlayMode();                           // 获取音乐的回放模式
    void setPlayMode(enPlaybackMode _play_mode);            // 设置音乐的回放模式

Q_SIGNALS:
    void sigMusicIndexChanged(int _index);

protected Q_SLOTS:
    void slot_mediaPlayer_stateChanged(QMediaPlayer::State _new_state);

protected:
    explicit CSoundIntf(QObject *parent = nullptr);

    static CSoundIntf *s_instance;
    static const char * const S_CLASS_NAME;     // 本类的类名

    QMediaPlaylist::PlaybackMode playbackModeToPlayerMode(enPlaybackMode _play_mode);   // 本接口定义的回放模式 转 QMediaPlaylist 定义的回放模式
    enPlaybackMode getPlaybackMode(QMediaPlaylist::PlaybackMode _play_mode_qt);         // QMediaPlaylist 定义的回放模式 转 本接口定义的回放模式

    QMediaPlayer *m_mediaPlayer = Q_NULLPTR;
    QMediaPlaylist *m_playList_Music = Q_NULLPTR;           // 音乐播放列表
    QMediaPlaylist *m_playList_Prompt = Q_NULLPTR;          // 警告声播放列表
    QMediaPlaylist *m_playList_Voice = Q_NULLPTR;           // 语音播放列表
    int m_lastMusicIndex = 0;                               // 当前音乐曲目       /* 在切换播放清单时，当前曲目好像会丢失，所以须在切换到警告音前保存 */
    int m_lastMusicVolume = DEFAULT_VOLUME_MUSIC;           // 当前音乐音量       /* 在切换播放清单时，音量会变，所以须在切换到警告音前保存 */

    QMediaPlayer::State m_playerState {QMediaPlayer::State::StoppedState};      // 播放器状态

};

#endif // CSOUNDINTF_H
