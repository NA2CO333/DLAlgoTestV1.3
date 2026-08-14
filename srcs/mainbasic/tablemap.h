#ifndef TABLEMAP_H
#define TABLEMAP_H

#include <QObject>
#include <QWidget>
#include <QMap>
#include <mysqlitepatients.h>
#include <QDebug>
#include <QList>

// 分页的行和数据索引号（从 0 开始）的映射关系
class tableMap
{
public:
    tableMap();
    ~tableMap();

    void loadTableData(int size);

    QList<int> previousPage();
    QList<int> nextPage();
    QList<int> currentPage();
    QList<int> firstPage();
    QList<int> lastPage();
    bool gotoPageByIndex(int _idx, QList<int> &_list_index);            // 翻页到指定索引号

    int getPageByIndex(int _idx);           // 根据索引号获取页码（返回页码，页码从 1 开始）

    int totalPage();
    void clear();

    int getCurrentPageNum();
    int getTotalPageNum();

private:
    int page;           // 总页数
    int cutPage;        // 当前页码，从 1 开始
    QMap<int,int> indexToPage;      // 数据索引号（第一维，从 0 开始）和每页行号（第二维，从 1 开始）的映射关系
};

#endif // TABLEMAP_H
