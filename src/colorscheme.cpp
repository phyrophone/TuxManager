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

#include "colorscheme.h"

#include <QApplication>
#include <QPalette>
#include <QVariant>

ColorScheme *ColorScheme::current = nullptr;

namespace
{
    const ColorScheme::ColorField kColorFields[] =
    {
        { "CpuGraphLineColor", &ColorScheme::CpuGraphLineColor },
        { "CpuGraphFillColor", &ColorScheme::CpuGraphFillColor },
        { "CpuGraphSecondaryFillColor", &ColorScheme::CpuGraphSecondaryFillColor },
        { "MemoryGraphLineColor", &ColorScheme::MemoryGraphLineColor },
        { "MemoryGraphFillColor", &ColorScheme::MemoryGraphFillColor },
        { "DiskGraphLineColor", &ColorScheme::DiskGraphLineColor },
        { "DiskGraphFillColor", &ColorScheme::DiskGraphFillColor },
        { "DiskTransferGraphLineColor", &ColorScheme::DiskTransferGraphLineColor },
        { "DiskTransferGraphFillColor", &ColorScheme::DiskTransferGraphFillColor },
        { "DiskTransferGraphSecondaryFillColor", &ColorScheme::DiskTransferGraphSecondaryFillColor },
        { "NetworkGraphLineColor", &ColorScheme::NetworkGraphLineColor },
        { "NetworkGraphFillColor", &ColorScheme::NetworkGraphFillColor },
        { "NetworkGraphSecondaryFillColor", &ColorScheme::NetworkGraphSecondaryFillColor },
        { "GpuGraphLineColor", &ColorScheme::GpuGraphLineColor },
        { "GpuGraphFillColor", &ColorScheme::GpuGraphFillColor },
        { "GpuGraphSecondaryFillColor", &ColorScheme::GpuGraphSecondaryFillColor },
        { "SwapUsageGraphLineColor", &ColorScheme::SwapUsageGraphLineColor },
        { "SwapUsageGraphFillColor", &ColorScheme::SwapUsageGraphFillColor },
        { "SwapActivityGraphLineColor", &ColorScheme::SwapActivityGraphLineColor },
        { "SwapActivityGraphFillColor", &ColorScheme::SwapActivityGraphFillColor },
        { "SwapActivityGraphSecondaryFillColor", &ColorScheme::SwapActivityGraphSecondaryFillColor },
        { "GraphGridColor", &ColorScheme::GraphGridColor },
        { "GraphOverlayTextColor", &ColorScheme::GraphOverlayTextColor },
        { "SidePanelBackgroundColor", &ColorScheme::SidePanelBackgroundColor },
        { "SidePanelItemSelectedBackgroundColor", &ColorScheme::SidePanelItemSelectedBackgroundColor },
        { "SidePanelItemHoverBackgroundColor", &ColorScheme::SidePanelItemHoverBackgroundColor },
        { "SidePanelItemBackgroundColor", &ColorScheme::SidePanelItemBackgroundColor },
        { "SidePanelItemSelectedTextColor", &ColorScheme::SidePanelItemSelectedTextColor },
        { "SidePanelItemTextColor", &ColorScheme::SidePanelItemTextColor },
        { "SidePanelItemSubtitleColor", &ColorScheme::SidePanelItemSubtitleColor },
        { "SidePanelItemSelectedBorderColor", &ColorScheme::SidePanelItemSelectedBorderColor },
        { "CpuTitleColor", &ColorScheme::CpuTitleColor },
        { "CpuHeaderValueColor", &ColorScheme::CpuHeaderValueColor },
        { "MemoryTitleColor", &ColorScheme::MemoryTitleColor },
        { "MemoryHeaderValueColor", &ColorScheme::MemoryHeaderValueColor },
        { "DiskTitleColor", &ColorScheme::DiskTitleColor },
        { "DiskHeaderValueColor", &ColorScheme::DiskHeaderValueColor },
        { "NetworkTitleColor", &ColorScheme::NetworkTitleColor },
        { "GpuTitleColor", &ColorScheme::GpuTitleColor },
        { "MutedTextColor", &ColorScheme::MutedTextColor },
        { "StatLabelColor", &ColorScheme::StatLabelColor },
        { "AxisLabelColor", &ColorScheme::AxisLabelColor },
        { "MemoryLegendTextColor", &ColorScheme::MemoryLegendTextColor },
        { "MemoryLegendUsedColor", &ColorScheme::MemoryLegendUsedColor },
        { "MemoryLegendDirtyColor", &ColorScheme::MemoryLegendDirtyColor },
        { "MemoryLegendCachedColor", &ColorScheme::MemoryLegendCachedColor },
        { "MemoryLegendFreeColor", &ColorScheme::MemoryLegendFreeColor },
        { "MemoryBarUsedColor", &ColorScheme::MemoryBarUsedColor },
        { "MemoryBarDirtyColor", &ColorScheme::MemoryBarDirtyColor },
        { "MemoryBarCachedColor", &ColorScheme::MemoryBarCachedColor },
        { "MemoryBarFreeColor", &ColorScheme::MemoryBarFreeColor },
        { "MemoryBarBorderColor", &ColorScheme::MemoryBarBorderColor }
    };

    QColor colorFromVariant(const QVariant &value, const QColor &fallback)
    {
        if (!value.isValid())
            return fallback;
        if (value.canConvert<QColor>())
        {
            const QColor color = value.value<QColor>();
            if (color.isValid())
                return color;
        }

        const QString text = value.toString().trimmed();
        if (text.isEmpty())
            return fallback;

        const QColor color = QColor::fromString(text);
        return color.isValid() ? color : fallback;
    }
}

ColorScheme *ColorScheme::GetCurrent()
{
    if (!ColorScheme::current)
        ColorScheme::current = new ColorScheme(ColorScheme::DefaultLight());
    return ColorScheme::current;
}

bool ColorScheme::DetectDarkMode()
{
    return QApplication::palette().color(QPalette::Window).lightness() <= 127;
}

const QVector<ColorScheme::ColorField> &ColorScheme::Fields()
{
    static const QVector<ColorScheme::ColorField> fields = []
    {
        QVector<ColorScheme::ColorField> items;
        items.reserve(static_cast<int>(sizeof(kColorFields) / sizeof(kColorFields[0])));
        for (const ColorScheme::ColorField &field : kColorFields)
            items.append(field);
        return items;
    }();
    return fields;
}

void ColorScheme::Install(ColorScheme *scheme)
{
    delete ColorScheme::current;
    ColorScheme::current = scheme;
}

ColorScheme::ColorScheme()
{}

ColorScheme ColorScheme::DefaultDark()
{
    ColorScheme scheme;
    scheme.DarkMode = true;

    scheme.CpuGraphLineColor = QColor(0x00, 0xbc, 0xff);
    scheme.CpuGraphFillColor = QColor(0x00, 0x4c, 0x8a, 120);
    scheme.CpuGraphSecondaryFillColor = QColor(0x00, 0x22, 0x55, 160);
    scheme.MemoryGraphLineColor = QColor(0xcc, 0x44, 0xcc);
    scheme.MemoryGraphFillColor = QColor(0x66, 0x11, 0x66, 130);
    scheme.DiskGraphLineColor = QColor(0x66, 0xbb, 0x44);
    scheme.DiskGraphFillColor = QColor(0x33, 0x66, 0x22, 120);
    scheme.DiskTransferGraphLineColor = QColor(0x88, 0xcc, 0x66);
    scheme.DiskTransferGraphFillColor = QColor(0x33, 0x66, 0x22, 100);
    scheme.DiskTransferGraphSecondaryFillColor = QColor(0x1f, 0x44, 0x15, 120);
    scheme.NetworkGraphLineColor = QColor(0xdb, 0x8b, 0x3a);
    scheme.NetworkGraphFillColor = QColor(0x66, 0x3f, 0x1f, 110);
    scheme.NetworkGraphSecondaryFillColor = QColor(0x4a, 0x28, 0x10, 130);
    scheme.GpuGraphLineColor = QColor(0x44, 0xa8, 0xff);
    scheme.GpuGraphFillColor = QColor(0x1e, 0x4d, 0x82, 110);
    scheme.GpuGraphSecondaryFillColor = QColor(0x14, 0x33, 0x58, 130);
    scheme.SwapUsageGraphLineColor = QColor(0xcc, 0x88, 0x44);
    scheme.SwapUsageGraphFillColor = QColor(0x66, 0x33, 0x11, 120);
    scheme.SwapActivityGraphLineColor = QColor(0xcc, 0xaa, 0x66);
    scheme.SwapActivityGraphFillColor = QColor(0x66, 0x44, 0x22, 100);
    scheme.SwapActivityGraphSecondaryFillColor = QColor(0x4a, 0x2d, 0x14, 120);
    scheme.GraphGridColor = QColor(0x88, 0x88, 0x99, 150);
    scheme.GraphOverlayTextColor = QColor(245, 245, 245, 220);
    scheme.SidePanelBackgroundColor = QColor(0x12, 0x12, 0x1a);
    scheme.SidePanelItemSelectedBackgroundColor = QColor(0x1a, 0x4a, 0x8a, 200);
    scheme.SidePanelItemHoverBackgroundColor = QColor(0x30, 0x30, 0x40, 120);
    scheme.SidePanelItemBackgroundColor = QColor(0x1a, 0x1a, 0x22, 180);
    scheme.SidePanelItemSelectedTextColor = QColor(0xff, 0xff, 0xff);
    scheme.SidePanelItemTextColor = QColor(0xcc, 0xcc, 0xcc);
    scheme.SidePanelItemSubtitleColor = QColor(0x88, 0xaa, 0xcc);
    scheme.SidePanelItemSelectedBorderColor = QColor(0x44, 0x88, 0xff);
    scheme.CpuTitleColor = scheme.CpuGraphLineColor;
    scheme.CpuHeaderValueColor = QColor(0xaa, 0xcc, 0xff);
    scheme.MemoryTitleColor = scheme.MemoryGraphLineColor;
    scheme.MemoryHeaderValueColor = QColor(0xdd, 0xaa, 0xdd);
    scheme.DiskTitleColor = scheme.DiskGraphLineColor;
    scheme.DiskHeaderValueColor = QColor(0xaa, 0xdd, 0xaa);
    scheme.NetworkTitleColor = scheme.NetworkGraphLineColor;
    scheme.GpuTitleColor = scheme.GpuGraphLineColor;
    scheme.MutedTextColor = QColor(0xaa, 0xaa, 0xaa);
    scheme.StatLabelColor = QColor(0x88, 0x88, 0x88);
    scheme.AxisLabelColor = QColor(0x66, 0x66, 0x66);
    scheme.MemoryLegendTextColor = QColor(0xaa, 0xaa, 0xaa);
    scheme.MemoryLegendUsedColor = scheme.MemoryGraphLineColor;
    scheme.MemoryLegendDirtyColor = QColor(0xbb, 0x88, 0x00);
    scheme.MemoryLegendCachedColor = QColor(0x55, 0x22, 0x55);
    scheme.MemoryLegendFreeColor = QColor(0x33, 0x33, 0x33);
    scheme.MemoryBarUsedColor = scheme.MemoryGraphLineColor;
    scheme.MemoryBarDirtyColor = QColor(0xbb, 0x88, 0x00);
    scheme.MemoryBarCachedColor = QColor(0x55, 0x22, 0x55);
    scheme.MemoryBarFreeColor = QColor(0x11, 0x08, 0x11);
    scheme.MemoryBarBorderColor = QColor(0x88, 0x44, 0x88);
    return scheme;
}

ColorScheme ColorScheme::DefaultLight()
{
    ColorScheme scheme;
    scheme.DarkMode = false;

    scheme.CpuGraphLineColor = QColor(0x00, 0x8f, 0xcc);
    scheme.CpuGraphFillColor = QColor(0x99, 0xd9, 0xff, 120);
    scheme.CpuGraphSecondaryFillColor = QColor(0x5c, 0xb9, 0xec, 130);
    scheme.MemoryGraphLineColor = QColor(0xb0, 0x3d, 0xb0);
    scheme.MemoryGraphFillColor = QColor(0xe7, 0xba, 0xe7, 130);
    scheme.DiskGraphLineColor = QColor(0x5a, 0x9d, 0x3b);
    scheme.DiskGraphFillColor = QColor(0xc9, 0xe1, 0xbf, 120);
    scheme.DiskTransferGraphLineColor = QColor(0x77, 0xb8, 0x4f);
    scheme.DiskTransferGraphFillColor = QColor(0xd8, 0xea, 0xcf, 110);
    scheme.DiskTransferGraphSecondaryFillColor = QColor(0xbc, 0xd8, 0xad, 125);
    scheme.NetworkGraphLineColor = QColor(0xc8, 0x74, 0x1d);
    scheme.NetworkGraphFillColor = QColor(0xf0, 0xcd, 0xaa, 115);
    scheme.NetworkGraphSecondaryFillColor = QColor(0xe2, 0xb1, 0x7a, 125);
    scheme.GpuGraphLineColor = QColor(0x2e, 0x87, 0xd1);
    scheme.GpuGraphFillColor = QColor(0xb6, 0xd6, 0xf3, 115);
    scheme.GpuGraphSecondaryFillColor = QColor(0x8b, 0xbf, 0xe9, 125);
    scheme.SwapUsageGraphLineColor = QColor(0xb4, 0x76, 0x35);
    scheme.SwapUsageGraphFillColor = QColor(0xe7, 0xc9, 0xaa, 120);
    scheme.SwapActivityGraphLineColor = QColor(0xb3, 0x8d, 0x4b);
    scheme.SwapActivityGraphFillColor = QColor(0xe8, 0xd8, 0xb8, 110);
    scheme.SwapActivityGraphSecondaryFillColor = QColor(0xd5, 0xb8, 0x88, 125);
    scheme.GraphGridColor = QColor(0x80, 0x80, 0x80, 72);
    scheme.GraphOverlayTextColor = QColor(35, 35, 35, 220);
    scheme.SidePanelBackgroundColor = QColor(0xf3, 0xf5, 0xf8);
    scheme.SidePanelItemSelectedBackgroundColor = QColor(0x66, 0xa8, 0xff, 96);
    scheme.SidePanelItemHoverBackgroundColor = QColor(0x00, 0x00, 0x00, 20);
    scheme.SidePanelItemBackgroundColor = QColor(0xff, 0xff, 0xff);
    scheme.SidePanelItemSelectedTextColor = QColor(0x12, 0x24, 0x36);
    scheme.SidePanelItemTextColor = QColor(0x24, 0x24, 0x24);
    scheme.SidePanelItemSubtitleColor = QColor(0x5e, 0x7a, 0x96);
    scheme.SidePanelItemSelectedBorderColor = QColor(0x44, 0x88, 0xff);
    scheme.CpuTitleColor = scheme.CpuGraphLineColor;
    scheme.CpuHeaderValueColor = QColor(0x5d, 0x84, 0xaa);
    scheme.MemoryTitleColor = scheme.MemoryGraphLineColor;
    scheme.MemoryHeaderValueColor = QColor(0xb5, 0x7f, 0xb5);
    scheme.DiskTitleColor = scheme.DiskGraphLineColor;
    scheme.DiskHeaderValueColor = QColor(0x74, 0xa5, 0x5d);
    scheme.NetworkTitleColor = scheme.NetworkGraphLineColor;
    scheme.GpuTitleColor = scheme.GpuGraphLineColor;
    scheme.MutedTextColor = QColor(0x77, 0x77, 0x77);
    scheme.StatLabelColor = QColor(0x7a, 0x7a, 0x7a);
    scheme.AxisLabelColor = QColor(0x6d, 0x6d, 0x6d);
    scheme.MemoryLegendTextColor = QColor(0x77, 0x77, 0x77);
    scheme.MemoryLegendUsedColor = scheme.MemoryGraphLineColor;
    scheme.MemoryLegendDirtyColor = QColor(0xb0, 0x85, 0x23);
    scheme.MemoryLegendCachedColor = QColor(0x9b, 0x75, 0x9b);
    scheme.MemoryLegendFreeColor = QColor(0x8c, 0x8c, 0x8c);
    scheme.MemoryBarUsedColor = QColor(0xc9, 0x7f, 0xc9);
    scheme.MemoryBarDirtyColor = QColor(0xd1, 0xa0, 0x3e);
    scheme.MemoryBarCachedColor = QColor(0xd7, 0xbf, 0xd7);
    scheme.MemoryBarFreeColor = QColor(0xe9, 0xe9, 0xe9);
    scheme.MemoryBarBorderColor = QColor(0xb0, 0x93, 0xb0);
    return scheme;
}

QVariantMap ColorScheme::ToVariantMap() const
{
    QVariantMap map;
    map.insert("DarkMode", this->DarkMode);
    for (const ColorScheme::ColorField &field : ColorScheme::Fields())
        map.insert(field.Name, (this->*field.Member).name(QColor::HexArgb));
    return map;
}

void ColorScheme::ApplyVariantMap(const QVariantMap &map)
{
    this->DarkMode = map.value("DarkMode", this->DarkMode).toBool();
    for (const ColorScheme::ColorField &field : ColorScheme::Fields())
        this->*field.Member = colorFromVariant(map.value(field.Name), this->*field.Member);
}
