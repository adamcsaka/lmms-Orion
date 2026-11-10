/*
 * PatternStore.cpp - model-component of Pattern Editor
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


#include "PatternStore.h"

#include <numeric>
#include "AutomatableModel.h"
#include "AutomationClip.h"
#include "AutomationTrack.h"
#include "Clip.h"
#include "Engine.h"
#include "PatternTrack.h"
#include "Song.h"

namespace lmms
{


PatternStore::PatternStore() :
	TrackContainer(),
	m_patternComboBoxModel(this)
{
	setType(Type::Pattern);
}




bool PatternStore::play(TimePos start, f_cnt_t frames, f_cnt_t offset, int clipNum)
{
	bool notePlayed = false;

	if (lengthOfPattern(clipNum) <= 0)
	{
		return false;
	}

	const TrackList& tl = tracks();
	for (Track * t : tl)
	{
		if (t->play(start, frames, offset, clipNum))
		{
			notePlayed = true;
		}
	}

	return notePlayed;
}




void PatternStore::updateAfterTrackAdd()
{
	if (numOfPatterns() == 0 && !Engine::getSong()->isLoadingProject())
	{
		Engine::getSong()->addPatternTrack();
	}
}




bar_t PatternStore::lengthOfPattern(int pattern) const
{
	tick_t maxTicks = TimePos::ticksPerBar();
	tick_t polyTicks = TimePos::ticksPerBar();

	const TrackList & tl = tracks();
	for (Track * t : tl)
	{
		if (pattern < t->numOfClips())
		{
			auto c = t->getClip(pattern);
			if (c && c->length() > 0)
			{
				maxTicks = std::max(maxTicks, static_cast<tick_t>(c->length()));
				polyTicks = std::lcm(polyTicks, static_cast<tick_t>(c->length()));
				if (polyTicks > 32 * TimePos::ticksPerBar())
				{
					polyTicks = std::max(maxTicks, static_cast<tick_t>(32 * TimePos::ticksPerBar()));
					break;
				}
			}
		}
	}

	return TimePos(polyTicks).nextFullBar();
}




int PatternStore::numOfPatterns() const
{
	return Engine::getSong()->countTracks(Track::Type::Pattern);
}




void PatternStore::removePattern(int pattern)
{
	const TrackList& tl = tracks();
	for (Track * t : tl)
	{
		delete t->getClip(pattern);
		t->removeBar(pattern * DefaultTicksPerBar);
	}
	if (pattern <= currentPattern())
	{
		setCurrentPattern(std::max(currentPattern() - 1, 0));
	}
}




void PatternStore::swapPattern(int pattern1, int pattern2)
{
	const TrackList& tl = tracks();
	for (Track * t : tl)
	{
		t->swapPositionOfClips(pattern1, pattern2);
	}
	updateComboBox();
}




void PatternStore::updatePatternTrack(Clip* clip)
{
	PatternTrack * t = PatternTrack::findPatternTrack(clip->startPosition() / DefaultTicksPerBar);
	if (t != nullptr)
	{
		t->dataChanged();
	}
	emit trackUpdated();
}




void PatternStore::fixIncorrectPositions()
{
	const TrackList& tl = tracks();
	for (Track * t : tl)
	{
		for (int i = 0; i < numOfPatterns(); ++i)
		{
			t->getClip(i)->movePosition(TimePos(i, 0));
		}
	}
}




void PatternStore::play()
{
	if (Engine::getSong()->playMode() != Song::PlayMode::Pattern)
	{
		Engine::getSong()->playPattern();
	}
	else
	{
		Engine::getSong()->togglePause();
	}
}




void PatternStore::stop()
{
	Engine::getSong()->stop();
}




void PatternStore::updateComboBox()
{
	const int curPattern = currentPattern();

	m_patternComboBoxModel.clear();

	for (int i = 0; i < numOfPatterns(); ++i)
	{
		PatternTrack* pt = PatternTrack::findPatternTrack(i);
		m_patternComboBoxModel.addItem(pt->name());
	}
	setCurrentPattern(curPattern);
}




void PatternStore::createClipsForPattern(int pattern)
{
	const TrackList& tl = tracks();
	for (Track * t : tl)
	{
		t->createClipsForPattern(pattern);
	}
}

void PatternStore::handleLiveAutomation(AutomatableModel* model)
{
	if (!m_recordAutomation || !model || !Engine::getSong() || !Engine::getSong()->isPlaying())
	{
		return;
	}

	const int curPat = currentPattern();
	if (curPat < 0) { return; }

	// Look for existing AutomationClip in currentPattern controlling this model
	AutomationClip* targetClip = nullptr;
	for (Track* t : tracks())
	{
		if (t->type() == Track::Type::Automation && curPat < t->numOfClips())
		{
			auto ac = dynamic_cast<AutomationClip*>(t->getClip(curPat));
			if (ac)
			{
				const auto& objs = ac->objects();
				if (std::find(objs.begin(), objs.end(), model) != objs.end())
				{
					targetClip = ac;
					break;
				}
			}
		}
	}

	// If not found, create a new AutomationTrack in PatternStore
	if (!targetClip)
	{
		auto track = dynamic_cast<AutomationTrack*>(Track::create(Track::Type::Automation, this));
		if (track)
		{
			track->setName(model->fullDisplayName());
			if (curPat < track->numOfClips())
			{
				targetClip = dynamic_cast<AutomationClip*>(track->getClip(curPat));
			}
			if (targetClip)
			{
				targetClip->addObject(model);
			}
		}
	}

	if (targetClip)
	{
		TimePos curPos = Engine::getSong()->getPlayPos(Song::PlayMode::Pattern);
		if (targetClip->length() > 0)
		{
			curPos = curPos % targetClip->length();
		}
		targetClip->recordValue(curPos, model->inverseScaledValue(model->value<float>()));
		emit targetClip->dataChanged();
	}
}




AutomatedValueMap PatternStore::automatedValuesAt(TimePos time, int clipNum) const
{
	Q_ASSERT(clipNum >= 0);
	Q_ASSERT(time.getTicks() >= 0);

	return TrackContainer::automatedValuesAt(time + (TimePos::ticksPerBar() * clipNum), clipNum);
}


} // namespace lmms
