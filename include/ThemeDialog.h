/*
 * ThemeDialog.h - Dedicated Theme & Appearance Dialog for LMMS Orion
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

#ifndef LMMS_GUI_THEME_DIALOG_H
#define LMMS_GUI_THEME_DIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QHash>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>
#include <QPair>
#include <QVector>

#include "ThemeManager.h"
#include "lmms_export.h"

class QGridLayout;

namespace lmms::gui
{

class LMMS_EXPORT ThemeDialog : public QDialog
{
	Q_OBJECT
public:
	ThemeDialog(QWidget* parent = nullptr);

private slots:
	void onPresetChanged(int index);
	void onPickAccentColor();
	void onPickBaseColor();
	void onPickBorderColor();
	void onPickWindowColor();
	void onPickKeyedColor();
	void onBrowseWallpaper();
	void onClearWallpaper();
	void onSaveAsPreset();
	void onResetToDefault();
	void onApplyProjectThemesToggled(bool checked);
	void onRandomizeAllColors();

private:
	void updateSwatches();
	void populatePresets();
	void addColorRow(const QString& label, const QString& key);
	void formatSwatch(QPushButton* btn, const QString& hex);
	void randomizeThemeColor(const QString& key);
	static QColor randomColor();
	QPushButton* makeRandomColorButton(const QString& key, QWidget* parent);

	QComboBox* m_presetCombo;
	QPushButton* m_accentSwatchBtn;
	QPushButton* m_baseSwatchBtn;
	QPushButton* m_borderSwatchBtn;
	QPushButton* m_windowSwatchBtn;
	QVector<QPair<QString, QPushButton*>> m_keyedSwatchBtns;
	::QGridLayout* m_keyedColorLayout = nullptr;
	int m_keyedColorRow = 0;
	QLineEdit* m_wallpaperPathEdit;
	QCheckBox* m_saveWithProjectCheck;
	QCheckBox* m_applyProjectThemesCheck;
};

} // namespace lmms::gui

#endif // LMMS_GUI_THEME_DIALOG_H
