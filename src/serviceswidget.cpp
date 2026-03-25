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

#include "serviceswidget.h"
#include "ui_serviceswidget.h"

#include "configuration.h"

#include <QHeaderView>
#include <QMetaObject>
#include <QTableWidgetItem>

void ServiceRefreshWorker::fetch(quint64 token)
{
    QString reason;
    const bool systemdAvailable = OS::Service::IsSystemdAvailable(&reason);
    QString error;
    QList<OS::Service> services;
    if (systemdAvailable)
        services = OS::Service::LoadAll(&error);

    emit fetched(token, systemdAvailable, reason, services, error);
}

ServicesWidget::ServicesWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::ServicesWidget)
    , m_refreshTimer(new QTimer(this))
    , m_workerThread(new QThread(this))
    , m_worker(new ServiceRefreshWorker())
{
    this->ui->setupUi(this);
    qRegisterMetaType<OS::Service>("Os::Service");
    qRegisterMetaType<QList<OS::Service>>("QList<Os::Service>");

    this->ui->tableWidget->setColumnCount(5);
    this->ui->tableWidget->setHorizontalHeaderLabels(
    {
        tr("Service"),
        tr("Load"),
        tr("Active"),
        tr("Sub"),
        tr("Description")
    });
    this->ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    this->ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    this->ui->tableWidget->setAlternatingRowColors(true);

    QHeaderView *hv = this->ui->tableWidget->horizontalHeader();
    hv->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    hv->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    hv->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    hv->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    hv->setSectionResizeMode(4, QHeaderView::Stretch);

    connect(this->m_refreshTimer, &QTimer::timeout,
            this, &ServicesWidget::onTimerTick);

    this->m_worker->moveToThread(this->m_workerThread);
    connect(this->m_workerThread, &QThread::finished, this->m_worker, &QObject::deleteLater);
    connect(this, &ServicesWidget::requestRefresh, this->m_worker, &ServiceRefreshWorker::fetch, Qt::QueuedConnection);
    connect(this->m_worker, &ServiceRefreshWorker::fetched, this, &ServicesWidget::onRefreshFinished, Qt::QueuedConnection);
    this->m_workerThread->start();
}

ServicesWidget::~ServicesWidget()
{
    this->m_refreshTimer->stop();
    this->m_workerThread->quit();
    this->m_workerThread->wait(1000);
    delete this->ui;
}

void ServicesWidget::SetActive(bool active)
{
    if (this->m_active == active)
        return;

    this->m_active = active;
    if (active)
    {
        this->startRefresh();
        this->m_refreshTimer->start(CFG->RefreshRateMs);
    } else
    {
        this->m_refreshTimer->stop();
        this->m_refreshPending = false;
    }
}

void ServicesWidget::onTimerTick()
{
    this->startRefresh();
}

void ServicesWidget::startRefresh()
{
    if (this->m_refreshInFlight)
    {
        this->m_refreshPending = true;
        return;
    }

    this->m_refreshInFlight = true;
    this->m_refreshPending = false;
    ++this->m_refreshToken;
    emit requestRefresh(this->m_refreshToken);
}

void ServicesWidget::onRefreshFinished(quint64 token, bool systemdAvailable, const QString &reason, const QList<OS::Service> &services, const QString &error)
{
    if (token != this->m_refreshToken)
        return;

    this->m_refreshInFlight = false;

    if (!this->m_active)
        return;

    if (!systemdAvailable)
    {
        this->ui->tableWidget->setVisible(false);
        this->ui->unavailableLabel->setVisible(true);
        this->ui->unavailableLabel->setText(tr("systemd required (%1)").arg(reason));
        this->ui->statusLabel->setText(tr("Services unavailable"));
    } else if (!error.isEmpty())
    {
        this->ui->tableWidget->setVisible(false);
        this->ui->unavailableLabel->setVisible(true);
        this->ui->unavailableLabel->setText(tr("Failed to query services: %1").arg(error));
        this->ui->statusLabel->setText(tr("Services unavailable"));
    } else
    {
        this->ui->unavailableLabel->setVisible(false);
        this->ui->tableWidget->setVisible(true);
        //this->rebuildTable(services);
        this->ui->statusLabel->setText(tr("Services: %1").arg(services.size()));
    }

    if (this->m_active && this->m_refreshPending)
    {
        this->m_refreshPending = false;
        QMetaObject::invokeMethod(this, &ServicesWidget::startRefresh, Qt::QueuedConnection);
    }
}

void ServicesWidget::rebuildTable(const QList<OS::Service> &services)
{
    this->ui->tableWidget->setSortingEnabled(false);
    this->ui->tableWidget->clearContents();
    this->ui->tableWidget->setRowCount(services.size());

    for (int r = 0; r < services.size(); ++r)
    {
        const OS::Service &s = services.at(r);

        auto mk = [](const QString &text) -> QTableWidgetItem *
        {
            auto *it = new QTableWidgetItem(text);
            it->setFlags(Qt::ItemIsSelectable | Qt::ItemIsEnabled);
            return it;
        };

        this->ui->tableWidget->setItem(r, 0, mk(s.unit));
        this->ui->tableWidget->setItem(r, 1, mk(s.loadState));
        this->ui->tableWidget->setItem(r, 2, mk(s.activeState));
        this->ui->tableWidget->setItem(r, 3, mk(s.subState));
        this->ui->tableWidget->setItem(r, 4, mk(s.description));
    }

    this->ui->tableWidget->setSortingEnabled(true);
    this->ui->tableWidget->sortByColumn(0, Qt::AscendingOrder);
}
