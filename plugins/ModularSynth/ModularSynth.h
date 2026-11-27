/*
 * ModularSynth.h - declaration of class ModularSynth, a modular
 *                  oscillator instrument with unlimited voices,
 *                  FM routing, ADSR and precise numeric control
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

#ifndef LMMS_MODULAR_SYNTH_H
#define LMMS_MODULAR_SYNTH_H

#include <vector>

class QVBoxLayout;
class QScrollArea;
class QPushButton;

#include "Instrument.h"
#include "InstrumentView.h"
#include "AutomatableModel.h"

namespace lmms
{

namespace gui { class ModularSynthView; }

class ModularOscObject : public Model
{
	Q_OBJECT
public:
	ModularOscObject( Model * _parent, int _idx );

	BoolModel m_enabledModel;
	IntModel m_waveModel;
	FloatModel m_freqModel;
	FloatModel m_ampModel;
	FloatModel m_panModel;
	IntModel m_fmSrcModel;
	FloatModel m_fmDepthModel;

	friend class ModularSynth;
	friend class gui::ModularSynthView;
} ;




class ModularSynth : public Instrument
{
	Q_OBJECT
public:
	ModularSynth( InstrumentTrack * _track );
	~ModularSynth() override;

	void playNote( NotePlayHandle * _n,
						SampleFrame* _working_buffer ) override;
	void deleteNotePluginData( NotePlayHandle * _n ) override;

	void saveSettings( QDomDocument & _doc, QDomElement & _parent ) override;
	void loadSettings( const QDomElement & _this ) override;

	QString nodeName() const override;

	float desiredReleaseTimeMs() const override
	{
		return 5000.f;
	}

	gui::PluginView* instantiateView( QWidget * _parent ) override;

	// oscillator management (called from the view)
	void addOscillator();
	void removeOscillator( int _idx );
	int oscillatorCount() const { return static_cast<int>( m_oscs.size() ); }
	ModularOscObject * oscillatorAt( int _idx ) { return m_oscs[_idx]; }

	static constexpr int MaxOscillators = 64;

	// master / envelope models (public for the view)
	FloatModel m_masterVolumeModel;
	FloatModel m_masterPanModel;
	FloatModel m_attackModel;
	FloatModel m_decayModel;
	FloatModel m_sustainModel;
	FloatModel m_releaseModel;

private:
	std::vector<ModularOscObject *> m_oscs;

	friend class gui::ModularSynthView;
} ;




namespace gui
{

class Knob;
class LcdFloatSpinBox;
class ModularOscBox;


class ModularSynthView : public InstrumentView
{
	Q_OBJECT
public:
	ModularSynthView( Instrument * _instrument, QWidget * _parent );
	~ModularSynthView() override = default;

	ModularSynth * synth() const { return m_synth; }
	void addOscBox();
	void removeOscBox( ModularOscBox * _box );

private:
	void modelChanged() override;

	ModularSynth * m_synth = nullptr;
	QVBoxLayout * m_oscsLayout = nullptr;
	QScrollArea * m_scrollArea = nullptr;
	QWidget * m_content = nullptr;
	QPushButton * m_addButton = nullptr;
	std::vector<ModularOscBox *> m_boxes;

	void rebuildOscBoxes();

	// master controls
	Knob * m_masterVolKnob = nullptr;
	Knob * m_masterPanKnob = nullptr;
	LcdFloatSpinBox * m_attackSpin = nullptr;
	LcdFloatSpinBox * m_decaySpin = nullptr;
	Knob * m_sustainKnob = nullptr;
	LcdFloatSpinBox * m_releaseSpin = nullptr;
} ;


} // namespace gui

} // namespace lmms

#endif // LMMS_MODULAR_SYNTH_H
