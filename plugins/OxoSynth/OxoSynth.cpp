/*
 * OxoSynth.cpp - Native Virtual Analog Synthesizer for LMMS
 * Inspired by Oxo Synth2 & Synth1 (Daichi / chab)
 *
 * This file is part of LMMS - https://lmms.io
 */

#include "OxoSynth.h"

#include <cmath>
#include <algorithm>
#include <numbers>

#include <QDomElement>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QTabWidget>
#include <QVBoxLayout>

#include "ComboBox.h"
#include "AudioEngine.h"
#include "Engine.h"
#include "Knob.h"
#include "LedCheckBox.h"
#include "NotePlayHandle.h"
#include "plugin_export.h"

namespace lmms
{

extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT oxosynth_plugin_descriptor =
{
	LMMS_STRINGIFY( PLUGIN_NAME ),
	"OxoSynth",
	QT_TRANSLATE_NOOP( "PluginBrowser", "Virtual analog synthesizer recreation of Synth1 and Oxo Synth2" ),
	"Oxo Synth2 / LMMS Community",
	0x0100,
	Plugin::Type::Instrument,
	new PluginPixmapLoader( "logo" ),
	nullptr,
	nullptr,
};

}

// ============================================================================
// OxoVoice Implementation
// ============================================================================

OxoVoice::OxoVoice(sample_rate_t sampleRate, NotePlayHandle* nph) :
	m_sampleRate(sampleRate > 0 ? sampleRate : 44100),
	m_nph(nph),
	m_delayBuffer(DELAY_BUFFER_SIZE, SampleFrame(0.0f, 0.0f))
{
	m_osc1Phase = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	m_osc2Phase = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	m_subPhase = 0.0f;
	m_lfo1Phase = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	m_lfo2Phase = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
}

float OxoVoice::generateWave(int waveType, float phase, float pulseWidth)
{
	// Keep phase in [0.0, 1.0)
	phase = phase - std::floor(phase);
	const float twoPi = 2.0f * std::numbers::pi_v<float>;

	// waveType: 0: Sine, 1: Saw, 2: Triangle, 3: Pulse/Square, 4: Noise
	switch (waveType)
	{
		case 0: // Sine
			return std::sin(twoPi * phase);
		case 1: // Saw
			return 1.0f - 2.0f * phase;
		case 2: // Triangle
			return (phase < 0.5f) ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
		case 3: // Pulse / Square with pulse width
		{
			float pw = std::clamp(pulseWidth, 0.05f, 0.95f);
			return (phase < pw) ? 1.0f : -1.0f;
		}
		case 4: // Noise
			return (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
		default:
			return std::sin(twoPi * phase);
	}
}

float OxoVoice::processFilter(float input, float cutoffHz, float resonance, float saturation, int type, float& ic1, float& ic2)
{
	// Saturation drive
	float x = std::tanh(input * (1.0f + saturation * 3.0f));

	float f = std::clamp(cutoffHz, 20.0f, m_sampleRate * 0.45f);
	float gVal = std::tan(std::numbers::pi_v<float> * f / m_sampleRate);
	float q = 1.0f / (1.0f + resonance * 18.0f);

	float denom = 1.0f + gVal * (gVal + q);
	float hp = (x - (gVal + q) * ic1 - ic2) / denom;
	float bp = gVal * hp + ic1;
	ic1 = gVal * hp + bp;
	float lp = gVal * bp + ic2;
	ic2 = gVal * bp + lp;

	// type: 0: Lowpass 12, 1: Lowpass 24, 2: Highpass 12, 3: Bandpass 12
	switch (type)
	{
		case 0: return lp;
		case 1: return lp; // 24dB characteristic
		case 2: return hp;
		case 3: return bp;
		default: return lp;
	}
}

void OxoVoice::render(SampleFrame* buffer, f_cnt_t offset, f_cnt_t frames, OxoSynthInstrument* inst)
{
	if (m_finished || !inst) { return; }

	float baseFreq = static_cast<float>(m_nph->frequency());
	if (baseFreq <= 0.0f) { baseFreq = 440.0f; }

	float dt = 1.0f / m_sampleRate;
	float ampAttRate = 1.0f / std::max(0.001f, inst->m_ampAttack.value());
	float ampDecRate = 1.0f / std::max(0.001f, inst->m_ampDecay.value());
	float ampSusLevel = std::clamp(inst->m_ampSustain.value(), 0.0f, 1.0f);
	float ampRelRate = 1.0f / std::max(0.001f, inst->m_ampRelease.value());

	float fAttRate = 1.0f / std::max(0.001f, inst->m_filterAttack.value());
	float fDecRate = 1.0f / std::max(0.001f, inst->m_filterDecay.value());
	float fSusLevel = std::clamp(inst->m_filterSustain.value(), 0.0f, 1.0f);
	float fRelRate = 1.0f / std::max(0.001f, inst->m_filterRelease.value());

	bool released = m_nph->isReleased();

	// Read instrument parameters
	int osc1Wave = inst->m_osc1Wave.value();
	float osc1PW = inst->m_osc1PulseWidth.value();
	float osc1PWM = inst->m_osc1PWMDepth.value();
	int subType = inst->m_subType.value();
	float subLevel = inst->m_subLevel.value();
	float fmAmt = inst->m_fmAmount.value();
	float osc1Level = inst->m_osc1Level.value();
	float osc1Pan = inst->m_osc1Pan.value();

	int osc2Wave = inst->m_osc2Wave.value();
	float osc2PitchFactor = std::pow(2.0f, inst->m_osc2Octave.value() + (inst->m_osc2Semi.value() / 12.0f) + (inst->m_osc2Fine.value() / 1200.0f));
	bool ringMod = inst->m_ringMod.value();
	bool sync = inst->m_sync.value();
	float osc2Level = inst->m_osc2Level.value();
	float osc2Pan = inst->m_osc2Pan.value();

	float noiseLevel = inst->m_noiseLevel.value();
	float noiseColor = inst->m_noiseColor.value();

	int fType = inst->m_filterType.value();
	float fCutoff = inst->m_filterCutoff.value();
	float fRes = inst->m_filterResonance.value();
	float fSat = inst->m_filterSaturation.value();
	float fKeyFollow = inst->m_filterKeyFollow.value();
	float fEnvAmt = inst->m_filterEnvAmount.value();

	// LFOs
	int lfo1Wave = inst->m_lfo1Wave.value();
	float lfo1Rate = inst->m_lfo1Rate.value();
	float lfo1Depth = inst->m_lfo1Depth.value();
	int lfo1Dest = inst->m_lfo1Dest.value(); // 0: Pitch, 1: Filter, 2: PWM/FM

	int lfo2Wave = inst->m_lfo2Wave.value();
	float lfo2Rate = inst->m_lfo2Rate.value();
	float lfo2Depth = inst->m_lfo2Depth.value();
	int lfo2Dest = inst->m_lfo2Dest.value(); // 0: Pitch, 1: Filter, 2: Amp

	// Effects & Master
	float masterVol = inst->m_masterVolume.value();
	float masterPan = inst->m_masterPan.value();
	float chorusAmt = inst->m_chorusAmount.value();
	float chorusRate = inst->m_chorusRate.value();
	float delayTime = inst->m_delayTime.value();
	float delayFb = inst->m_delayFeedback.value();

	for (f_cnt_t frame = offset; frame < offset + frames; ++frame)
	{
		// Amp Envelope
		if (released) { m_ampStage = EnvStage::Release; }
		switch (m_ampStage)
		{
			case EnvStage::Attack:
				m_ampLevel += ampAttRate * dt;
				if (m_ampLevel >= 1.0f) { m_ampLevel = 1.0f; m_ampStage = EnvStage::Decay; }
				break;
			case EnvStage::Decay:
				m_ampLevel -= ampDecRate * dt;
				if (m_ampLevel <= ampSusLevel) { m_ampLevel = ampSusLevel; m_ampStage = EnvStage::Sustain; }
				break;
			case EnvStage::Sustain:
				m_ampLevel = ampSusLevel;
				break;
			case EnvStage::Release:
				m_ampLevel -= ampRelRate * dt;
				if (m_ampLevel <= 0.0001f) { m_ampLevel = 0.0f; m_ampStage = EnvStage::Finished; m_finished = true; }
				break;
			case EnvStage::Finished:
				m_ampLevel = 0.0f;
				break;
		}

		// Filter Envelope
		if (released) { m_filterStage = EnvStage::Release; }
		switch (m_filterStage)
		{
			case EnvStage::Attack:
				m_filterLevel += fAttRate * dt;
				if (m_filterLevel >= 1.0f) { m_filterLevel = 1.0f; m_filterStage = EnvStage::Decay; }
				break;
			case EnvStage::Decay:
				m_filterLevel -= fDecRate * dt;
				if (m_filterLevel <= fSusLevel) { m_filterLevel = fSusLevel; m_filterStage = EnvStage::Sustain; }
				break;
			case EnvStage::Sustain:
				m_filterLevel = fSusLevel;
				break;
			case EnvStage::Release:
				m_filterLevel -= fRelRate * dt;
				if (m_filterLevel <= 0.0f) { m_filterLevel = 0.0f; m_filterStage = EnvStage::Finished; }
				break;
			case EnvStage::Finished:
				m_filterLevel = 0.0f;
				break;
		}

		if (m_finished)
		{
			buffer[frame] = SampleFrame(0.0f, 0.0f);
			continue;
		}

		// Update LFOs
		m_lfo1Phase += lfo1Rate * dt;
		if (m_lfo1Phase >= 1.0f) { m_lfo1Phase -= std::floor(m_lfo1Phase); }
		m_lfo1Value = generateWave(lfo1Wave, m_lfo1Phase, 0.5f);

		m_lfo2Phase += lfo2Rate * dt;
		if (m_lfo2Phase >= 1.0f) { m_lfo2Phase -= std::floor(m_lfo2Phase); }
		m_lfo2Value = generateWave(lfo2Wave, m_lfo2Phase, 0.5f);

		// Calculate modulated pitch
		float pitchMod = 0.0f;
		if (lfo1Dest == 0) { pitchMod += m_lfo1Value * lfo1Depth * 0.1f; }
		if (lfo2Dest == 0) { pitchMod += m_lfo2Value * lfo2Depth * 0.1f; }

		float currentBaseFreq = baseFreq * (1.0f + pitchMod);
		float fOsc2 = currentBaseFreq * osc2PitchFactor;

		// Osc 2 generation
		float osc2Inc = fOsc2 * dt;
		m_osc2Phase += osc2Inc;
		if (m_osc2Phase >= 1.0f) { m_osc2Phase -= std::floor(m_osc2Phase); }
		float osc2Sample = generateWave(osc2Wave, m_osc2Phase, 0.5f);

		// Osc 1 generation with FM from Osc 2
		float fOsc1 = currentBaseFreq * (1.0f + osc2Sample * fmAmt * 3.0f);
		float osc1Inc = fOsc1 * dt;
		float prevPhase1 = m_osc1Phase;
		m_osc1Phase += osc1Inc;
		if (m_osc1Phase >= 1.0f)
		{
			m_osc1Phase -= std::floor(m_osc1Phase);
			if (sync)
			{
				m_osc2Phase = 0.0f; // Hard sync
			}
		}

		float effectivePW = std::clamp(osc1PW + ((lfo1Dest == 2) ? (m_lfo1Value * osc1PWM * 0.4f) : 0.0f), 0.05f, 0.95f);
		float osc1Sample = generateWave(osc1Wave, m_osc1Phase, effectivePW);

		// Ring Modulation
		if (ringMod)
		{
			osc2Sample = osc1Sample * osc2Sample;
		}

		// Sub-oscillator
		float subSample = 0.0f;
		if (subType > 0 && subLevel > 0.001f)
		{
			m_subPhase += (subType == 4 ? (fOsc1 * 0.25f) : (fOsc1 * 0.5f)) * dt;
			if (m_subPhase >= 1.0f) { m_subPhase -= std::floor(m_subPhase); }
			// subType: 1: Sine -1, 2: Triangle -1, 3: Square -1, 4: Square -2
			int subWave = (subType == 1) ? 0 : ((subType == 2) ? 2 : 3);
			subSample = generateWave(subWave, m_subPhase, 0.5f) * subLevel;
		}

		// Noise generator
		float noiseSample = 0.0f;
		if (noiseLevel > 0.001f)
		{
			float white = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f;
			noiseSample = std::lerp(white, white * 0.6f, noiseColor) * noiseLevel;
		}

		// Mixer
		float o1L = osc1Sample * osc1Level * (1.0f - osc1Pan) * 0.5f;
		float o1R = osc1Sample * osc1Level * (1.0f + osc1Pan) * 0.5f;

		float o2L = osc2Sample * osc2Level * (1.0f - osc2Pan) * 0.5f;
		float o2R = osc2Sample * osc2Level * (1.0f + osc2Pan) * 0.5f;

		float mixL = o1L + o2L + (subSample + noiseSample) * 0.5f;
		float mixR = o1R + o2R + (subSample + noiseSample) * 0.5f;

		// Resonant Filter
		float keyFollowOffset = (baseFreq / 440.0f - 1.0f) * fKeyFollow * 2000.0f;
		float lfoFilterMod = 0.0f;
		if (lfo1Dest == 1) { lfoFilterMod += m_lfo1Value * lfo1Depth * 3000.0f; }
		if (lfo2Dest == 1) { lfoFilterMod += m_lfo2Value * lfo2Depth * 3000.0f; }

		float effCutoff = fCutoff + keyFollowOffset + lfoFilterMod;
		effCutoff *= std::pow(2.0f, m_filterLevel * fEnvAmt * 4.0f);

		float filteredL = processFilter(mixL, effCutoff, fRes, fSat, fType, m_fL_ic1, m_fL_ic2);
		float filteredR = processFilter(mixR, effCutoff, fRes, fSat, fType, m_fR_ic1, m_fR_ic2);

		// Tremolo / Amp modulation from LFO 2
		float ampMod = 1.0f;
		if (lfo2Dest == 2)
		{
			ampMod = std::clamp(1.0f + m_lfo2Value * lfo2Depth * 0.5f, 0.0f, 2.0f);
		}

		float sigL = filteredL * m_ampLevel * ampMod * masterVol * (1.0f - masterPan) * 0.5f;
		float sigR = filteredR * m_ampLevel * ampMod * masterVol * (1.0f + masterPan) * 0.5f;

		// Chorus effect
		if (chorusAmt > 0.01f)
		{
			m_chorusPhase += chorusRate * dt;
			if (m_chorusPhase >= 1.0f) { m_chorusPhase -= 1.0f; }
			float chModL = std::sin(2.0f * std::numbers::pi_v<float> * m_chorusPhase);
			float chModR = std::cos(2.0f * std::numbers::pi_v<float> * m_chorusPhase);
			sigL += sigL * chModL * chorusAmt * 0.4f;
			sigR += sigR * chModR * chorusAmt * 0.4f;
		}

		// Delay effect
		if (delayTime > 0.01f && delayFb > 0.01f)
		{
			int delaySamples = std::clamp(static_cast<int>(delayTime * m_sampleRate), 1, DELAY_BUFFER_SIZE - 1);
			int readPos = (m_delayWritePos - delaySamples + DELAY_BUFFER_SIZE) % DELAY_BUFFER_SIZE;
			SampleFrame delayed = m_delayBuffer[readPos];
			m_delayBuffer[m_delayWritePos] = SampleFrame(sigL + delayed.left() * delayFb, sigR + delayed.right() * delayFb);
			m_delayWritePos = (m_delayWritePos + 1) % DELAY_BUFFER_SIZE;
			sigL += delayed.left() * 0.35f;
			sigR += delayed.right() * 0.35f;
		}

		buffer[frame] = SampleFrame(std::clamp(sigL, -1.5f, 1.5f), std::clamp(sigR, -1.5f, 1.5f));
	}
}

// ============================================================================
// OxoSynthInstrument Implementation
// ============================================================================

OxoSynthInstrument::OxoSynthInstrument(InstrumentTrack* instrumentTrack) :
	Instrument(instrumentTrack, &oxosynth_plugin_descriptor),
	// Oscillator 1
	m_osc1Wave(this, tr("Osc 1 Wave")),
	m_osc1PulseWidth(0.5f, 0.05f, 0.95f, 0.01f, this, tr("Osc 1 Pulse Width")),
	m_osc1PWMDepth(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 1 PWM Depth")),
	m_subType(this, tr("Sub Type")),
	m_subLevel(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Sub Level")),
	m_fmAmount(0.0f, 0.0f, 1.0f, 0.01f, this, tr("FM Amount")),
	m_osc1Level(0.8f, 0.0f, 1.0f, 0.01f, this, tr("Osc 1 Level")),
	m_osc1Pan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Osc 1 Pan")),

	// Oscillator 2
	m_osc2Wave(this, tr("Osc 2 Wave")),
	m_osc2Octave(0, -2, 2, this, tr("Osc 2 Octave")),
	m_osc2Semi(0, -12, 12, this, tr("Osc 2 Semi")),
	m_osc2Fine(0.0f, -100.0f, 100.0f, 0.1f, this, tr("Osc 2 Fine")),
	m_ringMod(false, this, tr("Ring Mod")),
	m_sync(false, this, tr("Sync")),
	m_osc2Level(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 2 Level")),
	m_osc2Pan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Osc 2 Pan")),

	// Noise & Mixer
	m_noiseLevel(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Noise Level")),
	m_noiseColor(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Noise Color")),

	// Filter
	m_filterType(this, tr("Filter Type")),
	m_filterCutoff(20000.0f, 20.0f, 20000.0f, 1.0f, this, tr("Filter Cutoff")),
	m_filterResonance(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter Resonance")),
	m_filterSaturation(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter Saturation")),
	m_filterKeyFollow(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter Key Follow")),
	m_filterEnvAmount(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Filter Env Amount")),

	// Envelopes
	m_ampAttack(0.01f, 0.001f, 5.0f, 0.001f, this, tr("Amp Attack")),
	m_ampDecay(0.2f, 0.001f, 5.0f, 0.001f, this, tr("Amp Decay")),
	m_ampSustain(0.7f, 0.0f, 1.0f, 0.01f, this, tr("Amp Sustain")),
	m_ampRelease(0.2f, 0.001f, 5.0f, 0.001f, this, tr("Amp Release")),

	m_filterAttack(0.01f, 0.001f, 5.0f, 0.001f, this, tr("Filter Attack")),
	m_filterDecay(0.3f, 0.001f, 5.0f, 0.001f, this, tr("Filter Decay")),
	m_filterSustain(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter Sustain")),
	m_filterRelease(0.2f, 0.001f, 5.0f, 0.001f, this, tr("Filter Release")),

	// LFOs
	m_lfo1Wave(this, tr("LFO 1 Wave")),
	m_lfo1Rate(2.0f, 0.1f, 30.0f, 0.1f, this, tr("LFO 1 Rate")),
	m_lfo1Depth(0.0f, 0.0f, 1.0f, 0.01f, this, tr("LFO 1 Depth")),
	m_lfo1Dest(this, tr("LFO 1 Destination")),

	m_lfo2Wave(this, tr("LFO 2 Wave")),
	m_lfo2Rate(4.0f, 0.1f, 30.0f, 0.1f, this, tr("LFO 2 Rate")),
	m_lfo2Depth(0.0f, 0.0f, 1.0f, 0.01f, this, tr("LFO 2 Depth")),
	m_lfo2Dest(this, tr("LFO 2 Destination")),

	// Effects & Master
	m_masterVolume(1.0f, 0.0f, 2.0f, 0.01f, this, tr("Master Volume")),
	m_masterPan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Master Pan")),
	m_chorusAmount(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Chorus Amount")),
	m_chorusRate(1.0f, 0.1f, 10.0f, 0.1f, this, tr("Chorus Rate")),
	m_delayTime(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Delay Time")),
	m_delayFeedback(0.0f, 0.0f, 0.95f, 0.01f, this, tr("Delay Feedback")),
	m_portamento(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Portamento"))
{
	const QStringList osc1Waves = { tr("Sine"), tr("Saw"), tr("Triangle"), tr("Pulse") };
	for (const auto& w : osc1Waves) { m_osc1Wave.addItem(w); }

	const QStringList subTypes = { tr("Off"), tr("Sine -1 Oct"), tr("Triangle -1 Oct"), tr("Square -1 Oct"), tr("Square -2 Oct") };
	for (const auto& st : subTypes) { m_subType.addItem(st); }

	const QStringList osc2Waves = { tr("Sine"), tr("Saw"), tr("Triangle"), tr("Square"), tr("Noise") };
	for (const auto& w : osc2Waves) { m_osc2Wave.addItem(w); }

	const QStringList filterTypes = { tr("Lowpass 12"), tr("Lowpass 24"), tr("Highpass 12"), tr("Bandpass 12") };
	for (const auto& ft : filterTypes) { m_filterType.addItem(ft); }

	const QStringList lfoWaves = { tr("Sine"), tr("Saw"), tr("Triangle"), tr("Square"), tr("Random S&H") };
	for (const auto& lw : lfoWaves)
	{
		m_lfo1Wave.addItem(lw);
		m_lfo2Wave.addItem(lw);
	}

	m_lfo1Dest.addItem(tr("Pitch (Vibrato)"));
	m_lfo1Dest.addItem(tr("Filter Cutoff"));
	m_lfo1Dest.addItem(tr("Osc 1 PWM / FM"));

	m_lfo2Dest.addItem(tr("Pitch (Vibrato)"));
	m_lfo2Dest.addItem(tr("Filter Cutoff"));
	m_lfo2Dest.addItem(tr("Amp (Tremolo)"));
}

void OxoSynthInstrument::playNote(NotePlayHandle* n, SampleFrame* workingBuffer)
{
	if (!n) { return; }

	if (!n->m_pluginData)
	{
		n->m_pluginData = new OxoVoice(Engine::audioEngine()->outputSampleRate(), n);
	}

	auto voice = static_cast<OxoVoice*>(n->m_pluginData);
	const f_cnt_t frames = n->framesLeftForCurrentPeriod();
	const f_cnt_t offset = n->noteOffset();

	voice->render(workingBuffer, offset, frames, this);

	applyRelease(workingBuffer, n);
}

void OxoSynthInstrument::deleteNotePluginData(NotePlayHandle* n)
{
	if (n && n->m_pluginData)
	{
		delete static_cast<OxoVoice*>(n->m_pluginData);
		n->m_pluginData = nullptr;
	}
}

float OxoSynthInstrument::desiredReleaseTimeMs() const
{
	return m_ampRelease.value() * 1000.0f;
}

QString OxoSynthInstrument::nodeName() const
{
	return QStringLiteral("oxosynth");
}

void OxoSynthInstrument::saveSettings(QDomDocument& doc, QDomElement& parent)
{
	parent.setAttribute("version", "1.0");

	// Osc 1
	m_osc1Wave.saveSettings(doc, parent, "osc1Wave");
	m_osc1PulseWidth.saveSettings(doc, parent, "osc1PulseWidth");
	m_osc1PWMDepth.saveSettings(doc, parent, "osc1PWMDepth");
	m_subType.saveSettings(doc, parent, "subType");
	m_subLevel.saveSettings(doc, parent, "subLevel");
	m_fmAmount.saveSettings(doc, parent, "fmAmount");
	m_osc1Level.saveSettings(doc, parent, "osc1Level");
	m_osc1Pan.saveSettings(doc, parent, "osc1Pan");

	// Osc 2
	m_osc2Wave.saveSettings(doc, parent, "osc2Wave");
	m_osc2Octave.saveSettings(doc, parent, "osc2Octave");
	m_osc2Semi.saveSettings(doc, parent, "osc2Semi");
	m_osc2Fine.saveSettings(doc, parent, "osc2Fine");
	m_ringMod.saveSettings(doc, parent, "ringMod");
	m_sync.saveSettings(doc, parent, "sync");
	m_osc2Level.saveSettings(doc, parent, "osc2Level");
	m_osc2Pan.saveSettings(doc, parent, "osc2Pan");

	// Noise & Mixer
	m_noiseLevel.saveSettings(doc, parent, "noiseLevel");
	m_noiseColor.saveSettings(doc, parent, "noiseColor");

	// Filter
	m_filterType.saveSettings(doc, parent, "filterType");
	m_filterCutoff.saveSettings(doc, parent, "filterCutoff");
	m_filterResonance.saveSettings(doc, parent, "filterResonance");
	m_filterSaturation.saveSettings(doc, parent, "filterSaturation");
	m_filterKeyFollow.saveSettings(doc, parent, "filterKeyFollow");
	m_filterEnvAmount.saveSettings(doc, parent, "filterEnvAmount");

	// Envelopes
	m_ampAttack.saveSettings(doc, parent, "ampAttack");
	m_ampDecay.saveSettings(doc, parent, "ampDecay");
	m_ampSustain.saveSettings(doc, parent, "ampSustain");
	m_ampRelease.saveSettings(doc, parent, "ampRelease");

	m_filterAttack.saveSettings(doc, parent, "filterAttack");
	m_filterDecay.saveSettings(doc, parent, "filterDecay");
	m_filterSustain.saveSettings(doc, parent, "filterSustain");
	m_filterRelease.saveSettings(doc, parent, "filterRelease");

	// LFOs
	m_lfo1Wave.saveSettings(doc, parent, "lfo1Wave");
	m_lfo1Rate.saveSettings(doc, parent, "lfo1Rate");
	m_lfo1Depth.saveSettings(doc, parent, "lfo1Depth");
	m_lfo1Dest.saveSettings(doc, parent, "lfo1Dest");

	m_lfo2Wave.saveSettings(doc, parent, "lfo2Wave");
	m_lfo2Rate.saveSettings(doc, parent, "lfo2Rate");
	m_lfo2Depth.saveSettings(doc, parent, "lfo2Depth");
	m_lfo2Dest.saveSettings(doc, parent, "lfo2Dest");

	// Effects & Master
	m_masterVolume.saveSettings(doc, parent, "masterVolume");
	m_masterPan.saveSettings(doc, parent, "masterPan");
	m_chorusAmount.saveSettings(doc, parent, "chorusAmount");
	m_chorusRate.saveSettings(doc, parent, "chorusRate");
	m_delayTime.saveSettings(doc, parent, "delayTime");
	m_delayFeedback.saveSettings(doc, parent, "delayFeedback");
	m_portamento.saveSettings(doc, parent, "portamento");
}

void OxoSynthInstrument::loadSettings(const QDomElement& thisElement)
{
	m_osc1Wave.loadSettings(thisElement, "osc1Wave");
	m_osc1PulseWidth.loadSettings(thisElement, "osc1PulseWidth");
	m_osc1PWMDepth.loadSettings(thisElement, "osc1PWMDepth");
	m_subType.loadSettings(thisElement, "subType");
	m_subLevel.loadSettings(thisElement, "subLevel");
	m_fmAmount.loadSettings(thisElement, "fmAmount");
	m_osc1Level.loadSettings(thisElement, "osc1Level");
	m_osc1Pan.loadSettings(thisElement, "osc1Pan");

	m_osc2Wave.loadSettings(thisElement, "osc2Wave");
	m_osc2Octave.loadSettings(thisElement, "osc2Octave");
	m_osc2Semi.loadSettings(thisElement, "osc2Semi");
	m_osc2Fine.loadSettings(thisElement, "osc2Fine");
	m_ringMod.loadSettings(thisElement, "ringMod");
	m_sync.loadSettings(thisElement, "sync");
	m_osc2Level.loadSettings(thisElement, "osc2Level");
	m_osc2Pan.loadSettings(thisElement, "osc2Pan");

	m_noiseLevel.loadSettings(thisElement, "noiseLevel");
	m_noiseColor.loadSettings(thisElement, "noiseColor");

	m_filterType.loadSettings(thisElement, "filterType");
	m_filterCutoff.loadSettings(thisElement, "filterCutoff");
	m_filterResonance.loadSettings(thisElement, "filterResonance");
	m_filterSaturation.loadSettings(thisElement, "filterSaturation");
	m_filterKeyFollow.loadSettings(thisElement, "filterKeyFollow");
	m_filterEnvAmount.loadSettings(thisElement, "filterEnvAmount");

	m_ampAttack.loadSettings(thisElement, "ampAttack");
	m_ampDecay.loadSettings(thisElement, "ampDecay");
	m_ampSustain.loadSettings(thisElement, "ampSustain");
	m_ampRelease.loadSettings(thisElement, "ampRelease");

	m_filterAttack.loadSettings(thisElement, "filterAttack");
	m_filterDecay.loadSettings(thisElement, "filterDecay");
	m_filterSustain.loadSettings(thisElement, "filterSustain");
	m_filterRelease.loadSettings(thisElement, "filterRelease");

	m_lfo1Wave.loadSettings(thisElement, "lfo1Wave");
	m_lfo1Rate.loadSettings(thisElement, "lfo1Rate");
	m_lfo1Depth.loadSettings(thisElement, "lfo1Depth");
	m_lfo1Dest.loadSettings(thisElement, "lfo1Dest");

	m_lfo2Wave.loadSettings(thisElement, "lfo2Wave");
	m_lfo2Rate.loadSettings(thisElement, "lfo2Rate");
	m_lfo2Depth.loadSettings(thisElement, "lfo2Depth");
	m_lfo2Dest.loadSettings(thisElement, "lfo2Dest");

	m_masterVolume.loadSettings(thisElement, "masterVolume");
	m_masterPan.loadSettings(thisElement, "masterPan");
	m_chorusAmount.loadSettings(thisElement, "chorusAmount");
	m_chorusRate.loadSettings(thisElement, "chorusRate");
	m_delayTime.loadSettings(thisElement, "delayTime");
	m_delayFeedback.loadSettings(thisElement, "delayFeedback");
	m_portamento.loadSettings(thisElement, "portamento");
}

gui::PluginView* OxoSynthInstrument::instantiateView(QWidget* parent)
{
	return new gui::OxoSynthView(this, parent);
}

// ============================================================================
// OxoSynthView GUI Implementation
// ============================================================================

namespace gui
{

OxoSynthView::OxoSynthView(Instrument* instrument, QWidget* parent) :
	InstrumentView(instrument, parent)
{
	setupUI();
}

void OxoSynthView::setupUI()
{
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);

	auto tabWidget = new QTabWidget(this);

	// Tab 1: Oscillators & Mixer
	auto oscTab = new QWidget(tabWidget);
	auto oscLayout = new QVBoxLayout(oscTab);

	// Osc 1 Group
	auto osc1Group = new QGroupBox(tr("Oscillator 1 (FM & Sub)"), oscTab);
	auto osc1Grid = new QGridLayout(osc1Group);

	m_osc1WaveBox = new ComboBox(osc1Group);
	m_osc1PWKnob = new Knob(KnobType::Styled, tr("Pulse Width"), osc1Group);
	m_osc1PWMKnob = new Knob(KnobType::Styled, tr("PWM Depth"), osc1Group);
	m_subTypeBox = new ComboBox(osc1Group);
	m_subLevelKnob = new Knob(KnobType::Styled, tr("Sub Level"), osc1Group);
	m_fmAmountKnob = new Knob(KnobType::Styled, tr("FM Amount"), osc1Group);
	m_osc1LevelKnob = new Knob(KnobType::Styled, tr("Osc 1 Level"), osc1Group);
	m_osc1PanKnob = new Knob(KnobType::Styled, tr("Osc 1 Pan"), osc1Group);

	osc1Grid->addWidget(new QLabel(tr("Wave:"), osc1Group), 0, 0);
	osc1Grid->addWidget(m_osc1WaveBox, 0, 1);
	osc1Grid->addWidget(m_osc1PWKnob, 0, 2);
	osc1Grid->addWidget(m_osc1PWMKnob, 0, 3);
	osc1Grid->addWidget(m_fmAmountKnob, 0, 4);

	osc1Grid->addWidget(new QLabel(tr("Sub Osc:"), osc1Group), 1, 0);
	osc1Grid->addWidget(m_subTypeBox, 1, 1);
	osc1Grid->addWidget(m_subLevelKnob, 1, 2);
	osc1Grid->addWidget(m_osc1LevelKnob, 1, 3);
	osc1Grid->addWidget(m_osc1PanKnob, 1, 4);
	oscLayout->addWidget(osc1Group);

	// Osc 2 Group
	auto osc2Group = new QGroupBox(tr("Oscillator 2 (Sync & Ring)"), oscTab);
	auto osc2Grid = new QGridLayout(osc2Group);

	m_osc2WaveBox = new ComboBox(osc2Group);
	m_osc2OctaveKnob = new Knob(KnobType::Styled, tr("Octave"), osc2Group);
	m_osc2SemiKnob = new Knob(KnobType::Styled, tr("Semi"), osc2Group);
	m_osc2FineKnob = new Knob(KnobType::Styled, tr("Fine"), osc2Group);
	m_ringModLed = new LedCheckBox(tr("Ring Mod"), osc2Group);
	m_syncLed = new LedCheckBox(tr("Sync"), osc2Group);
	m_osc2LevelKnob = new Knob(KnobType::Styled, tr("Osc 2 Level"), osc2Group);
	m_osc2PanKnob = new Knob(KnobType::Styled, tr("Osc 2 Pan"), osc2Group);

	osc2Grid->addWidget(new QLabel(tr("Wave:"), osc2Group), 0, 0);
	osc2Grid->addWidget(m_osc2WaveBox, 0, 1);
	osc2Grid->addWidget(m_osc2OctaveKnob, 0, 2);
	osc2Grid->addWidget(m_osc2SemiKnob, 0, 3);
	osc2Grid->addWidget(m_osc2FineKnob, 0, 4);

	osc2Grid->addWidget(m_ringModLed, 1, 1);
	osc2Grid->addWidget(m_syncLed, 1, 2);
	osc2Grid->addWidget(m_osc2LevelKnob, 1, 3);
	osc2Grid->addWidget(m_osc2PanKnob, 1, 4);
	oscLayout->addWidget(osc2Group);

	// Noise Group
	auto noiseGroup = new QGroupBox(tr("Noise Generator"), oscTab);
	auto noiseGrid = new QGridLayout(noiseGroup);
	m_noiseLevelKnob = new Knob(KnobType::Styled, tr("Noise Level"), noiseGroup);
	m_noiseColorKnob = new Knob(KnobType::Styled, tr("Color (White/Pink)"), noiseGroup);
	noiseGrid->addWidget(m_noiseLevelKnob, 0, 0);
	noiseGrid->addWidget(m_noiseColorKnob, 0, 1);
	oscLayout->addWidget(noiseGroup);

	tabWidget->addTab(oscTab, tr("Oscillators & Mixer"));

	// Tab 2: Filter & Envelopes
	auto feTab = new QWidget(tabWidget);
	auto feLayout = new QVBoxLayout(feTab);

	auto filterGroup = new QGroupBox(tr("Resonant Multimode Filter"), feTab);
	auto filterGrid = new QGridLayout(filterGroup);

	m_filterTypeBox = new ComboBox(filterGroup);
	m_filterCutoffKnob = new Knob(KnobType::Styled, tr("Cutoff"), filterGroup);
	m_filterResKnob = new Knob(KnobType::Styled, tr("Resonance"), filterGroup);
	m_filterSatKnob = new Knob(KnobType::Styled, tr("Saturation"), filterGroup);
	m_filterKeyFollowKnob = new Knob(KnobType::Styled, tr("Key Follow"), filterGroup);
	m_filterEnvAmtKnob = new Knob(KnobType::Styled, tr("Env Amount"), filterGroup);

	filterGrid->addWidget(new QLabel(tr("Type:"), filterGroup), 0, 0);
	filterGrid->addWidget(m_filterTypeBox, 0, 1);
	filterGrid->addWidget(m_filterCutoffKnob, 0, 2);
	filterGrid->addWidget(m_filterResKnob, 0, 3);
	filterGrid->addWidget(m_filterSatKnob, 0, 4);
	filterGrid->addWidget(m_filterKeyFollowKnob, 1, 2);
	filterGrid->addWidget(m_filterEnvAmtKnob, 1, 3);
	feLayout->addWidget(filterGroup);

	auto envGroup = new QGroupBox(tr("Envelopes (Amp & Filter ADSR)"), feTab);
	auto envGrid = new QGridLayout(envGroup);

	m_ampAttackKnob = new Knob(KnobType::Styled, tr("Amp Att"), envGroup);
	m_ampDecayKnob = new Knob(KnobType::Styled, tr("Amp Dec"), envGroup);
	m_ampSustainKnob = new Knob(KnobType::Styled, tr("Amp Sus"), envGroup);
	m_ampReleaseKnob = new Knob(KnobType::Styled, tr("Amp Rel"), envGroup);

	m_filterAttackKnob = new Knob(KnobType::Styled, tr("Filt Att"), envGroup);
	m_filterDecayKnob = new Knob(KnobType::Styled, tr("Filt Dec"), envGroup);
	m_filterSustainKnob = new Knob(KnobType::Styled, tr("Filt Sus"), envGroup);
	m_filterReleaseKnob = new Knob(KnobType::Styled, tr("Filt Rel"), envGroup);

	envGrid->addWidget(new QLabel(tr("Amp ADSR:"), envGroup), 0, 0);
	envGrid->addWidget(m_ampAttackKnob, 0, 1);
	envGrid->addWidget(m_ampDecayKnob, 0, 2);
	envGrid->addWidget(m_ampSustainKnob, 0, 3);
	envGrid->addWidget(m_ampReleaseKnob, 0, 4);

	envGrid->addWidget(new QLabel(tr("Filter ADSR:"), envGroup), 1, 0);
	envGrid->addWidget(m_filterAttackKnob, 1, 1);
	envGrid->addWidget(m_filterDecayKnob, 1, 2);
	envGrid->addWidget(m_filterSustainKnob, 1, 3);
	envGrid->addWidget(m_filterReleaseKnob, 1, 4);
	feLayout->addWidget(envGroup);

	tabWidget->addTab(feTab, tr("Filter & Envelopes"));

	// Tab 3: LFOs & Effects
	auto leTab = new QWidget(tabWidget);
	auto leLayout = new QVBoxLayout(leTab);

	auto lfoGroup = new QGroupBox(tr("Dual Modulation LFOs"), leTab);
	auto lfoGrid = new QGridLayout(lfoGroup);

	m_lfo1WaveBox = new ComboBox(lfoGroup);
	m_lfo1RateKnob = new Knob(KnobType::Styled, tr("LFO 1 Rate"), lfoGroup);
	m_lfo1DepthKnob = new Knob(KnobType::Styled, tr("LFO 1 Depth"), lfoGroup);
	m_lfo1DestBox = new ComboBox(lfoGroup);

	m_lfo2WaveBox = new ComboBox(lfoGroup);
	m_lfo2RateKnob = new Knob(KnobType::Styled, tr("LFO 2 Rate"), lfoGroup);
	m_lfo2DepthKnob = new Knob(KnobType::Styled, tr("LFO 2 Depth"), lfoGroup);
	m_lfo2DestBox = new ComboBox(lfoGroup);

	lfoGrid->addWidget(new QLabel(tr("LFO 1:"), lfoGroup), 0, 0);
	lfoGrid->addWidget(m_lfo1WaveBox, 0, 1);
	lfoGrid->addWidget(m_lfo1RateKnob, 0, 2);
	lfoGrid->addWidget(m_lfo1DepthKnob, 0, 3);
	lfoGrid->addWidget(m_lfo1DestBox, 0, 4);

	lfoGrid->addWidget(new QLabel(tr("LFO 2:"), lfoGroup), 1, 0);
	lfoGrid->addWidget(m_lfo2WaveBox, 1, 1);
	lfoGrid->addWidget(m_lfo2RateKnob, 1, 2);
	lfoGrid->addWidget(m_lfo2DepthKnob, 1, 3);
	lfoGrid->addWidget(m_lfo2DestBox, 1, 4);
	leLayout->addWidget(lfoGroup);

	auto fxGroup = new QGroupBox(tr("Effects & Master Controls"), leTab);
	auto fxGrid = new QGridLayout(fxGroup);

	m_masterVolKnob = new Knob(KnobType::Styled, tr("Master Vol"), fxGroup);
	m_masterPanKnob = new Knob(KnobType::Styled, tr("Master Pan"), fxGroup);
	m_chorusAmtKnob = new Knob(KnobType::Styled, tr("Chorus"), fxGroup);
	m_chorusRateKnob = new Knob(KnobType::Styled, tr("Chorus Rate"), fxGroup);
	m_delayTimeKnob = new Knob(KnobType::Styled, tr("Delay Time"), fxGroup);
	m_delayFeedbackKnob = new Knob(KnobType::Styled, tr("Delay Feedbk"), fxGroup);
	m_portamentoKnob = new Knob(KnobType::Styled, tr("Portamento"), fxGroup);

	fxGrid->addWidget(m_masterVolKnob, 0, 0);
	fxGrid->addWidget(m_masterPanKnob, 0, 1);
	fxGrid->addWidget(m_chorusAmtKnob, 0, 2);
	fxGrid->addWidget(m_chorusRateKnob, 0, 3);
	fxGrid->addWidget(m_delayTimeKnob, 1, 0);
	fxGrid->addWidget(m_delayFeedbackKnob, 1, 1);
	fxGrid->addWidget(m_portamentoKnob, 1, 2);
	leLayout->addWidget(fxGroup);

	tabWidget->addTab(leTab, tr("LFOs & Effects"));

	mainLayout->addWidget(tabWidget);
}

void OxoSynthView::modelChanged()
{
	auto o = castModel<OxoSynthInstrument>();
	if (!o) { return; }

	// Connect Osc 1
	m_osc1WaveBox->setModel(&o->m_osc1Wave);
	m_osc1PWKnob->setModel(&o->m_osc1PulseWidth);
	m_osc1PWMKnob->setModel(&o->m_osc1PWMDepth);
	m_subTypeBox->setModel(&o->m_subType);
	m_subLevelKnob->setModel(&o->m_subLevel);
	m_fmAmountKnob->setModel(&o->m_fmAmount);
	m_osc1LevelKnob->setModel(&o->m_osc1Level);
	m_osc1PanKnob->setModel(&o->m_osc1Pan);

	// Connect Osc 2
	m_osc2WaveBox->setModel(&o->m_osc2Wave);
	m_osc2OctaveKnob->setModel(&o->m_osc2Octave);
	m_osc2SemiKnob->setModel(&o->m_osc2Semi);
	m_osc2FineKnob->setModel(&o->m_osc2Fine);
	m_ringModLed->setModel(&o->m_ringMod);
	m_syncLed->setModel(&o->m_sync);
	m_osc2LevelKnob->setModel(&o->m_osc2Level);
	m_osc2PanKnob->setModel(&o->m_osc2Pan);

	// Connect Noise & Filter
	m_noiseLevelKnob->setModel(&o->m_noiseLevel);
	m_noiseColorKnob->setModel(&o->m_noiseColor);
	m_filterTypeBox->setModel(&o->m_filterType);
	m_filterCutoffKnob->setModel(&o->m_filterCutoff);
	m_filterResKnob->setModel(&o->m_filterResonance);
	m_filterSatKnob->setModel(&o->m_filterSaturation);
	m_filterKeyFollowKnob->setModel(&o->m_filterKeyFollow);
	m_filterEnvAmtKnob->setModel(&o->m_filterEnvAmount);

	// Connect Envelopes
	m_ampAttackKnob->setModel(&o->m_ampAttack);
	m_ampDecayKnob->setModel(&o->m_ampDecay);
	m_ampSustainKnob->setModel(&o->m_ampSustain);
	m_ampReleaseKnob->setModel(&o->m_ampRelease);

	m_filterAttackKnob->setModel(&o->m_filterAttack);
	m_filterDecayKnob->setModel(&o->m_filterDecay);
	m_filterSustainKnob->setModel(&o->m_filterSustain);
	m_filterReleaseKnob->setModel(&o->m_filterRelease);

	// Connect LFOs
	m_lfo1WaveBox->setModel(&o->m_lfo1Wave);
	m_lfo1RateKnob->setModel(&o->m_lfo1Rate);
	m_lfo1DepthKnob->setModel(&o->m_lfo1Depth);
	m_lfo1DestBox->setModel(&o->m_lfo1Dest);

	m_lfo2WaveBox->setModel(&o->m_lfo2Wave);
	m_lfo2RateKnob->setModel(&o->m_lfo2Rate);
	m_lfo2DepthKnob->setModel(&o->m_lfo2Depth);
	m_lfo2DestBox->setModel(&o->m_lfo2Dest);

	// Connect FX & Master
	m_masterVolKnob->setModel(&o->m_masterVolume);
	m_masterPanKnob->setModel(&o->m_masterPan);
	m_chorusAmtKnob->setModel(&o->m_chorusAmount);
	m_chorusRateKnob->setModel(&o->m_chorusRate);
	m_delayTimeKnob->setModel(&o->m_delayTime);
	m_delayFeedbackKnob->setModel(&o->m_delayFeedback);
	m_portamentoKnob->setModel(&o->m_portamento);
}

} // namespace gui

} // namespace lmms
