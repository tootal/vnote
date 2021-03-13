#include "taskpage.h"

#include <QGridLayout>
#include <QPushButton>
#include <QFileInfo>
#include <QTableWidget>
#include <QHeaderView>

#include <widgets/treewidget.h>
#include <core/vnotex.h>
#include <core/taskmgr.h>
#include <core/configmgr.h>
#include <core/task.h>
#include <utils/widgetutils.h>

using namespace vnotex;

TaskPage::TaskPage(QWidget *p_parent)
    : SettingsPage(p_parent)
{
    setupUI();
}

void TaskPage::setupUI()
{
    auto mainLayout = new QGridLayout(this);
    
    m_taskExplorer = new TreeWidget(this);
    TreeWidget::setupSingleColumnHeaderlessTree(m_taskExplorer, true, true);
    TreeWidget::showHorizontalScrollbar(m_taskExplorer);
    mainLayout->addWidget(m_taskExplorer, 0, 0, 3, 2);
    connect(m_taskExplorer, &TreeWidget::itemClicked,
            this, &TaskPage::loadTaskDetail);

    m_taskViewer = new QTableWidget(this);
    setupTaskViewer();
    mainLayout->addWidget(m_taskViewer, 0, 2, 5, 1);
    
    auto refreshBtn = new QPushButton(tr("Refresh"), this);
    mainLayout->addWidget(refreshBtn, 3, 0, 1, 1);
    connect(refreshBtn, &QPushButton::clicked,
            this, [this]() {
        VNoteX::getInst().getTaskMgr().refresh();
        loadTasks();
    });
    
    auto openLocationBtn = new QPushButton(tr("Open Location"), this);
    mainLayout->addWidget(openLocationBtn, 3, 1, 1, 1);
    connect(openLocationBtn, &QPushButton::clicked,
            this, [this]() {
        auto task = currentTask();
        QString path = ConfigMgr::getInst().getUserTaskFolder();
        if (task) {
            path = QFileInfo(task->getFile()).absolutePath();
        }
        WidgetUtils::openUrlByDesktop(QUrl::fromLocalFile(path));
    });
    
    auto addBtn = new QPushButton(tr("Add"), this);
    mainLayout->addWidget(addBtn, 4, 0, 1, 1);
    
    auto deleteBtn = new QPushButton(tr("Delete"), this);
    mainLayout->addWidget(deleteBtn, 4, 1, 1, 1);
    connect(deleteBtn, &QPushButton::clicked,
            this, [this]() {
        VNoteX::getInst().getTaskMgr().deleteTask(currentTask());
    });
}

void TaskPage::setupTask(QTreeWidgetItem *p_item, Task *p_task)
{
    p_item->setText(0, p_task->getLabel());
    p_item->setData(0, Qt::UserRole, 
                    QVariant::fromValue(qobject_cast<QObject*>(p_task)));
}

void TaskPage::setupTaskViewer()
{
    auto v = m_taskViewer;
    v->setColumnCount(2);
    v->setRowCount(7);
    v->setHorizontalHeaderLabels({"Attribute", "Value"});
    v->horizontalHeader()->setStretchLastSection(true);
    v->setEditTriggers(QAbstractItemView::NoEditTriggers);
    v->setItem(0, 0, new QTableWidgetItem(tr("Version")));
    v->setItem(1, 0, new QTableWidgetItem(tr("Type")));
    v->setItem(2, 0, new QTableWidgetItem(tr("Label")));
    v->setItem(3, 0, new QTableWidgetItem(tr("Command")));
    v->setItem(4, 0, new QTableWidgetItem(tr("Args")));
    v->setItem(5, 0, new QTableWidgetItem(tr("Icon")));
    v->setItem(6, 0, new QTableWidgetItem(tr("Shortcut")));
}

void TaskPage::loadTaskDetail(QTreeWidgetItem *p_item, int)
{
    auto v = m_taskViewer;
    auto data = p_item->data(0, Qt::UserRole).value<QObject*>();
    auto dto = qobject_cast<Task*>(data)->getDTO();
    v->setItem(0, 1, new QTableWidgetItem(dto.version));
    v->setItem(1, 1, new QTableWidgetItem(dto.type));
    v->setItem(2, 1, new QTableWidgetItem(dto.label));
    v->setItem(3, 1, new QTableWidgetItem(dto.command));
    v->setItem(4, 1, new QTableWidgetItem(dto.args.join(",")));
    v->setItem(5, 1, new QTableWidgetItem(dto.icon));
    v->setItem(6, 1, new QTableWidgetItem(dto.shortcut));
}

void TaskPage::loadTasks()
{
    const auto &taskMgr = VNoteX::getInst().getTaskMgr();
    const auto &tasks = taskMgr.getAllTasks();
    
    m_taskExplorer->clear();
    for (const auto &task : tasks) {
        addTask(task);
    }
}

void TaskPage::addTask(Task *p_task, 
                       QTreeWidgetItem *p_parentItem)
{
    QTreeWidgetItem *item;
    item = p_parentItem ? new QTreeWidgetItem(p_parentItem)
                        : new QTreeWidgetItem(m_taskExplorer);
    setupTask(item, p_task);
    for (auto task : p_task->getTasks()) {
        addTask(task, item);
    }
}

Task *TaskPage::currentTask() const
{
    auto item = m_taskExplorer->currentItem();
    while (item && item->parent()) {
        item = item->parent();   
    }
    if (item) {
        auto data = item->data(0, Qt::UserRole).value<QObject*>();
        return qobject_cast<Task*>(data);
    }
    return nullptr;
}

void TaskPage::loadInternal()
{
    loadTasks();
}

void TaskPage::saveInternal()
{
}

QString TaskPage::title() const
{
    return tr("Task");
}
