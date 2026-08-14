#include "mpro-wx-svc-qr-code.h"
#include "ui_mpro-wx-svc-qr-code.h"

#include <QTimer>
#include <QApplication>

#include "global.h"
#include "mpro-sys-communic.h"
#include "windowsmanager.h"
#include "utilui.h"
#include "winmanage.h"

//
MProWxSvcQrCode::MProWxSvcQrCode(QWidget *parent) :
    QFrame(parent),
    ui(new Ui::MProWxSvcQrCode)
{
    ui->setupUi(this);

    //
    Util::Ui::clearStyleSheet(this);

    //
    ui->lblImage->setWordWrap(true);

    //
    ui->lblLoading->setVisible(false);

}

MProWxSvcQrCode::~MProWxSvcQrCode()
{
    delete ui;
}

void MProWxSvcQrCode::showEvent(QShowEvent *_evt)
{
    //
    QWidget::showEvent(_evt);

    // 尺寸适配
    if (m_isFirstShow) {
        QRect rect_parent = this->geometry();
        ui->lblImage->setGeometry(0, 0, rect_parent.width(), rect_parent.height());
        Util::Ui::centerWidget(ui->lblLoading);
        m_isFirstShow = false;
    }

    // 重新载入二维码
    globalService()->mproSysCommunic()->setDevCode(CGlobal::devNum);

    const QPixmap &qr_code = globalService()->mproSysCommunic()->wxServiceQrCodeImage();
    if (!qr_code.isNull()) {
        ui->lblImage->setPixmap(qr_code);
    } else {
        //
        if (g_WifiIntf->getIsConnected()) {         // NOTE: 这里如果未联网就访问服务器，会导致界面没响应。    // TODO: 等待期间避免阻塞 UI 线程？网络请求可由用户终止？
            // 清空旧二维码
            ui->lblImage->setPixmap(QPixmap());

            // 显示等待动画
            ui->lblLoading->setVisible(true);
            qApp->processEvents();

            //
            QTimer::singleShot(1, this, [this] () {                 // NOTE: 通过定时事件使此过程在之后的事件循环中被处理，避免妨碍底层的界面刷新
                // MPro微信服务二维码图片（激活二维码）下载
                //globalService()->mproSysCommunic()->setServiceAddr();      // NOTE: 此类内部已包含合适的地址，此处略过设置
                globalService()->mproSysCommunic()->setDevCode(CGlobal::devNum);

                QString err_msg_mpro_wx_svc_qr_code;
                bool succ_mpro_wx_svc_qr_code = globalService()->mproSysCommunic()->requestWxServiceQrCodeImage(err_msg_mpro_wx_svc_qr_code);
                if (succ_mpro_wx_svc_qr_code) {
                    const QPixmap &qr_code = globalService()->mproSysCommunic()->wxServiceQrCodeImage();
                    ui->lblImage->setPixmap(qr_code);
                } else {
                    logCritical("Failed to get MPro service QrCode image:\n" + err_msg_mpro_wx_svc_qr_code);
                    ui->lblImage->setText(tr("获取二维码失败！错误：\n") + err_msg_mpro_wx_svc_qr_code);    // "Failed to get QrCode! Error:\n"
                }

                // 隐藏等待动画
                ui->lblLoading->setVisible(false);
            });
        } else {
            ui->lblImage->setText(tr("设备未联网，下载二维码失败！"));  // "Device not connected to the internet, download QR code failed!"
        }
    }
}
