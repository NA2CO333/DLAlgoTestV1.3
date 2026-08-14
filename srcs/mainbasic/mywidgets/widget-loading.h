#ifndef WIDGETLOADING_H
#define WIDGETLOADING_H

#include <QLabel>
#include <QMovie>

class WidgetLoading : public QLabel
{
    Q_OBJECT
public:
    explicit WidgetLoading(QWidget *_parent = nullptr);
    ~WidgetLoading();

signals:

protected:
    void showEvent(QShowEvent *_evt) override;
    void hideEvent(QHideEvent *_evt) override;

    QMovie *m_movie{nullptr};

};

#endif // WIDGETLOADING_H
