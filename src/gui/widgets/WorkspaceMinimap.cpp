/*
 * WorkspaceMinimap.cpp - Radar and minimap navigation overview for LMMS workspace
 *
 * Copyright (c) 2026 LMMS Orion Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#include "WorkspaceMinimap.h"
#include "GuiApplication.h"
#include "MainWindow.h"
#include "ThemeManager.h"

#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

namespace lmms::gui
{

WorkspaceMinimap::WorkspaceMinimap(QMdiArea* workspace, QWidget* parent) :
	QWidget(parent),
	m_workspace(workspace),
	m_isPanning(false),
	m_refreshTimer(this)
{
	setWindowTitle(tr("Workspace Radar & Minimap"));
	setAttribute(Qt::WA_DeleteOnClose, false);
	setMinimumSize(220, 160);
	resize(260, 190);

	connect(&m_refreshTimer, &QTimer::timeout, this, [this]() {
		if (isVisible())
		{
			update();
		}
	});
	m_refreshTimer.start(100);
}

QRect WorkspaceMinimap::computeContentBounds() const
{
	if (!m_workspace)
	{
		return QRect(0, 0, 1920, 1080);
	}

	int minX = 0;
	int minY = 0;
	int maxX = std::max(1280, m_workspace->width());
	int maxY = std::max(720, m_workspace->height());

	for (auto* sw : m_workspace->subWindowList())
	{
		if (sw && sw->isVisible() && !sw->isMinimized())
		{
			minX = std::min(minX, sw->x() - 100);
			minY = std::min(minY, sw->y() - 100);
			maxX = std::max(maxX, sw->x() + sw->width() + 100);
			maxY = std::max(maxY, sw->y() + sw->height() + 100);
		}
	}

	return QRect(minX, minY, maxX - minX, maxY - minY);
}

void WorkspaceMinimap::panToPoint(const QPoint& minimapPos)
{
	if (!m_workspace) { return; }

	QRect bounds = computeContentBounds();
	QRect mapRect = rect().adjusted(10, 25, -10, -10);
	if (mapRect.width() <= 0 || mapRect.height() <= 0 || bounds.width() <= 0 || bounds.height() <= 0)
	{
		return;
	}

	float scaleX = static_cast<float>(bounds.width()) / mapRect.width();
	float scaleY = static_cast<float>(bounds.height()) / mapRect.height();

	int targetWorldX = bounds.x() + static_cast<int>((minimapPos.x() - mapRect.x()) * scaleX);
	int targetWorldY = bounds.y() + static_cast<int>((minimapPos.y() - mapRect.y()) * scaleY);

	int curCenterX = m_workspace->width() / 2;
	int curCenterY = m_workspace->height() / 2;

	int deltaX = curCenterX - targetWorldX;
	int deltaY = curCenterY - targetWorldY;

	for (auto* sw : m_workspace->subWindowList())
	{
		if (sw && !sw->isMaximized())
		{
			sw->move(sw->x() + deltaX, sw->y() + deltaY);
		}
	}

	update();
}

void WorkspaceMinimap::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_isPanning = true;
		panToPoint(event->pos());
		event->accept();
		return;
	}
	QWidget::mousePressEvent(event);
}

void WorkspaceMinimap::mouseMoveEvent(QMouseEvent* event)
{
	if (m_isPanning)
	{
		panToPoint(event->pos());
		event->accept();
		return;
	}
	QWidget::mouseMoveEvent(event);
}

void WorkspaceMinimap::mouseReleaseEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		m_isPanning = false;
		event->accept();
		return;
	}
	QWidget::mouseReleaseEvent(event);
}

void WorkspaceMinimap::paintEvent(QPaintEvent*)
{
	QPainter p(this);
	p.setRenderHint(QPainter::Antialiasing, true);

	QColor baseCol(24, 26, 32, 230);
	const auto& theme = ThemeManager::inst()->currentTheme();
	QColor borderCol(theme.borderColor.isEmpty() ? "#444955" : theme.borderColor);
	QColor accentCol(theme.accentColor.isEmpty() ? "#e5a93c" : theme.accentColor);

	// Outer card
	p.setBrush(baseCol);
	p.setPen(QPen(borderCol, 1));
	p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

	// Header
	p.setPen(accentCol);
	p.setFont(QFont("Segoe UI", 8, QFont::Bold));
	p.drawText(12, 18, tr("RADAR OVERVIEW"));

	if (!m_workspace) { return; }

	QRect bounds = computeContentBounds();
	QRect mapRect = rect().adjusted(10, 26, -10, -10);

	if (mapRect.width() <= 10 || mapRect.height() <= 10 || bounds.width() <= 0 || bounds.height() <= 0)
	{
		return;
	}

	// Map inner background
	p.setBrush(QColor(14, 15, 18, 240));
	p.setPen(QPen(QColor(40, 44, 54), 1));
	p.drawRect(mapRect);

	float scaleX = static_cast<float>(mapRect.width()) / bounds.width();
	float scaleY = static_cast<float>(mapRect.height()) / bounds.height();

	// Draw all subwindows
	for (auto* sw : m_workspace->subWindowList())
	{
		if (sw && sw->isVisible() && !sw->isMinimized())
		{
			int rx = mapRect.x() + static_cast<int>((sw->x() - bounds.x()) * scaleX);
			int ry = mapRect.y() + static_cast<int>((sw->y() - bounds.y()) * scaleY);
			int rw = std::max(4, static_cast<int>(sw->width() * scaleX));
			int rh = std::max(4, static_cast<int>(sw->height() * scaleY));

			QRect r(rx, ry, rw, rh);
			p.setBrush(QColor(60, 68, 85, 200));
			p.setPen(QPen(QColor(100, 115, 140), 1));
			p.drawRect(r);

			// Title label if large enough
			if (rw > 35 && rh > 12)
			{
				p.setPen(QColor(200, 210, 225));
				p.setFont(QFont("Segoe UI", 6));
				p.drawText(r.adjusted(2, 1, -2, -1), Qt::AlignLeft | Qt::AlignTop, sw->windowTitle());
			}
		}
	}

	// Draw current visible viewport
	int vx = mapRect.x() + static_cast<int>((0 - bounds.x()) * scaleX);
	int vy = mapRect.y() + static_cast<int>((0 - bounds.y()) * scaleY);
	int vw = std::max(6, static_cast<int>(m_workspace->width() * scaleX));
	int vh = std::max(6, static_cast<int>(m_workspace->height() * scaleY));

	QRect vrect(vx, vy, vw, vh);
	p.setBrush(QColor(accentCol.red(), accentCol.green(), accentCol.blue(), 35));
	p.setPen(QPen(accentCol, 2));
	p.drawRect(vrect);
}

} // namespace lmms::gui
