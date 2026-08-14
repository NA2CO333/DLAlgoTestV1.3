#ifndef MPROWXSVCQRCODE_H
#define MPROWXSVCQRCODE_H

#include <QFrame>

namespace Ui {
class MProWxSvcQrCode;
}

class MProWxSvcQrCode : public QFrame
{
    Q_OBJECT

public:
    explicit MProWxSvcQrCode(QWidget *parent = nullptr);
    ~MProWxSvcQrCode();

protected:
    void showEvent(QShowEvent *_evt) override;

    bool m_isFirstShow {true};      // 是否第一次显示

private:
    Ui::MProWxSvcQrCode *ui;
};

#endif // MPROWXSVCQRCODE_H
