/*
 * ThemeDialog.cpp - Dedicated Theme & Appearance Dialog for LMMS Orion
 *
 * Copyright (c) 2026 LMMS Orion Team
 *
 * This file is part of LMMS - https://lmms.io
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#include "ThemeDialog.h"
#include "ColorChooser.h"
#include "ConfigManager.h"
#include "embed.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QScrollArea>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>

namespace lmms::gui
{

ThemeDialog::ThemeDialog(QWidget* parent) :
	QDialog(parent)
{
	setWindowTitle(tr("Theme & Appearance - LMMS Orion"));
	setWindowIcon(embed::getIconPixmap("colorize"));
	setMinimumWidth(480);

	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setSpacing(12);

	// 1. Preset Selection Group
	auto presetGroup = new QGroupBox(tr("Theme Presets"), this);
	auto presetLayout = new QHBoxLayout(presetGroup);

	m_presetCombo = new QComboBox(presetGroup);
	presetLayout->addWidget(new QLabel(tr("Preset:"), presetGroup));
	presetLayout->addWidget(m_presetCombo, 1);

	auto savePresetBtn = new QPushButton(tr("Save As..."), presetGroup);
	connect(savePresetBtn, &QPushButton::clicked, this, &ThemeDialog::onSaveAsPreset);
	presetLayout->addWidget(savePresetBtn);

	mainLayout->addWidget(presetGroup);

	// 2. Color Palette Group
	auto colorGroup = new QGroupBox(tr("Color Customization"), this);
	auto colorLayout = new QGridLayout(colorGroup);
	colorLayout->setColumnStretch(1, 1);

	colorLayout->addWidget(new QLabel(tr("Accent Color (Knobs, LEDs, Selection):"), colorGroup), 0, 0);
	m_accentSwatchBtn = new QPushButton(colorGroup);
	m_accentSwatchBtn->setMinimumHeight(28);
	m_accentSwatchBtn->setCursor(Qt::PointingHandCursor);
	connect(m_accentSwatchBtn, &QPushButton::clicked, this, &ThemeDialog::onPickAccentColor);
	colorLayout->addWidget(m_accentSwatchBtn, 0, 1);

	colorLayout->addWidget(new QLabel(tr("Base Panel Color (Toolbars, Menus):"), colorGroup), 1, 0);
	m_baseSwatchBtn = new QPushButton(colorGroup);
	m_baseSwatchBtn->setMinimumHeight(28);
	m_baseSwatchBtn->setCursor(Qt::PointingHandCursor);
	connect(m_baseSwatchBtn, &QPushButton::clicked, this, &ThemeDialog::onPickBaseColor);
	colorLayout->addWidget(m_baseSwatchBtn, 1, 1);

	colorLayout->addWidget(new QLabel(tr("Window Borders & Frames Color:"), colorGroup), 2, 0);
	m_borderSwatchBtn = new QPushButton(colorGroup);
	m_borderSwatchBtn->setMinimumHeight(28);
	m_borderSwatchBtn->setCursor(Qt::PointingHandCursor);
	connect(m_borderSwatchBtn, &QPushButton::clicked, this, &ThemeDialog::onPickBorderColor);
	colorLayout->addWidget(m_borderSwatchBtn, 2, 1);

	colorLayout->addWidget(new QLabel(tr("Workspace Background Color:"), colorGroup), 3, 0);
	m_windowSwatchBtn = new QPushButton(colorGroup);
	m_windowSwatchBtn->setMinimumHeight(28);
	m_windowSwatchBtn->setCursor(Qt::PointingHandCursor);
	connect(m_windowSwatchBtn, &QPushButton::clicked, this, &ThemeDialog::onPickWindowColor);
	colorLayout->addWidget(m_windowSwatchBtn, 3, 1);

	// Extended precise color customization (scrollable area)
	auto keyedContainer = new QWidget(colorGroup);
	m_keyedColorLayout = new QGridLayout(keyedContainer);
	m_keyedColorLayout->setContentsMargins(0, 0, 0, 0);
	m_keyedColorLayout->setColumnStretch(1, 1);
	m_keyedColorRow = 0;

	addColorRow(tr("Accent Hover Color:"), QStringLiteral("accentHover"));
	addColorRow(tr("Text on Accent:"), QStringLiteral("textActive"));
	addColorRow(tr("General Text Color:"), QStringLiteral("textColor"));
	addColorRow(tr("Grid Lines Color:"), QStringLiteral("grid"));
	addColorRow(tr("Beat Lines Color:"), QStringLiteral("beatLine"));
	addColorRow(tr("Bar Lines Color:"), QStringLiteral("barLine"));
	addColorRow(tr("Note Color:"), QStringLiteral("note"));
	addColorRow(tr("Selection Color:"), QStringLiteral("selected"));
	addColorRow(tr("Input Fields Color:"), QStringLiteral("field"));
	addColorRow(tr("Sliders & Scrollbars Color:"), QStringLiteral("slider"));
	addColorRow(tr("Headers & Group Boxes Color:"), QStringLiteral("header"));

	auto keyedScroll = new QScrollArea(colorGroup);
	keyedScroll->setWidget(keyedContainer);
	keyedScroll->setWidgetResizable(true);
	keyedScroll->setFrameShape(QFrame::NoFrame);
	keyedScroll->setFixedHeight(190);
	keyedScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	colorLayout->addWidget(keyedScroll, 4, 0, 1, 2);

	mainLayout->addWidget(colorGroup);

	// 3. Workspace Wallpaper Group
	auto wallpaperGroup = new QGroupBox(tr("Workspace Wallpaper"), this);
	auto wallpaperLayout = new QHBoxLayout(wallpaperGroup);

	m_wallpaperPathEdit = new QLineEdit(wallpaperGroup);
	m_wallpaperPathEdit->setReadOnly(true);
	m_wallpaperPathEdit->setPlaceholderText(tr("No wallpaper selected (using background color)"));
	wallpaperLayout->addWidget(m_wallpaperPathEdit, 1);

	auto browseBtn = new QPushButton(tr("Browse..."), wallpaperGroup);
	connect(browseBtn, &QPushButton::clicked, this, &ThemeDialog::onBrowseWallpaper);
	wallpaperLayout->addWidget(browseBtn);

	auto clearBtn = new QPushButton(tr("Clear"), wallpaperGroup);
	connect(clearBtn, &QPushButton::clicked, this, &ThemeDialog::onClearWallpaper);
	wallpaperLayout->addWidget(clearBtn);

	mainLayout->addWidget(wallpaperGroup);

	// 4. Options Group
	auto optionsGroup = new QGroupBox(tr("Project Integration"), this);
	auto optionsLayout = new QVBoxLayout(optionsGroup);

	m_saveWithProjectCheck = new QCheckBox(tr("Save this custom theme and wallpaper with current project"), optionsGroup);
	m_saveWithProjectCheck->setChecked(ThemeManager::inst()->currentTheme().isProjectTheme);
	optionsLayout->addWidget(m_saveWithProjectCheck);

	m_applyProjectThemesCheck = new QCheckBox(tr("Apply project-specific themes when loading projects"), optionsGroup);
	m_applyProjectThemesCheck->setChecked(ThemeManager::inst()->applyProjectThemes());
	connect(m_applyProjectThemesCheck, &QCheckBox::toggled, this, &ThemeDialog::onApplyProjectThemesToggled);
	optionsLayout->addWidget(m_applyProjectThemesCheck);

	mainLayout->addWidget(optionsGroup);

	// 5. Bottom Buttons
	auto bottomLayout = new QHBoxLayout;

	auto resetBtn = new QPushButton(tr("Reset to Global Default"), this);
	connect(resetBtn, &QPushButton::clicked, this, &ThemeDialog::onResetToDefault);
	bottomLayout->addWidget(resetBtn);

	bottomLayout->addStretch();

	auto closeBtn = new QPushButton(tr("Close"), this);
	closeBtn->setDefault(true);
	connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
	bottomLayout->addWidget(closeBtn);

	mainLayout->addLayout(bottomLayout);

	populatePresets();
	updateSwatches();

	connect(m_presetCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ThemeDialog::onPresetChanged);
}

void ThemeDialog::populatePresets()
{
	m_presetCombo->blockSignals(true);
	m_presetCombo->clear();

	const auto& presets = ThemeManager::inst()->factoryPresets();
	int activeIndex = -1;
	QString currentName = ThemeManager::inst()->currentTheme().name;

	for (int i = 0; i < presets.size(); ++i)
	{
		m_presetCombo->addItem(presets[i].name);
		if (presets[i].name == currentName)
		{
			activeIndex = i;
		}
	}

	// Scan user themes directory
	QString userDir = ConfigManager::inst()->workingDir() + "themes/";
	QDir dir(userDir);
	if (dir.exists())
	{
		QStringList filters;
		filters << "*.lmms-theme";
		for (const QFileInfo& fi : dir.entryInfoList(filters, QDir::Files))
		{
			QString baseName = fi.baseName();
			m_presetCombo->addItem(baseName);
			if (baseName == currentName)
			{
				activeIndex = m_presetCombo->count() - 1;
			}
		}
	}

	if (activeIndex >= 0)
	{
		m_presetCombo->setCurrentIndex(activeIndex);
	}
	else
	{
		m_presetCombo->addItem(tr("Custom"));
		m_presetCombo->setCurrentIndex(m_presetCombo->count() - 1);
	}

	m_presetCombo->blockSignals(false);
}

void ThemeDialog::addColorRow(const QString& label, const QString& key)
{
	if (m_keyedColorLayout == nullptr) { return; }

	auto* rowLabel = new QLabel(label, m_keyedColorLayout->parentWidget());
	auto* btn = new QPushButton(m_keyedColorLayout->parentWidget());
	btn->setMinimumHeight(26);
	btn->setCursor(Qt::PointingHandCursor);
	btn->setProperty("colorKey", key);
	connect(btn, &QPushButton::clicked, this, &ThemeDialog::onPickKeyedColor);

	const int row = m_keyedColorRow++;
	m_keyedColorLayout->addWidget(rowLabel, row, 0);
	m_keyedColorLayout->addWidget(btn, row, 1);
	m_keyedSwatchBtns.append({key, btn});
}

void ThemeDialog::formatSwatch(QPushButton* btn, const QString& hex)
{
	QColor col(hex);
	QString textColor = (col.lightness() > 160) ? "#111111" : "#ffffff";
	btn->setText(hex.toUpper());
	btn->setStyleSheet(QString(
		"QPushButton { background-color: %1; color: %2; font-weight: bold; border-radius: 4px; border: 1px solid #555; }"
		"QPushButton:hover { border: 2px solid #fff; }"
	).arg(hex, textColor));
}

void ThemeDialog::updateSwatches()
{
	const auto& theme = ThemeManager::inst()->currentTheme();

	formatSwatch(m_accentSwatchBtn, theme.accentColor);
	formatSwatch(m_baseSwatchBtn, theme.baseColor);
	formatSwatch(m_borderSwatchBtn, theme.borderColor.isEmpty() ? theme.accentColor : theme.borderColor);
	formatSwatch(m_windowSwatchBtn, theme.windowColor);

	for (const auto& pair : m_keyedSwatchBtns)
	{
		QString hex = ThemeManager::inst()->colorValue(pair.first);
		if (!hex.isEmpty()) { formatSwatch(pair.second, hex); }
	}

	m_wallpaperPathEdit->setText(theme.backgroundPic);
}

void ThemeDialog::onPresetChanged(int index)
{
	if (index < 0) return;
	QString name = m_presetCombo->itemText(index);
	if (name == tr("Custom")) return;

	ThemeManager::inst()->applyThemeByName(name);
	updateSwatches();
}

void ThemeDialog::onPickAccentColor()
{
	QColor current(ThemeManager::inst()->currentTheme().accentColor);
	ColorChooser dlg(current, this);
	if (dlg.exec() == QDialog::Accepted)
	{
		ThemeManager::inst()->setAccentColor(dlg.selectedColor());
		updateSwatches();
	}
}

void ThemeDialog::onPickBaseColor()
{
	QColor current(ThemeManager::inst()->currentTheme().baseColor);
	ColorChooser dlg(current, this);
	if (dlg.exec() == QDialog::Accepted)
	{
		ThemeManager::inst()->setBaseColor(dlg.selectedColor());
		updateSwatches();
	}
}

void ThemeDialog::onPickBorderColor()
{
	QString currentHex = ThemeManager::inst()->currentTheme().borderColor;
	if (currentHex.isEmpty()) currentHex = ThemeManager::inst()->currentTheme().accentColor;
	QColor current(currentHex);
	ColorChooser dlg(current, this);
	if (dlg.exec() == QDialog::Accepted)
	{
		ThemeManager::inst()->setBorderColor(dlg.selectedColor());
		updateSwatches();
	}
}

void ThemeDialog::onPickWindowColor()
{
	QColor current(ThemeManager::inst()->currentTheme().windowColor);
	ColorChooser dlg(current, this);
	if (dlg.exec() == QDialog::Accepted)
	{
		ThemeManager::inst()->setWindowColor(dlg.selectedColor());
		updateSwatches();
	}
}

void ThemeDialog::onPickKeyedColor()
{
	auto* btn = qobject_cast<QPushButton*>(sender());
	if (btn == nullptr) { return; }

	const QString key = btn->property("colorKey").toString();
	QString currentHex = ThemeManager::inst()->colorValue(key);
	if (currentHex.isEmpty()) { return; }

	ColorChooser dlg(QColor(currentHex), this);
	if (dlg.exec() == QDialog::Accepted)
	{
		ThemeManager::inst()->setThemeColor(key, dlg.selectedColor());
		updateSwatches();
	}
}

void ThemeDialog::onBrowseWallpaper()
{
	QString filter = tr("Images (*.png *.jpg *.jpeg *.bmp *.svg);;All Files (*.*)");
	QString file = QFileDialog::getOpenFileName(this, tr("Select Workspace Wallpaper"), "", filter);
	if (!file.isEmpty())
	{
		ThemeManager::inst()->setBackgroundPicture(file);
		updateSwatches();
	}
}

void ThemeDialog::onClearWallpaper()
{
	ThemeManager::inst()->clearBackgroundPicture();
	updateSwatches();
}

void ThemeDialog::onSaveAsPreset()
{
	bool ok = false;
	QString name = QInputDialog::getText(this, tr("Save Theme Preset"),
		tr("Enter a name for this theme preset:"), QLineEdit::Normal, "", &ok);
	if (!ok || name.trimmed().isEmpty()) return;

	name = name.trimmed();
	QString userDir = ConfigManager::inst()->workingDir() + "themes/";
	QString path = userDir + name + ".lmms-theme";

	ThemePreset p = ThemeManager::inst()->currentTheme();
	p.name = name;

	if (ThemeManager::inst()->savePresetToFile(path, p))
	{
		populatePresets();
		QMessageBox::information(this, tr("Theme Saved"),
			tr("Theme preset '%1' was successfully saved!").arg(name));
	}
	else
	{
		QMessageBox::warning(this, tr("Save Error"),
			tr("Could not save theme preset to %1").arg(path));
	}
}

void ThemeDialog::onResetToDefault()
{
	ThemeManager::inst()->resetToGlobalDefault();
	populatePresets();
	updateSwatches();
}

void ThemeDialog::onApplyProjectThemesToggled(bool checked)
{
	ThemeManager::inst()->setApplyProjectThemes(checked);
}

} // namespace lmms::gui
