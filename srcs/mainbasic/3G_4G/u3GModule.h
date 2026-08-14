/*
 * u3GModule.h
 *
 *  Created on: 2012-7-12
 *      Author: lkp
 */

#ifndef U3GMODULE_H_
#define U3GMODULE_H_

#include <QWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QLabel>
#include <QTextEdit>
#include <QPushButton>
#include <QFile>
#include <QTimer>
#include <QRadioButton>
#include <QGroupBox>
#include <sys/types.h>
class Q3GModule:public QWidget
{
	Q_OBJECT
public:
	Q3GModule(QWidget *parent=0);
	virtual ~Q3GModule();
private:
	QTextEdit *m_pMsg;
	QPushButton *m_pConfirm;
	QPushButton *m_pCancel;
    QPushButton *m_pbClose;
    QComboBox *m_cbType;
    QRadioButton *m_Mobile;
    QRadioButton *m_Unicom;
    QRadioButton *m_Telecom;
    QLineEdit *m_ip;
    QLineEdit *m_remote;
    QLineEdit *m_dns1;
    QLineEdit *m_dns2;
    QLabel *m_State;
	QFile *m_pFile;
    QTimer *m_pTimer;
    QTimer *m_pDialTimer;
    int m_iReadLen;
    char cData[200];
    int m_iWaitCount;
private slots:
	void pbConfirmClicked();
	void pbCancelClicked();
    void pbCloseClicked();
	void readFile();
	void isSwitchSuccess();
};

#endif /* U3GMODULE_H_ */
