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
#include "os/processhelper.h"
#include "ui_processeswidget.h"
#include "configuration.h"
#include "logger.h"
#include "misc.h"
#include "ui/uihelper.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHeaderView>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QStyle>

#include <algorithm>
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

bool ProcessesWidget::SelectProcessByPid(pid_t pid)
{
    if (pid <= 0)
        return false;

    for (int row = 0; row < this->m_model->rowCount(); ++row)
    {
        const QModelIndex sourceIdx = this->m_model->index(row, OS::ProcessModel::ColPid);
        if (!sourceIdx.isValid())
            continue;
        if (static_cast<pid_t>(this->m_model->data(sourceIdx, Qt::UserRole).toLongLong()) != pid)
            continue;

        const QModelIndex proxyIdx = this->m_proxy->mapFromSource(sourceIdx);
        if (!proxyIdx.isValid())
            return false;

        QItemSelectionModel *selectionModel = this->ui->tableView->selectionModel();
        if (!selectionModel)
            return false;

        selectionModel->clearSelection();
        selectionModel->select(proxyIdx, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        selectionModel->setCurrentIndex(proxyIdx, QItemSelectionModel::NoUpdate);
        this->ui->tableView->scrollTo(proxyIdx, QAbstractItemView::PositionAtCenter);
        return true;
    }

    return false;
}

// ── Private setup ─────────────────────────────────────────────────────────────

void ProcessesWidget::setupTable()
{
    this->m_proxy->setSourceModel(this->m_model);

    // Load persisted view toggles
    this->m_proxy->ShowKernelTasks     = CFG->ShowKernelTasks;
    this->m_proxy->ShowOtherUsersProcs = CFG->ShowOtherUsersProcs;
    this->m_model->SetShowKernelTasks(CFG->ShowKernelTasks);
    this->m_model->SetShowOtherUsersProcs(CFG->ShowOtherUsersProcs);

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
    cb->clear();
    for (int ms : CFG->RefreshRateAvailableIntervals)
        cb->addItem(Misc::SimplifyTimeMS(ms), ms);

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
    if (this->m_tableContextMenuOpen)
        return;

    const UIHelper::TableSelectionSnapshot snapshot = UIHelper::CaptureTableSelection(
        this->ui->tableView,
        OS::ProcessModel::ColPid,
        [this](const QModelIndex &proxyKeyIndex) -> QVariant
        {
            const QModelIndex srcIdx = this->m_proxy->mapToSource(proxyKeyIndex);
            if (!srcIdx.isValid())
                return {};
            return this->m_model->data(srcIdx, Qt::UserRole);
        });

    this->m_model->Refresh();

    UIHelper::RestoreTableSelection(
        this->ui->tableView,
        OS::ProcessModel::ColPid,
        this->m_model->rowCount(),
        [this](int row) -> QModelIndex { return this->m_model->index(row, OS::ProcessModel::ColPid); },
        [this](const QModelIndex &sourceIndex) -> QModelIndex { return this->m_proxy->mapFromSource(sourceIndex); },
        [this](const QModelIndex &sourceKeyIndex) -> QVariant { return this->m_model->data(sourceKeyIndex, Qt::UserRole); },
        snapshot);
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
        const QString title = this->m_model->headerData(col, Qt::Horizontal).toString();
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
    // Pause refresh during context menu so we don't loose the selection
    this->m_tableContextMenuOpen = true;

    const QModelIndex clickedIndex = this->ui->tableView->indexAt(pos);
    const QModelIndex targetIndex = clickedIndex.isValid() ? clickedIndex : this->ui->tableView->currentIndex();
    const bool hasTargetCell = targetIndex.isValid();
    const QModelIndexList selectedRowsIdx = this->ui->tableView->selectionModel()->selectedRows();
    QList<int> selectedRows;
    selectedRows.reserve(selectedRowsIdx.size());
    for (const QModelIndex &idx : selectedRowsIdx)
        selectedRows.append(idx.row());
    std::sort(selectedRows.begin(), selectedRows.end());
    const bool hasSelectedRows = !selectedRows.isEmpty();
    const bool multipleRowsSelected = selectedRows.size() > 1;
    const bool hasRowTarget = hasTargetCell || hasSelectedRows;

    const QList<pid_t> pids = this->selectedPids();
    const bool hasSelection = !pids.isEmpty();

    QMenu menu(this);

    // ── Copy submenu ────────────────────────────────────────────────────────
    QMenu *copyMenu = menu.addMenu(tr("Copy"));

    QAction *copyRowAct = copyMenu->addAction(tr("Entire row"));
    copyRowAct->setEnabled(hasRowTarget);
    connect(copyRowAct, &QAction::triggered, this, [this, targetIndex, selectedRows, multipleRowsSelected]()
    {
        if (multipleRowsSelected)
        {
            QGuiApplication::clipboard()->setText(UIHelper::GetVisibleRowsText(this->ui->tableView, selectedRows));
            return;
        }

        const int row = targetIndex.isValid() ? targetIndex.row() : selectedRows.value(0, -1);
        QGuiApplication::clipboard()->setText(UIHelper::GetVisibleRowText(this->ui->tableView, row));
    });

    QAction *copyCellAct = copyMenu->addAction(tr("Selected cell"));
    copyCellAct->setEnabled(hasTargetCell && !multipleRowsSelected);
    connect(copyCellAct, &QAction::triggered, this, [this, targetIndex]()
    {
        QGuiApplication::clipboard()->setText(this->ui->tableView->model()->data(targetIndex, Qt::DisplayRole).toString());
    });

    menu.addSeparator();

    // ── View submenu — always visible ────────────────────────────────────────
    QMenu *viewMenu = menu.addMenu(tr("View"));

    QAction *kernelAct = viewMenu->addAction(tr("Kernel tasks"));
    kernelAct->setCheckable(true);
    kernelAct->setChecked(this->m_proxy->ShowKernelTasks);
    connect(kernelAct, &QAction::toggled, this, [this](bool checked)
    {
        this->m_proxy->ShowKernelTasks = checked;
        this->m_model->SetShowKernelTasks(checked);
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
        this->m_model->SetShowOtherUsersProcs(checked);
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
    QAction *customSig = signalMenu->addAction(tr("Custom signal..."));
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
    QAction *reniceAction = menu.addAction(tr("Change priority (renice)..."));
    reniceAction->setEnabled(hasSelection);
    connect(reniceAction, &QAction::triggered, this, &ProcessesWidget::reniceSelected);

    menu.exec(this->ui->tableView->viewport()->mapToGlobal(pos));

    this->m_tableContextMenuOpen = false;
}

// ── Helpers ───────────────────────────────────────────────────────────────────

void ProcessesWidget::updateStatusBar()
{
    const int totalTasks = this->m_model->rowCount();

    qlonglong totalThreads = 0;
    const QList<OS::Process> &all = this->m_model->GetProcesses();
    for (const OS::Process &p : all)
        totalThreads += qMax(0, p.Threads);
    const QString text = tr("Tasks: %1  Threads: %2").arg(totalTasks).arg(totalThreads);

    this->ui->statusLabel->setText(text);
}

QList<pid_t> ProcessesWidget::selectedPids() const
{
    QList<pid_t> pids;
    const QModelIndexList rows = this->ui->tableView->selectionModel()->selectedRows(OS::ProcessModel::ColPid);
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
    if (pids.isEmpty())
        return;

    QStringList pidStrings;
    pidStrings.reserve(pids.size());
    for (pid_t pid : pids)
        pidStrings << QString::number(pid);

    const QString signalLabel = OS::ProcessHelper::GetSignalName(signal);
    const QString signalText = signalLabel.isEmpty() ? QString::number(signal) : signalLabel;

    QString body;
    if (pids.size() == 1)
    {
        body = tr("You are about to send signal %1 to process PID %2.\n\n"
                  "Do you want to continue?")
                   .arg(signalText)
                   .arg(pidStrings.first());
    } else
    {
        body = tr("You are about to send signal %1 to %2 selected processes.\n"
                  "This will affect all selected processes.\n\n"
                  "PIDs: %3\n\n"
                  "Do you want to continue?")
                   .arg(signalText)
                   .arg(pids.size())
                   .arg(pidStrings.join(", "));
    }

    const QMessageBox::StandardButton answer = QMessageBox::warning(this, tr("Confirm Signal"), body, QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer != QMessageBox::Yes)
        return;

    QStringList errors;
    for (pid_t pid : pids)
    {
        QString err;
        if (!OS::ProcessHelper::SendSignal(pid, signal, err))
        {
            LOG_WARN(err);
            errors << err;
        } else
        {
            LOG_INFO(QString("Sent %1 to PID %2").arg(OS::ProcessHelper::GetSignalName(signal)).arg(pid));
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
        if (!OS::ProcessHelper::Renice(pid, nice, err))
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
