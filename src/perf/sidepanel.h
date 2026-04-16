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

#ifndef PERF_SIDEPANEL_H
#define PERF_SIDEPANEL_H

#include "sidepanelitem.h"

#include <QPoint>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QWidget>

namespace Perf
{
    /// Left-hand side panel of the Performance tab.
    /// Contains a vertical list of SidePanelItem entries with mutual-exclusive
    /// selection.  Wraps itself in a QScrollArea so it gracefully handles many
    /// entries (disk 0, disk 1, … ethernet 0, …) in the future.
    class SidePanel : public QWidget
    {
        Q_OBJECT

        public:
            explicit SidePanel(QWidget *parent = nullptr);

            /// Add a new item to the panel; returns the assigned index (0-based).
            int AddItem(SidePanelItem *item);
            void ApplyColorScheme();
            void SetItemOrder(const QList<SidePanelItem *> &items);

            void SetCurrentItem(SidePanelItem *item);
            SidePanelItem *GetCurrentItem() const { return this->m_currentItem; }
            void SetItemVisible(SidePanelItem *item, bool visible);
            bool IsItemVisible(SidePanelItem *item) const;
            SidePanelItem *FirstVisibleItem() const;
            int GetCount() const { return this->m_items.size(); }

        signals:
            void currentChanged(SidePanelItem *item);
            void itemContextMenuRequested(SidePanelItem *item, const QPoint &globalPos);

        private:
            QScrollArea           *m_scrollArea;
            QWidget               *m_container;
            QVBoxLayout           *m_containerLayout;
            QList<SidePanelItem *> m_items;
            SidePanelItem         *m_currentItem { nullptr };
    };
} // namespace Perf

#endif // PERF_SIDEPANEL_H
