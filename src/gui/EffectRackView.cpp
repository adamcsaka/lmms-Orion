/*
 * EffectRackView.cpp - view for effectChain model
 *
 * Copyright (c) 2006-2007 Danny McRae <khjklujn@netscape.net>
 * Copyright (c) 2008-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "EffectRackView.h"

#include <QApplication>
#include <QAction>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QClipboard>
#include <QMimeData>
#include <QDomDocument>
#include <QKeyEvent>
#include <QFile>
#include <QInputDialog>
#include <QTextStream>

#include "CaptionMenu.h"
#include "ConfigManager.h"
#include "FileDialog.h"
#include "embed.h"
#include "DeprecationHelper.h"
#include "EffectSelectDialog.h"
#include "EffectView.h"
#include "GroupBox.h"
#include "Song.h"
#include "ProjectJournal.h"
#include "TextFloat.h"


namespace lmms::gui
{

EffectRackView::EffectRackView( EffectChain* model, QWidget* parent ) :
	QWidget( parent ),
	ModelView( nullptr, this )
{
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(5, 5, 5, 5);

	m_effectsGroupBox = new GroupBox( tr( "EFFECTS CHAIN" ) );
	mainLayout->addWidget( m_effectsGroupBox );

	auto effectsLayout = new QVBoxLayout(m_effectsGroupBox);
	effectsLayout->setSpacing( 0 );
	effectsLayout->setContentsMargins( 2, m_effectsGroupBox->titleBarHeight() + 2, 2, 2 );

	m_scrollArea = new QScrollArea;
	m_scrollArea->setVerticalScrollBarPolicy( Qt::ScrollBarAlwaysOn );
	m_scrollArea->setHorizontalScrollBarPolicy( Qt::ScrollBarAlwaysOff );
	m_scrollArea->setPalette( QApplication::palette( m_scrollArea ) );
	m_scrollArea->setFrameStyle( QFrame::NoFrame );
	m_scrollArea->setWidget( new QWidget );

	effectsLayout->addWidget( m_scrollArea );

	auto btnLayout = new QHBoxLayout;
	btnLayout->setSpacing( 2 );
	btnLayout->setContentsMargins( 0, 0, 0, 0 );

	auto addButton = new QPushButton;
	addButton->setText( tr( "Add effect" ) );
	addButton->setFocusPolicy(Qt::NoFocus);
	connect( addButton, SIGNAL(clicked()), this, SLOT(addEffect()));
	btnLayout->addWidget( addButton );

	auto saveButton = new QPushButton;
	saveButton->setText( tr( "Save FX" ) );
	saveButton->setToolTip( tr( "Save effect group/chain to file" ) );
	saveButton->setFocusPolicy(Qt::NoFocus);
	connect( saveButton, SIGNAL(clicked()), this, SLOT(saveEffectChain()));
	btnLayout->addWidget( saveButton );

	auto loadButton = new QPushButton;
	loadButton->setText( tr( "Load FX" ) );
	loadButton->setToolTip( tr( "Load effect group/chain from file" ) );
	loadButton->setFocusPolicy(Qt::NoFocus);
	connect( loadButton, SIGNAL(clicked()), this, SLOT(loadEffectChain()));
	btnLayout->addWidget( loadButton );

	auto randomizeButton = new QPushButton;
	randomizeButton->setText( tr( "Random FX" ) );
	randomizeButton->setToolTip( tr( "Generate and randomize effects in this rack (specify count)" ) );
	randomizeButton->setFocusPolicy(Qt::NoFocus);
	connect( randomizeButton, SIGNAL(clicked()), this, SLOT(randomizeEffects()));
	btnLayout->addWidget( randomizeButton );

	effectsLayout->addLayout( btnLayout );

	m_lastY = 0;

	setModel( model );
}



EffectRackView::~EffectRackView()
{
	clearViews();
}





void EffectRackView::clearViews()
{
	while( m_effectViews.size() )
	{
		EffectView * e = m_effectViews[m_effectViews.size() - 1];
		m_effectViews.pop_back();
		delete e;
	}
}




void EffectRackView::moveUp( EffectView* view )
{
	if (fxChain()) { fxChain()->addJournalCheckPoint(); }
	fxChain()->moveUp( view->effect() );
	if( view != m_effectViews.first() )
	{
		int i = 0;
		for( QVector<EffectView *>::Iterator it = m_effectViews.begin(); 
					it != m_effectViews.end(); it++, i++ )
		{
			if( *it == view )
			{
				break;
			}
		}

		EffectView * temp = m_effectViews[ i - 1 ];

		m_effectViews[i - 1] = view;
		m_effectViews[i] = temp;

		update();

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}
	}
}




void EffectRackView::moveDown( EffectView* view )
{
	if( view != m_effectViews.last() )
	{
		// moving next effect up is the same
		moveUp( *( std::find( m_effectViews.begin(), m_effectViews.end(), view ) + 1 ) );
	}
}




void EffectRackView::deletePlugin( EffectView* view )
{
	if (fxChain()) { fxChain()->addJournalCheckPoint(); }
	Effect * e = view->effect();
	m_effectViews.erase( std::find( m_effectViews.begin(), m_effectViews.end(), view ) );
	delete view;
	fxChain()->removeEffect( e );
	e->deleteLater();
	update();

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}
}




void EffectRackView::update()
{
	QWidget * w = m_scrollArea->widget();
	QVector<bool> view_map( qMax<int>( fxChain()->m_effects.size(),
						m_effectViews.size() ), false );

	for (const auto& effect : fxChain()->m_effects)
	{
		int i = 0;
		for (const auto& effectView : m_effectViews)
		{
			if (effectView->model() == effect)
			{
				view_map[i] = true;
				break;
			}
			++i;
		}
		if( i >= m_effectViews.size() )
		{
			auto view = new EffectView(effect, w);
			connect(view, &EffectView::movedUp, this, &EffectRackView::moveUp);
			connect(view, &EffectView::movedDown, this, &EffectRackView::moveDown);
			connect(view, &EffectView::deletedPlugin, this, &EffectRackView::deletePlugin, Qt::QueuedConnection);
			connect(view, &EffectView::pasteEffectRequested, this, &EffectRackView::pasteEffect);

			QAction* moveUpAction = new QAction(view);
			moveUpAction->setShortcut(keySequence(Qt::Key_Up, Qt::AltModifier));
			moveUpAction->setShortcutContext(Qt::WidgetShortcut);
			connect(moveUpAction, &QAction::triggered, view, &EffectView::moveUp);
			view->addAction(moveUpAction);

			QAction* moveDownAction = new QAction(view);
			moveDownAction->setShortcut(keySequence(Qt::Key_Down, Qt::AltModifier));
			moveDownAction->setShortcutContext(Qt::WidgetShortcut);
			connect(moveDownAction, &QAction::triggered, view, &EffectView::moveDown);
			view->addAction(moveDownAction);

			view->show();
			m_effectViews.append( view );
			if( i < view_map.size() )
			{
				view_map[i] = true;
			}
			else
			{
				view_map.append( true );
			}

		}
	}

	int i = 0, nView = 0;

	const int EffectViewMargin = 3;
	m_lastY = EffectViewMargin;

	for( QVector<EffectView *>::Iterator it = m_effectViews.begin(); 
					it != m_effectViews.end(); i++ )
	{
		if( i < view_map.size() && view_map[i] == false )
		{
			delete m_effectViews[nView];
			it = m_effectViews.erase( it );
		}
		else
		{
			( *it )->move( EffectViewMargin, m_lastY );
			m_lastY += ( *it )->height();
			++nView;
			++it;
		}
	}

	w->setFixedSize(EffectView::DEFAULT_WIDTH + 2 * EffectViewMargin, m_lastY);

	QWidget::update();
}




void EffectRackView::addEffect()
{
	EffectSelectDialog esd( this );
	esd.exec();

	if( esd.result() == QDialog::Rejected )
	{
		return;
	}

	Effect * fx = esd.instantiateSelectedPlugin( fxChain() );

	fxChain()->addJournalCheckPoint();
	fxChain()->appendEffect( fx );
	update();

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}

	// Find the effectView, and show the controls
	for (const auto& effectView : m_effectViews)
	{
		if (effectView->effect() == fx)
		{
			effectView->editControls();
			break;
		}
	}


}




void EffectRackView::modelChanged()
{
	//clearViews();
	m_effectsGroupBox->setModel( &fxChain()->m_enabledModel );
	connect( fxChain(), SIGNAL(aboutToClear()), this, SLOT(clearViews()));
	update();

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}
}




QSize EffectRackView::sizeHint() const
{
	// Use the formula from InstrumentTrackWindow.cpp
	return QSize{EffectRackView::DEFAULT_WIDTH, 254 /* INSTRUMENT_HEIGHT */ - 4 - 1};
}

void EffectRackView::pasteEffect()
{
	if (!fxChain()) { return; }

	const auto clipboard = QApplication::clipboard();
	const auto mimeData = clipboard ? clipboard->mimeData() : nullptr;
	if (!mimeData || (!mimeData->hasFormat( "application/x-lmms-effect" ) && !mimeData->hasText()))
	{
		return;
	}

	QByteArray data = mimeData->hasFormat( "application/x-lmms-effect" )
		? mimeData->data( "application/x-lmms-effect" )
		: mimeData->text().toUtf8();

	QDomDocument doc;
	if (doc.setContent( data ))
	{
		QDomElement root = doc.documentElement();
		QDomElement effectData = root.firstChildElement( "effect" );
		if (!effectData.isNull())
		{
			const QString name = effectData.attribute( "name" );
			EffectKey key( effectData.elementsByTagName( "key" ).item( 0 ).toElement() );
			Effect* e = Effect::instantiate( name, fxChain(), &key );
			if (e != nullptr && e->isOkay())
			{
				fxChain()->addJournalCheckPoint();
				e->restoreState( effectData );
				fxChain()->appendEffect( e );
			}
		}
	}
}

void EffectRackView::keyPressEvent( QKeyEvent* event )
{
	if (event->matches( QKeySequence::Paste ))
	{
		pasteEffect();
		event->accept();
		return;
	}
	QWidget::keyPressEvent( event );
}

void EffectRackView::saveEffectChain()
{
	if (!fxChain()) { return; }
	QString fileName = FileDialog::getSaveFileName(
		this,
		tr("Save Effect Group"),
		ConfigManager::inst()->workingDir(),
		tr("LMMS Effect Chain (*.lmms-fxgroup *.xml);;All files (*.*)")
	);
	if (fileName.isEmpty()) { return; }
	if (!fileName.endsWith(".lmms-fxgroup", Qt::CaseInsensitive) && !fileName.endsWith(".xml", Qt::CaseInsensitive))
	{
		fileName += ".lmms-fxgroup";
	}

	QDomDocument doc("lmms-fxchain");
	QDomElement root = doc.createElement("lmms-effect-chain");
	root.setAttribute("version", "1.0");
	doc.appendChild(root);

	QDomElement fxElement = doc.createElement("effectchain");
	fxChain()->saveSettings(doc, fxElement);
	root.appendChild(fxElement);

	QFile file(fileName);
	if (file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
	{
		QTextStream stream(&file);
		stream << doc.toString();
		file.close();
	}
}

void EffectRackView::loadEffectChain()
{
	if (!fxChain()) { return; }
	QString fileName = FileDialog::getOpenFileName(
		this,
		tr("Load Effect Group"),
		ConfigManager::inst()->workingDir(),
		tr("LMMS Effect Chain (*.lmms-fxgroup *.xml);;All files (*.*)")
	);
	if (fileName.isEmpty()) { return; }

	QFile file(fileName);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) { return; }

	QDomDocument doc;
	if (!doc.setContent(&file))
	{
		file.close();
		return;
	}
	file.close();

	QDomElement root = doc.documentElement();
	QDomElement fxElement;
	if (root.nodeName() == "effectchain")
	{
		fxElement = root;
	}
	else
	{
		fxElement = root.firstChildElement("effectchain");
	}

	if (!fxElement.isNull())
	{
		fxChain()->addJournalCheckPoint();
		fxChain()->clear();
		fxChain()->loadSettings(fxElement);
		update();

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}
	}
}

void EffectRackView::contextMenuEvent( QContextMenuEvent* event )
{
	QPointer<CaptionMenu> contextMenu = new CaptionMenu( tr( "Effects Chain" ), this );
	contextMenu->addAction( embed::getIconPixmap( "add" ), tr( "&Add effect" ), this, SLOT(addEffect()) );
	contextMenu->addAction( embed::getIconPixmap( "edit_paste" ), tr( "&Paste effect" ), this, SLOT(pasteEffect()), QKeySequence::Paste );
	contextMenu->addSeparator();
	contextMenu->addAction( embed::getIconPixmap( "file_save" ), tr( "&Save effect group..." ), this, SLOT(saveEffectChain()) );
	contextMenu->addAction( embed::getIconPixmap( "file_open" ), tr( "&Load effect group..." ), this, SLOT(loadEffectChain()) );
	contextMenu->addSeparator();
	contextMenu->addAction( embed::getIconPixmap( "arp_random" ), tr( "&Randomize effect rack..." ), this, SLOT(randomizeEffects()) );
	contextMenu->addSeparator();
	contextMenu->addAction( embed::getIconPixmap( "cancel" ), tr( "&Clear all effects" ), this, [this]() {
		if (fxChain()) { fxChain()->addJournalCheckPoint(); fxChain()->clear(); update(); }
	} );
	contextMenu->exec( QCursor::pos() );
	delete contextMenu;
}

void EffectRackView::randomizeEffects()
{
	if (!fxChain()) { return; }

	int initialCount = ConfigManager::inst()->value("ui", "random_rack_effect_count", "3").toInt();
	if (initialCount < 1 || initialCount > 10) { initialCount = 3; }
	bool ok = false;
	int count = QInputDialog::getInt(
		this,
		tr("Randomize Effect Rack"),
		tr("Number of random effects to generate:"),
		initialCount, 1, 10, 1, &ok);
	if (!ok || count <= 0) { return; }
	ConfigManager::inst()->setValue("ui", "random_rack_effect_count", QString::number(count));

	const char* fxPlugins[] = {
		"amplifier", "bassbooster", "bitcrush", "crossovereq", "delay",
		"dispersion", "dualfilter", "dynamicsprocessor", "eq", "flanger",
		"frequencyshifter", "multitapecho", "reverbsc", "slewdistortion",
		"stereomatrix", "waveshaper"
	};
	const int numFx = sizeof(fxPlugins) / sizeof(fxPlugins[0]);

	fxChain()->addJournalCheckPoint();

	// Each effect parameter setValue() would push its own journal checkpoint;
	// suppress them so the whole rack randomization is a single undo step.
	auto* journal = Engine::projectJournal();
	const bool prevJournalling = journal ? journal->isJournalling() : true;
	if (journal) { journal->setJournalling(false); }

	fxChain()->clear();

	for (int i = 0; i < count; ++i)
	{
		const char* chosenFx = fxPlugins[rand() % numFx];
		Effect* e = Effect::instantiate(QString::fromUtf8(chosenFx), fxChain(), nullptr);
		if (e && e->isOkay())
		{
			fxChain()->appendEffect(e);
			const auto fxModels = e->findChildren<AutomatableModel*>();
			for (auto* fm_obj : fxModels)
			{
				if (auto fm = fm_obj->dynamicCast<FloatModel>())
				{
					float minVal = fm->minValue();
					float maxVal = fm->maxValue();
					if (std::isfinite(minVal) && std::isfinite(maxVal) && maxVal > minVal)
					{
						float r = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
						fm->setValue(minVal + r * (maxVal - minVal));
					}
				}
				else if (auto im = fm_obj->dynamicCast<IntModel>())
				{
					int minVal = im->minValue();
					int maxVal = im->maxValue();
					if (maxVal > minVal && (maxVal - minVal) <= 32)
					{
						im->setValue(minVal + (rand() % (maxVal - minVal + 1)));
					}
				}
			}
		}
	}

	update();

	if (journal) { journal->setJournalling(prevJournalling); }

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}

	TextFloat::displayMessage(
		tr("Effect Rack Randomized"),
		tr("Generated and randomized %1 effect(s)!").arg(count),
		embed::getIconPixmap("arp_random"),
		2500
	);
}


Effect* EffectRackView::addEffectByName(const QString& pluginName)
{
	if (!fxChain()) { return nullptr; }

	Effect* fx = Effect::instantiate(pluginName, fxChain(), nullptr);
	if (fx == nullptr || !fx->isOkay())
	{
		delete fx;
		return nullptr;
	}

	fxChain()->addJournalCheckPoint();
	fxChain()->appendEffect(fx);
	update();

	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}

	for (const auto& effectView : m_effectViews)
	{
		if (effectView->effect() == fx)
		{
			effectView->editControls();
			break;
		}
	}

	return fx;
}


void EffectRackView::editEffectControls(Effect* effect)
{
	if (effect == nullptr) { return; }

	for (const auto& effectView : m_effectViews)
	{
		if (effectView->effect() == effect)
		{
			effectView->editControls();
			return;
		}
	}
}


Effect* EffectRackView::findEffectByPluginName(const QString& pluginName) const
{
	if (!fxChain()) { return nullptr; }

	for (Effect* e : fxChain()->effects())
	{
		if (e->descriptor() != nullptr && pluginName == QString::fromUtf8(e->descriptor()->name))
		{
			return e;
		}
	}

	return nullptr;
}




} // namespace lmms::gui
