/*
 * ModularSynth.cpp - modular oscillator instrument with unlimited
 *                    oscillators, FM routing, ADSR and precise control
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

#include <QComboBox>
#include <QDomElement>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtGlobal>

#include "ModularSynth.h"
#include "AudioEngine.h"
#include "AutomatableButton.h"
#include "Engine.h"
#include "InstrumentTrack.h"
#include "Knob.h"
#include "LcdFloatSpinBox.h"
#include "NotePlayHandle.h"
#include "embed.h"
#include "plugin_export.h"

namespace lmms
{


extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT modularsynth_plugin_descriptor =
{
	LMMS_STRINGIFY( PLUGIN_NAME ),
	"ModularSynth",
	QT_TRANSLATE_NOOP( "PluginBrowser",
				"Unlimited modular oscillators with FM, ADSR\n"
				"and precise numeric control - a sound design tool" ),
	"LMMS Orion",
	0x0100,
	Plugin::Type::Instrument,
	new PluginPixmapLoader( "logo" ),
	nullptr,
	nullptr,
} ;

}




// ---------------------------------------------------------------------------
// Oscillator module model
// ---------------------------------------------------------------------------

ModularOscObject::ModularOscObject( Model * _parent, int _idx ) :
	Model( _parent ),
	m_enabledModel( true, this, tr( "Osc %1 enabled" ).arg( _idx + 1 ) ),
	m_waveModel( 0, 0, 4, this, tr( "Osc %1 waveform" ).arg( _idx + 1 ) ),
	m_freqModel( 440.0f, -1000000.0f, 1000000.0f, 0.01f, this,
				tr( "Osc %1 frequency (Hz)" ).arg( _idx + 1 ) ),
	m_ampModel( 0.8f, 0.0f, 4.0f, 0.01f, this,
				tr( "Osc %1 amplitude" ).arg( _idx + 1 ) ),
	m_panModel( 0.0f, -1.0f, 1.0f, 0.01f, this,
				tr( "Osc %1 pan" ).arg( _idx + 1 ) ),
	m_fmSrcModel( -1, -1, ModularSynth::MaxOscillators - 1, this,
				tr( "Osc %1 FM source" ).arg( _idx + 1 ) ),
	m_fmDepthModel( 0.0f, -100000.0f, 100000.0f, 0.1f, this,
				tr( "Osc %1 FM depth (Hz)" ).arg( _idx + 1 ) )
{
}




// ---------------------------------------------------------------------------
// Instrument
// ---------------------------------------------------------------------------

ModularSynth::ModularSynth( InstrumentTrack * _track ) :
	Instrument( _track, &modularsynth_plugin_descriptor ),
	m_masterVolumeModel( 1.0f, 0.0f, 2.0f, 0.01f, this,
						 tr( "Master volume" ) ),
	m_masterPanModel( 0.0f, -1.0f, 1.0f, 0.01f, this,
					 tr( "Master pan" ) ),
	m_attackModel( 10.0f, 0.0f, 5000.0f, 0.1f, this,
				  tr( "Attack (ms)" ) ),
	m_decayModel( 100.0f, 0.0f, 5000.0f, 0.1f, this,
				 tr( "Decay (ms)" ) ),
	m_sustainModel( 0.7f, 0.0f, 1.0f, 0.01f, this,
				tr( "Sustain level" ) ),
	m_releaseModel( 300.0f, 0.0f, 5000.0f, 0.1f, this,
				   tr( "Release (ms)" ) )
{
	addOscillator();
}




ModularSynth::~ModularSynth()
{
	for (auto* osc : m_oscs) { delete osc; }
	m_oscs.clear();
}




void ModularSynth::addOscillator()
{
	if (static_cast<int>( m_oscs.size() ) >= MaxOscillators) { return; }

	Engine::audioEngine()->requestChangeInModel();
	const int idx = static_cast<int>( m_oscs.size() );
	auto* osc = new ModularOscObject( this, idx );
	// stagger default pitches a fifth apart so added oscs are audible
	osc->m_freqModel.setValue( 440.0f * std::pow( 1.5f, idx % 12 ) );
	m_oscs.push_back( osc );
	Engine::audioEngine()->doneChangeInModel();
}




void ModularSynth::removeOscillator( int _idx )
{
	if ( _idx < 0 || _idx >= static_cast<int>( m_oscs.size() ) ||
		 m_oscs.size() <= 1 )
	{
		return;
	}

	Engine::audioEngine()->requestChangeInModel();

	// retarget FM sources that referenced the removed oscillator
	for ( int i = 0; i < static_cast<int>( m_oscs.size() ); ++i )
	{
		const int src = m_oscs[i]->m_fmSrcModel.value();
		if ( src == _idx )
		{
			m_oscs[i]->m_fmSrcModel.setValue( -1 );
		}
		else if ( src > _idx )
		{
			m_oscs[i]->m_fmSrcModel.setValue( src - 1 );
		}
	}

	delete m_oscs[_idx];
	m_oscs.erase( m_oscs.begin() + _idx );

	Engine::audioEngine()->doneChangeInModel();
}




// ---------------------------------------------------------------------------
// DSP
// ---------------------------------------------------------------------------

namespace
{

struct OscState
{
	double phase = 0.0;
	float prevOut = 0.0f;
	uint32_t rng = 0x9e3779b9u;
} ;

struct NoteData
{
	std::vector<OscState> oscs;
	f_cnt_t envFrames = 0;
	bool releaseCaptured = false;
	float releaseLevel = 0.0f;
	f_cnt_t releaseFramesAtCapture = 0;
} ;


inline float polyBlept( float t, float dt )
{
	if ( t < dt ) { return t / dt - 1.0f; }
	if ( t > 1.0f - dt ) { return ( t - 1.0f ) / dt + 1.0f; }
	return 0.0f;
}


inline float sawWave( double phase, float dt )
{
	const float p = static_cast<float>( phase - std::floor( phase ) );
	const float adt = std::fabs( dt );
	float v = 2.0f * p - 1.0f;
	if ( adt > 1e-6f ) { v -= polyBlept( p, std::min( adt, 0.5f ) ); }
	return v;
}


inline float waveValue( int wave, double phase, float dt, uint32_t& rng )
{
	const float p = static_cast<float>( phase - std::floor( phase ) );

	switch ( wave )
	{
		case 0: // sine
			return std::sin( 6.28318530717958647692 * phase );
		case 1: // saw
			return sawWave( phase, dt );
		case 2: // square (difference of two band-limited saws)
		{
			const float adt = std::fabs( dt );
			const double p2 = phase + 0.5;
			return sawWave( p2, dt ) - sawWave( phase, dt );
		}
		case 3: // triangle
			return p < 0.5f ? 4.0f * p - 1.0f : 3.0f - 4.0f * p;
		case 4: // noise (xorshift32)
		{
			rng ^= rng << 13;
			rng ^= rng >> 17;
			rng ^= rng << 5;
			return static_cast<float>( static_cast<int32_t>( rng ) ) /
					   static_cast<float>( INT32_MAX );
		}
		default:
			return 0.0f;
	}
}


} // anonymous namespace




void ModularSynth::playNote( NotePlayHandle * _n,
							 SampleFrame* _working_buffer )
{
	if ( !_n->m_pluginData )
	{
		auto* nd = new NoteData;
		nd->oscs.resize( m_oscs.size() );
		_n->m_pluginData = nd;
	}
	auto* nd = static_cast<NoteData *>( _n->m_pluginData );

	const f_cnt_t frames = _n->framesLeftForCurrentPeriod();
	const f_cnt_t offset = _n->noteOffset();
	const double invSr = 1.0 / Engine::audioEngine()->outputSampleRate();

	// envelope parameters (live, so they can be automated)
	const float attackMs = m_attackModel.value();
	const float decayMs = m_decayModel.value();
	const float sustain = m_sustainModel.value();
	const float releaseMs = m_releaseModel.value();
	const double sr = Engine::audioEngine()->outputSampleRate();
	const f_cnt_t attackF = static_cast<f_cnt_t>( attackMs * sr * 0.001 );
	const f_cnt_t decayF = static_cast<f_cnt_t>( decayMs * sr * 0.001 );
	const f_cnt_t releaseF = static_cast<f_cnt_t>( releaseMs * sr * 0.001 );

	const float masterVol = m_masterVolumeModel.value();
	const float masterPan = m_masterPanModel.value();
	const float panL = 1.0f - std::max( masterPan, 0.0f );
	const float panR = 1.0f + std::min( masterPan, 0.0f );

	const int oscCount = static_cast<int>( m_oscs.size() );

	// Cache oscillator parameters once per buffer period - model values
	// cannot meaningfully change inside a single audio period, and this
	// keeps the per-sample loop cheap even with many oscillators.
	struct CachedOsc
	{
		float freq;
		float amp;
		float panL;
		float panR;
		float fmDepth;
		int wave;
		int fmSrc;
		bool enabled;
	} ;
	CachedOsc cached[MaxOscillators];
	int activeOscCount = 0;
	for ( int o = 0; o < oscCount && o < MaxOscillators; ++o )
	{
		auto* osc = m_oscs[o];
		CachedOsc& c = cached[o];
		c.enabled = osc->m_enabledModel.value();
		c.wave = osc->m_waveModel.value();
		c.freq = osc->m_freqModel.value();
		c.amp = osc->m_ampModel.value();
		c.fmSrc = osc->m_fmSrcModel.value();
		c.fmDepth = osc->m_fmDepthModel.value();
		const float pan = osc->m_panModel.value();
		c.panL = 1.0f - std::max( pan, 0.0f );
		c.panR = 1.0f + std::min( pan, 0.0f );
		if ( c.enabled && c.amp != 0.0f ) { ++activeOscCount; }
	}

	// silent patch: skip the per-sample loop entirely
	if ( activeOscCount == 0 )
	{
		nd->envFrames += frames;
		return;
	}

	for ( f_cnt_t i = 0; i < frames; ++i )
	{
		const f_cnt_t g = nd->envFrames + i;

		// ---- envelope ----
		float env;
		if ( _n->isReleased() )
		{
			if ( !nd->releaseCaptured )
			{
				nd->releaseCaptured = true;
				nd->releaseFramesAtCapture = _n->releaseFramesDone();
				// level the envelope had reached at this point
				if ( attackF > 0 && g < attackF )
				{
					nd->releaseLevel = static_cast<float>( g ) /
									   static_cast<float>( attackF );
				}
				else if ( decayF > 0 && g < attackF + decayF )
				{
					nd->releaseLevel = 1.0f - ( 1.0f - sustain ) *
						( static_cast<float>( g - attackF ) /
						  static_cast<float>( decayF ) );
				}
				else
				{
					nd->releaseLevel = sustain;
				}
			}
			const float prog = releaseF > 0
				? std::min( 1.0f, static_cast<float>(
						nd->releaseFramesAtCapture + i ) /
								   static_cast<float>( releaseF ) )
				: 1.0f;
			env = nd->releaseLevel * ( 1.0f - prog );
		}
		else if ( attackF > 0 && g < attackF )
		{
			env = static_cast<float>( g ) / static_cast<float>( attackF );
		}
		else if ( decayF > 0 && g < attackF + decayF )
		{
			env = 1.0f - ( 1.0f - sustain ) *
				  ( static_cast<float>( g - attackF ) /
					static_cast<float>( decayF ) );
		}
		else
		{
			env = sustain;
		}

		// ---- oscillators ----
		float mixL = 0.0f;
		float mixR = 0.0f;

		for ( int o = 0; o < oscCount && o < static_cast<int>( nd->oscs.size() ); ++o )
		{
			auto& st = nd->oscs[o];
			const CachedOsc& c = cached[o];

			if ( !c.enabled || c.amp == 0.0f ) { continue; }

			// FM: one-sample-delayed modulator output keeps cycles stable
			float mod = 0.0f;
			if ( c.fmSrc >= 0 && c.fmSrc < oscCount && c.fmSrc != o &&
				 c.fmSrc < static_cast<int>( nd->oscs.size() ) )
			{
				mod = nd->oscs[c.fmSrc].prevOut;
			}

			const float dt = static_cast<float>(
				( c.freq + c.fmDepth * mod ) * invSr );

			const float v = waveValue( c.wave, st.phase, dt, st.rng );

			st.phase += static_cast<double>( dt );
			st.phase -= std::floor( st.phase );

			const float out = v * c.amp;
			st.prevOut = out;

			mixL += out * c.panL;
			mixR += out * c.panR;
		}

		const float level = env * masterVol;
		_working_buffer[offset + i][0] += mixL * level * panL;
		_working_buffer[offset + i][1] += mixR * level * panR;
	}

	nd->envFrames += frames;
}




void ModularSynth::deleteNotePluginData( NotePlayHandle * _n )
{
	delete static_cast<NoteData *>( _n->m_pluginData );
}




// ---------------------------------------------------------------------------
// Persistence
// ---------------------------------------------------------------------------

void ModularSynth::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	_this.setAttribute( "numoscs", static_cast<int>( m_oscs.size() ) );

	m_masterVolumeModel.saveSettings( _doc, _this, "mastervol" );
	m_masterPanModel.saveSettings( _doc, _this, "masterpan" );
	m_attackModel.saveSettings( _doc, _this, "attack" );
	m_decayModel.saveSettings( _doc, _this, "decay" );
	m_sustainModel.saveSettings( _doc, _this, "sustain" );
	m_releaseModel.saveSettings( _doc, _this, "release" );

	for ( int i = 0; i < static_cast<int>( m_oscs.size() ); ++i )
	{
		const QString p = QStringLiteral( "o%1_" ).arg( i );
		auto* osc = m_oscs[i];
		osc->m_enabledModel.saveSettings( _doc, _this, p + "enabled" );
		osc->m_waveModel.saveSettings( _doc, _this, p + "wave" );
		osc->m_freqModel.saveSettings( _doc, _this, p + "freq" );
		osc->m_ampModel.saveSettings( _doc, _this, p + "amp" );
		osc->m_panModel.saveSettings( _doc, _this, p + "pan" );
		osc->m_fmSrcModel.saveSettings( _doc, _this, p + "fmsrc" );
		osc->m_fmDepthModel.saveSettings( _doc, _this, p + "fmdepth" );
	}
}




void ModularSynth::loadSettings( const QDomElement & _this )
{
	const int numOscs = _this.attribute( "numoscs" ).toInt();

	// rebuild oscillator list to match saved state
	while ( static_cast<int>( m_oscs.size() ) > std::max( 1, numOscs ) )
	{
		delete m_oscs.back();
		m_oscs.pop_back();
	}
	while ( static_cast<int>( m_oscs.size() ) < numOscs &&
			static_cast<int>( m_oscs.size() ) < MaxOscillators )
	{
		Engine::audioEngine()->requestChangeInModel();
		m_oscs.push_back( new ModularOscObject(
			this, static_cast<int>( m_oscs.size() ) ) );
		Engine::audioEngine()->doneChangeInModel();
	}

	m_masterVolumeModel.loadSettings( _this, "mastervol" );
	m_masterPanModel.loadSettings( _this, "masterpan" );
	m_attackModel.loadSettings( _this, "attack" );
	m_decayModel.loadSettings( _this, "decay" );
	m_sustainModel.loadSettings( _this, "sustain" );
	m_releaseModel.loadSettings( _this, "release" );

	for ( int i = 0; i < static_cast<int>( m_oscs.size() ); ++i )
	{
		const QString p = QStringLiteral( "o%1_" ).arg( i );
		auto* osc = m_oscs[i];
		osc->m_enabledModel.loadSettings( _this, p + "enabled" );
		osc->m_waveModel.loadSettings( _this, p + "wave" );
		osc->m_freqModel.loadSettings( _this, p + "freq" );
		osc->m_ampModel.loadSettings( _this, p + "amp" );
		osc->m_panModel.loadSettings( _this, p + "pan" );
		osc->m_fmSrcModel.loadSettings( _this, p + "fmsrc" );
		osc->m_fmDepthModel.loadSettings( _this, p + "fmdepth" );
	}
}




QString ModularSynth::nodeName() const
{
	return QStringLiteral( "modularsynth" );
}




gui::PluginView * ModularSynth::instantiateView( QWidget * _parent )
{
	return new gui::ModularSynthView( this, _parent );
}




// ---------------------------------------------------------------------------
// View
// ---------------------------------------------------------------------------

namespace gui
{


static const char* const WAVE_NAMES[] =
{
	"Sine", "Saw", "Square", "Triangle", "Noise"
} ;




class ModularOscBox : public QGroupBox
{
public:
	ModularOscBox( ModularOscObject * _osc, int _idx,
				   ModularSynthView * _view ) :
		QGroupBox( tr( "Oscillator %1" ).arg( _idx + 1 ) ),
		m_osc( _osc ),
		m_view( _view )
	{
		auto* grid = new QGridLayout( this );
		grid->setContentsMargins( 6, 6, 6, 6 );
		grid->setSpacing( 4 );

		m_enableBtn = new AutomatableButton( this );
		m_enableBtn->setModel( &_osc->m_enabledModel );
		m_enableBtn->setText( tr( "On" ) );
		m_enableBtn->setCheckable( true );
		m_enableBtn->setChecked( _osc->m_enabledModel.value() );
		grid->addWidget( m_enableBtn, 0, 0 );

		m_waveCombo = new QComboBox( this );
		for ( const char* name : WAVE_NAMES ) { m_waveCombo->addItem( tr( name ) ); }
		m_waveCombo->setCurrentIndex( _osc->m_waveModel.value() );
		connect( m_waveCombo, qOverload<int>( &QComboBox::activated ),
				 this, [this]( int idx ) {
				 m_osc->m_waveModel.setValue( idx );
			 } );
		connect( &_osc->m_waveModel, &IntModel::dataChanged,
				 this, [this]() {
				 m_waveCombo->setCurrentIndex( m_osc->m_waveModel.value() );
			 } );
		grid->addWidget( m_waveCombo, 0, 1 );

		m_freqSpin = new LcdFloatSpinBox( 7, 4, tr( "Freq" ), this );
		m_freqSpin->setModel( &_osc->m_freqModel );
		m_freqSpin->setLabel( tr( "FREQ Hz" ) );
		m_freqSpin->setToolTip( tr( "Frequency in Hz - no upper limit, "
								   "type exact values with a double click" ) );
		grid->addWidget( m_freqSpin, 0, 2 );

		m_ampKnob = new Knob( KnobType::Dark28, tr( "Amp" ), this );
		m_ampKnob->setModel( &_osc->m_ampModel );
		grid->addWidget( m_ampKnob, 1, 0 );

		m_panKnob = new Knob( KnobType::Dark28, tr( "Pan" ), this );
		m_panKnob->setModel( &_osc->m_panModel );
		grid->addWidget( m_panKnob, 1, 1 );

		m_fmSrcCombo = new QComboBox( this );
		m_fmSrcCombo->addItem( tr( "FM: None" ) );
		for ( int i = 0; i < _view->synth()->oscillatorCount(); ++i )
		{
			m_fmSrcCombo->addItem( tr( "Osc %1" ).arg( i + 1 ) );
		}
		m_fmSrcCombo->setCurrentIndex( _osc->m_fmSrcModel.value() + 1 );
		connect( m_fmSrcCombo, qOverload<int>( &QComboBox::activated ),
				 this, [this]( int idx ) {
				 m_osc->m_fmSrcModel.setValue( idx - 1 );
			 } );
		connect( &_osc->m_fmSrcModel, &IntModel::dataChanged,
				 this, [this]() {
				 m_fmSrcCombo->setCurrentIndex(
					 m_osc->m_fmSrcModel.value() + 1 );
			 } );
		grid->addWidget( m_fmSrcCombo, 1, 2 );

		m_fmDepthSpin = new LcdFloatSpinBox( 6, 2, tr( "FM depth" ), this );
		m_fmDepthSpin->setModel( &_osc->m_fmDepthModel );
		m_fmDepthSpin->setLabel( tr( "FM DEPTH Hz" ) );
		m_fmDepthSpin->setToolTip( tr( "FM deviation in Hz (can be negative)" ) );
		grid->addWidget( m_fmDepthSpin, 2, 0, 1, 3 );

		m_removeBtn = new QPushButton( tr( "Remove" ), this );
		m_removeBtn->setEnabled( _view->synth()->oscillatorCount() > 1 );
		connect( m_removeBtn, &QPushButton::clicked, this,
				 [this, _view]() { _view->removeOscBox( this ); } );
		grid->addWidget( m_removeBtn, 3, 0, 1, 3 );
	}

	~ModularOscBox() override = default;

	AutomatableButton * m_enableBtn;
	QComboBox * m_waveCombo;
	LcdFloatSpinBox * m_freqSpin;
	Knob * m_ampKnob;
	Knob * m_panKnob;
	QComboBox * m_fmSrcCombo;
	LcdFloatSpinBox * m_fmDepthSpin;
	QPushButton * m_removeBtn;

private:
	ModularOscObject * m_osc;
	ModularSynthView * m_view;
} ;




ModularSynthView::ModularSynthView( Instrument * _instrument,
									QWidget * _parent ) :
	InstrumentView( _instrument, _parent ),
	m_synth( dynamic_cast<ModularSynth *>( _instrument ) )
{
	auto* mainLayout = new QVBoxLayout( this );
	mainLayout->setContentsMargins( 4, 4, 4, 4 );
	mainLayout->setSpacing( 4 );

	// ---- master section ----
	auto* masterBox = new QGroupBox( tr( "Master & Envelope" ), this );
	auto* masterGrid = new QGridLayout( masterBox );
	masterGrid->setContentsMargins( 6, 6, 6, 6 );
	masterGrid->setSpacing( 4 );

	m_masterVolKnob = new Knob( KnobType::Dark28, tr( "Vol" ), masterBox );
	m_masterVolKnob->setModel( &m_synth->m_masterVolumeModel );
	masterGrid->addWidget( m_masterVolKnob, 0, 0 );

	m_masterPanKnob = new Knob( KnobType::Dark28, tr( "Pan" ), masterBox );
	m_masterPanKnob->setModel( &m_synth->m_masterPanModel );
	masterGrid->addWidget( m_masterPanKnob, 0, 1 );

	auto* envLabel = new QLabel( tr( "ADSR (ms / level)" ), masterBox );
	masterGrid->addWidget( envLabel, 0, 2 );

	m_attackSpin = new LcdFloatSpinBox( 4, 1, tr( "Attack" ), masterBox );
	m_attackSpin->setModel( &m_synth->m_attackModel );
	m_attackSpin->setLabel( tr( "ATTACK ms" ) );
	masterGrid->addWidget( m_attackSpin, 1, 0 );

	m_decaySpin = new LcdFloatSpinBox( 4, 1, tr( "Decay" ), masterBox );
	m_decaySpin->setModel( &m_synth->m_decayModel );
	m_decaySpin->setLabel( tr( "DECAY ms" ) );
	masterGrid->addWidget( m_decaySpin, 1, 1 );

	m_sustainKnob = new Knob( KnobType::Dark28, tr( "Sustain" ), masterBox );
	m_sustainKnob->setModel( &m_synth->m_sustainModel );
	masterGrid->addWidget( m_sustainKnob, 1, 2 );

	m_releaseSpin = new LcdFloatSpinBox( 4, 1, tr( "Release" ), masterBox );
	m_releaseSpin->setModel( &m_synth->m_releaseModel );
	m_releaseSpin->setLabel( tr( "RELEASE ms" ) );
	masterGrid->addWidget( m_releaseSpin, 2, 0 );

	mainLayout->addWidget( masterBox );

	// ---- oscillator scroll area ----
	m_scrollArea = new QScrollArea( this );
	m_scrollArea->setWidgetResizable( true );
	m_scrollArea->setFrameShape( QFrame::NoFrame );

	m_content = new QWidget( m_scrollArea );
	m_oscsLayout = new QVBoxLayout( m_content );
	m_oscsLayout->setContentsMargins( 0, 0, 0, 0 );
	m_oscsLayout->setSpacing( 4 );
	m_oscsLayout->addStretch( 1 );
	m_scrollArea->setWidget( m_content );

	mainLayout->addWidget( m_scrollArea, 1 );

	m_addButton = new QPushButton( tr( "+ Add oscillator" ), this );
	connect( m_addButton, &QPushButton::clicked, this,
			 [this]() { addOscBox(); } );
	mainLayout->addWidget( m_addButton );

	rebuildOscBoxes();
}




void ModularSynthView::modelChanged()
{
	InstrumentView::modelChanged();
	rebuildOscBoxes();
}




void ModularSynthView::rebuildOscBoxes()
{
	if ( m_synth == nullptr ) { return; }

	// remove stale boxes
	for ( auto* box : m_boxes ) { delete box; }
	m_boxes.clear();

	const int count = m_synth->oscillatorCount();
	for ( int i = 0; i < count; ++i )
	{
		auto* box = new ModularOscBox( m_synth->oscillatorAt( i ), i, this );
		m_boxes.push_back( box );
		m_oscsLayout->insertWidget( m_oscsLayout->count() - 1, box );
	}

	for ( auto* box : m_boxes )
	{
		box->m_removeBtn->setEnabled( count > 1 );
	}
}




void ModularSynthView::addOscBox()
{
	if ( m_synth == nullptr ) { return; }
	m_synth->addOscillator();
	rebuildOscBoxes();
}




void ModularSynthView::removeOscBox( ModularOscBox * _box )
{
	if ( m_synth == nullptr ) { return; }

	// find index of the box
	int idx = -1;
	for ( int i = 0; i < static_cast<int>( m_boxes.size() ); ++i )
	{
		if ( m_boxes[i] == _box ) { idx = i; break; }
	}
	if ( idx < 0 ) { return; }

	m_synth->removeOscillator( idx );
	rebuildOscBoxes();
}



} // namespace gui


} // namespace lmms


extern "C"
{

// necessary for getting instance out of shared lib
Q_DECL_EXPORT lmms::Plugin * lmms_plugin_main( lmms::Model * m, void * )
{
	return new lmms::ModularSynth( static_cast<lmms::InstrumentTrack *>( m ) );
}

}
