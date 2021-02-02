#include "taskhelper.h"

#include <QRegularExpression>

#include "vnotex.h"
#include "notebookmgr.h"
#include <widgets/mainwindow.h>
#include <widgets/viewarea.h>
#include <widgets/viewwindow.h>
#include <widgets/dialogs/selectdialog.h>
#include <widgets/markdownviewwindow.h>
#include <widgets/textviewwindow.h>
#include <utils/pathutils.h>

using namespace vnotex;

QString TaskHelper::normalPath(const QString &p_text)
{
#if defined (Q_OS_WIN)
    return QString(p_text).replace('/', '\\');
#endif
    return p_text; 
}

QString TaskHelper::spaceQuote(const QString &p_text, const QString &p_chars)
{
    if (p_text.contains(' ')) {
        return QString("%1%2%1").arg(p_chars, p_text, p_chars);
    }
    return p_text;
}

QStringList TaskHelper::spaceQuote(QStringList &p_list, const QString &p_chars)
{
    QStringList list;
    for (const auto &s : p_list) {
        list << spaceQuote(s, p_chars);
    }
    return list;
}

QString TaskHelper::getCurrentFile()
{
    auto win = VNoteX::getInst().getMainWindow()->getViewArea()->getCurrentViewWindow();
    QString file;
    if (win && win->getBuffer()) {
        file = win->getBuffer()->getPath();
    }
    return file;
}

QSharedPointer<Notebook> TaskHelper::getCurrentNotebook()
{
    const auto &notebookMgr = VNoteX::getInst().getNotebookMgr();
    auto id = notebookMgr.getCurrentNotebookId();
    if (id == Notebook::InvalidId) return nullptr;
    return notebookMgr.findNotebookById(id);
}

QString TaskHelper::getFileNotebookFolder(const QString p_currentFile)
{
    const auto &notebookMgr = VNoteX::getInst().getNotebookMgr();
    const auto &notebooks = notebookMgr.getNotebooks();
    for (auto notebook : notebooks) {
        auto rootPath = notebook->getRootFolderAbsolutePath();
        if (PathUtils::pathContains(rootPath, p_currentFile)) {
            return rootPath;
        }
    }
    return QString();
}

QString TaskHelper::getSelectedText()
{
    auto window = VNoteX::getInst().getMainWindow()->getViewArea()->getCurrentViewWindow();
    {
        auto win = dynamic_cast<MarkdownViewWindow*>(window);
        if (win) {
            return win->selectedText();
        }
    }
    {
        auto win = dynamic_cast<TextViewWindow*>(window);
        if (win) {
            return win->selectedText();
        }
    }
    return QString();
}

QStringList TaskHelper::getAllSpecialVariables(const QString &p_name, const QString &p_text)
{
    QStringList list;
    QRegularExpression re(QString(R"(\$\{[\t ]*%1[\t ]*:[\t ]*(.*?)[\t ]*\})").arg(p_name));
    auto it = re.globalMatch(p_text);
    while (it.hasNext()) {
        list << it.next().captured(1);
    }
    return list;
}

QString TaskHelper::getPathSeparator()
{
#if defined (Q_OS_WIN)
    return "\\";
#else
    return "/";
#endif
}

TaskHelper::TaskHelper()
{
    
}
