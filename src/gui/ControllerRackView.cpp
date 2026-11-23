/*
 * ControllerRackView.cpp - view for song's controllers
 *
 * Copyright (c) 2008-2009 Paul Giblock <drfaygo/at/gmail.com>
 * Copyright (c) 2010-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */

#include "ControllerRackView.h"

#include <QApplication>
#include <QAction>
#include <QPushButton>
#include <QScrollArea>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDesktopServices>
#include <QUrl>

#include "ConfigManager.h"
#include "ControllerView.h"
#include "DeprecationHelper.h"
#include "embed.h"
#include "FileDialog.h"
#include "GuiApplication.h"
#include "LfoController.h"
#include "MainWindow.h"
#include "Song.h"
#include "SubWindow.h"

namespace lmms::gui
{


ControllerRackView::ControllerRackView()
	: QWidget{}
	, m_nextIndex{0}
{
	setWindowIcon( embed::getIconPixmap( "controller" ) );
	setWindowTitle( tr( "Controller Rack" ) );

	m_scrollArea = new QScrollArea( this );
	m_scrollArea->setPalette( QApplication::palette( m_scrollArea ) );
	m_scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );

	auto scrollAreaWidget = new QWidget(m_scrollArea);
	m_scrollAreaLayout = new QVBoxLayout( scrollAreaWidget );
	m_scrollAreaLayout->addStretch();
	scrollAreaWidget->setLayout( m_scrollAreaLayout );

	m_scrollArea->setWidget( scrollAreaWidget );
	m_scrollArea->setWidgetResizable( true );

	m_addButton = new QPushButton( tr( "+ Add" ), this );
	m_savePresetButton = new QPushButton( tr( "Save Preset" ), this );
	m_loadPresetButton = new QPushButton( tr( "Load Preset" ), this );
	m_presetsMenuButton = new QPushButton( tr( "Presets..." ), this );

	auto btnLayout = new QHBoxLayout();
	btnLayout->addWidget( m_addButton );
	btnLayout->addWidget( m_savePresetButton );
	btnLayout->addWidget( m_loadPresetButton );
	btnLayout->addWidget( m_presetsMenuButton );

	connect( m_addButton, &QPushButton::clicked, this, qOverload<>(&ControllerRackView::addController) );
	connect( m_savePresetButton, &QPushButton::clicked, this, &ControllerRackView::savePresets );
	connect( m_loadPresetButton, &QPushButton::clicked, this, &ControllerRackView::loadPresets );
	connect( m_presetsMenuButton, &QPushButton::clicked, this, &ControllerRackView::showPresetsMenu );

	Song * song = Engine::getSong();
	connect(song, &Song::controllerAdded, this, qOverload<Controller*>(&ControllerRackView::addController));
	connect(song, &Song::controllerRemoved, this, &ControllerRackView::removeController);

	auto layout = new QVBoxLayout();
	layout->addWidget( m_scrollArea );
	layout->addLayout( btnLayout );
	this->setLayout( layout );

	SubWindow* subWin = getGUI()->mainWindow()->addWindowedWidget(this);

	setFixedWidth(380);
	setMinimumHeight(200);

	// No maximize button
	Qt::WindowFlags flags = subWin->windowFlags();
	flags &= ~Qt::WindowMaximizeButtonHint;
	subWin->setWindowFlags( flags );
	
	subWin->setAttribute( Qt::WA_DeleteOnClose, false );
	subWin->move( 680, 310 );
}




void ControllerRackView::saveSettings( QDomDocument & _doc,
							QDomElement & _this )
{
	MainWindow::saveWidgetState( this, _this );
}




void ControllerRackView::loadSettings( const QDomElement & _this )
{
	MainWindow::restoreWidgetState( this, _this );
}




void ControllerRackView::deleteController( ControllerView * _view )
{
	Controller * c = _view->getController();

	if( c->connectionCount() > 0 )
	{
		QMessageBox msgBox;
		msgBox.setIcon( QMessageBox::Question );
		msgBox.setWindowTitle( tr("Confirm Delete") );
		msgBox.setText( tr("Confirm delete? There are existing connection(s) "
				"associated with this controller. There is no way to undo.") );
		msgBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
		if( msgBox.exec() != QMessageBox::Ok )
		{
			return;
		}
	}

	Song * song = Engine::getSong();
	song->removeController( c );
}

void ControllerRackView::moveUp(ControllerView* view)
{
	if (view == m_controllerViews.first()) { return; }

	const auto storedView = std::find(m_controllerViews.begin(), m_controllerViews.end(), view);
	assert(storedView != m_controllerViews.end());

	const auto index = std::distance(m_controllerViews.begin(), storedView);

	std::swap(m_controllerViews[index - 1], m_controllerViews[index]);
	m_scrollAreaLayout->removeWidget(view);
	m_scrollAreaLayout->insertWidget(index - 1, view);
}

void ControllerRackView::moveDown(ControllerView* view)
{
	if (view == m_controllerViews.last()) { return; }

	const auto storedView = std::find(m_controllerViews.begin(), m_controllerViews.end(), view);
	assert(storedView != m_controllerViews.end());
	moveUp(*std::next(storedView));
}

void ControllerRackView::addController(Controller* controller)
{
	QWidget * scrollAreaWidget = m_scrollArea->widget();

	auto controllerView = new ControllerView(controller, scrollAreaWidget);

	connect(controllerView, &ControllerView::movedUp, this, &ControllerRackView::moveUp);
	connect(controllerView, &ControllerView::movedDown, this, &ControllerRackView::moveDown);
	connect(controllerView, &ControllerView::removedController, this, &ControllerRackView::deleteController, Qt::QueuedConnection);

	auto moveUpAction = new QAction(controllerView);
	moveUpAction->setShortcut(keySequence(Qt::Key_Up, Qt::AltModifier));
	moveUpAction->setShortcutContext(Qt::WidgetShortcut);
	connect(moveUpAction, &QAction::triggered, controllerView, &ControllerView::moveUp);
	controllerView->addAction(moveUpAction);

	auto moveDownAction = new QAction(controllerView);
	moveDownAction->setShortcut(keySequence(Qt::Key_Down, Qt::AltModifier));
	moveDownAction->setShortcutContext(Qt::WidgetShortcut);
	connect(moveDownAction, &QAction::triggered, controllerView, &ControllerView::moveDown);
	controllerView->addAction(moveDownAction);


	m_controllerViews.append( controllerView );
	m_scrollAreaLayout->insertWidget( m_nextIndex, controllerView );

	++m_nextIndex;
}




void ControllerRackView::removeController(Controller* removedController)
{
	ControllerView * viewOfRemovedController = 0;

	QVector<ControllerView *>::const_iterator end = m_controllerViews.end();
	for ( QVector<ControllerView *>::const_iterator it = m_controllerViews.begin(); it != end; ++it)
	{
		ControllerView *currentControllerView = *it;
		if ( currentControllerView->getController() == removedController )
		{
			viewOfRemovedController = currentControllerView;
			break;
		}
	}

	if (viewOfRemovedController )
	{
		m_controllerViews.erase( std::find( m_controllerViews.begin(),
					m_controllerViews.end(), viewOfRemovedController ) );

		delete viewOfRemovedController;
		--m_nextIndex;
	}
}




void ControllerRackView::addController()
{
	// TODO: Eventually let the user pick from available controller types

	Engine::getSong()->addController( new LfoController( Engine::getSong() ) );

	// fix bug which always made ControllerRackView loose focus when adding
	// new controller
	setFocus();
}

void ControllerRackView::savePresets()
{
	Song * song = Engine::getSong();
	if (song->controllers().empty())
	{
		QMessageBox::information(this, tr("Save Controller Preset"),
			tr("There are no controllers in the rack to save."));
		return;
	}

	const QString presetDir = QDir(ConfigManager::inst()->userPresetsDir()).filePath("LFO Presets");
	QDir().mkpath(presetDir);

	QString filter = tr("LMMS Controller Preset (*.xpf *.xml);;All files (*.*)");
	QString fileName = FileDialog::getSaveFileName(this, tr("Save Controller Preset"), presetDir, filter);
	if (fileName.isEmpty()) { return; }
	if (!fileName.endsWith(".xpf", Qt::CaseInsensitive) && !fileName.endsWith(".xml", Qt::CaseInsensitive))
	{
		fileName += ".xpf";
	}

	QDomDocument doc("lmms-controller-preset");
	QDomElement root = doc.createElement("lmms-controller-preset");
	root.setAttribute("version", "1.0");
	doc.appendChild(root);

	song->saveControllerStates(doc, root);

	QFile file(fileName);
	if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Save Error"), tr("Could not write to file %1").arg(fileName));
		return;
	}

	QTextStream stream(&file);
	doc.save(stream, 2);
	file.close();
}

void ControllerRackView::loadPresetFile(const QString & fileName)
{
	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
	{
		QMessageBox::warning(this, tr("Load Error"), tr("Could not open file %1").arg(fileName));
		return;
	}

	QDomDocument doc;
	QString errorMsg;
	int errorLine = 0, errorCol = 0;
	if (!doc.setContent(&file, &errorMsg, &errorLine, &errorCol))
	{
		file.close();
		QMessageBox::warning(this, tr("Load Error"),
			tr("Failed to parse XML preset: %1 (line %2)").arg(errorMsg).arg(errorLine));
		return;
	}
	file.close();

	QDomElement root = doc.documentElement();
	QDomElement controllersNode = root.firstChildElement("controllers");
	if (controllersNode.isNull())
	{
		if (root.nodeName() == "controllers")
		{
			controllersNode = root;
		}
	}

	if (controllersNode.isNull())
	{
		QMessageBox::warning(this, tr("Load Error"), tr("No controller data found in preset file."));
		return;
	}

	Song * song = Engine::getSong();

	if (!song->controllers().empty())
	{
		QMessageBox::StandardButton reply = QMessageBox::question(this, tr("Load Preset"),
			tr("Replace existing controllers in rack? Select 'No' to append them instead."),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

		if (reply == QMessageBox::Cancel) { return; }
		if (reply == QMessageBox::Yes)
		{
			song->removeAllControllers();
		}
	}

	song->restoreControllerStates(controllersNode);
	song->setModified();
}

void ControllerRackView::loadPresets()
{
	const QString presetDir = QDir(ConfigManager::inst()->userPresetsDir()).filePath("LFO Presets");
	QDir().mkpath(presetDir);

	QString filter = tr("LMMS Controller Preset (*.xpf *.xml);;All files (*.*)");
	QString fileName = FileDialog::getOpenFileName(this, tr("Load Controller Preset"), presetDir, filter);
	if (fileName.isEmpty()) { return; }

	loadPresetFile(fileName);
}

void ControllerRackView::showPresetsMenu()
{
	const QString presetDir = QDir(ConfigManager::inst()->userPresetsDir()).filePath("LFO Presets");
	QDir().mkpath(presetDir);

	QMenu menu(this);
	QDir dir(presetDir);
	QStringList filters;
	filters << "*.xpf" << "*.xml";
	QFileInfoList files = dir.entryInfoList(filters, QDir::Files, QDir::Name);

	if (files.isEmpty())
	{
		QAction * emptyAction = menu.addAction(tr("No presets found in 'LFO Presets'"));
		emptyAction->setEnabled(false);
	}
	else
	{
		for (const QFileInfo & fi : files)
		{
			QAction * act = menu.addAction(fi.completeBaseName());
			connect(act, &QAction::triggered, this, [this, path = fi.absoluteFilePath()] {
				loadPresetFile(path);
			});
		}
	}

	menu.addSeparator();
	QAction * openFolderAction = menu.addAction(tr("Open Presets Folder..."));
	connect(openFolderAction, &QAction::triggered, this, [presetDir] {
		QDesktopServices::openUrl(QUrl::fromLocalFile(presetDir));
	});

	menu.exec(m_presetsMenuButton->mapToGlobal(QPoint(0, m_presetsMenuButton->height())));
}

} // namespace lmms::gui
