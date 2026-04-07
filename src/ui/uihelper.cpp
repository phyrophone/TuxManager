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

#include "uihelper.h"

#include <QAbstractItemModel>
#include <QHeaderView>
#include <QStringList>
#include <QTableView>

namespace UIHelper
{
    QString GetVisibleRowText(const QTableView *view, int row)
    {
        if (!view || !view->model() || row < 0)
            return QString();

        const QAbstractItemModel *model = view->model();
        const QHeaderView *header = view->horizontalHeader();
        QStringList parts;
        parts.reserve(model->columnCount());
        for (int col = 0; col < model->columnCount(); ++col)
        {
            if (header && header->isSectionHidden(col))
                continue;

            const QModelIndex idx = model->index(row, col);
            parts << model->data(idx, Qt::DisplayRole).toString();
        }
        return parts.join('\t');
    }

    QString GetVisibleRowsText(const QTableView *view, const QList<int> &rows)
    {
        QStringList lines;
        lines.reserve(rows.size());
        for (int row : rows)
            lines << GetVisibleRowText(view, row);

        return lines.join('\n');
    }
}
