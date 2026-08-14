#include "report-text.h"

#include <QDebug>

#include "util-common.h"

namespace Common {

// =====================================================================================================================
// class CReportText
// =====================================================================================================================

CReportText::CReportText(QObject *_parent) : QObject(_parent)
{

}

bool CReportText::getText(QString &_text, QString &_err_msg)
{
    //
    if (m_lines.isEmpty()) {
        bool succ_template = buildReportLayout(_err_msg);
        if (!succ_template) {
            _err_msg = "Failed to generate template: " + _err_msg;
            return false;
        }
    }

    //
    _text.clear();
    _err_msg.clear();

    //
    bool succ = true;
    QString line_str;
    for (int i = 0; i < m_lines.size(); i++) {
        CReportTextLine &line = m_lines[i];
        succ = line.getText(line_str, _err_msg);
        if (succ) {
            _text += line_str;
        } else {
            break;
        }
    }

    //qDebug() << _text.toLocal8Bit().constData();

    //
    return succ;

}

CReportTextLine &CReportText::operator<<(const CReportTextLine &_line)
{
    //
    m_lines.append(_line);

    //
    CReportTextLine &line = m_lines.last();
    line.setLineWidth(m_lineWidth);

    //
    return line;
}

// =====================================================================================================================
// class CReportTextLine
// =====================================================================================================================

CReportTextLine::CReportTextLine()
{

}

bool CReportTextLine::getText(QString &_text, QString &_err_msg)
{
    // 域宽度自适应处理
    if (!m_isAutoAdaptDone) {
        if (m_fields.size() == 1) {         // 若是只有一域，且是右对齐，则自动占满整行
            CReportTextField &field = m_fields[0];
            if (Qt::AlignRight == field.alignment() ||
                    Qt::AlignCenter == field.alignment() ||
                    Qt::AlignHCenter == field.alignment()
                    ) {
                field.setFieldWidth(m_lineWidth);
            }
        } else if (m_fields.size() == 2) {  // 若是有两域，且分别是左对齐和右对齐，且行宽度足够，则自动适配宽度
            CReportTextField &field_1 = m_fields[0];
            CReportTextField &field_2 = m_fields[1];
            if (Qt::AlignLeft == field_1.alignment() && Qt::AlignRight == field_2.alignment()) {
                int width_1 = field_1.formattedWidth();
                int width_2 = field_2.formattedWidth();
                if (width_1 + width_2 < m_lineWidth) {
                    field_1.setFieldWidth(width_1);
                    field_2.setFieldWidth(m_lineWidth - width_1);
                } else {    // 若行宽度不足，则左侧域加换行，右侧域宽度等于打印宽度
                    field_1.setText(field_1.rawText() + "\n");
                    field_2.setFieldWidth(m_lineWidth);
                }
            }
        }

        //
        m_isAutoAdaptDone = true;
    }

    //
    _text.clear();
    _err_msg.clear();

    //
    bool succ = true;
    int width_occupied = 0;     // 被占用的宽度
    QString str_curr;           // 当前域的文本
    int col_last = 0;           // 当前渲染到的列
    for (int i = 0; i < m_fields.size(); i++) {
        CReportTextField &field = m_fields[i];
        str_curr = field.formattedText();

        // 若超出长度且不可截断，则所在行的后续域移到下一行，且下移的域的列位置保持不变，且本域的文字可延伸到新行的开头一直到下一个域的头部
        if (field.formattedWidth() > field.fieldWidth() && !field.canTruncation()) {
            int col_curr = (col_last + field.formattedWidth()) % m_lineWidth;   // 当前域的末尾位置

            // 若最后一字符位置超出了下一域的开头，则添加换行              // TODO: 若本行有足够的空格位置，则自适应宽度
            if (i < m_fields.size() - 1) {
                int diff_align = col_curr - (col_last + field.fieldWidth());      // 域对齐的差（实际位置相对于预料位置的差）
                if (diff_align > 0) {
                    int right_raw_width = 0;        // 右侧所有域的原始宽度和
                    for (int i_c = i + 1; i_c < m_fields.size(); i_c++) {
                        right_raw_width += m_fields.at(i_c).rawWidth();
                    }

                    if (right_raw_width >= m_lineWidth - col_curr && i < m_fields.size() - 2) {      /* 若是最后一域及其前一域，不用加换行 */
                        str_curr += m_endLine;
                        // 在新行的开头填充空格，直到下一域的开始位置
                        str_curr += QString(' ').repeated(col_last + field.fieldWidth());
                    } else {
                        width_occupied += diff_align;
                    }
                }
            }
        }

        //
        _text += str_curr;
        col_last += Util::calcMonospacePrintingLength(str_curr);
    }

    //
    _text += m_endLine;

    //
    return succ;
}

CReportTextLine &CReportTextLine::operator<<(const CReportTextField &_field)
{
    m_fields.append(_field);
    return (*this);
}

// =====================================================================================================================
// class CReportTextField
// =====================================================================================================================

CReportTextField::CReportTextField(const QString &_text, int _width, Qt::AlignmentFlag _alignment)
    : m_text(_text), m_fieldWidth(_width), m_alignment(_alignment)
{
    m_rawWidth = Util::calcMonospacePrintingLength(_text);
}

void CReportTextField::setText(QString _text)
{
     m_text = _text;
     m_rawWidth = Util::calcMonospacePrintingLength(_text);
     m_formattedText.clear();
}

void CReportTextField::formatText()
{
    //
    m_formattedText.clear();
    m_formattedWidth = -1;

    //
    QString text;
    if (m_rawWidth > m_fieldWidth) {
        if (!m_canTruncation) {
            text = m_text;
        } else {
            text = m_text.left(m_fieldWidth - 3) + "...";
        }
    } else if (m_rawWidth < m_fieldWidth) {
        int len_space = m_fieldWidth - m_rawWidth;
        QString spaces = QString(' ').repeated(len_space);
        if (Qt::AlignLeft == m_alignment) {             // 左对齐
            text = m_text + spaces;
        } else if (Qt::AlignRight == m_alignment) {     // 右对齐
            text = spaces + m_text;
        } else {                                        // 居中对齐
            int len_half = len_space / 2;
            text = spaces.left(len_half) + m_text + spaces.right(len_space - len_half);
        }
    } else {
        text = m_text;
    }

    //
    m_formattedText = text;
    m_formattedWidth = Util::calcMonospacePrintingLength(text);
}

const QString &CReportTextField::formattedText()
{
    if (m_formattedText.isEmpty() /*&& !m_text.isEmpty()*/) {
        formatText();
    }
    return m_formattedText;
}

int CReportTextField::formattedWidth()
{
    //
    if (m_formattedWidth < 0 /*&& !m_text.isEmpty()*/) {
        formatText();
    }

    //
    return m_formattedWidth;
}

void CReportTextField::setFieldWidth(int _width)
{
    //
    m_fieldWidth = _width;

    // 若文本不为空，则重新排版
    if (!m_formattedText.isEmpty()) {
        m_formattedText.clear();
        m_formattedWidth = -1;
    }
}

}   // namespace Common
