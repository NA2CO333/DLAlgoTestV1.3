#include "soundintf.h"

#include <QApplication>

#include "appsetting.h"
#include "global.h"

//
const QString URL_PROMT_LOWBATTRY   = "qrc:/resource/music/lowBattery.mp3";
const QString URL_PROMT_SHUTDOWN    = "qrc:/resource/music/shutdown.mp3";

//
const char *voicePromptToFileName(enVoicePrompt _voice)
{
    switch (_voice) {
    case enVoicePrompt::MeasurementStarting : return "measurement-starting.mp3" ;
    case enVoicePrompt::FocusOnLight        : return "focus-on-light.mp3"       ;
    case enVoicePrompt::PupilNotDetected    : return "pupil-not-detected.mp3"   ;
    case enVoicePrompt::MeasurementTimeout  : return "measurement-timeout.mp3"  ;
    case enVoicePrompt::MeasurementFinished : return "measurement-finished.mp3" ;
    }
    return "";
}

//
CSoundIntf *CSoundIntf::s_instance = Q_NULLPTR;
const char * const CSoundIntf::S_CLASS_NAME = CSoundIntf::staticMetaObject.className();

CSoundIntf *CSoundIntf::getInstance()
{
    if (!s_instance) {
        s_instance = new CSoundIntf();
    }
    return s_instance;
}

void CSoundIntf::releaseInstance()
{
    if (s_instance) {
        delete s_instance;
        s_instance = Q_NULLPTR;
    }
}

CSoundIntf::CSoundIntf(QObject *parent) : QObject(parent)
{
    //
    m_playList_Music = new QMediaPlaylist();
    QObject::connect(m_playList_Music, &QMediaPlaylist::currentIndexChanged, this, &CSoundIntf::sigMusicIndexChanged);

    // TODO: 将 class MusicSetting 的播放列表设置代码转移到这里


    //
    m_playList_Voice = new QMediaPlaylist();

    int idx_min_voice = static_cast<int>(enVoicePrompt::Min);
    int idx_max_voice = static_cast<int>(enVoicePrompt::Max);
    for (int i = idx_min_voice; i <= idx_max_voice; i++) {
        enVoicePrompt voice = static_cast<enVoicePrompt>(i);

        QString file_path = QString("%1/voice/%2/%3")
                .arg(qApp->applicationDirPath())
                .arg(G_LANGUAGE_CHINESE == CGlobal::language ? G_LANGUAGE_CHINESE : G_LANGUAGE_ENGLISH)
                .arg(voicePromptToFileName(voice));

        m_playList_Voice->addMedia(QUrl::fromLocalFile(file_path));
    }

    //
    m_playList_Prompt = new QMediaPlaylist();

    m_playList_Prompt->addMedia(QUrl(URL_PROMT_LOWBATTRY));    /* 注意：这里的加入媒体的位置与提示音枚举类型的值一致 */
    m_playList_Prompt->addMedia(QUrl(URL_PROMT_SHUTDOWN));

    //
    m_mediaPlayer = new QMediaPlayer();
    QObject::connect(m_mediaPlayer, &QMediaPlayer::stateChanged, this, &CSoundIntf::slot_mediaPlayer_stateChanged, Qt::QueuedConnection);

}

CSoundIntf::~CSoundIntf()
{

}

void CSoundIntf::playMusics()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    if (m_playList_Music->mediaCount() == 0) {
        return;
    }

    //
    if (this->isPlaying()) {
        this->stop();
    }

    //
    if (m_playList_Music != m_mediaPlayer->playlist()) {
        //
        m_mediaPlayer->setPlaylist(m_playList_Music);

        // 将播放清单切换回来后要重设当前条目和音量
        m_playList_Music->setCurrentIndex(m_lastMusicIndex);        /* 调试发现，好像要设置了 playList 再设置 currentIndex 才有效？ */
        logDebug((QString(__PRETTY_FUNCTION__) + ": set currIdx to %1").arg(m_lastMusicIndex), CGlobal::LOG_SYS);

        m_mediaPlayer->setVolume(m_lastMusicVolume);
        logDebug((QString(__PRETTY_FUNCTION__) + ": set volume to %1").arg(m_lastMusicVolume), CGlobal::LOG_SYS);
    }

    //
    m_mediaPlayer->play();
}

void CSoundIntf::playVoice(enVoicePrompt _voice)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    if (_voice < enVoicePrompt::Min || _voice > enVoicePrompt::Max) {
        logWarning("CSoundIntf::playPrompt() : _voice is out of range!", CGlobal::LOG_SYS);
        return;
    }

    //
    if (m_playList_Voice->mediaCount() == 0) {
        return;
    }

    //
    if (this->isPlaying()) {
        this->stop();
    }

    //
    if (m_playList_Voice != m_mediaPlayer->playlist()) {
        //
        m_mediaPlayer->setPlaylist(m_playList_Voice);
        m_playList_Voice->setPlaybackMode(QMediaPlaylist::CurrentItemOnce);

        // 设置音量
        m_mediaPlayer->setVolume(m_lastMusicVolume);
        logDebug((QString(__PRETTY_FUNCTION__) + ": set volume to %1").arg(m_lastMusicVolume), CGlobal::LOG_SYS);
    }

    // 设置播放条目
    int idx = static_cast<int>(_voice);
    m_playList_Voice->setCurrentIndex(idx);      /* 调试发现，好像要设置了 playList 再设置 currentIndex 才有效？ */
    logDebug((QString(__PRETTY_FUNCTION__) + ": set currIdx to %1").arg(idx), CGlobal::LOG_SYS);
    m_playList_Voice->setPlaybackMode(QMediaPlaylist::CurrentItemOnce);

    //
    m_mediaPlayer->play();
}

void CSoundIntf::playPrompt(enSoundPrompt _sound)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //
    if (_sound < soundPrompt_Min || _sound > soundPrompt_Max) {
        logWarning("CSoundIntf::playPrompt() : _sound is out of range!", CGlobal::LOG_SYS);
        return;
    }

    //
    if (m_playList_Prompt->mediaCount() == 0) {
        return;
    }

    //
    if (this->isPlaying()) {
        this->stop();
    }

    //
    if (m_playList_Prompt != m_mediaPlayer->playlist()) {
        // 切换到警告音前先保存音乐当前条目和音量
        m_lastMusicVolume = m_mediaPlayer->volume();

        //
        m_mediaPlayer->setPlaylist(m_playList_Prompt);

        // 切换播放清单后，重设音量
        m_mediaPlayer->setVolume(DEFAULT_VOLUME_PROMPT);      // NOTE: 提示音的音量采用固定值
        logDebug((QString(__PRETTY_FUNCTION__) + ": set volume to %1").arg(DEFAULT_VOLUME_PROMPT), CGlobal::LOG_SYS);
    }

    // 设置播放条目
    int idx = static_cast<int>(_sound);
    m_playList_Prompt->setCurrentIndex(idx);      /* 调试发现，好像要设置了 playList 再设置 currentIndex 才有效？ */
    logDebug((QString(__PRETTY_FUNCTION__) + ": set currIdx to %1").arg(idx), CGlobal::LOG_SYS);
    m_playList_Prompt->setPlaybackMode(QMediaPlaylist::CurrentItemOnce);

    //
    m_mediaPlayer->play();
}

void CSoundIntf::stop()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //emit sendSIGNAL(sysSignal_MusicOff);
    //if (QMediaPlayer::StoppedState != m_mediaPlayer->state()) {   // NOTE: GStreamer 版本较低时，QMediaPlayer->state() 有一定概率出现非法访问异常！
    if (QMediaPlayer::StoppedState != m_playerState) {
        m_mediaPlayer->stop();
    }
}

bool CSoundIntf::isPlaying()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    //return (QMediaPlayer::PlayingState == m_mediaPlayer->state());  // NOTE: GStreamer 版本较低时，QMediaPlayer->state() 有一定概率出现非法访问异常！

    return (QMediaPlayer::PlayingState == m_playerState);
}

void CSoundIntf::setVolume(int _volume)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    m_mediaPlayer->setVolume(_volume);
    m_lastMusicVolume = _volume;
    logDebug((QString(__PRETTY_FUNCTION__) + ": set volume to %1").arg(_volume), CGlobal::LOG_SYS);
}

int CSoundIntf::getVolume()
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered ...";

    int volume = m_mediaPlayer->volume();
    logDebug(QString("CSoundIntf::getVolume(): volume = %1").arg(volume), CGlobal::LOG_SYS);
    return volume;
}

enPlaybackMode CSoundIntf::getPlayMode()
{
    return getPlaybackMode(m_playList_Music->playbackMode());
}

void CSoundIntf::setPlayMode(enPlaybackMode _play_mode)
{
    m_playList_Music->setPlaybackMode(playbackModeToPlayerMode(_play_mode));
}

void CSoundIntf::slot_mediaPlayer_stateChanged(QMediaPlayer::State _new_state)
{
    qDebug() << S_CLASS_NAME << "::" << __FUNCTION__ << "(): entered, _new_state = " << (int)_new_state;

    //
    m_playerState = _new_state;
}

/**
 * @brief 设置当前条目
 * @param _idx: 当前条目索引号。引用传递，若返回值为 false，则表示该值被修改过
 * @return 表示 索引号 参数是否被修改过
 */
bool CSoundIntf::setCurrentMusicIndex(int &_idx)
{
    bool is_valid = true;   // 索引号是否合法（不合法则表示被修改过）
    if (_idx >= 0) {
        if (m_playList_Music->mediaCount() > 0) {
            if (_idx > m_playList_Music->mediaCount() - 1) {
                logWarning("CSoundIntf::setCurrentMusicIndex() : _idx > MediaCount - 1, not valid!", CGlobal::LOG_SYS);
                _idx = m_playList_Music->mediaCount() - 1;
                is_valid = false;
            }
        } else {
            logWarning(QString("CSoundIntf::setCurrentMusicIndex() : PlayList is empty! currIdx can't be set to %1").arg(_idx), CGlobal::LOG_SYS);
            _idx = -1;
            is_valid = false;
        }
    } else {
        if (-1 != _idx) {
            _idx = -1;          // TODO: -1 合法吗？
            is_valid = false;
        }
    }

    //
    //this->stop();

    //
    bool is_playing = this->isPlaying();
    if (is_playing) {
        this->stop();
    }
    m_playList_Music->setCurrentIndex(_idx);
    m_lastMusicIndex = _idx;
    logDebug((QString(__PRETTY_FUNCTION__) + ": set currIdx to %1").arg(_idx), CGlobal::LOG_SYS);
    if (is_playing) {
        this->playMusics();
    }

    //
    //this->playMusics();

    //
    return is_valid;
}

int CSoundIntf::getCurrentMusicIndex()
{
    return m_playList_Music->currentIndex();
}

void CSoundIntf::clearMusicList()
{
    m_playList_Music->clear();
}

int CSoundIntf::getMusicListSize()
{
    return m_playList_Music->mediaCount();
}

void CSoundIntf::addMusicUrl(QUrl _url)
{
    m_playList_Music->addMedia(_url);
}

void CSoundIntf::getMusicUrlList(QList<QUrl> &_list_url)
{
    _list_url.clear();
    for (int i = 0; i < m_playList_Music->mediaCount(); i++) {
        _list_url.append(m_playList_Music->media(i).request().url());
    }
}

void CSoundIntf::getMusicUrlStrList(QStringList &_list_url_str)
{
    _list_url_str.clear();
    for (int i = 0; i < m_playList_Music->mediaCount(); i++) {
        _list_url_str.append(m_playList_Music->media(i).request().url().toString());
    }
}

bool CSoundIntf::getMusicUrl(int _idx, QUrl &_url)
{
    bool is_succ = false;
    if (_idx >= 0 && _idx < m_playList_Music->mediaCount()) {
        _url = m_playList_Music->media(_idx).request().url();
        is_succ = true;
    }
    return is_succ;
}

bool CSoundIntf::removeMusic(int _idx)
{
    bool is_succ = false;
    if (_idx >= 0 && _idx < m_playList_Music->mediaCount()) {
        is_succ = m_playList_Music->removeMedia(_idx);
    }
    return is_succ;
}

QMediaPlaylist::PlaybackMode CSoundIntf::playbackModeToPlayerMode(enPlaybackMode _play_mode)
{
    QMediaPlaylist::PlaybackMode play_mode_qt = QMediaPlaylist::CurrentItemInLoop;

    if (playbackMode_SingleLoop == _play_mode) {
        play_mode_qt = QMediaPlaylist::CurrentItemInLoop;
    } else if (playbackMode_ListLoop == _play_mode) {
        play_mode_qt = QMediaPlaylist::Loop;
    }

    return play_mode_qt;
}

enPlaybackMode CSoundIntf::getPlaybackMode(QMediaPlaylist::PlaybackMode _play_mode_qt)
{
    enPlaybackMode play_mode = playbackMode_SingleLoop;

    switch (_play_mode_qt) {
    case QMediaPlaylist::CurrentItemOnce:
    case QMediaPlaylist::CurrentItemInLoop:
        play_mode = playbackMode_SingleLoop;
        break;
    case QMediaPlaylist::Sequential:
    case QMediaPlaylist::Loop:
    case QMediaPlaylist::Random:
        play_mode = playbackMode_ListLoop;
        break;
    default:
        break;
    }

    return play_mode;
}
