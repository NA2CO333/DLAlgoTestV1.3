#ifndef CWAITINGMOVIE_H
#define CWAITINGMOVIE_H

#include <QMovie>

#include "mylabel.h"

// 等待动画
class CWaitingMovie : public CMyLabel
{
    Q_OBJECT
public:
    explicit CWaitingMovie(QWidget *_parent, QString _uri, int _width);

    void setIsPlaying(bool _is_playing);

protected:
    QMovie *movie = Q_NULLPTR;


signals:

};

#endif // CWAITINGMOVIE_H
