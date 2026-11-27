/*
 * TrackOperationsWidget.cpp - implementation of TrackOperationsWidget class
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "TrackOperationsWidget.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMap>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSet>
#include <QVector>

#include "AutomatableButton.h"
#include "AutomationClip.h"
#include "AutomationTrackView.h"
#include "ColorChooser.h"
#include "ConfigManager.h"
#include "embed.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "InstrumentTrackView.h"
#include "lmms_math.h"
#include "KeyboardShortcuts.h"
#include "Song.h"
#include "StringPairDrag.h"
#include "TextFloat.h"
#include "Track.h"
#include "TrackContainer.h"
#include "TrackContainerView.h"
#include "TrackGrip.h"
#include "TrackView.h"

namespace lmms::gui
{

TrackOperationsWidget::TrackOperationsWidget(TrackView* parent)
	: QWidget(parent)
	, m_trackView(parent)
{
	setToolTip(tr("Press <%1> while clicking on move-grip "
				"to begin a new drag'n'drop action." ).arg(UI_CTRL_KEY) );

	auto toMenu = new QMenu(this);
	connect( toMenu, SIGNAL(aboutToShow()), this, SLOT(updateMenu()));


	setObjectName( "automationEnabled" );

	auto layout = new QHBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	layout->setSpacing(0);
	layout->setAlignment(Qt::AlignTop);

	m_trackGrip = new TrackGrip(m_trackView, this);
	layout->addWidget(m_trackGrip);

	// This widget holds the gear icon and the mute and solo
	// buttons in a layout.
	auto operationsWidget = new QWidget(this);
	auto operationsLayout = new QHBoxLayout(operationsWidget);
	operationsLayout->setContentsMargins(2, 6, 0, 6);
	operationsLayout->setSpacing(2);

	m_trackOps = new QPushButton(operationsWidget);
	m_trackOps->setFocusPolicy( Qt::NoFocus );
	m_trackOps->setMenu( toMenu );
	m_trackOps->setToolTip(tr("Actions"));
	m_trackOps->setCursor(Qt::PointingHandCursor);

	m_muteBtn = new AutomatableButton(operationsWidget, tr("Mute"));
	m_muteBtn->setCheckable(true);
	m_muteBtn->setToolTip(tr("Mute"));
	m_muteBtn->setObjectName("btn-mute");
	m_soloBtn = new AutomatableButton(operationsWidget, tr("Solo"));
	m_soloBtn->setCheckable(true);
	m_soloBtn->setToolTip(tr("Solo"));
	m_soloBtn->setObjectName("btn-solo");

	operationsLayout->addWidget(m_trackOps);
	operationsLayout->addWidget(m_muteBtn);
	operationsLayout->addWidget(m_soloBtn);

	layout->addWidget(operationsWidget, 0, Qt::AlignTop | Qt::AlignLeading);

	connect( this, SIGNAL(trackRemovalScheduled(lmms::gui::TrackView*)),
			m_trackView->trackContainerView(),
				SLOT(deleteTrackView(lmms::gui::TrackView*)),
							Qt::QueuedConnection );

	connect( m_trackView->getTrack()->getMutedModel(), SIGNAL(dataChanged()),
			this, SLOT(update()));

	connect(m_trackView->getTrack(), SIGNAL(colorChanged()), this, SLOT(update()));
}


void TrackOperationsWidget::paintEvent(QPaintEvent*)
{
	QPainter p( this );

	p.fillRect(rect(), palette().brush(QPalette::Window));
}


bool TrackOperationsWidget::confirmRemoval()
{
	bool needConfirm = ConfigManager::inst()->value("ui", "trackdeletionwarning", "1").toInt();
	if (!needConfirm){ return true; }
	
	QString messageRemoveTrack = tr("After removing a track, it can not "
					"be recovered. Are you sure you want to remove track \"%1\"?")
					.arg(m_trackView->getTrack()->name());
	QString messageTitleRemoveTrack = tr("Confirm removal");
	QString askAgainText = tr("Don't ask again");
	auto askAgainCheckBox = new QCheckBox(askAgainText, nullptr);
	auto onCheckedStateChanged = [](auto state){
		// Invert button state, if it's checked we *shouldn't* ask again
		ConfigManager::inst()->setValue("ui", "trackdeletionwarning", state != Qt::Unchecked ? "0" : "1");
	};

#if (QT_VERSION >= QT_VERSION_CHECK(6, 7, 0))
	connect(askAgainCheckBox, &QCheckBox::checkStateChanged, onCheckedStateChanged);
#else
	connect(askAgainCheckBox, &QCheckBox::stateChanged, onCheckedStateChanged);
#endif

	QMessageBox mb;
	mb.setText(messageRemoveTrack);
	mb.setWindowTitle(messageTitleRemoveTrack);
	mb.setIcon(QMessageBox::Warning);
	mb.addButton(QMessageBox::Cancel);
	mb.addButton(QMessageBox::Ok);
	mb.setCheckBox(askAgainCheckBox);
	mb.setDefaultButton(QMessageBox::Cancel);

	return mb.exec() == QMessageBox::Ok;
}


void TrackOperationsWidget::cloneTrack()
{
	TrackContainerView *tcView = m_trackView->trackContainerView();

	Track *newTrack = m_trackView->getTrack()->clone();
	TrackView *newTrackView = tcView->createTrackView( newTrack );

	int index = tcView->trackViews().indexOf( m_trackView );
	int i = tcView->trackViews().size();
	while ( i != index + 1 )
	{
		tcView->moveTrackView( newTrackView, i - 1 );
		i--;
	}

	if (m_soloBtn->model()->value())
	{
		// if this track was solo, make the new track the new solo
		newTrack->toggleSolo();
	}

	newTrackView->update();
	tcView->update();
}


void TrackOperationsWidget::clearTrack()
{
	Track * t = m_trackView->getTrack();
	t->addJournalCheckPoint();
	t->lock();
	t->deleteClips();
	t->unlock();
}


void TrackOperationsWidget::removeTrack()
{
	if (confirmRemoval())
	{
		emit trackRemovalScheduled(m_trackView);
	}
}

void TrackOperationsWidget::selectTrackColor()
{
	const auto newColor = ColorChooser{this}
		.withPalette(ColorChooser::Palette::Track)
		->getColor(m_trackView->getTrack()->color().value_or(Qt::white));

	if (!newColor.isValid()) { return; }

	const auto track = m_trackView->getTrack();
	track->addJournalCheckPoint();
	track->setColor(newColor);
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::resetTrackColor()
{
	auto track = m_trackView->getTrack();
	track->addJournalCheckPoint();
	track->setColor(std::nullopt);
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::randomizeTrackColor()
{
	QColor buffer = ColorChooser::getPalette(ColorChooser::Palette::Track)[fastRand(48)];
	auto track = m_trackView->getTrack();
	track->addJournalCheckPoint();
	track->setColor(buffer);
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::resetClipColors()
{
	auto track = m_trackView->getTrack();
	track->addJournalCheckPoint();
	for (auto clip : track->getClips())
	{
		clip->setColor(std::nullopt);
	}
	Engine::getSong()->setModified();
}


// -- batch operations on the multi-track selection ---------------------

void TrackOperationsWidget::cloneSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty()) { return; }

	// Clone every selected track, then move the clones as a block right
	// after the lowest selected track, preserving their order.
	const int insertPos = tcv->trackViews().indexOf(selected.last()) + 1;
	QVector<TrackView*> clones;
	for (auto* tv : selected)
	{
		if (!tv || !tv->getTrack()) { continue; }
		if (TrackView* newView = tcv->createTrackView(tv->getTrack()->clone()))
		{
			clones.append(newView);
		}
	}
	int target = insertPos;
	for (auto* clone : clones)
	{
		while (tcv->trackViews().indexOf(clone) > target)
		{
			tcv->moveTrackViewUp(clone);
		}
		++target;
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::removeSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty()) { return; }

	const auto ret = QMessageBox::warning(this, tr("Remove tracks"),
		tr("Remove %1 tracks including their clips?").arg(selected.size()),
		QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Cancel);
	if (ret != QMessageBox::Ok) { return; }

	// Remove this widget's own track last (via the queued signal) so the
	// menu stays valid while the other views are deleted.
	for (auto* tv : selected)
	{
		if (tv && tv != m_trackView) { tcv->deleteTrackView(tv); }
	}
	if (selected.contains(m_trackView))
	{
		emit trackRemovalScheduled(m_trackView);
	}
}

void TrackOperationsWidget::clearSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty() || tcv->fixedClips()) { return; }

	tcv->model()->addJournalCheckPoint();
	for (auto* tv : selected)
	{
		if (!tv) { continue; }
		Track* t = tv->getTrack();
		if (!t) { continue; }
		t->lock();
		t->deleteClips();
		t->unlock();
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::moveSelectedTracksUp()
{
	auto* tcv = m_trackView->trackContainerView();
	for (auto* tv : tcv->selectedTrackViews())
	{
		if (tv) { tcv->moveTrackViewUp(tv); }
	}
}

void TrackOperationsWidget::moveSelectedTracksDown()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	// Bottom-most first so selected neighbours don't overtake each other.
	for (auto it = selected.crbegin(); it != selected.crend(); ++it)
	{
		if (*it) { tcv->moveTrackViewDown(*it); }
	}
}

void TrackOperationsWidget::muteSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	for (auto* tv : tcv->selectedTrackViews())
	{
		if (tv && tv->getTrack()) { tv->getTrack()->setMuted(true); }
	}
}

void TrackOperationsWidget::unmuteSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	for (auto* tv : tcv->selectedTrackViews())
	{
		if (tv && tv->getTrack()) { tv->getTrack()->setMuted(false); }
	}
}

void TrackOperationsWidget::soloSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty()) { return; }

	QSet<Track*> selTracks;
	for (auto* tv : selected)
	{
		if (tv && tv->getTrack()) { selTracks.insert(tv->getTrack()); }
	}
	if (selTracks.isEmpty()) { return; }

	// Mirrors Track::toggleSolo, applied to the whole selection at once.
	const bool soloLegacy = ConfigManager::inst()->value("app", "sololegacybehavior", "0").toInt();
	for (Track* t : tcv->model()->tracks())
	{
		if (selTracks.contains(t))
		{
			t->setSolo(true);
			t->setMuted(false);
		}
		else
		{
			t->setSolo(false);
			if (soloLegacy || t->type() != Track::Type::Automation)
			{
				t->setMuted(true);
			}
		}
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::selectColorForSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty()) { return; }

	const auto newColor = ColorChooser{this}
		.withPalette(ColorChooser::Palette::Track)
		->getColor(m_trackView->getTrack()->color().value_or(Qt::white));

	if (!newColor.isValid()) { return; }

	tcv->model()->addJournalCheckPoint();
	for (auto* tv : selected)
	{
		if (tv && tv->getTrack()) { tv->getTrack()->setColor(newColor); }
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::groupSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.size() < 2) { return; }

	const int gid = tcv->nextGroupId();
	bool ok = false;
	QString name = QInputDialog::getText(this, tr("Track group"),
		tr("Group name:"), QLineEdit::Normal,
		tr("Group %1").arg(gid), &ok);
	if (!ok) { return; }

	tcv->model()->addJournalCheckPoint();
	for (auto* tv : selected)
	{
		if (tv && tv->getTrack()) { tv->getTrack()->setGroup(gid, name); }
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::ungroupSelectedTracks()
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty()) { return; }

	tcv->model()->addJournalCheckPoint();
	for (auto* tv : selected)
	{
		if (tv && tv->getTrack() && tv->getTrack()->groupId() > 0)
		{
			tv->getTrack()->setGroup(0);
		}
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::addSelectedToGroup(int groupId)
{
	auto* tcv = m_trackView->trackContainerView();
	const auto selected = tcv->selectedTrackViews();
	if (selected.isEmpty() || groupId <= 0) { return; }

	QString name;
	for (const auto* t : tcv->model()->tracks())
	{
		if (t->groupId() == groupId) { name = t->groupName(); break; }
	}

	tcv->model()->addJournalCheckPoint();
	for (auto* tv : selected)
	{
		if (tv && tv->getTrack() && tv->getTrack()->groupId() != groupId)
		{
			tv->getTrack()->setGroup(groupId, name);
		}
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::renameSelectedGroup()
{
	auto* tcv = m_trackView->trackContainerView();
	Track* self = m_trackView->getTrack();
	const int gid = self ? self->groupId() : 0;
	if (gid <= 0) { return; }

	bool ok = false;
	QString name = QInputDialog::getText(this, tr("Rename group"),
		tr("Group name:"), QLineEdit::Normal,
		self->groupName().isEmpty() ? tr("Group %1").arg(gid) : self->groupName(), &ok);
	if (!ok) { return; }

	tcv->model()->addJournalCheckPoint();
	for (Track* t : tcv->model()->tracks())
	{
		if (t->groupId() == gid) { t->setGroup(gid, name); }
	}
	Engine::getSong()->setModified();
}

void TrackOperationsWidget::toggleSelectedGroupCollapsed()
{
	auto* tcv = m_trackView->trackContainerView();
	Track* self = m_trackView->getTrack();
	const int gid = self ? self->groupId() : 0;
	if (gid <= 0) { return; }
	tcv->setGroupCollapsed(gid, !tcv->isGroupCollapsed(gid));
}


void TrackOperationsWidget::updateMenu()
{
	QMenu * toMenu = m_trackOps->menu();
	toMenu->clear();
	toMenu->addAction( embed::getIconPixmap( "edit_copy", 16, 16 ),
						tr( "Clone this track" ),
						this, SLOT(cloneTrack()));
	toMenu->addAction( embed::getIconPixmap( "cancel", 16, 16 ),
						tr( "Remove this track" ),
						this, SLOT(removeTrack()));

	if( ! m_trackView->trackContainerView()->fixedClips() )
	{
		toMenu->addAction( tr( "Clear this track" ), this, SLOT(clearTrack()));
	}
	if (QMenu *mixerMenu = m_trackView->createMixerMenu(tr("Channel %1: %2"), tr("Assign to new Mixer Channel")))
	{
		toMenu->addMenu(mixerMenu);
	}

	if (auto trackView = dynamic_cast<InstrumentTrackView*>(m_trackView))
	{
		toMenu->addSeparator();
		toMenu->addAction( embed::getIconPixmap( "random_wave_active", 16, 16 ),
						tr( "Randomize this instrument" ),
						this, SLOT(randomizeInstrument()));
		toMenu->addMenu(trackView->midiMenu());
	}
	if( dynamic_cast<AutomationTrackView *>( m_trackView ) )
	{
		toMenu->addAction( tr( "Turn all recording on" ), this, SLOT(recordingOn()));
		toMenu->addAction( tr( "Turn all recording off" ), this, SLOT(recordingOff()));
	}

	// Batch operations when this track is part of a multi-track selection.
	auto* tcv = m_trackView->trackContainerView();
	const auto selectedViews = tcv->selectedTrackViews();
	if (selectedViews.size() > 1 && selectedViews.contains(m_trackView))
	{
		toMenu->addSeparator();
		QMenu* selMenu = toMenu->addMenu(tr("%1 tracks selected").arg(selectedViews.size()));

		selMenu->addAction(tr("Clone selected tracks"), this, &TrackOperationsWidget::cloneSelectedTracks);
		selMenu->addAction(tr("Remove selected tracks"), this, &TrackOperationsWidget::removeSelectedTracks);
		if (!tcv->fixedClips())
		{
			selMenu->addAction(tr("Clear selected tracks"), this, &TrackOperationsWidget::clearSelectedTracks);
		}
		selMenu->addSeparator();
		selMenu->addAction(tr("Move selected up"), this, &TrackOperationsWidget::moveSelectedTracksUp);
		selMenu->addAction(tr("Move selected down"), this, &TrackOperationsWidget::moveSelectedTracksDown);
		selMenu->addSeparator();
		selMenu->addAction(tr("Mute selected"), this, &TrackOperationsWidget::muteSelectedTracks);
		selMenu->addAction(tr("Unmute selected"), this, &TrackOperationsWidget::unmuteSelectedTracks);
		selMenu->addAction(tr("Solo selected"), this, &TrackOperationsWidget::soloSelectedTracks);
		selMenu->addAction(tr("Set color for selected..."), this, &TrackOperationsWidget::selectColorForSelectedTracks);
		selMenu->addSeparator();

		QMenu* groupMenu = selMenu->addMenu(tr("Track group"));
		groupMenu->addAction(tr("Group selected tracks"), this, &TrackOperationsWidget::groupSelectedTracks);
		groupMenu->addAction(tr("Ungroup selected"), this, &TrackOperationsWidget::ungroupSelectedTracks);

		QMap<int, QString> existingGroups;
		for (const auto* t : tcv->model()->tracks())
		{
			if (t->groupId() > 0) { existingGroups[t->groupId()] = t->groupName(); }
		}
		if (!existingGroups.isEmpty())
		{
			QMenu* addToMenu = groupMenu->addMenu(tr("Add to group"));
			for (auto it = existingGroups.constBegin(); it != existingGroups.constEnd(); ++it)
			{
				const QString label = it.value().isEmpty() ? tr("Group %1").arg(it.key()) : it.value();
				addToMenu->addAction(label, this, [this, gid = it.key()] { addSelectedToGroup(gid); });
			}
		}

		const int ownGroup = m_trackView->getTrack()->groupId();
		if (ownGroup > 0)
		{
			groupMenu->addSeparator();
			groupMenu->addAction(tr("Rename group..."), this, &TrackOperationsWidget::renameSelectedGroup);
			groupMenu->addAction(tcv->isGroupCollapsed(ownGroup) ? tr("Expand group") : tr("Collapse group"),
								 this, &TrackOperationsWidget::toggleSelectedGroupCollapsed);
		}
	}

	toMenu->addSeparator();

	QMenu* colorMenu = toMenu->addMenu(tr("Track color"));
	colorMenu->setIcon(embed::getIconPixmap("colorize"));
	colorMenu->addAction(tr("Change"), this, SLOT(selectTrackColor()));
	colorMenu->addAction(tr("Reset"), this, SLOT(resetTrackColor()));
	colorMenu->addAction(tr("Pick random"), this, SLOT(randomizeTrackColor()));
	colorMenu->addSeparator();
	colorMenu->addAction(tr("Reset clip colors"), this, SLOT(resetClipColors()));
}


void TrackOperationsWidget::toggleRecording( bool on )
{
	auto atv = dynamic_cast<AutomationTrackView*>(m_trackView);
	if( atv )
	{
		for( Clip * clip : atv->getTrack()->getClips() )
		{
			auto ap = dynamic_cast<AutomationClip*>(clip);
			if( ap ) { ap->setRecording( on ); }
		}
		atv->update();
	}
}


void TrackOperationsWidget::recordingOn()
{
	toggleRecording( true );
}


void TrackOperationsWidget::recordingOff()
{
	toggleRecording( false );
}


void TrackOperationsWidget::randomizeInstrument()
{
	if (auto it = dynamic_cast<InstrumentTrack*>(m_trackView->model()))
	{
		it->randomizeParameters();
		TextFloat::displayMessage(
			tr("Instrument Randomized"),
			tr("Randomized parameters for %1!").arg(it->name()),
			embed::getIconPixmap("random_wave_active"),
			2500
		);
	}
}


} // namespace lmms::gui
