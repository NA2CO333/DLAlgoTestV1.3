#include "widget-optical-type-options.h"
#include "ui_widget-optical-type-options.h"

//
WidgetOpticalTypeOptions *WidgetOpticalTypeOptions::s_instance {nullptr};

WidgetOpticalTypeOptions *WidgetOpticalTypeOptions::instance()
{
    if (!s_instance) {
        s_instance = new WidgetOpticalTypeOptions();
    }
    return s_instance;
}

WidgetOpticalTypeOptions::WidgetOpticalTypeOptions(QWidget *_parent) :
    CBaseDialog(_parent),
    ui(new Ui::WidgetOpticalTypeOptions)
{
    //
    ui->setupUi(this);

    //


}

void WidgetOpticalTypeOptions::hideEvent(QHideEvent *_evt)
{
    //
    QDialog::hideEvent(_evt);

    //
    if (this->parentWidget()) {
        this->setParent(nullptr);       // 若没有这个动作，若对话框因父窗口的隐藏而被隐藏，QDialog::exec() 函数居然不会返回？！
    }
}

WidgetOpticalTypeOptions::~WidgetOpticalTypeOptions()
{
    delete ui;
}

void WidgetOpticalTypeOptions::on_btnOpticalTypeGeneral_clicked()
{
    m_selectedOpticalPathType = opticalPathType_General;
    this->accept();
}

void WidgetOpticalTypeOptions::on_btnOpticalTypeLShape_clicked()
{
    m_selectedOpticalPathType = opticalPathType_LShape;
    this->accept();
}

void WidgetOpticalTypeOptions::on_btnOpticalTypeSquare_clicked()
{
    m_selectedOpticalPathType = opticalPathType_Square;
    this->accept();
}
