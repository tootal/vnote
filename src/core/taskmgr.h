#ifndef TASKMGR_H
#define TASKMGR_H

#include <QObject>

#include <QVector>

#include "task.h"

class QFileSystemWatcher;

namespace vnotex
{
    class TaskMgr : public QObject
    {
        Q_OBJECT
    public:
        
        explicit TaskMgr(QObject *parent = nullptr);
        
        // It will be invoked after MainWindow show
        void init();
        
        void refresh();
        
        const QVector<Task*> &getAllTasks() const;
        
        void deleteTask(Task *p_task);
        
        static void addSearchPath(const QString &p_path);
        
        static QString getNotebookTaskFolder();
        
    signals:
        void taskChanged();
        
    private:
        void addWatchPaths(const QStringList &list);
        
        void clearWatchPaths();
        
        void addAllTaskFolder();
        
        void loadAllTask();
        
        void watchTaskEntrys();
        
        void loadAvailableTasks();
        
        void loadTasks(const QString &p_path);
        
        void checkAndAddTaskFile(const QString &p_file);
        
        QVector<Task*> m_tasks;
        
        // all json files in task folder
        // maybe invalid
        QStringList m_files;
        
        QFileSystemWatcher *m_watcher;
        
        // List of path to search for tasks.
        static QStringList s_searchPaths;
    };
} // ns vnotex

#endif // TASKMGR_H
