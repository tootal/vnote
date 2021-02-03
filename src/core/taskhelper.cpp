#include "taskhelper.h"

#include <QRegularExpression>
#include <QJsonArray>

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

QString TaskHelper::replaceAllSepcialVariables(const QString &p_name, 
                                               const QString &p_text, 
                                               const QMap<QString, QString> &p_map)
{
    auto text = p_text;
    for (auto i = p_map.begin(); i != p_map.end(); i++) {
        auto key = QString(i.key()).replace(".", "\\.").replace("[", "\\[").replace("]", "\\]");
        auto pattern = QRegularExpression(QString(R"(\$\{[\t ]*%1[\t ]*:[\t ]*%2[\t ]*\})").arg(p_name, key));
        text = text.replace(pattern, i.value());
    }
    return text;
}

QString TaskHelper::evaluateJsonExpr(const QJsonObject &p_obj, const QString &p_expr)
{
    QJsonValue value = p_obj;
    for (auto token : p_expr.split('.')) {
        auto pos = token.indexOf('[');
        auto name = token.mid(0, pos);
        value = value.toObject().value(name);
        if (pos == -1) continue;
        if (token.back() == ']') {
            for (auto idx : token.mid(pos+1, token.length()-pos-2).split("][")) {
                bool ok;
                auto index = idx.toInt(&ok);
                if (!ok) throw "Config variable syntax error!";
                value = value.toArray().at(index);
            }
        } else {
            throw "Config variable syntax error!";
        }
    }
    if (value.isBool()) {
        if (value.toBool()) return "true";
        else return "false";
    } else if (value.isDouble()) {
        return QString::number(value.toDouble());
    } else if (value.isNull()) {
        return "null";
    } else if (value.isUndefined()) {
        return "undefined";
    }
    return value.toString();
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
