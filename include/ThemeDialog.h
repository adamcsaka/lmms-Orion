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
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QLabel>

#include "ThemeManager.h"
#include "lmms_export.h"

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
	void onPickWindowColor();
	void onBrowseWallpaper();
	void onClearWallpaper();
	void onSaveAsPreset();
	void onResetToDefault();
	void onApplyProjectThemesToggled(bool checked);

private:
	void updateSwatches();
	void populatePresets();

	QComboBox* m_presetCombo;
	QPushButton* m_accentSwatchBtn;
	QPushButton* m_baseSwatchBtn;
	QPushButton* m_windowSwatchBtn;
	QLineEdit* m_wallpaperPathEdit;
	QCheckBox* m_saveWithProjectCheck;
	QCheckBox* m_applyProjectThemesCheck;
};

} // namespace lmms::gui

#endif // LMMS_GUI_THEME_DIALOG_H
