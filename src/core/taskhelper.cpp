#include "taskhelper.h"

#include <QRegularExpression>
#include <QJsonArray>
#include <QMessageBox>
#include <QPushButton>
#include <QInputDialog>

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

QString TaskHelper::handleCommand(const QString &p_text, 
                                  QProcess *p_process)
{
    QRegularExpression re(R"(^::([a-zA-Z-]+)(.*?)?::(.*?)$)", 
                          QRegularExpression::MultilineOption);
    auto i = re.globalMatch(p_text);
    while (i.hasNext()) {
        auto match = i.next();
        auto cmd = match.captured(1).toLower();
        auto args = match.captured(2).trimmed().split(',');
        auto value = match.captured(3);
        
        QMap<QString, QString> arg;
        for (const auto &i : args) {
            auto s = i.trimmed();
            auto p = s.indexOf('=');
            auto name = s.mid(0, p);
            QString val;
            if (p != -1) {
                val = s.mid(p+1);
            }
            arg.insert(name, val);
        }
        if (cmd == "show-messagebox") {
            QMessageBox box;
            box.setText(value);
            box.setWindowTitle(arg.value("title"));
            QVector<QPushButton*> buttons;
            // add buttons
            {
                auto btns = arg.value("buttons").split("|");
                for (const auto &btn : btns) {
                    QPushButton *button = box.addButton(btn, QMessageBox::ActionRole);
                    buttons.append(button);
                }
            }
            box.exec();
            int clickedBtnId;
            for (clickedBtnId = 0; clickedBtnId < buttons.size(); clickedBtnId++) {
                if (box.clickedButton() == buttons.at(clickedBtnId)) {
                    break;
                }
            }
            if (p_process) {
                if (p_process->state() == QProcess::Running) {
                    p_process->write(QByteArray::number(clickedBtnId)+"\n");
                }
            } else {
                qWarning() << "process finished!";
            }
        } else if (cmd == "show-inputdialog") {
            QInputDialog dialog;
            dialog.setWindowTitle(arg.value("title"));
            
        } else if (cmd == "show-info") {
            QMessageBox::information(VNoteX::getInst().getMainWindow(),
                                     arg.value("title"),
                                     value);
        } else if (cmd == "show-question") {
            auto ret = QMessageBox::question(VNoteX::getInst().getMainWindow(),
                                             arg.value("title"),
                                             value);
            if (p_process) {
                if (p_process->state() == QProcess::Running) {
                    p_process->write(QByteArray::number(ret)+"\n");
                }
            } else {
                qWarning() << "process finished!";
            }
        }
    }
    auto text = p_text;
    return text.replace(re, "");
}

TaskHelper::TaskHelper()
{
    
}
