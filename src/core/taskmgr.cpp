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

using namespace vnotex;

QStringList TaskMgr::s_searchPaths;

TaskMgr::TaskMgr(QObject *parent) 
    : QObject(parent)
{
    loadAvailableTasks();
    
    m_watcher = new QFileSystemWatcher(this);
    
    watchTaskEntrys();
    
    connect(m_watcher, &QFileSystemWatcher::directoryChanged,
            this, &TaskMgr::onTaskChanged);
    connect(m_watcher, &QFileSystemWatcher::fileChanged,
            this, &TaskMgr::onTaskChanged);
}

const QVector<Task*> &TaskMgr::getAllTasks() const
{
    return m_tasks;
}

void TaskMgr::refresh()
{
    loadAvailableTasks();
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

void TaskMgr::onTaskChanged()
{
    refresh();
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
