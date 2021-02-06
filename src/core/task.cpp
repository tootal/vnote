#include "task.h"

#include <QJsonDocument>
#include <QVersionNumber>
#include <QDebug>
#include <QJsonValue>
#include <QJsonArray>
#include <QAction>
#include <QRegularExpression>
#include <QInputDialog>
#include <QTextCodec>
#include <QRandomGenerator>

#include "utils/fileutils.h"
#include "utils/pathutils.h"
#include "vnotex.h"
#include "exception.h"
#include "taskhelper.h"
#include "notebook/notebook.h"
#include "shellexecution.h"


using namespace vnotex;

QString Task::s_latestVersion = "0.1.3";
TaskVariableMgr Task::s_vars;

Task *Task::fromFile(const QString &p_file, 
                     const QJsonDocument &p_json,
                     const QString &p_locale, 
                     QObject *p_parent)
{
    auto task = new Task(p_locale, p_file, p_parent);
    return fromJson(task, p_json.object());
}

Task* Task::fromJson(Task *p_task,
                     const QJsonObject &p_obj)
{
    if (p_obj.contains("version")) {
        p_task->m_version = p_obj["version"].toString();
    }
    
    auto version = QVersionNumber::fromString(p_task->getVersion());
    if (version < QVersionNumber(1, 0, 0)) {
        return fromJsonV0(p_task, p_obj);
    }
    qWarning() << "Unknow Task Version" << version << p_task->m_file;
    return p_task;
}

Task *Task::fromJsonV0(Task *p_task,
                       const QJsonObject &p_obj,
                       bool mergeTasks)
{
    if (p_obj.contains("type")) {
        p_task->m_type = p_obj["type"].toString();
    }
    
    if (p_obj.contains("icon")) {
        QString path = p_obj["icon"].toString();
        QDir iconPath(path);
        if (iconPath.isRelative()) {
            QDir taskDir(p_task->m_file);
            taskDir.cdUp();
            path = QDir(taskDir.filePath(path)).absolutePath();
        }
        if (QFile::exists(path)) {
            p_task->m_icon = path;
        } else {
            qWarning() << "task icon not exists" << path;
        }
    }
    
    if (p_obj.contains("shortcut")) {
        p_task->m_shortcut = p_obj["shortcut"].toString();
    }
    
    if (p_obj.contains("type")) {
        p_task->m_type = p_obj["type"].toString();
    }
    
    if (p_obj.contains("command")) {
        p_task->m_command = getLocaleString(p_obj["command"], p_task->m_locale);
    }
    
    if (p_obj.contains("args")) {
        p_task->m_args = getLocaleStringList(p_obj["args"], p_task->m_locale);
    }
    
    if (p_obj.contains("label")) {
        p_task->m_label = getLocaleString(p_obj["label"], p_task->m_locale);
    } else if (p_task->m_label.isNull() && !p_task->m_command.isNull()) {
        p_task->m_label = p_task->m_command;
    }
    
    if (p_obj.contains("options")) {
        auto options = p_obj["options"].toObject();
        
        if (options.contains("cwd")) {
            p_task->m_options_cwd = options["cwd"].toString();
        }
        
        if (options.contains("env")) {
            p_task->m_options_env.clear();
            auto env = options["env"].toObject();
            for (auto i = env.begin(); i != env.end(); i++) {
                auto key = i.key();
                auto value = getLocaleString(i.value(), p_task->m_locale);
                p_task->m_options_env.insert(key, value);
            }
        }
        
        if (options.contains("shell") && p_task->getType() == "shell") {
            auto shell = options["shell"].toObject();
            
            if (shell.contains("executable")) {
                p_task->m_options_shell_executable = shell["executable"].toString();
            }
            
            if (shell.contains("args")) {
                p_task->m_options_shell_args.clear();
                
                for (auto arg : shell["args"].toArray()) {
                    p_task->m_options_shell_args << arg.toString();
                }
            }
        }
    }
    
    if (p_obj.contains("tasks")) {
        if (!mergeTasks) p_task->m_tasks.clear();
        auto tasks = p_obj["tasks"].toArray();
        
        for (const auto &task : tasks) {
            auto t = new Task(p_task->m_locale,
                              p_task->getFile(),
                              p_task);
            p_task->m_tasks.append(fromJson(t, task.toObject()));
        }
    }
    
    if (p_obj.contains("inputs")) {
        p_task->m_inputs.clear();
        auto inputs = p_obj["inputs"].toArray();
        
        for (const auto &input : inputs) {
            auto in = input.toObject();
            Input i;
            if (in.contains("id")) {
                i.m_id = in["id"].toString();
            } else {
                qWarning() << "Input configuration not contain id";
            }
            
            if (in.contains("type")) {
                i.m_type = in["type"].toString();
            } else {
                i.m_type = "promptString";
            }
            
            if (in.contains("description")) {
                i.m_description = getLocaleString(in["description"], p_task->m_locale);
            }
            
            if (in.contains("default")) {
                i.m_default = getLocaleString(in["default"], p_task->m_locale);
            }
            
            if (i.m_type == "promptString" && in.contains("password")) {
                i.m_password = in["password"].toBool();
            } else {
                i.m_password = false;
            }
            
            if (i.m_type == "pickString" && in.contains("options")) {
                i.m_options = getLocaleStringList(in["options"], p_task->m_locale);
            }
            
            if (i.m_type == "pickString" && !i.m_default.isNull() && !i.m_options.contains(i.m_default)) {
                qWarning() << "default must be one of the option values";
            }
            
            p_task->m_inputs << i;
        }
    }
    
    // OS-specific task configuration
#if defined (Q_OS_WIN)
#define OS_SPEC "windows"
#endif
#if defined (Q_OS_MACOS)
#define OS_SPEC "osx"
#endif
#if defined (Q_OS_LINUX)
#define OS_SPEC "linux"
#endif
    if (p_obj.contains(OS_SPEC)) {
        auto os = p_obj[OS_SPEC].toObject();
        fromJsonV0(p_task, os, true);
    }
#undef OS_SPEC

    return p_task;
}

QString Task::getVersion() const
{
    return m_version;
}

QString Task::getType() const
{
    return m_type;
}

QString Task::getCommand() const
{
    return s_vars.evaluate(m_command, this);
}

QStringList Task::getArgs() const
{
    return s_vars.evaluate(m_args, this);
}

QString Task::getLabel() const
{
    return m_label;
}

QString Task::getIcon() const
{
    return m_icon;
}

QString Task::getShortcut() const
{
    return m_shortcut;
}

QString Task::getOptionsCwd() const
{
    auto cwd = m_options_cwd;
    if (!cwd.isNull()) {
        return s_vars.evaluate(cwd, this);
    }
    auto notebook = TaskHelper::getCurrentNotebook();
    if (notebook) cwd = notebook->getRootFolderAbsolutePath();
    if (!cwd.isNull()) {
        return cwd;
    }
    cwd = TaskHelper::getCurrentFile();
    if (!cwd.isNull()) {
        return QFileInfo(cwd).dir().absolutePath();
    }
    return QFileInfo(m_file).dir().absolutePath();
}

const QMap<QString, QString> &Task::getOptionsEnv() const
{
    return m_options_env;
}

QString Task::getOptionsShellExecutable() const
{
    return m_options_shell_executable;
}

QStringList Task::getOptionsShellArgs() const
{
    if (m_options_shell_args.isEmpty()) {
        return ShellExecution::defaultShellArguments(m_options_shell_executable);
    } else {
        return s_vars.evaluate(m_options_shell_args, this);
    }
}    

const QVector<Task *> &Task::getTasks() const
{
    return m_tasks;
}

const QVector<Task::Input> &Task::getInputs() const
{
    return m_inputs;
}

Task::Input Task::getInput(const QString &p_id) const
{
    for (auto i : m_inputs) {
        if (i.m_id == p_id) {
            return i;
        }
    }
    qDebug() << getLabel();
    qWarning() << "input" << p_id << "not found";
    throw "Input variable can not found";
}

QString Task::getFile() const
{
    return m_file;
}

Task::Task(const QString &p_locale, 
           const QString &p_file,
           QObject *p_parent)
    : QObject(p_parent)
{   
    m_file = p_file;
    m_version = s_latestVersion;
    m_type = "shell";
    m_options_shell_executable = ShellExecution::defaultShell();

    
    // inherit configuration
    m_parent = qobject_cast<Task*>(p_parent);
    if (m_parent) {
        m_version = m_parent->m_version;
        m_type = m_parent->m_type;
        m_command = m_parent->m_command;
        m_args = m_parent->m_args;
        m_options_cwd = m_parent->m_options_cwd;
        m_options_env = m_parent->m_options_env;
        m_options_shell_executable = m_parent->m_options_shell_executable;
        m_options_shell_args = m_parent->m_options_shell_args;
        // not inherit label/inputs/tasks
    } else {
        m_label = QFileInfo(p_file).baseName();
    }
    
    if (!p_locale.isNull()) {
        m_locale = p_locale;
    }
}

QProcess *Task::setupProcess() const
{
    // Set process property
    auto command = getCommand();
    if (command.isEmpty()) return nullptr;
    auto process = new QProcess(this->parent());
    process->setWorkingDirectory(getOptionsCwd());
    
    auto options_env = getOptionsEnv();
    if (!options_env.isEmpty()) {
        auto env = QProcessEnvironment::systemEnvironment();
        for (auto i = options_env.begin(); i != options_env.end(); i++) {
            env.insert(i.key(), i.value());
        }
        process->setProcessEnvironment(env);
    }
    
    auto args = getArgs();
    auto type = getType();
    
    // set program and args
    if (type == "shell") {
        ShellExecution::setupProcess(process,
                                     command,
                                     args,
                                     getOptionsShellExecutable(),
                                     getOptionsShellArgs());
    } else if (getType() == "process") {
        process->setProgram(command);
        process->setArguments(args);
    }

    // connect signal and slot
    connect(process, &QProcess::started,
            this, [this]() {
        emit showOutput(tr("[Task %1 started]\n").arg(getLabel()));
    });
    connect(process, &QProcess::readyReadStandardOutput,
            this, [process, this]() {
        auto text = textDecode(process->readAllStandardOutput());
        text = TaskHelper::handleCommand(text, process);
        emit showOutput(text);
    });
    connect(process, &QProcess::readyReadStandardError,
            this, [process, this]() {
        auto text = process->readAllStandardError();
        emit showOutput(textDecode(text));
    });
    connect(process, &QProcess::errorOccurred,
            this, [this](QProcess::ProcessError error) {
        emit showOutput(tr("[Task %1 error occurred with code %2]\n").arg(getLabel(), QString::number(error)));
    });
    connect(process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, process](int exitCode) {
        emit showOutput(tr("\n[Task %1 finished with exit code %2]\n")
                        .arg(getLabel(), QString::number(exitCode)));
        process->deleteLater();
    });
    return process;
}

void Task::run() const
{
    QProcess *process;
    try {
        process = setupProcess();
    }  catch (const char *msg) {
        qDebug() << msg;
        return ;
    }
    if (process) {
        // start process
        qInfo() << "run task" << process->program() << process->arguments();
        process->start();
    }
}

QString Task::textDecode(const QByteArray &p_text)
{
    static QByteArrayList codecNames = {
        "UTF-8",
        "System",
        "UTF-16",
        "GB18030"
    };
    for (auto name : codecNames) {
        auto text = textDecode(p_text, name);
        if (!text.isNull()) return text;
    }
    return p_text;
}

QString Task::textDecode(const QByteArray &p_text, const QByteArray &name)
{
    auto codec = QTextCodec::codecForName(name);
    if (codec) {
        QTextCodec::ConverterState state;
        auto text = codec->toUnicode(p_text.data(), p_text.size(), &state);
        if (state.invalidChars > 0) return QString();
        return text;
    }
    return QString();
}


bool Task::isValidTaskFile(const QString &p_file, 
                           QJsonDocument &p_json)
{
    QFile file(p_file);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    QJsonParseError error;
    p_json = QJsonDocument::fromJson(file.readAll(), &error);
    file.close();
    if (p_json.isNull()) {
        qDebug() << "load task" << p_file << "failed: " << error.errorString()
                 << "at offset" << error.offset;
        return false;
    }
    
    return true;
}

QString Task::getLocaleString(const QJsonValue &p_value, const QString &p_locale)
{
    if (p_value.isObject()) {
        auto obj = p_value.toObject();
        if (obj.contains(p_locale)) {
            return obj.value(p_locale).toString();
        } else {
            qWarning() << "current locale" << p_locale << "not found";
            if (!obj.isEmpty()){
                return obj.begin().value().toString();
            } else {
                return QString();
            }
        }
    } else {
        return p_value.toString();
    }
}

QStringList Task::getLocaleStringList(const QJsonValue &p_value, const QString &p_locale)
{
    QStringList list;
    for (auto value : p_value.toArray()) {
        list << getLocaleString(value, p_locale);
    }
    return list;
}

QStringList Task::getStringList(const QJsonValue &p_value)
{
    QStringList list;
    for (auto value : p_value.toArray()) {
        list << value.toString();
    }
    return list;
}
