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

#ifndef OS_SERVICEMODEL_H
#define OS_SERVICEMODEL_H

#include "service.h"

#include <QAbstractTableModel>

namespace OS
{
    class ServiceModel : public QAbstractTableModel
    {
        Q_OBJECT

        public:
            enum Column
            {
                ColService = 0,
                ColLoad,
                ColActive,
                ColSubState,
                ColDescription,
                ColCount
            };

            explicit ServiceModel(QObject *parent = nullptr);

            int rowCount(const QModelIndex &parent = QModelIndex()) const override;
            int columnCount(const QModelIndex &parent = QModelIndex()) const override;
            QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
            QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
            Qt::ItemFlags flags(const QModelIndex &index) const override;

            void SetServices(const QList<Service> &services);
            const QList<Service> &services() const { return this->m_services; }

        private:
            QList<Service> m_services;
    };
}

#endif // OS_SERVICEMODEL_H
