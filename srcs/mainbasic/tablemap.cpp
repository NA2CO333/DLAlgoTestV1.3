#include "tablemap.h"

// 每页行数
static const int SIZE_PER_PAGE = 5;

//
tableMap::tableMap()
{
    page = 1;
    cutPage = 1;
    indexToPage.clear();
}

tableMap::~tableMap()
{

}

void tableMap::loadTableData(int size)
{
    this->clear();

    //if (size <= 0 || size > MAX_RECORD_COUNT * 10) {        // TODO: 这里不应该做最大记录数限制，系统瓶颈不应该在这里，而且更不应该在记录数超限时完全没有显示而无任何反馈
    //    return;
    //}

    //
    int row = 0;
    for (int i = 0; i < size; i++) {
        row++;
        if (row > SIZE_PER_PAGE) {
            page++;
            row = 1;
        }
        indexToPage.insert(i,page);
    }
    if (cutPage > page)
        cutPage = page;
    qDebug()<<"loadTableData--total pages:"<<page;
}

QList<int> tableMap::previousPage()
{
    if(cutPage > 1){
        cutPage--;
        qDebug()<<"currentPage="<<cutPage;

        return indexToPage.keys(cutPage);
    }
    else
        return indexToPage.keys(1);
}

QList<int> tableMap::nextPage()
{
    if(cutPage < page){
        cutPage++;
        qDebug()<<"currentPage="<<cutPage;

        return indexToPage.keys(cutPage);
    }
    else
        return indexToPage.keys(page);

}

QList<int> tableMap::currentPage()
{
    if(cutPage>page)
        cutPage = page;
    qDebug()<<"currentPage="<<cutPage<<",page="<<page;
    return indexToPage.keys(cutPage);

}

QList<int> tableMap::firstPage()
{
    cutPage = 1;
    return indexToPage.keys(1);
}

QList<int> tableMap::lastPage()
{
    cutPage = page;
    return indexToPage.keys(page);
}

bool tableMap::gotoPageByIndex(int _idx, QList<int> &_list_index)
{
    int page_idx = getPageByIndex(_idx);
    if (page_idx >= 1 && page_idx <= page) {
        cutPage = page_idx;
        _list_index = indexToPage.keys(cutPage);
        return true;
    } else {
        return false;
    }
}

int tableMap::getPageByIndex(int _idx)
{
    if (_idx >= 0 && _idx < indexToPage.size()) {
        return indexToPage.value(_idx);
    } else {
        return -1;
    }
}

int tableMap::totalPage()
{
    return page;
}

void tableMap::clear()
{
    page = 1;
    indexToPage.clear();
}

int tableMap::getCurrentPageNum()
{
    return cutPage;
}

int tableMap::getTotalPageNum()
{
    return page;
}
