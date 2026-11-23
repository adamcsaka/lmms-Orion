/*
 * ThemeManager.cpp - Centralized theme, styling, color palette, and wallpaper manager
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

#include "ThemeManager.h"
#include "ConfigManager.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "Engine.h"
#include "Song.h"

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <QDebug>

namespace lmms::gui
{

ThemeManager* ThemeManager::inst()
{
	static ThemeManager s_instance;
	return &s_instance;
}

ThemeManager::ThemeManager()
{
	initFactoryPresets();

	// Global default is Orion Obsidian Gold
	QString defName = ConfigManager::inst()->value("ui", "defaultthemename");
	if (defName.isEmpty())
	{
		defName = "Orion Obsidian Gold";
	}

	for (const auto& p : m_factoryPresets)
	{
		if (p.name == defName)
		{
			m_globalDefaultTheme = p;
			break;
		}
	}
	if (m_globalDefaultTheme.name.isEmpty() && !m_factoryPresets.isEmpty())
	{
		m_globalDefaultTheme = m_factoryPresets.first();
	}

	m_currentTheme = m_globalDefaultTheme;
}

void ThemeManager::initFactoryPresets()
{
	m_factoryPresets.clear();

	// 1. Orion Obsidian Gold (Signature LMMS Orion Theme)
	{
		ThemePreset p;
		p.name = "Orion Obsidian Gold";
		p.baseColor = "#18191c";
		p.windowColor = "#111214";
		p.accentColor = "#e5a93c";
		p.accentHover = "#f5ba55";
		p.textActive = "#1a1a1a";
		p.textColor = "#d6d9e0";
		p.gridColor = "#26272b";
		p.beatLineColor = "#7d5b1e";
		p.barLineColor = "#e5a93c";
		p.noteColor = "#e5a93c";
		p.selectedColor = "#996f24";
		p.backgroundPic = "";
		m_factoryPresets.append(p);
	}

	// 2. Cyber Neon Cyan
	{
		ThemePreset p;
		p.name = "Cyber Neon Cyan";
		p.baseColor = "#12131d";
		p.windowColor = "#0b0c13";
		p.accentColor = "#00e5ff";
		p.accentHover = "#4debff";
		p.textActive = "#000000";
		p.textColor = "#d8e2f0";
		p.gridColor = "#1f2130";
		p.beatLineColor = "#9c27b0";
		p.barLineColor = "#00e5ff";
		p.noteColor = "#00e5ff";
		p.selectedColor = "#0277bd";
		p.backgroundPic = "";
		m_factoryPresets.append(p);
	}

	// 3. Studio Dark Lime
	{
		ThemePreset p;
		p.name = "Studio Dark Lime";
		p.baseColor = "#1d1e22";
		p.windowColor = "#131417";
		p.accentColor = "#10b981";
		p.accentHover = "#34d399";
		p.textActive = "#ffffff";
		p.textColor = "#e2e8f0";
		p.gridColor = "#282a30";
		p.beatLineColor = "#065f46";
		p.barLineColor = "#10b981";
		p.noteColor = "#10b981";
		p.selectedColor = "#047857";
		p.backgroundPic = "";
		m_factoryPresets.append(p);
	}

	// 4. Solarized Amber
	{
		ThemePreset p;
		p.name = "Solarized Amber";
		p.baseColor = "#1c2128";
		p.windowColor = "#14171d";
		p.accentColor = "#f59e0b";
		p.accentHover = "#fbbf24";
		p.textActive = "#1c2128";
		p.textColor = "#e5e9f0";
		p.gridColor = "#2d333b";
		p.beatLineColor = "#92400e";
		p.barLineColor = "#f59e0b";
		p.noteColor = "#f59e0b";
		p.selectedColor = "#b45309";
		p.backgroundPic = "";
		m_factoryPresets.append(p);
	}

	// 5. Classic LMMS
	{
		ThemePreset p;
		p.name = "Classic LMMS";
		p.baseColor = "#2c2d30";
		p.windowColor = "#141616";
		p.accentColor = "#0bd556";
		p.accentHover = "#27e06a";
		p.textActive = "#ffffff";
		p.textColor = "#d1d8e4";
		p.gridColor = "#292929";
		p.beatLineColor = "#4a3bba";
		p.barLineColor = "#8173fe";
		p.noteColor = "#0bd556";
		p.selectedColor = "#064d79";
		p.backgroundPic = "";
		m_factoryPresets.append(p);
	}
}

void ThemeManager::ensureBaseStyleSheetLoaded()
{
	if (m_baseStyleSheet.isEmpty())
	{
		QFile file("resources:style.css");
		if (file.open(QIODevice::ReadOnly))
		{
			m_baseStyleSheet = QString::fromUtf8(file.readAll());
		}
	}
}

void ThemeManager::applyTheme(const ThemePreset& preset, bool markProject)
{
	ensureBaseStyleSheetLoaded();
	m_currentTheme = preset;

	if (markProject)
	{
		m_currentTheme.isProjectTheme = true;
		if (Engine::getSong())
		{
			Engine::getSong()->setModified();
		}
	}

	// Dynamic QSS application
	QString dynamicQss = generateThemeQss(m_currentTheme);
	qApp->setStyleSheet(m_baseStyleSheet + "\n" + dynamicQss);

	// Workspace background application
	if (getGUI() && getGUI()->mainWindow())
	{
		getGUI()->mainWindow()->setWorkspaceBackground(m_currentTheme.backgroundPic);
	}

	emit themeChanged(m_currentTheme);
}

void ThemeManager::applyThemeByName(const QString& name)
{
	for (const auto& p : m_factoryPresets)
	{
		if (p.name.compare(name, Qt::CaseInsensitive) == 0)
		{
			applyTheme(p);
			return;
		}
	}

	// Check user preset file
	QString userDir = ConfigManager::inst()->workingDir() + "themes/";
	QString path = userDir + name + ".lmms-theme";
	ThemePreset p;
	if (loadPresetFromFile(path, p))
	{
		applyTheme(p);
	}
}

void ThemeManager::setAccentColor(const QColor& color)
{
	if (!color.isValid()) return;
	m_currentTheme.accentColor = color.name();
	m_currentTheme.accentHover = color.lighter(115).name();
	m_currentTheme.barLineColor = color.name();
	m_currentTheme.noteColor = color.name();
	m_currentTheme.beatLineColor = color.darker(150).name();
	m_currentTheme.selectedColor = color.darker(130).name();
	m_currentTheme.textActive = (color.lightness() > 160) ? "#101010" : "#ffffff";
	applyTheme(m_currentTheme, true);
}

void ThemeManager::setBaseColor(const QColor& color)
{
	if (!color.isValid()) return;
	m_currentTheme.baseColor = color.name();
	applyTheme(m_currentTheme, true);
}

void ThemeManager::setWindowColor(const QColor& color)
{
	if (!color.isValid()) return;
	m_currentTheme.windowColor = color.name();
	applyTheme(m_currentTheme, true);
}

void ThemeManager::setBackgroundPicture(const QString& path)
{
	m_currentTheme.backgroundPic = path;
	applyTheme(m_currentTheme, true);
}

void ThemeManager::clearBackgroundPicture()
{
	m_currentTheme.backgroundPic = "";
	applyTheme(m_currentTheme, true);
}

void ThemeManager::resetToGlobalDefault()
{
	applyTheme(m_globalDefaultTheme, false);
}

bool ThemeManager::applyProjectThemes() const
{
	QString val = ConfigManager::inst()->value("ui", "applyprojectthemes");
	if (val.isEmpty()) return true; // default enabled
	return val.toInt() != 0;
}

void ThemeManager::setApplyProjectThemes(bool enable)
{
	ConfigManager::inst()->setValue("ui", "applyprojectthemes", QString::number(enable ? 1 : 0));
}

void ThemeManager::saveProjectTheme(QDomDocument& doc, QDomElement& parent)
{
	if (!m_currentTheme.isProjectTheme && m_currentTheme.name == m_globalDefaultTheme.name)
	{
		// No custom project theme to serialize
		return;
	}

	QDomElement elem = doc.createElement("projecttheme");
	elem.setAttribute("preset", m_currentTheme.name);
	elem.setAttribute("base", m_currentTheme.baseColor);
	elem.setAttribute("window", m_currentTheme.windowColor);
	elem.setAttribute("accent", m_currentTheme.accentColor);
	elem.setAttribute("accentHover", m_currentTheme.accentHover);
	elem.setAttribute("textActive", m_currentTheme.textActive);
	elem.setAttribute("textColor", m_currentTheme.textColor);
	elem.setAttribute("grid", m_currentTheme.gridColor);
	elem.setAttribute("beatLine", m_currentTheme.beatLineColor);
	elem.setAttribute("barLine", m_currentTheme.barLineColor);
	elem.setAttribute("note", m_currentTheme.noteColor);
	elem.setAttribute("selected", m_currentTheme.selectedColor);

	// Store background image as relative path if located inside project directory
	QString bg = m_currentTheme.backgroundPic;
	if (!bg.isEmpty() && Engine::getSong())
	{
		QString projPath = Engine::getSong()->projectFileName();
		if (!projPath.isEmpty())
		{
			QDir projDir(QFileInfo(projPath).absolutePath());
			if (bg.startsWith(projDir.absolutePath()))
			{
				bg = projDir.relativeFilePath(bg);
			}
		}
	}
	elem.setAttribute("bg", bg);
	elem.setAttribute("enabled", "1");

	parent.appendChild(elem);
}

void ThemeManager::loadProjectTheme(const QDomElement& elem, const QString& projectPath)
{
	if (!applyProjectThemes())
	{
		// User chose to ignore project-specific themes
		resetToGlobalDefault();
		return;
	}

	ThemePreset p;
	p.name = elem.attribute("preset", "Custom Project Theme");
	p.baseColor = elem.attribute("base", "#18191c");
	p.windowColor = elem.attribute("window", "#111214");
	p.accentColor = elem.attribute("accent", "#e5a93c");
	p.accentHover = elem.attribute("accentHover", "#f5ba55");
	p.textActive = elem.attribute("textActive", "#1a1a1a");
	p.textColor = elem.attribute("textColor", "#d6d9e0");
	p.gridColor = elem.attribute("grid", "#26272b");
	p.beatLineColor = elem.attribute("beatLine", "#7d5b1e");
	p.barLineColor = elem.attribute("barLine", "#e5a93c");
	p.noteColor = elem.attribute("note", "#e5a93c");
	p.selectedColor = elem.attribute("selected", "#996f24");

	QString bg = elem.attribute("bg", "");
	if (!bg.isEmpty())
	{
		if (QFileInfo(bg).isRelative() && !projectPath.isEmpty())
		{
			QDir dir(projectPath);
			QString absPath = dir.absoluteFilePath(bg);
			if (QFile::exists(absPath))
			{
				p.backgroundPic = absPath;
			}
			else
			{
				p.backgroundPic = "";
			}
		}
		else if (QFile::exists(bg))
		{
			p.backgroundPic = bg;
		}
		else
		{
			p.backgroundPic = "";
		}
	}
	else
	{
		p.backgroundPic = "";
	}

	p.isProjectTheme = true;
	applyTheme(p, false);
}

bool ThemeManager::savePresetToFile(const QString& filePath, const ThemePreset& p)
{
	QJsonObject root;
	root["name"] = p.name;
	root["baseColor"] = p.baseColor;
	root["windowColor"] = p.windowColor;
	root["accentColor"] = p.accentColor;
	root["accentHover"] = p.accentHover;
	root["textActive"] = p.textActive;
	root["textColor"] = p.textColor;
	root["gridColor"] = p.gridColor;
	root["beatLineColor"] = p.beatLineColor;
	root["barLineColor"] = p.barLineColor;
	root["noteColor"] = p.noteColor;
	root["selectedColor"] = p.selectedColor;
	root["backgroundPic"] = p.backgroundPic;

	QJsonDocument doc(root);
	QFile f(filePath);
	QFileInfo fi(filePath);
	QDir().mkpath(fi.absolutePath());

	if (!f.open(QIODevice::WriteOnly))
	{
		return false;
	}
	f.write(doc.toJson());
	return true;
}

bool ThemeManager::loadPresetFromFile(const QString& filePath, ThemePreset& p)
{
	QFile f(filePath);
	if (!f.open(QIODevice::ReadOnly))
	{
		return false;
	}
	QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
	if (doc.isNull() || !doc.isObject())
	{
		return false;
	}
	QJsonObject root = doc.object();
	p.name = root["name"].toString();
	p.baseColor = root["baseColor"].toString();
	p.windowColor = root["windowColor"].toString();
	p.accentColor = root["accentColor"].toString();
	p.accentHover = root["accentHover"].toString();
	p.textActive = root["textActive"].toString();
	p.textColor = root["textColor"].toString();
	p.gridColor = root["gridColor"].toString();
	p.beatLineColor = root["beatLineColor"].toString();
	p.barLineColor = root["barLineColor"].toString();
	p.noteColor = root["noteColor"].toString();
	p.selectedColor = root["selectedColor"].toString();
	p.backgroundPic = root["backgroundPic"].toString();
	return true;
}

QString ThemeManager::generateThemeQss(const ThemePreset& p) const
{
	QString qss;
	QTextStream ts(&qss);

	ts << "\n/* --- DYNAMIC THEME: " << p.name << " --- */\n";

	// QMdiArea (Workspace background)
	ts << "QMdiArea {\n"
	   << "    background-color: " << p.windowColor << ";\n"
	   << "}\n";

	// Main Toolbar
	ts << "QWidget#mainToolbar {\n"
	   << "    background-color: " << p.baseColor << ";\n"
	   << "}\n";

	// Checked buttons and active toggle states
	ts << "QToolButton:checked, QPushButton:checked {\n"
	   << "    background-color: " << p.accentColor << ";\n"
	   << "    color: " << p.textActive << ";\n"
	   << "    border: 1px solid " << p.accentHover << ";\n"
	   << "}\n";

	// Hover highlight
	ts << "QToolButton:hover, QPushButton:hover {\n"
	   << "    border: 1px solid " << p.accentColor << ";\n"
	   << "}\n";

	// Selected tabs
	ts << "QTabBar::tab:selected {\n"
	   << "    background-color: " << p.baseColor << ";\n"
	   << "    border-top: 2px solid " << p.accentColor << ";\n"
	   << "    color: " << p.textActive << ";\n"
	   << "}\n";

	// Knobs: Active arc and pointer indicator
	ts << "lmms--gui--Knob {\n"
	   << "    qproperty-lineActiveColor: " << p.accentColor << ";\n"
	   << "    qproperty-arcActiveColor: " << p.accentColor << ";\n"
	   << "}\n";

	// PianoRoll styling
	ts << "lmms--gui--PianoRoll {\n"
	   << "    background-color: " << p.windowColor << ";\n"
	   << "    qproperty-noteColor: " << p.noteColor << ";\n"
	   << "    qproperty-noteModeColor: " << p.noteColor << ";\n"
	   << "    qproperty-selectedNoteColor: " << p.selectedColor << ";\n"
	   << "    qproperty-barLineColor: " << p.barLineColor << ";\n"
	   << "    qproperty-beatLineColor: " << p.beatLineColor << ";\n"
	   << "    qproperty-lineColor: " << p.gridColor << ";\n"
	   << "}\n";

	// AutomationEditor styling
	ts << "lmms--gui--AutomationEditor {\n"
	   << "    background-color: " << p.windowColor << ";\n"
	   << "    qproperty-nodeInValueColor: " << p.accentColor << ";\n"
	   << "    qproperty-barLineColor: " << p.barLineColor << ";\n"
	   << "    qproperty-beatLineColor: " << p.beatLineColor << ";\n"
	   << "    qproperty-lineColor: " << p.gridColor << ";\n"
	   << "}\n";

	// Menu and List selections
	ts << "QMenu::item:selected {\n"
	   << "    background-color: " << p.accentColor << ";\n"
	   << "    color: " << p.textActive << ";\n"
	   << "}\n";
	ts << "QTreeWidget::item:selected, QTreeView::item:selected, QListWidget::item:selected {\n"
	   << "    background-color: " << p.accentColor << ";\n"
	   << "    color: " << p.textActive << ";\n"
	   << "}\n";

	// Progress bars
	ts << "QProgressBar::chunk {\n"
	   << "    background-color: " << p.accentColor << ";\n"
	   << "}\n";

	return qss;
}

} // namespace lmms::gui
