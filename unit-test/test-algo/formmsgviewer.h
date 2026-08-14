#ifndef FORMMSGVIEWER_H
#define FORMMSGVIEWER_H

#include <QWidget>

namespace Ui {
class FormMsgViewer;
}

class FormMsgViewer : public QWidget
{
    Q_OBJECT

public:
    explicit FormMsgViewer(QWidget *parent = nullptr);
    ~FormMsgViewer();

    static FormMsgViewer instance;

    void setText(QString &_txt);
    void setText(QStringList &_list_str);

protected:
    void showEvent(QShowEvent *_event);

private:
    Ui::FormMsgViewer *ui;
};

//
FormMsgViewer *getMsgViewer();

#endif // FORMMSGVIEWER_H
