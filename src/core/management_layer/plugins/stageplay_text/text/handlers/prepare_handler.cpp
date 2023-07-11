#include "prepare_handler.h"

#include "../stageplay_text_edit.h"

#include <business_layer/templates/stageplay_template.h>
#include <business_layer/templates/templates_facade.h>

#include <QKeyEvent>
#include <QTextBlock>

using Ui::StageplayTextEdit;


namespace KeyProcessingLayer {

PrepareHandler::PrepareHandler(StageplayTextEdit* _editor)
    : AbstractKeyHandler(_editor)
{
}

bool PrepareHandler::needSendEventToBaseClass() const
{
    return m_needSendEventToBaseClass;
}

bool PrepareHandler::needEnsureCursorVisible() const
{
    return m_needEnsureCursorVisible;
}

bool PrepareHandler::needPrehandle() const
{
    return m_needPrehandle;
}

void PrepareHandler::prepareForHandle(QKeyEvent* _event)
{
    m_needSendEventToBaseClass = true;
    m_needEnsureCursorVisible = true;
    m_needPrehandle = false;

    //
    // Если нажат альт, шифт, или контрол, то не нужно прокручивать окно редактора к тому месту, где
    // установлен курсор
    //
    if (_event->modifiers().testFlag(Qt::AltModifier)
        || _event->modifiers().testFlag(Qt::MetaModifier)
        || _event->modifiers().testFlag(Qt::ShiftModifier)
        || _event->modifiers().testFlag(Qt::ControlModifier)) {
        m_needEnsureCursorVisible = false;
    }
}

void PrepareHandler::handleEnter(QKeyEvent* _event)
{
    m_needSendEventToBaseClass = _event->modifiers().testFlag(Qt::ShiftModifier) ? true : false;
    m_needPrehandle = true;
}

void PrepareHandler::handleTab(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
    m_needPrehandle = true;
}

void PrepareHandler::handleDelete(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
    m_needPrehandle = true;

    //
    // Получим необходимые значения
    //
    // ... курсор в текущем положении
    QTextCursor cursor = editor()->textCursor();

    //
    // Получим стиль первого блока в выделении
    //
    QTextCursor topCursor(editor()->document());
    topCursor.setPosition(qMin(cursor.selectionStart(), cursor.selectionEnd()));

    //
    // Получим стиль последнего блока в выделении
    //
    QTextCursor bottomCursor(editor()->document());
    bottomCursor.setPosition(qMax(cursor.selectionStart(), cursor.selectionEnd()));
}

void PrepareHandler::handleBackspace(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
    m_needPrehandle = true;

    //
    // Получим необходимые значения
    //
    // ... курсор в текущем положении
    QTextCursor cursor = editor()->textCursor();

    //
    // Получим стиль первого блока в выделении
    //
    QTextCursor topCursor(editor()->document());
    topCursor.setPosition(qMin(cursor.selectionStart(), cursor.selectionEnd()));

    //
    // Получим стиль последнего блока в выделении
    //
    QTextCursor bottomCursor(editor()->document());
    bottomCursor.setPosition(qMax(cursor.selectionStart(), cursor.selectionEnd()));
}

void PrepareHandler::handleEscape(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
    m_needPrehandle = false;
}

void PrepareHandler::handleUp(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
}

void PrepareHandler::handleDown(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
}

void PrepareHandler::handlePageUp(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
}

void PrepareHandler::handlePageDown(QKeyEvent*)
{
    m_needSendEventToBaseClass = false;
}

void PrepareHandler::handleOther(QKeyEvent* _event)
{
    m_needPrehandle = true;

    //
    // Получим необходимые значения
    //
    // ... курсор в текущем положении
    QTextCursor cursor = editor()->textCursor();

    //
    // Получим стиль первого блока в выделении
    //
    QTextCursor topCursor(editor()->document());
    topCursor.setPosition(qMin(cursor.selectionStart(), cursor.selectionEnd()));
    const auto& topStyle = editor()->stageplayTemplate().paragraphStyle(
        BusinessLayer::TextBlockStyle::forBlock(topCursor.block()));

    //
    // Получим стиль последнего блока в выделении
    //
    QTextCursor bottomCursor(editor()->document());
    bottomCursor.setPosition(qMax(cursor.selectionStart(), cursor.selectionEnd()));
    const auto& bottomStyle = editor()->stageplayTemplate().paragraphStyle(
        BusinessLayer::TextBlockStyle::forBlock(bottomCursor.block()));

    if (!_event->text().isEmpty()) {
        //
        // Не все стили можно редактировать
        //
        m_needSendEventToBaseClass = topStyle.isCanModify() && bottomStyle.isCanModify();
    } else {
        m_needSendEventToBaseClass = true;
    }
}

} // namespace KeyProcessingLayer
