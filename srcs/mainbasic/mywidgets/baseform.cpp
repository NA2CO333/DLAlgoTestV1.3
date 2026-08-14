#include "baseform.h"

#include <QApplication>
#include <QScreen>
#include <QFile>
#include <QTimer>

#include "statusbarform.h"
#include "global.h"

///=============================================================================================================
/// class CBaseFormIntf

//
CBaseFormIntf::CBaseFormIntf()
{

}

//
void CBaseFormIntf::centerWidget(QWidget *_widget, QWidget *_parent)
{
#if (QPA_PLATFORM_TYPE == 1)
    if (!_parent) {
        return;
    }
#else
    Q_UNUSED(_parent)
#endif

    if (!_widget) {
        return;
    }

    if (!_parent) {
        _parent = _widget->parentWidget();
    }

    QSize parent_size;
    if (_parent) {
        parent_size = _parent->size();
    } else {
        parent_size = QApplication::primaryScreen()->size();
    }

    QPoint point;
    if (parent_size.width() > _widget->width()) {
        point.setX((parent_size.width() - _widget->width()) / 2);
    } else {
        point.setX(0);
    }
    if (parent_size.height() > _widget->height()) {
        point.setY((parent_size.height() - _widget->height()) / 2);
    } else {
        point.setY(0);
    }

    if (_widget->parentWidget() == _parent) {
        _widget->move(point);
    } else {
        point.rx() += _parent->x();
        point.ry() += _parent->y();
        _widget->move(point);
    }
}

bool CBaseFormIntf::changeAppStyleSheet(const QString &_file_path)
{
    QString style_str;
    bool is_succ = Util::readFileToQStr(_file_path, style_str);
    if (is_succ) {
        qApp->setStyleSheet(style_str);
    }
    return is_succ;
}

void CBaseFormIntf::callAfterShow(QObject *_parent)
{
    /* 尝试了调用 show() 之后，且 qApp->processEvents() 之后执行，还是没能得到布局之后的部件尺寸，所以这里用定时器延时执行 */
    //QTimer::singleShot(1, _parent, [this] {
    //    this->afterShow();
    //});
}

///=============================================================================================================
/// class CBaseWidget

CBaseWidget::CBaseWidget(QWidget *parent, Qt::WindowFlags flags) : QFrame(parent, flags), CBaseFormIntf()
{
    //this->setObjectName("CBaseWidget");         // TODO: 增加这句 setObjectName() 后程序闪现主界面然后无界面显示？
    //this->setAutoFillBackground(true);

//#if (QPA_PLATFORM_TYPE != 1 && !SHOW_WINDOW_FRAME)
    this->setWindowFlag(Qt::FramelessWindowHint, true);
//#endif

}

void CBaseWidget::open()
{
    //
    m_isDialogMode = true;

    //
    //QDialog::open();
    this->setWindowModality(Qt::WindowModality::NonModal);
    QWidget::show();
}

void CBaseWidget::done(int _code)
{
    //
    m_isDialogMode = false;

    //
    //QDialog::done(_code);
    m_dialogCode = (QDialog::DialogCode)_code;
    emit sigDialogFinished(_code);
    QWidget::hide();
}

void CBaseWidget::showEvent(QShowEvent *_evt)
{
    //
    QWidget::showEvent(_evt);

    //
    emit sigVisibleChanged(true);
}

void CBaseWidget::hideEvent(QHideEvent *_evt)
{
    //
    QWidget::hideEvent(_evt);

    //
    emit sigVisibleChanged(false);
}

///=============================================================================================================
/// class CBaseMainWindow

CBaseMainWindow::CBaseMainWindow(QWidget *parent, Qt::WindowFlags flags) : QMainWindow(parent, flags), CBaseFormIntf()
{
    //this->setObjectName("CBaseMainWindow");       // TODO: 增加这句 setObjectName() 后程序闪现主界面然后无界面显示？
    //this->setAutoFillBackground(true);

//#if (QPA_PLATFORM_TYPE != 1 && !SHOW_WINDOW_FRAME)
    this->setWindowFlag(Qt::FramelessWindowHint, true);
//#endif

}

///=============================================================================================================
/// class CBaseDialog

CBaseDialog::CBaseDialog(QWidget *parent, Qt::WindowFlags flags) : QDialog(parent, flags), CBaseFormIntf()
{
    //this->setObjectName("CBaseDialog");         // TODO: 增加这句 setObjectName() 后程序闪现主界面然后无界面显示？
    //this->setAutoFillBackground(true);

//#if (QPA_PLATFORM_TYPE != 1 && !SHOW_WINDOW_FRAME)
    this->setWindowFlag(Qt::FramelessWindowHint, true);
//#endif

}

bool CBaseWidget::isDialogMode()
{
    return m_isDialogMode;
}
