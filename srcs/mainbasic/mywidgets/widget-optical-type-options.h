#ifndef WIDGETOPTICALTYPEOPTIONS_H
#define WIDGETOPTICALTYPEOPTIONS_H

#include <QDialog>

#include "baseform.h"
#include "globaltypes.h"

namespace Ui {
class WidgetOpticalTypeOptions;
}

class WidgetOpticalTypeOptions : public CBaseDialog
{
    Q_OBJECT

public:
    static WidgetOpticalTypeOptions *instance();
    ~WidgetOpticalTypeOptions();

    enOpticalPathType selectedOpticalPathType() { return m_selectedOpticalPathType; }

protected:
    explicit WidgetOpticalTypeOptions(QWidget *_parent = nullptr);
    static WidgetOpticalTypeOptions *s_instance;

    void hideEvent(QHideEvent *_evt) override;

    enOpticalPathType m_selectedOpticalPathType {opticalPathType_General};

private slots:
    void on_btnOpticalTypeGeneral_clicked();
    void on_btnOpticalTypeLShape_clicked();
    void on_btnOpticalTypeSquare_clicked();
private:
    Ui::WidgetOpticalTypeOptions *ui;
};

#endif // WIDGETOPTICALTYPEOPTIONS_H
