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

#ifndef SERVICESWIDGET_H
#define SERVICESWIDGET_H

#include "os/service.h"
#include "os/servicemodel.h"
#include "os/servicefilterproxy.h"

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QWidget>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class ServicesWidget;
}
QT_END_NAMESPACE

class ServiceRefreshWorker : public QObject
{
    Q_OBJECT

    public slots:
        void fetch(quint64 token);

    signals:
        void fetched(quint64 token, bool systemdAvailable, const QString &reason, const QList<OS::Service> &services, const QString &error);
};

class ServicesWidget : public QWidget
{
    Q_OBJECT

    public:
        explicit ServicesWidget(QWidget *parent = nullptr);
        ~ServicesWidget();
        void SetActive(bool active);
        bool IsActive() const { return this->m_active; }

    private slots:
        void onTimerTick();
        void onRefreshFinished(quint64 token, bool systemdAvailable, const QString &reason, const QList<OS::Service> &services, const QString &error);
        void onTableContextMenu(const QPoint &pos);

    signals:
        void requestRefresh(quint64 token);

    private:
        Ui::ServicesWidget *ui;
        OS::ServiceModel      *m_model { nullptr };
        OS::ServiceFilterProxy *m_proxy { nullptr };
        QTimer             *m_refreshTimer { nullptr };
        QThread            *m_workerThread { nullptr };
        ServiceRefreshWorker *m_worker { nullptr };
        bool                m_active { false };
        bool                m_tableContextMenuOpen { false };
        bool                m_refreshInFlight { false };
        bool                m_refreshPending { false };
        quint64             m_refreshToken { 0 };

        void startRefresh();
};

#endif // SERVICESWIDGET_H
