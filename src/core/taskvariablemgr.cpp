#include "taskvariablemgr.h"

#include <QRegularExpression>
#include <QInputDialog>
#include <QApplication>
#include <QRandomGenerator>

#include "vnotex.h"
#include "task.h"
#include "taskhelper.h"
#include "notebook/notebook.h"
#include <widgets/mainwindow.h>
#include <widgets/dialogs/selectdialog.h>

using namespace vnotex;


TaskVariable::TaskVariable(TaskVariable::Type p_type, 
                           const QString &p_name, 
                           TaskVariable::Func p_func)
    : m_type(p_type), m_name(p_name), m_func(p_func)
{
    
}


TaskVariableMgr::TaskVariableMgr()
    : m_initialized(false)
{
    
}

void TaskVariableMgr::refresh()
{
    init();
}

QString TaskVariableMgr::evaluate(const QString &p_text, 
                                   const Task *p_task) const
{
    auto text = p_text;
    auto replace = [&text](const QString &p_name, const QString &p_value, bool p_isPath = false) {
        auto reg = QRegularExpression(QString(R"(\$\{[\t ]*%1[\t ]*\})").arg(p_name));
        text.replace(reg, p_isPath ? TaskHelper::normalPath(p_value) : p_value);
    };
    
    // current notebook variables
    {
        auto notebook = TaskHelper::getCurrentNotebook();
        if (notebook) {
            auto folder = notebook->getRootFolderAbsolutePath();
            replace("notebookFolder", folder, true);
            replace("notebookFolderBasename", QDir(folder).dirName());
            replace("notebookName", notebook->getName());
            replace("notebookDescription", notebook->getDescription());
        }
    }
    
    // current file variables
    auto curFile = TaskHelper::getCurrentFile();
    auto curInfo = QFileInfo(curFile);
    {
        replace("file", curFile, true);
        auto folder = TaskHelper::getFileNotebookFolder(curFile);
        replace("fileNotebookFolder", folder, true);
        replace("relativeFile", QDir(folder).relativeFilePath(curFile));
        replace("fileBasename", curInfo.fileName());
        replace("fileBasenameNoExtension", curInfo.baseName());
        replace("fileDirname", curInfo.dir().absolutePath(), true);
        replace("fileExtname", "." + curInfo.suffix());
        replace("selectedText", TaskHelper::getSelectedText());
        replace("cwd", p_task->getOptionsCwd(), true);
        replace("taskFile", p_task->getFile(), true);
        replace("taskDirname", QFileInfo(p_task->getFile()).dir().absolutePath(), true);
        replace("execPath", qApp->applicationFilePath(), true);
        replace("pathSeparator", TaskHelper::getPathSeparator());
    }
    
    // Magic variables
    {
        auto cDT = QDateTime::currentDateTime();
        for(auto s : {
            "d", "dd", "ddd", "dddd", "M", "MM", "MMM", "MMMM", 
            "yy", "yyyy", "h", "hh", "H", "HH", "m", "mm", 
            "s", "ss", "z", "zzz", "AP", "A", "ap", "a"
        }) replace(QString("magic:%1").arg(s), cDT.toString(s)); 
        replace("magic:random", QString::number(QRandomGenerator::global()->generate()));
        replace("magic:random_d", QString::number(QRandomGenerator::global()->generate()));
        replace("magic:date", cDT.toString("yyyy-MM-dd"));
        replace("magic:da", cDT.toString("yyyyMMdd"));
        replace("magic:time", cDT.toString("hh:mm:ss"));
        replace("magic:datetime", cDT.toString("yyyy-MM-dd hh:mm:ss"));
        replace("magic:dt", cDT.toString("yyyyMMdd hh:mm:ss"));
        replace("magic:note", curInfo.fileName());
        replace("magic:no", curInfo.completeBaseName());
        replace("magic:t", curInfo.completeBaseName());
        replace("magic:w", QString::number(cDT.date().weekNumber()));
        // TODO: replace("magic:att", "undefined");
    }
    
    // environment variables
    {
        QMap<QString, QString> map;
        auto list = TaskHelper::getAllSpecialVariables("env", p_text);
        list.erase(std::unique(list.begin(), list.end()), list.end());
        for (const auto &name : list) {
            auto value = QProcessEnvironment::systemEnvironment().value(name);
            map.insert(name, value);
        }
        TaskHelper::replaceAllSepcialVariables("env", text, map);
    }
    
    text = evaluateInputVariables(text, p_task);
    return text;
}

QStringList TaskVariableMgr::evaluate(const QStringList &p_list, 
                                      const Task *p_task) const
{
    QStringList list;
    for (const auto &s : p_list) {
        list << evaluate(s, p_task);
    }
    return list;
}

void TaskVariableMgr::init()
{
    if (m_initialized) return ;
    m_initialized = true;
    
    add(TaskVariable::FunctionBased,
        "file",
        [](const TaskVariable *, const Task *) {
         return QString();
    });
}

void TaskVariableMgr::add(TaskVariable::Type p_type, 
                          const QString &p_name, 
                          TaskVariable::Func p_func)
{
    m_predefs.insert(p_name, TaskVariable(p_type, p_name, p_func));
}

QString TaskVariableMgr::evaluateInputVariables(const QString &p_text, 
                                                const Task *p_task) const
{
    QMap<QString, QString> map;
    auto list = TaskHelper::getAllSpecialVariables("input", p_text);
    list.erase(std::unique(list.begin(), list.end()), list.end());
    for (const auto &id : list) {
        auto input = p_task->getInput(id);
        QString text;
        auto mainwin = VNoteX::getInst().getMainWindow();
        if (input.m_type == "promptString") {
            auto desc = evaluate(input.m_description, p_task);
            auto defaultText = evaluate(input.m_default, p_task);
            QInputDialog dialog(mainwin);
            dialog.setInputMode(QInputDialog::TextInput);
            if (input.m_password) dialog.setTextEchoMode(QLineEdit::Password);
            else dialog.setTextEchoMode(QLineEdit::Normal);
            dialog.setWindowTitle(p_task->getLabel());
            dialog.setLabelText(desc);
            dialog.setTextValue(defaultText);
            if (dialog.exec() == QDialog::Accepted) {
                text = dialog.textValue();
            } else {
                throw "TaskCancle";
            }
        } else if (input.m_type == "pickString") {
            // TODO: select description
            SelectDialog dialog(p_task->getLabel(), mainwin);
            for (int i = 0; i < input.m_options.size(); i++) {
                qDebug() << "addSelection" << input.m_options.at(i);
                dialog.addSelection(input.m_options.at(i), i);
            }
    
            if (dialog.exec() == QDialog::Accepted) {
                int selection = dialog.getSelection();
                text = input.m_options.at(selection);
            } else {
                throw "TaskCancle";
            }
        }
        map.insert(input.m_id, text);
    }
    return TaskHelper::replaceAllSepcialVariables("input", p_text, map);
}

