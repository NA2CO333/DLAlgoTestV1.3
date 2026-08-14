#ifndef DATEPAGE_H
#define DATEPAGE_H

#include <QComboBox>
#include <QCalendarWidget>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QUdpSocket>

#include "baseform.h"
#include "statusbarform.h"

namespace Ui {
class datePage;
}

class datePage : public CBaseWidget
{
    Q_OBJECT

public:
    explicit datePage(QWidget *parent = 0);
    ~datePage();

public slots:
#if (1 == OS_TYPE)
    void slot_updateNetworkTime();
#endif

private:
    Ui::datePage *ui;

    QUdpSocket *udpsocket;

    QDateTime dateTimeEditing;

protected:
    void showEvent(QShowEvent *) override;
    void paintEvent(QPaintEvent *) override;

#if (1 == OS_TYPE)
    void connectsucess();
    void readingDataGrams();
#endif

    void setBusiDataToUi(QDateTime _date_time);
    bool checkIsNeedSave();

    void Save_prompt_dialog();

    void doAfterYearMonthChanged();

    void setCbbYearValue(int _year);
    void setCbbMonthValue(int _month);

    void setCbbHourValue(int _hour);
    void setCbbMinuteValue(int _minute);

private slots:
    void on_cbbYear_activated(int _idx);
    void on_cbbMonth_activated(int _idx);

    void on_pushButton_Save_clicked();
    void on_pushButton_Back_clicked();
    void on_pushButton_Home_clicked();
    void on_calendarWidget_clicked(const QDate &_date);
};

#endif // DATEPAGE_H
