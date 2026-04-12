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

#include "runtaskdialog.h"
#include "ui_runtaskdialog.h"
#include "configuration.h"

#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QLineEdit>
#include <QPushButton>

RunTaskDialog::RunTaskDialog(QWidget *parent) : QDialog(parent), ui(new Ui::RunTaskDialog)
{
    this->ui->setupUi(this);

    this->ui->commandCombo->addItems(CFG->TaskHistory);
    this->ui->commandCombo->setCurrentText(QString());
    this->ui->commandCombo->setFocus();

    this->m_okButton = this->ui->buttonBox->button(QDialogButtonBox::Ok);

    connect(this->ui->buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(this->ui->buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(this->ui->browseButton, &QPushButton::clicked, this, &RunTaskDialog::browseForCommand);
    connect(this->ui->commandCombo->lineEdit(), &QLineEdit::textChanged, this, [this]()
    {
        this->updateOkButton();
    });

    this->updateOkButton();
}

RunTaskDialog::~RunTaskDialog()
{
    delete this->ui;
}

QString RunTaskDialog::Command() const
{
    return this->ui->commandCombo->currentText().trimmed();
}

void RunTaskDialog::browseForCommand()
{
    QString startDir = CFG->LastTaskDirectory;
    if (startDir.isEmpty() || !QDir(startDir).exists())
        startDir = QStringLiteral("/usr/bin");

    const QString path = QFileDialog::getOpenFileName(this, tr("Select executable"), startDir, tr("All files (*)"));
    if (path.isEmpty())
        return;

    CFG->LastTaskDirectory = QFileInfo(path).absolutePath();
    CFG->Save();
    this->ui->commandCombo->setCurrentText(shellQuote(path));
}

void RunTaskDialog::updateOkButton()
{
    if (this->m_okButton)
        this->m_okButton->setEnabled(!this->Command().isEmpty());
}

QString RunTaskDialog::shellQuote(const QString &text)
{
    QString escaped = text;
    escaped.replace('\'', QStringLiteral("'\"'\"'"));
    return QStringLiteral("'%1'").arg(escaped);
}
