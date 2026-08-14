#include "batchuploadlist.h"
#include "ui_batchuploadlist.h"

#include <QPushButton>
#include <QPainter>
#include <QHeaderView>

#include "global.h"

//
QVector<QCheckBox*> batchUploadList::batchUploadselect;
std::vector<Patients> batchUploadList::mLk;
MySQLitePatients *batchUploadList::mysql = NULL;
QStringList  batchUploadList::batchList;

batchUploadList::batchUploadList(QWidget *parent) :
    CBaseDialog(parent),
    ui(new Ui::batchUploadList)
{
    ui->setupUi(this);

    int row = batchList.size()/2;
    int rNum = batchList.size()%2;
    if(rNum>0)
        row++;
    qDebug()<<"row:"<<row;
    this->setGeometry(150,90,500,300);
    this->setStyleSheet("border:3px black;background-color:rgb(250,250,250);");
    tableWidget = new QTableWidget(row,2,this);
    tableWidget->setRowCount(row);
    tableWidget->setColumnCount(2);
    tableWidget->setColumnWidth(0,200);
    tableWidget->setColumnWidth(1,200);
    tableWidget->setEditTriggers( QAbstractItemView::NoEditTriggers );
    tableWidget->verticalHeader()->hide();
    tableWidget->horizontalHeader()->hide();
    tableWidget->setShowGrid(false);
    tableWidget->setFrameShape(QFrame::NoFrame);

    int Height = row*40;
    if(Height>300)
        Height = 300;

    tableWidget->setBaseSize(440,Height);
    ui->vLayout->addSpacerItem(new QSpacerItem(20, 50, QSizePolicy::Minimum, QSizePolicy::Expanding));
    ui->vLayout->addWidget(tableWidget);
    ui->vLayout->addSpacerItem(new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding));
    addCheckBox(batchList);

    QPushButton* yesButton =  ui->buttonBox->button(QDialogButtonBox::Ok);
    QPushButton* noButton = ui->buttonBox->button(QDialogButtonBox::Cancel);
    yesButton->setMinimumSize(110,40);
    noButton->setMinimumSize(110,40);
    yesButton->setStyleSheet("border:1px groove gray;background-color:rgb(250,250,250);");
    noButton->setStyleSheet("border:1px groove gray;;background-color:rgb(250,250,250);");
    qDebug()<<"uploadlist setup Ui finish";

    if(language){
        ui->title->setText("请选择筛查批次");
        yesButton->setText("上传");
        noButton->setText("取消");
    }
    else{
        ui->title->setText("Please select batch num");
    }


}

batchUploadList::~batchUploadList()
{
    delete ui;
}


void batchUploadList::addCheckBox(QStringList list)
{
    int size = list.size();
    int row_num = size/2;
    if((size%2)>0){
        row_num += 1;
    }
    qDebug()<<"addcheckBox list:"<<list;

    int temp = size;
    for(int row=0;row<row_num;row++){
        for(int col=0;col<2;col++){
            if(temp>0){
                QWidget *mCheckBox = new QWidget(tableWidget);
                QHBoxLayout *mLayout = new QHBoxLayout(mCheckBox);
                QString contain = list.at(size-temp);
                if(contain==""){
                    qDebug()<<"contain==NULL";
                    if(language)
                        contain = "无批号";
                    else
                        contain = "No batchNo";
                }
                QCheckBox *qc = new QCheckBox(contain,this);
                batchUploadselect.push_back(qc);
                mLayout->addWidget(qc);
                mLayout->setAlignment(Qt::AlignHCenter);
                //    mLayout->setMargin(0);
                mLayout->setContentsMargins(20,5,20,5);
                mCheckBox->setLayout(mLayout);

                tableWidget->setCellWidget(row,col,mCheckBox);
                qDebug()<<"<row:"<<row<<",col:"<<col<<"> "<<contain;
                temp--;
            }
        }
    }


}

bool batchUploadList::ScreenBatchNo(QString batchNo)
{
    /*
    bool state = true;
    QStringList::Iterator iter = batchList.begin();
    for(;iter!=batchList.end();iter++){
        QString text = *iter;
        if(text==batchNo){
            state = false;
            qDebug()<<"finded:"<<batchNo<<"in the batchList";
        }
    }
    if(state){
        batchList.push_back(batchNo);
    }

    return state;

    */

    if(!batchList.contains(batchNo))
    {
        batchList.push_back(batchNo);
        return true;
    }
    else
        return false;
}

bool batchUploadList::getBatchNo()
{
    mLk.clear();
    mysql = MySQLitePatients::getInstance();
    mLk = mysql->getInforForBatch(true);     //db信息获得
    clearList();
    qDebug()<<"mLk.size="<<mLk.size();
    for(int i=0;i<mLk.size();i++){
        QString batchNo = mLk.at(i).batchNo;
        if(batchNo != "")
            ScreenBatchNo(batchNo);
    }

    qDebug()<<"batchNo.size:"<<batchList.size();
    if(batchList.size()>0)
        return true;
    else
        return false;
}

void batchUploadList::clearList()
{
    qDebug()<<"clear batchUploatList";
    QVector<QCheckBox*>::Iterator iter = batchUploadList::batchUploadselect.begin();
    for(;iter!=batchUploadList::batchUploadselect.end();iter++){
        QCheckBox *mCheckBox = *iter;
        delete mCheckBox;
        mCheckBox = nullptr;
    }
    batchUploadselect.clear();
    batchList.clear();
}

void batchUploadList::paintEvent(QPaintEvent *)
{
    QPainter pt(this);
    pt.setPen(QPen(Qt::gray,3,Qt::SolidLine));
    pt.drawRect(0,0,499,299);
}

void batchUploadList::on_buttonBox_accepted()
{
    qDebug()<<"accepted press!!";
    batchList.clear();
    QVector<QCheckBox*>::Iterator iter = batchUploadList::batchUploadselect.begin();
    for(;iter!=batchUploadList::batchUploadselect.end();iter++){
        QCheckBox *mCheckBox = *iter;
        if(mCheckBox->isChecked()){
            QString text = mCheckBox->text();
            qDebug()<<text<<"isChecked";
            batchList.push_back(text);
        }
    }
    qDebug()<<"batchList:"<<batchList;

}
