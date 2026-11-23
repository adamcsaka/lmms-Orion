/*
 * ThemeManager.h - Centralized theme, styling, color palette, and wallpaper manager
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

#ifndef LMMS_GUI_THEME_MANAGER_H
#define LMMS_GUI_THEME_MANAGER_H

#include <QColor>
#include <QDomDocument>
#include <QDomElement>
#include <QObject>
#include <QString>
#include <QVector>

#include "lmms_export.h"

namespace lmms::gui
{

struct LMMS_EXPORT ThemePreset
{
	QString name;
	QString baseColor;          // Panels, menus, toolbars (e.g. #18191c)
	QString windowColor;        // Workspace background (e.g. #111214)
	QString accentColor;        // Knobs, LEDs, active buttons, selection (e.g. #e5a93c)
	QString accentHover;        // Hover highlights (e.g. #f5ba55)
	QString textActive;         // Text on accent button (e.g. #1a1a1a)
	QString textColor;          // General text (e.g. #d6d9e0)
	QString gridColor;          // Grid subdivisions (e.g. #26272b)
	QString beatLineColor;      // Beat lines (e.g. #7d5b1e)
	QString barLineColor;       // Measure/bar lines (e.g. #e5a93c)
	QString noteColor;          // Piano roll note color
	QString selectedColor;      // Selection highlight
	QString backgroundPic;      // Optional wallpaper path
	bool isProjectTheme = false;// Whether this is custom to the current project
};

class LMMS_EXPORT ThemeManager : public QObject
{
	Q_OBJECT
public:
	static ThemeManager* inst();

	const QVector<ThemePreset>& factoryPresets() const { return m_factoryPresets; }
	const ThemePreset& currentTheme() const { return m_currentTheme; }
	const ThemePreset& globalDefaultTheme() const { return m_globalDefaultTheme; }

	void applyTheme(const ThemePreset& preset, bool markProject = false);
	void applyThemeByName(const QString& name);
	void setAccentColor(const QColor& color);
	void setBaseColor(const QColor& color);
	void setWindowColor(const QColor& color);
	void setBackgroundPicture(const QString& path);
	void clearBackgroundPicture();

	void resetToGlobalDefault();

	bool applyProjectThemes() const;
	void setApplyProjectThemes(bool enable);

	// Per-project XML persistence
	void saveProjectTheme(QDomDocument& doc, QDomElement& parent);
	void loadProjectTheme(const QDomElement& element, const QString& projectPath);

	// User preset saving/loading
	bool savePresetToFile(const QString& filePath, const ThemePreset& preset);
	bool loadPresetFromFile(const QString& filePath, ThemePreset& preset);

	QString generateThemeQss(const ThemePreset& preset) const;

signals:
	void themeChanged(const ThemePreset& preset);

private:
	ThemeManager();
	void initFactoryPresets();
	void ensureBaseStyleSheetLoaded();

	ThemePreset m_currentTheme;
	ThemePreset m_globalDefaultTheme;
	QVector<ThemePreset> m_factoryPresets;
	QString m_baseStyleSheet;
};

} // namespace lmms::gui

#endif // LMMS_GUI_THEME_MANAGER_H
