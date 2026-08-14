#ifndef CREPORTTEXT_H
#define CREPORTTEXT_H

#include <QObject>
#include <QString>
#include <QVector>

namespace Common {

/* =============================================================================
“纯文本报表工具”

设计目标：实现一定的自动排版功能，简化小票格式的文字排版编程。

软件模型：

一、小票模板的设置
1、可设置行最大宽度。
2、先添加行。
3、然后在各行中添加0个或多个“域”。
4、每个域都有“宽度”属性，用于对齐。
5、每个域都有“对齐方式”属性，以及文本超出宽度时的处理方式。
6、计算文本长度时，中文字符的宽度为英文宽度的一半。

二、小票文本的生成
1、设置各个域的文本。
2、调用 getText() 方法，即可得到格式化好的小票文本。

============================================================================= */

// 预声明
class CReportTextLine;
class CReportTextField;

// =====================================================================================================================
// class CReportText
// =====================================================================================================================

// 纯文本报表（基类）
class CReportText : public QObject
{
    Q_OBJECT
public:
    explicit CReportText(QObject *_parent = nullptr);

    void setLineWidth(int _width) { m_lineWidth = _width; }     // 设置行宽（不包括换行符）

    virtual void updateData() = 0;                              // 更新报表数据
    bool getText(QString &_text, QString &_err_msg);            // 获取报表文本

    CReportTextLine &operator<<(const CReportTextLine &_line);        // 添加新行到报表的末尾

protected:
    virtual bool buildReportLayout(QString &_err_msg) = 0;      // 构造报表布局

    QVector<CReportTextLine> m_lines;
    int m_lineWidth {-1};

};

// =====================================================================================================================
// class CReportTextLine
// =====================================================================================================================

//
class CReportTextLine
{
    friend class CReportText;
public:
    CReportTextLine();

    bool getText(QString &_text, QString &_err_msg);

    CReportTextLine &operator<<(const CReportTextField &_field);      // 添加域到本行

protected:
    void setLineWidth(int _width) { m_lineWidth = _width; }

    QString m_endLine {"\r\n"};         // 换行符
    QVector<CReportTextField> m_fields;
    int m_lineWidth {-1};
    bool m_isAutoAdaptDone {false};     // 宽度自适应是否已处理

};

// =====================================================================================================================
// class CReportTextField
// =====================================================================================================================

//
class CReportTextField
{
    friend class CReportTextLine;
public:
    CReportTextField(const QString &_text = "", int _width = -1, Qt::AlignmentFlag _alignment = Qt::AlignLeft);

    void setText(QString _text);
    const QString &rawText() { return m_text; }
    const QString &formattedText();                             // 格式化后的文本

    int fieldWidth() const { return m_fieldWidth; }             // 预设的域宽（单位为半角字符宽度）
    int rawWidth() const { return m_rawWidth; };                // 原文宽度（单位为半角字符宽度）
    int formattedWidth();                                       // 格式化后的宽度（单位为半角字符宽度）

    Qt::AlignmentFlag alignment() { return m_alignment; }       // 对齐方式。支持 Qt::AlignLeft, Qt::AlignRight, Qt::AlignHCenter, Qt::AlignCenter

    void setCanTruncation(bool _can_truncation) { m_canTruncation = _can_truncation; }  // 能否截断（超出长度的部分以...显示）
    bool canTruncation() const { return m_canTruncation; }

    CReportTextField &operator=(const QString &_text) { setText(_text); return (*this); }

protected:
    void setFieldWidth(int _width);
    void formatText();

    QString m_text;
    QString m_formattedText;

    int m_fieldWidth {-1};          // 预设的域宽（单位为半角字符宽度）
    int m_rawWidth {-1};            // 原文宽度（单位为半角字符宽度）
    int m_formattedWidth {-1};      // 格式化后的宽度（单位为半角字符宽度）

    Qt::AlignmentFlag m_alignment {Qt::AlignLeft};
    bool m_canTruncation {false};                       // 能否截断（超出长度的部分以...显示）

};

}   // namespace Common

#endif // CREPORTTEXT_H
