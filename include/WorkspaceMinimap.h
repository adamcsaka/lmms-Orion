/*
 * WorkspaceMinimap.h - Radar and minimap navigation overview for LMMS workspace
 *
 * Copyright (c) 2026 LMMS Orion Team
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 */

#pragma once

#include <QWidget>
#include <QTimer>

class QMdiArea;

namespace lmms::gui
{

class WorkspaceMinimap : public QWidget
{
	Q_OBJECT
public:
	explicit WorkspaceMinimap(QMdiArea* workspace, QWidget* parent = nullptr);
	~WorkspaceMinimap() override = default;

	void zoomIn();
	void zoomOut();
	void resetZoom();
	void fitAll();

protected:
	void paintEvent(QPaintEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;

private:
	void panToPoint(const QPoint& minimapPos);
	QRect computeContentBounds() const;

	QMdiArea* m_workspace;
	bool m_isPanning;
	QTimer m_refreshTimer;
};

} // namespace lmms::gui
