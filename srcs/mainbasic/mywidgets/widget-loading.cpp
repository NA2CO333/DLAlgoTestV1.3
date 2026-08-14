#include "widget-loading.h"

WidgetLoading::WidgetLoading(QWidget *_parent) : QLabel(_parent)
{
    m_movie = new QMovie(":/resource/loading.gif");
    this->setMovie(m_movie);
}

WidgetLoading::~WidgetLoading()
{
    m_movie->deleteLater();
    m_movie = nullptr;
}

void WidgetLoading::showEvent(QShowEvent *_evt)
{
    QLabel::showEvent(_evt);

    m_movie->start();
}

void WidgetLoading::hideEvent(QHideEvent *_evt)
{
    QLabel::hideEvent(_evt);

    m_movie->stop();
}
