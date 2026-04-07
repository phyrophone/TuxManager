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

#ifndef OS_SERVICE_H
#define OS_SERVICE_H

#include <QList>
#include <QMetaType>
#include <QString>

namespace OS
{
    class Service
    {
        public:
            QString Unit;         ///< e.g. ssh.service
            QString LoadState;    ///< loaded/not-found/...
            QString ActiveState;  ///< active/inactive/failed/...
            QString SubState;     ///< running/exited/dead/...
            QString Description;

            static QList<Service> LoadAll(QString *error = nullptr);
    };
} // namespace Os

Q_DECLARE_METATYPE(OS::Service)
Q_DECLARE_METATYPE(QList<OS::Service>)

#endif // OS_SERVICE_H
