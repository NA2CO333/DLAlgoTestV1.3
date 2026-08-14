#include "waitingmovie.h"


//
CWaitingMovie::CWaitingMovie(QWidget *_parent, QString _uri, int _width) : CMyLabel(_parent)
{
    //
    this->setFrameShape(QFrame::NoFrame);

    //
    movie = new QMovie(_uri);

    //movie->setScaledSize(QSize(_width, _width));
    //movie->setSpeed(175);
    //qDebug() << movie->speed();
    //qDebug() << movie->scaledSize();      // TODO: 输出 (-1, -1) ？能不能得到 gif 的尺寸？

    this->setGeometry(0, 0, _width, _width);

    //
    this->setMovie(movie);
    this->setVisible(false);

}

void CWaitingMovie::setIsPlaying(bool _is_playing)
{

    if(_is_playing)
    {
        if (this->isHidden()) {
            this->setVisible(true);    //显示眨眼
            movie->start();
            qApp->processEvents();
        }
    }
    else
    {
        if (!this->isHidden()) {
            movie->stop();     //停止眨眼
            this->setVisible(false);
            qApp->processEvents();
        }
    }

}
