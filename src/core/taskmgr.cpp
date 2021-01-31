#include "taskmgr.h"

#include <QDir>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QJsonDocument>

#include "configmgr.h"
#include "coreconfig.h"
#include "utils/pathutils.h"
#include "utils/fileutils.h"
#include "vnotex.h"
#include "notebookmgr.h"
#include "notebookconfigmgr/bundlenotebookconfigmgr.h"

using namespace vnotex;

QStringList TaskMgr::s_searchPaths;

TaskMgr::TaskMgr(QObject *parent) 
    : QObject(parent)
{
    m_watcher = new QFileSystemWatcher(this);
}

void TaskMgr::init()
{   
    // load all tasks and watch them
    loadAllTask();
    
    connect(&VNoteX::getInst().getNotebookMgr(), &NotebookMgr::currentNotebookChanged,
            this, &TaskMgr::loadAllTask);
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &TaskMgr::loadAllTask);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &TaskMgr::loadAllTask);
}

void TaskMgr::refresh()
{
    loadAvailableTasks();
}

const QVector<Task*> &TaskMgr::getAllTasks() const
{
    return m_tasks;
}

void TaskMgr::addSearchPath(const QString &p_path)
{
    s_searchPaths << p_path;
}

void TaskMgr::addWatchPaths(const QStringList &list)
{
    if (list.isEmpty()) return ;
    qDebug() << "addWatchPaths" << list;
    m_watcher->addPaths(list);
}

void TaskMgr::clearWatchPaths()
{
    auto entrys = m_watcher->files();
    if (!entrys.isEmpty()) {
        m_watcher->removePaths(entrys);
    }
    entrys = m_watcher->directories();
    if (!entrys.isEmpty()) {
        m_watcher->removePaths(entrys);
    }
}

void TaskMgr::addAllTaskFolder()
{
    s_searchPaths.clear();
    auto &configMgr = ConfigMgr::getInst();
    // App scope task folder
    TaskMgr::addSearchPath(configMgr.getAppTaskFolder());
    // User scope task folder
    TaskMgr::addSearchPath(configMgr.getUserTaskFolder());
    // Notebook scope task folder
    const auto &notebookMgr = VNoteX::getInst().getNotebookMgr();
    auto id = notebookMgr.getCurrentNotebookId();
    do {
        if (id == Notebook::InvalidId) break;
        auto notebook = notebookMgr.findNotebookById(id);
        if (!notebook) break;
        auto configMgr = notebook->getConfigMgr();
        if (!configMgr) break;
        auto configMgrName = configMgr->getName();
        if (configMgrName == "vx.vnotex") {
            QDir dir(notebook->getRootFolderAbsolutePath());
            dir.cd(BundleNotebookConfigMgr::getConfigFolderName());
            if (!dir.cd("tasks")) break;
            addSearchPath(dir.absolutePath());
        } else {
            qWarning() << "Unknow notebook config type"<< configMgrName <<"task will not be load.";
        }
    } while(0);
}

void TaskMgr::loadAllTask()
{
    addAllTaskFolder();
    loadAvailableTasks();
    watchTaskEntrys();
    emit taskChanged();
}

void TaskMgr::watchTaskEntrys()
{
    clearWatchPaths();
    addWatchPaths(s_searchPaths);
    for (const auto &pa : s_searchPaths) {
        addWatchPaths(FileUtils::entryListRecursively(pa, QStringList(), QDir::AllDirs));
    }
    addWatchPaths(m_files);
}

void TaskMgr::loadAvailableTasks()
{
    m_tasks.clear();
    m_files.clear();
    
    for (const auto &pa : s_searchPaths) {
        loadTasks(pa);
    }
    
    {
        QStringList list;
        for (auto task : m_tasks) {
            list << QFileInfo(task->getFile()).fileName();
        }
        if (!m_tasks.isEmpty()) qDebug() << "load tasks" << list;
    }
}

void TaskMgr::loadTasks(const QString &p_path)
{
    const auto taskFiles = FileUtils::entryListRecursively(p_path, {"*.json"}, QDir::Files);
    for (auto &file : taskFiles) {
        m_files << file;
        checkAndAddTaskFile(file);
    }
}

void TaskMgr::checkAndAddTaskFile(const QString &p_file)
{
    QJsonDocument json;
    if (Task::isValidTaskFile(p_file, json)) {
        const auto localeStr = ConfigMgr::getInst().getCoreConfig().getLocaleToUse();
        m_tasks.push_back(Task::fromFile(p_file, json, localeStr, this));
    }
}

void TaskMgr::deleteTask(Task *p_task)
{
    FileUtils::removeFile(p_task->getFile());
    for (int i = 0; i < m_tasks.size(); i++) {
        if (m_tasks.at(i) == p_task) {
            m_tasks.remove(i);
            break;
        }
    }
}
