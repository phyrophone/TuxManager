/*
 * Tux Manager - Linux system monitor
 * Copyright (C) 2026 Petr Bena <petr@bena.rocks>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "processeswidget.h"
#include "ui_processeswidget.h"
#include "configuration.h"
#include "logger.h"

#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QScrollBar>
#include <QStyle>

#include <signal.h>

ProcessesWidget::ProcessesWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ProcessesWidget)
    , m_model(new OS::ProcessModel(this))
    , m_proxy(new OS::ProcessFilterProxy(this))
    , m_refreshTimer(new QTimer(this))
{
    this->ui->setupUi(this);

    this->setupTable();
    this->setupRefreshCombo();

    connect(this->ui->searchEdit, &QLineEdit::textChanged, this->m_proxy, &OS::ProcessFilterProxy::setFilterFixedString);
    connect(this->ui->refreshCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ProcessesWidget::onRefreshRateChanged);
    connect(this->ui->tableView->horizontalHeader(), &QHeaderView::customContextMenuRequested, this, &ProcessesWidget::onHeaderContextMenu);
    connect(this->m_refreshTimer, &QTimer::timeout, this, &ProcessesWidget::onTimerTick);

    // Update status bar whenever the proxy's visible row count changes
    // (model reset after refresh, or filter toggle)
    connect(this->m_proxy, &QAbstractItemModel::modelReset, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_proxy, &QAbstractItemModel::rowsInserted, this, &ProcessesWidget::updateStatusBar);
    connect(this->m_proxy, &QAbstractItemModel::rowsRemoved, this, &ProcessesWidget::updateStatusBar);

    // MainWindow controls active/inactive state based on current top tab.
}

ProcessesWidget::~ProcessesWidget()
{
    delete this->ui;
}

// ── Private setup ─────────────────────────────────────────────────────────────

void ProcessesWidget::setupTable()
{
    this->m_proxy->setSourceModel(this->m_model);

    // Load persisted view toggles
    this->m_proxy->ShowKernelTasks     = CFG->ShowKernelTasks;
    this->m_proxy->ShowOtherUsersProcs = CFG->ShowOtherUsersProcs;
    this->m_model->setShowKernelTasks(CFG->ShowKernelTasks);
    this->m_model->setShowOtherUsersProcs(CFG->ShowOtherUsersProcs);

    QTableView *tv = this->ui->tableView;
    tv->setModel(this->m_proxy);
    tv->sortByColumn(CFG->ProcessListSortColumn, static_cast<Qt::SortOrder>(CFG->ProcessListSortOrder));

    QHeaderView *hv = tv->horizontalHeader();
    hv->setSectionsMovable(true);
    hv->setStretchLastSection(true);
    hv->setContextMenuPolicy(Qt::CustomContextMenu);
    hv->setSectionResizeMode(QHeaderView::Interactive);

    connect(hv, &QHeaderView::sortIndicatorChanged, this, [](int column, Qt::SortOrder order)
    {
        CFG->ProcessListSortColumn = column;
        CFG->ProcessListSortOrder  = static_cast<int>(order);
    });

    tv->verticalHeader()->hide();
    tv->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tv->setSelectionMode(QAbstractItemView::ExtendedSelection);
    tv->setContextMenuPolicy(Qt::CustomContextMenu);

    connect(tv, &QTableView::customContextMenuRequested, this, &ProcessesWidget::onTableContextMenu);

    // Reasonable default column widths
    tv->setColumnWidth(OS::ProcessModel::ColPid,      60);
    tv->setColumnWidth(OS::ProcessModel::ColName,    160);
    tv->setColumnWidth(OS::ProcessModel::ColUser,     90);
    tv->setColumnWidth(OS::ProcessModel::ColState,    90);
    tv->setColumnWidth(OS::ProcessModel::ColCpu,      65);
    tv->setColumnWidth(OS::ProcessModel::ColMemRss,   80);
    tv->setColumnWidth(OS::ProcessModel::ColMemVirt,  80);
    tv->setColumnWidth(OS::ProcessModel::ColThreads,  65);
    tv->setColumnWidth(OS::ProcessModel::ColPriority, 65);
    tv->setColumnWidth(OS::ProcessModel::ColNice,     50);

    // Hide less-common columns by default
    hv->hideSection(OS::ProcessModel::ColMemVirt);
    hv->hideSection(OS::ProcessModel::ColPriority);
    hv->hideSection(OS::ProcessModel::ColNice);
}

void ProcessesWidget::setupRefreshCombo()
{
    QComboBox *cb = this->ui->refreshCombo;
    cb->addItem(tr("250 ms"),    250);
    cb->addItem(tr("500 ms"),    500);
    cb->addItem(tr("1 second"),  1000);
    cb->addItem(tr("2 seconds"), 2000);
    cb->addItem(tr("5 seconds"), 5000);

    // Pre-select the value stored in config
    for (int i = 0; i < cb->count(); ++i)
    {
        if (cb->itemData(i).toInt() == CFG->RefreshRateMs)
        {
            cb->setCurrentIndex(i);
            break;
        }
    }
}

void ProcessesWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        // Refresh immediately when user switches to Processes tab.
        this->m_model->Refresh();
        this->updateStatusBar();
        this->m_refreshTimer->start(CFG->RefreshRateMs);
    } else
    {
        this->m_refreshTimer->stop();
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ProcessesWidget::onTimerTick()
{
    // Preserve scroll position and selection across refresh
    QScrollBar *vsb       = this->ui->tableView->verticalScrollBar();
    const int   scrollPos = vsb->value();

    // Remember which PID row is selected so we can restore it after refresh
    const QModelIndex proxyIdx = this->ui->tableView->currentIndex();
    QVariant selectedPid;
    if (proxyIdx.isValid())
    {
        const QModelIndex srcIdx =
            this->m_proxy->mapToSource(proxyIdx.sibling(proxyIdx.row(), OS::ProcessModel::ColPid));
        selectedPid = this->m_model->data(srcIdx, Qt::UserRole);
    }

    this->m_model->Refresh();

    // Restore selection by PID
    if (!selectedPid.isNull())
    {
        for (int row = 0; row < this->m_model->rowCount(); ++row)
        {
            const QModelIndex idx = this->m_model->index(row, OS::ProcessModel::ColPid);
            if (this->m_model->data(idx, Qt::UserRole) == selectedPid)
            {
                const QModelIndex proxyRestored =
                    this->m_proxy->mapFromSource(idx);
                this->ui->tableView->setCurrentIndex(proxyRestored);
                break;
            }
        }
    }

    // Restore scroll position (do this after selection restore so
    // setCurrentIndex() does not scroll the view away)
    vsb->setValue(scrollPos);
}

void ProcessesWidget::onRefreshRateChanged(int comboIndex)
{
    const int ms = this->ui->refreshCombo->itemData(comboIndex).toInt();
    if (ms <= 0)
        return;
    CFG->RefreshRateMs = ms;
    this->m_refreshTimer->setInterval(ms);
    LOG_DEBUG(QString("Processes refresh rate set to %1 ms").arg(ms));
}

void ProcessesWidget::onHeaderContextMenu(const QPoint &pos)
{
    QHeaderView *hv = this->ui->tableView->horizontalHeader();
    QMenu menu(this);

    for (int col = 0; col < OS::ProcessModel::ColCount; ++col)
    {
        const QString title =
            this->m_model->headerData(col, Qt::Horizontal).toString();
        QAction *action = menu.addAction(title);
        action->setCheckable(true);
        action->setChecked(!hv->isSectionHidden(col));
        action->setData(col);
    }

    const QAction *chosen = menu.exec(hv->mapToGlobal(pos));
    if (!chosen)
        return;

    const int col = chosen->data().toInt();
    if (chosen->isChecked())
        hv->showSection(col);
    else
        hv->hideSection(col);
}

void ProcessesWidget::onTableContextMenu(const QPoint &pos)
{
    const QList<pid_t> pids = this->selectedPids();
    const bool hasSelection = !pids.isEmpty();

    QMenu menu(this);

    // ── View submenu — always visible ────────────────────────────────────────
    QMenu *viewMenu = menu.addMenu(tr("View"));

    QAction *kernelAct = viewMenu->addAction(tr("Kernel tasks"));
    kernelAct->setCheckable(true);
    kernelAct->setChecked(this->m_proxy->ShowKernelTasks);
    connect(kernelAct, &QAction::toggled, this, [this](bool checked)
    {
        this->m_proxy->ShowKernelTasks = checked;
        this->m_model->setShowKernelTasks(checked);
        CFG->ShowKernelTasks = checked;
        this->m_proxy->ApplyFilters();
        this->onTimerTick();
        LOG_DEBUG(QString("ShowKernelTasks = %1").arg(checked));
    });

    QAction *otherUsersAct = viewMenu->addAction(tr("Processes of other users"));
    otherUsersAct->setCheckable(true);
    otherUsersAct->setChecked(this->m_proxy->ShowOtherUsersProcs);
    connect(otherUsersAct, &QAction::toggled, this, [this](bool checked)
    {
        this->m_proxy->ShowOtherUsersProcs = checked;
        this->m_model->setShowOtherUsersProcs(checked);
        CFG->ShowOtherUsersProcs = checked;
        this->m_proxy->ApplyFilters();
        this->onTimerTick();
        LOG_DEBUG(QString("ShowOtherUsersProcs = %1").arg(checked));
    });
    // ── Send signal submenu — requires selection ────────────────────────────
    menu.addSeparator();
    QMenu *signalMenu = menu.addMenu(tr("Send signal"));
    signalMenu->setEnabled(hasSelection);

    struct { const char *label; int sig; } commonSignals[] =
    {
        { "SIGTERM  (15) — Terminate",   SIGTERM  },
        { "SIGKILL   (9) — Kill (force)", SIGKILL  },
        { "SIGHUP    (1) — Hangup",       SIGHUP   },
        { "SIGSTOP  (19) — Stop",         SIGSTOP  },
        { "SIGCONT  (18) — Continue",     SIGCONT  },
        { "SIGINT    (2) — Interrupt",    SIGINT   },
        { "SIGUSR1  (10) — User 1",       SIGUSR1  },
        { "SIGUSR2  (12) — User 2",       SIGUSR2  },
    };
    for (const auto &s : commonSignals)
    {
        QAction *a = signalMenu->addAction(tr(s.label));
        a->setData(s.sig);
        connect(a, &QAction::triggered, this, [this, s]()
        {
            this->sendSignalToSelected(s.sig);
        });
    }

    signalMenu->addSeparator();
    QAction *customSig = signalMenu->addAction(tr("Custom signal…"));
    connect(customSig, &QAction::triggered, this, [this]()
    {
        bool ok;
        const int sig = QInputDialog::getInt(
            this,
            tr("Send custom signal"),
            tr("Signal number:"),
            1, 1, 64, 1, &ok);
        if (ok)
            this->sendSignalToSelected(sig);
    });

    // ── Quick-access kill / term — requires selection ───────────────────────
    menu.addSeparator();
    QAction *termAction = menu.addAction(tr("Terminate  (SIGTERM)"));
    termAction->setEnabled(hasSelection);
    connect(termAction, &QAction::triggered, this, [this]()
    {
        this->sendSignalToSelected(SIGTERM);
    });

    QAction *killAction = menu.addAction(tr("Kill  (SIGKILL)"));
    killAction->setEnabled(hasSelection);
    killAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxCritical));
    connect(killAction, &QAction::triggered, this, [this]()
    {
        this->sendSignalToSelected(SIGKILL);
    });

    // ── Priority ─────────────────────────────────────────────────────────────
    menu.addSeparator();
    QAction *reniceAction = menu.addAction(tr("Change priority (renice)…"));
    reniceAction->setEnabled(hasSelection);
    connect(reniceAction, &QAction::triggered, this, &ProcessesWidget::reniceSelected);

    menu.exec(this->ui->tableView->viewport()->mapToGlobal(pos));
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ProcessesWidget::updateStatusBar()
{
    const int total   = this->m_model->rowCount();
    const int visible = this->m_proxy->rowCount();

    QString text = tr("Tasks: %1").arg(total);
    if (visible != total)
        text += tr("  (Showing %1)").arg(visible);

    this->ui->statusLabel->setText(text);
}

QList<pid_t> ProcessesWidget::selectedPids() const
{
    QList<pid_t> pids;
    const QModelIndexList rows =
        this->ui->tableView->selectionModel()->selectedRows(OS::ProcessModel::ColPid);
    pids.reserve(rows.size());
    for (const QModelIndex &proxyIdx : rows)
    {
        const QModelIndex srcIdx = this->m_proxy->mapToSource(proxyIdx);
        const QVariant v = this->m_model->data(srcIdx, Qt::UserRole);
        pids.append(static_cast<pid_t>(v.toLongLong()));
    }
    return pids;
}

void ProcessesWidget::sendSignalToSelected(int signal)
{
    const QList<pid_t> pids = this->selectedPids();
    QStringList errors;
    for (pid_t pid : pids)
    {
        QString err;
        if (!OS::ProcessHelper::sendSignal(pid, signal, err))
        {
            LOG_WARN(err);
            errors << err;
        } else
        {
            LOG_INFO(QString("Sent %1 to PID %2").arg(OS::ProcessHelper::signalName(signal)).arg(pid));
        }
    }
    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Signal failed"), errors.join('\n'));
    }
}

void ProcessesWidget::reniceSelected()
{
    const QList<pid_t> pids = this->selectedPids();
    if (pids.isEmpty())
        return;

    bool ok;
    const int nice = QInputDialog::getInt(this, tr("Change priority"), tr("Nice value (-20 = highest priority, 19 = lowest):"), 0, -20, 19, 1, &ok);
    if (!ok)
        return;

    QStringList errors;
    for (pid_t pid : pids)
    {
        QString err;
        if (!OS::ProcessHelper::renice(pid, nice, err))
        {
            LOG_WARN(err);
            errors << err;
        } else
        {
            LOG_INFO(QString("Reniced PID %1 to nice=%2").arg(pid).arg(nice));
        }
    }

    if (!errors.isEmpty())
    {
        QMessageBox::warning(this, tr("Renice failed"), errors.join('\n'));
    }
}
