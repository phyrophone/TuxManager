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

#ifndef COLORSCHEMEDIALOG_H
#define COLORSCHEMEDIALOG_H

#include "colorscheme.h"

#include <QDialog>
#include <QVector>

class QFrame;
class QFormLayout;
class QPushButton;

QT_BEGIN_NAMESPACE
namespace Ui { class ColorSchemeDialog; }
QT_END_NAMESPACE

class ColorSchemeDialog : public QDialog
{
    Q_OBJECT

    public:
        explicit ColorSchemeDialog(QWidget *parent = nullptr);
        ~ColorSchemeDialog();

        bool UseCustomScheme() const;
        ColorScheme BuildScheme() const;

    private:
        struct RowWidgets
        {
            const ColorScheme::ColorField *Field { nullptr };
            QFrame *Preview { nullptr };
            QPushButton *PickButton { nullptr };
            QPushButton *ResetButton { nullptr };
        };

        void refreshUi();
        void setColor(const ColorScheme::ColorField &field, const QColor &color);
        static QString labelForField(const QString &name);

        Ui::ColorSchemeDialog *ui { nullptr };
        ColorScheme m_scheme;
        ColorScheme m_defaultScheme;
        QVector<RowWidgets> m_rows;
};

#endif // COLORSCHEMEDIALOG_H
