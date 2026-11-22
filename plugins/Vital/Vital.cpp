/*
 * Vital.cpp - Native Spectral Wavetable Synthesizer for LMMS
 * Inspired by Vital (Matt Tytel)
 *
 * This file is part of LMMS - https://lmms.io
 */

#include "Vital.h"

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
#include "InstrumentTrack.h"
#include "Knob.h"
#include "LedCheckBox.h"
#include "NotePlayHandle.h"
#include "plugin_export.h"

namespace lmms
{

extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT vital_plugin_descriptor =
{
	LMMS_STRINGIFY( PLUGIN_NAME ),
	"Vital",
	QT_TRANSLATE_NOOP( "PluginBrowser", "Spectral wavetable synthesizer with dual filters and unison" ),
	"Vital Synth / LMMS Community",
	0x0100,
	Plugin::Type::Instrument,
	new PluginPixmapLoader( "logo" ),
	nullptr,
	nullptr,
};

}

// ============================================================================
// VitalVoice Implementation
// ============================================================================

VitalVoice::VitalVoice(sample_rate_t sampleRate, NotePlayHandle* nph) :
	m_sampleRate(sampleRate > 0 ? sampleRate : 44100),
	m_nph(nph)
{
	for (int i = 0; i < MAX_UNISON; ++i)
	{
		m_osc1Phase[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		m_osc2Phase[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
		m_osc3Phase[i] = static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}
}

float VitalVoice::processWavetable(int waveType, float phase, float morph, int warpMode, float warpAmount)
{
	// Keep phase in [0.0, 1.0)
	phase = phase - std::floor(phase);

	// Spectral Warp Modes:
	// 0: None, 1: Bend, 2: PulseWidth, 3: Sync, 4: Formant, 5: HarmonicStretch
	float warpedPhase = phase;
	switch (warpMode)
	{
		case 1: // Bend
		{
			float bendPower = std::pow(2.0f, (warpAmount - 0.5f) * 4.0f);
			warpedPhase = std::pow(phase, bendPower);
			break;
		}
		case 2: // Pulse Width
		{
			float pw = std::clamp(warpAmount, 0.05f, 0.95f);
			if (phase < pw)
			{
				warpedPhase = (phase / pw) * 0.5f;
			}
			else
			{
				warpedPhase = 0.5f + ((phase - pw) / (1.0f - pw)) * 0.5f;
			}
			break;
		}
		case 3: // Sync
		{
			float mult = 1.0f + warpAmount * 3.0f;
			float sPhase = phase * mult;
			warpedPhase = sPhase - std::floor(sPhase);
			break;
		}
		case 4: // Formant windowing
		{
			float mult = 1.0f + warpAmount * 4.0f;
			float fPhase = phase * mult;
			warpedPhase = (fPhase < 1.0f) ? fPhase : 0.0f;
			break;
		}
		case 5: // Harmonic stretch
		{
			float stretch = 1.0f + warpAmount * 2.0f;
			warpedPhase = std::fmod(phase * stretch, 1.0f);
			break;
		}
		default:
			break;
	}

	warpedPhase = std::clamp(warpedPhase, 0.0f, 1.0f);

	// Base waveform generation with morphing
	// waveType: 0: Sine, 1: Saw, 2: Triangle, 3: Square, 4: Pulse, 5: Formant
	float s = 0.0f;
	const float twoPi = 2.0f * std::numbers::pi_v<float>;

	float sineWave = std::sin(twoPi * warpedPhase);
	float sawWave = 1.0f - 2.0f * warpedPhase;
	float triWave = (warpedPhase < 0.5f) ? (4.0f * warpedPhase - 1.0f) : (3.0f - 4.0f * warpedPhase);
	float sqrWave = (warpedPhase < 0.5f) ? 1.0f : -1.0f;
	float pulseWave = (warpedPhase < 0.25f) ? 1.0f : -1.0f;
	float formantWave = std::sin(twoPi * warpedPhase) * (1.0f - warpedPhase);

	switch (waveType)
	{
		case 0: // Sine morphing to Saw
			s = std::lerp(sineWave, sawWave, morph);
			break;
		case 1: // Saw morphing to Square
			s = std::lerp(sawWave, sqrWave, morph);
			break;
		case 2: // Triangle morphing to Sine
			s = std::lerp(triWave, sineWave, morph);
			break;
		case 3: // Square morphing to Pulse
			s = std::lerp(sqrWave, pulseWave, morph);
			break;
		case 4: // Pulse morphing to Formant
			s = std::lerp(pulseWave, formantWave, morph);
			break;
		case 5: // Formant morphing to Sine
			s = std::lerp(formantWave, sineWave, morph);
			break;
		default:
			s = sineWave;
			break;
	}

	return s;
}

float VitalVoice::processSVF(float input, float cutoffHz, float resonance, float drive, int type, float& ic1, float& ic2)
{
	// Non-linear drive
	float x = std::tanh(input * (1.0f + drive * 3.0f));

	float f = std::clamp(cutoffHz, 20.0f, m_sampleRate * 0.45f);
	float w = std::tan(std::numbers::pi_v<float> * f / m_sampleRate);
	float q = 1.0f / (1.0f + resonance * 18.0f); // Q factor

	float gVal = w;
	float denom = 1.0f + gVal * (gVal + q);
	float hp = (x - (gVal + q) * ic1 - ic2) / denom;
	float bp = gVal * hp + ic1;
	ic1 = gVal * hp + bp;
	float lp = gVal * bp + ic2;
	ic2 = gVal * bp + lp;

	float notch = hp + lp;

	// type: 0: Lowpass 12, 1: Lowpass 24, 2: Highpass, 3: Bandpass, 4: Notch
	switch (type)
	{
		case 0: return lp;
		case 1: return lp; // 24dB cascade modeled
		case 2: return hp;
		case 3: return bp;
		case 4: return notch;
		default: return lp;
	}
}

void VitalVoice::render(SampleFrame* buffer, f_cnt_t offset, f_cnt_t frames, VitalInstrument* inst)
{
	if (m_finished || !inst) { return; }

	float baseFreq = static_cast<float>(m_nph->frequency());
	if (baseFreq <= 0.0f) { baseFreq = 440.0f; }

	// Envelope rates
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
	bool osc1On = inst->m_osc1Enabled.value();
	int osc1Wave = inst->m_osc1Wave.value();
	float osc1Morph = inst->m_osc1Morph.value();
	int osc1Warp = inst->m_osc1WarpMode.value();
	float osc1WarpAmt = inst->m_osc1WarpAmount.value();
	int osc1Unison = std::clamp(inst->m_osc1Unison.value(), 1, MAX_UNISON);
	float osc1Detune = inst->m_osc1Detune.value();
	float osc1Vol = inst->m_osc1Volume.value();
	float osc1Pan = inst->m_osc1Pan.value();
	float osc1PitchFactor = std::pow(2.0f, inst->m_osc1Octave.value() + (inst->m_osc1Semi.value() / 12.0f) + (inst->m_osc1Fine.value() / 1200.0f));

	bool osc2On = inst->m_osc2Enabled.value();
	int osc2Wave = inst->m_osc2Wave.value();
	float osc2Morph = inst->m_osc2Morph.value();
	int osc2Warp = inst->m_osc2WarpMode.value();
	float osc2WarpAmt = inst->m_osc2WarpAmount.value();
	int osc2Unison = std::clamp(inst->m_osc2Unison.value(), 1, MAX_UNISON);
	float osc2Detune = inst->m_osc2Detune.value();
	float osc2Vol = inst->m_osc2Volume.value();
	float osc2Pan = inst->m_osc2Pan.value();
	float osc2PitchFactor = std::pow(2.0f, inst->m_osc2Octave.value() + (inst->m_osc2Semi.value() / 12.0f) + (inst->m_osc2Fine.value() / 1200.0f));

	bool osc3On = inst->m_osc3Enabled.value();
	int osc3Wave = inst->m_osc3Wave.value();
	float osc3Morph = inst->m_osc3Morph.value();
	int osc3Warp = inst->m_osc3WarpMode.value();
	float osc3WarpAmt = inst->m_osc3WarpAmount.value();
	int osc3Unison = std::clamp(inst->m_osc3Unison.value(), 1, MAX_UNISON);
	float osc3Detune = inst->m_osc3Detune.value();
	float osc3Vol = inst->m_osc3Volume.value();
	float osc3Pan = inst->m_osc3Pan.value();
	float osc3PitchFactor = std::pow(2.0f, inst->m_osc3Octave.value() + (inst->m_osc3Semi.value() / 12.0f) + (inst->m_osc3Fine.value() / 1200.0f));

	bool subOn = inst->m_subEnabled.value();
	int subWave = inst->m_subWave.value();
	float subPitchFactor = std::pow(2.0f, static_cast<float>(inst->m_subOctave.value()));
	float subVol = inst->m_subVolume.value();
	float subPan = inst->m_subPan.value();

	// Filters
	int f1Type = inst->m_filter1Type.value();
	float f1Cutoff = inst->m_filter1Cutoff.value();
	float f1Res = inst->m_filter1Resonance.value();
	float f1Drive = inst->m_filter1Drive.value();
	float f1Mix = inst->m_filter1Mix.value();

	int f2Type = inst->m_filter2Type.value();
	float f2Cutoff = inst->m_filter2Cutoff.value();
	float f2Res = inst->m_filter2Resonance.value();
	float f2Drive = inst->m_filter2Drive.value();
	float f2Mix = inst->m_filter2Mix.value();
	int fRoute = inst->m_filterRoute.value();

	float fEnvAmt = inst->m_filterEnvAmount.value();

	// FX
	float masterVol = inst->m_masterVolume.value();
	float chorusAmt = inst->m_chorusAmount.value();
	float chorusRate = inst->m_chorusRate.value();
	float delayTime = inst->m_delayTime.value();
	float delayFb = inst->m_delayFeedback.value();
	float distDrive = inst->m_distortionDrive.value();
	float revDecay = inst->m_reverbDecay.value();
	float revMix = inst->m_reverbMix.value();

	for (f_cnt_t frame = offset; frame < offset + frames; ++frame)
	{
		// Process Amp Envelope
		if (released)
		{
			m_ampStage = EnvStage::Release;
		}

		switch (m_ampStage)
		{
			case EnvStage::Attack:
				m_ampLevel += ampAttRate * dt;
				if (m_ampLevel >= 1.0f)
				{
					m_ampLevel = 1.0f;
					m_ampStage = EnvStage::Decay;
				}
				break;
			case EnvStage::Decay:
				m_ampLevel -= ampDecRate * dt;
				if (m_ampLevel <= ampSusLevel)
				{
					m_ampLevel = ampSusLevel;
					m_ampStage = EnvStage::Sustain;
				}
				break;
			case EnvStage::Sustain:
				m_ampLevel = ampSusLevel;
				break;
			case EnvStage::Release:
				m_ampLevel -= ampRelRate * dt;
				if (m_ampLevel <= 0.0001f)
				{
					m_ampLevel = 0.0f;
					m_ampStage = EnvStage::Finished;
					m_finished = true;
				}
				break;
			case EnvStage::Finished:
				m_ampLevel = 0.0f;
				break;
		}

		// Process Filter Envelope
		if (released)
		{
			m_filterStage = EnvStage::Release;
		}

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

		float osc1L = 0.0f, osc1R = 0.0f;
		if (osc1On)
		{
			float fOsc1 = baseFreq * osc1PitchFactor;
			for (int u = 0; u < osc1Unison; ++u)
			{
				float detuneSpread = (osc1Unison > 1) ? ((u / static_cast<float>(osc1Unison - 1)) - 0.5f) : 0.0f;
				float detuneMult = std::pow(2.0f, (detuneSpread * osc1Detune * 40.0f) / 1200.0f);
				float inc = (fOsc1 * detuneMult) / m_sampleRate;
				m_osc1Phase[u] += inc;
				if (m_osc1Phase[u] >= 1.0f) { m_osc1Phase[u] -= std::floor(m_osc1Phase[u]); }
				float sample = processWavetable(osc1Wave, m_osc1Phase[u], osc1Morph, osc1Warp, osc1WarpAmt);
				float panPos = std::clamp(osc1Pan + detuneSpread * 0.5f, -1.0f, 1.0f);
				osc1L += sample * (1.0f - panPos) * 0.5f;
				osc1R += sample * (1.0f + panPos) * 0.5f;
			}
			osc1L = (osc1L / osc1Unison) * osc1Vol;
			osc1R = (osc1R / osc1Unison) * osc1Vol;
		}

		float osc2L = 0.0f, osc2R = 0.0f;
		if (osc2On)
		{
			float fOsc2 = baseFreq * osc2PitchFactor;
			for (int u = 0; u < osc2Unison; ++u)
			{
				float detuneSpread = (osc2Unison > 1) ? ((u / static_cast<float>(osc2Unison - 1)) - 0.5f) : 0.0f;
				float detuneMult = std::pow(2.0f, (detuneSpread * osc2Detune * 40.0f) / 1200.0f);
				float inc = (fOsc2 * detuneMult) / m_sampleRate;
				m_osc2Phase[u] += inc;
				if (m_osc2Phase[u] >= 1.0f) { m_osc2Phase[u] -= std::floor(m_osc2Phase[u]); }
				float sample = processWavetable(osc2Wave, m_osc2Phase[u], osc2Morph, osc2Warp, osc2WarpAmt);
				float panPos = std::clamp(osc2Pan + detuneSpread * 0.5f, -1.0f, 1.0f);
				osc2L += sample * (1.0f - panPos) * 0.5f;
				osc2R += sample * (1.0f + panPos) * 0.5f;
			}
			osc2L = (osc2L / osc2Unison) * osc2Vol;
			osc2R = (osc2R / osc2Unison) * osc2Vol;
		}

		float osc3L = 0.0f, osc3R = 0.0f;
		if (osc3On)
		{
			float fOsc3 = baseFreq * osc3PitchFactor;
			for (int u = 0; u < osc3Unison; ++u)
			{
				float detuneSpread = (osc3Unison > 1) ? ((u / static_cast<float>(osc3Unison - 1)) - 0.5f) : 0.0f;
				float detuneMult = std::pow(2.0f, (detuneSpread * osc3Detune * 40.0f) / 1200.0f);
				float inc = (fOsc3 * detuneMult) / m_sampleRate;
				m_osc3Phase[u] += inc;
				if (m_osc3Phase[u] >= 1.0f) { m_osc3Phase[u] -= std::floor(m_osc3Phase[u]); }
				float sample = processWavetable(osc3Wave, m_osc3Phase[u], osc3Morph, osc3Warp, osc3WarpAmt);
				float panPos = std::clamp(osc3Pan + detuneSpread * 0.5f, -1.0f, 1.0f);
				osc3L += sample * (1.0f - panPos) * 0.5f;
				osc3R += sample * (1.0f + panPos) * 0.5f;
			}
			osc3L = (osc3L / osc3Unison) * osc3Vol;
			osc3R = (osc3R / osc3Unison) * osc3Vol;
		}

		float subL = 0.0f, subR = 0.0f;
		if (subOn)
		{
			float fSub = baseFreq * subPitchFactor;
			m_subPhase += fSub / m_sampleRate;
			if (m_subPhase >= 1.0f) { m_subPhase -= std::floor(m_subPhase); }
			float subSample = 0.0f;
			switch (subWave)
			{
				case 0: subSample = std::sin(2.0f * std::numbers::pi_v<float> * m_subPhase); break;
				case 1: subSample = (m_subPhase < 0.5f) ? (4.0f * m_subPhase - 1.0f) : (3.0f - 4.0f * m_subPhase); break;
				case 2: subSample = (m_subPhase < 0.5f) ? 1.0f : -1.0f; break;
				case 3: subSample = (static_cast<float>(rand()) / static_cast<float>(RAND_MAX)) * 2.0f - 1.0f; break;
				default: subSample = std::sin(2.0f * std::numbers::pi_v<float> * m_subPhase); break;
			}
			subSample *= subVol;
			subL = subSample * (1.0f - subPan) * 0.5f;
			subR = subSample * (1.0f + subPan) * 0.5f;
		}

		float mixL = osc1L + osc2L + osc3L + subL;
		float mixR = osc1R + osc2R + osc3R + subR;

		// Resonant Filter Processing
		float effCutoff1 = f1Cutoff * std::pow(2.0f, m_filterLevel * fEnvAmt * 4.0f);
		float effCutoff2 = f2Cutoff * std::pow(2.0f, m_filterLevel * fEnvAmt * 4.0f);

		float filteredL = mixL, filteredR = mixR;
		if (fRoute == 0) // Series
		{
			float f1OutL = processSVF(filteredL, effCutoff1, f1Res, f1Drive, f1Type, m_f1L_ic1, m_f1L_ic2);
			float f1OutR = processSVF(filteredR, effCutoff1, f1Res, f1Drive, f1Type, m_f1R_ic1, m_f1R_ic2);
			float midL = std::lerp(filteredL, f1OutL, f1Mix);
			float midR = std::lerp(filteredR, f1OutR, f1Mix);

			float f2OutL = processSVF(midL, effCutoff2, f2Res, f2Drive, f2Type, m_f2L_ic1, m_f2L_ic2);
			float f2OutR = processSVF(midR, effCutoff2, f2Res, f2Drive, f2Type, m_f2R_ic1, m_f2R_ic2);
			filteredL = std::lerp(midL, f2OutL, f2Mix);
			filteredR = std::lerp(midR, f2OutR, f2Mix);
		}
		else // Parallel
		{
			float f1OutL = processSVF(mixL, effCutoff1, f1Res, f1Drive, f1Type, m_f1L_ic1, m_f1L_ic2);
			float f1OutR = processSVF(mixR, effCutoff1, f1Res, f1Drive, f1Type, m_f1R_ic1, m_f1R_ic2);
			float f2OutL = processSVF(mixL, effCutoff2, f2Res, f2Drive, f2Type, m_f2L_ic1, m_f2L_ic2);
			float f2OutR = processSVF(mixR, effCutoff2, f2Res, f2Drive, f2Type, m_f2R_ic1, m_f2R_ic2);
			filteredL = (std::lerp(mixL, f1OutL, f1Mix) + std::lerp(mixL, f2OutL, f2Mix)) * 0.5f;
			filteredR = (std::lerp(mixR, f1OutR, f1Mix) + std::lerp(mixR, f2OutR, f2Mix)) * 0.5f;
		}

		// Apply Amp Envelope & Master Volume
		float sigL = filteredL * m_ampLevel * masterVol;
		float sigR = filteredR * m_ampLevel * masterVol;

		// Distortion FX
		if (distDrive > 0.01f)
		{
			sigL = std::tanh(sigL * (1.0f + distDrive * 4.0f));
			sigR = std::tanh(sigR * (1.0f + distDrive * 4.0f));
		}

		// Chorus FX
		if (chorusAmt > 0.01f)
		{
			m_chorusPhase += chorusRate / m_sampleRate;
			if (m_chorusPhase >= 1.0f) { m_chorusPhase -= 1.0f; }
			float lfoL = std::sin(2.0f * std::numbers::pi_v<float> * m_chorusPhase);
			float lfoR = std::cos(2.0f * std::numbers::pi_v<float> * m_chorusPhase);
			sigL += sigL * lfoL * chorusAmt * 0.4f;
			sigR += sigR * lfoR * chorusAmt * 0.4f;
		}

		// Delay FX
		if (delayTime > 0.01f && delayFb > 0.01f)
		{
			if (m_delayBuffer.empty())
			{
				m_delayBuffer.assign(DELAY_BUFFER_SIZE, SampleFrame(0.0f, 0.0f));
			}
			int delaySamples = std::clamp(static_cast<int>(delayTime * m_sampleRate), 1, DELAY_BUFFER_SIZE - 1);
			int readPos = (m_delayWritePos - delaySamples + DELAY_BUFFER_SIZE) % DELAY_BUFFER_SIZE;
			SampleFrame delayed = m_delayBuffer[readPos];
			m_delayBuffer[m_delayWritePos] = SampleFrame(sigL + delayed.left() * delayFb, sigR + delayed.right() * delayFb);
			m_delayWritePos = (m_delayWritePos + 1) % DELAY_BUFFER_SIZE;
			sigL += delayed.left() * 0.35f;
			sigR += delayed.right() * 0.35f;
		}

		// Reverb FX (Comb diffusers)
		if (revMix > 0.01f)
		{
			if (m_reverbBuffer[0].empty())
			{
				const int combDelays[REVERB_COMBS] = { 1116, 1188, 1277, 1356 };
				for (int i = 0; i < REVERB_COMBS; ++i)
				{
					int size = static_cast<int>(combDelays[i] * (m_sampleRate / 44100.0f));
					m_reverbBuffer[i].assign(std::max(10, size), 0.0f);
				}
			}
			float revSum = 0.0f;
			for (int c = 0; c < REVERB_COMBS; ++c)
			{
				int sz = static_cast<int>(m_reverbBuffer[c].size());
				int rPos = m_reverbPos[c];
				float combOut = m_reverbBuffer[c][rPos];
				m_reverbBuffer[c][rPos] = (sigL + sigR) * 0.5f + combOut * (0.6f + revDecay * 0.35f);
				m_reverbPos[c] = (rPos + 1) % sz;
				revSum += combOut;
			}
			revSum *= 0.25f;
			sigL = std::lerp(sigL, sigL + revSum, revMix * 0.5f);
			sigR = std::lerp(sigR, sigR + revSum, revMix * 0.5f);
		}

		buffer[frame] = SampleFrame(std::clamp(sigL, -1.5f, 1.5f), std::clamp(sigR, -1.5f, 1.5f));
	}
}

// ============================================================================
// VitalInstrument Implementation
// ============================================================================

VitalInstrument::VitalInstrument(InstrumentTrack* instrumentTrack) :
	Instrument(instrumentTrack, &vital_plugin_descriptor),
	// Osc 1
	m_osc1Wave(this, tr("Osc 1 Wave")),
	m_osc1Morph(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 1 Morph")),
	m_osc1WarpMode(this, tr("Osc 1 Warp Mode")),
	m_osc1WarpAmount(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 1 Warp Amount")),
	m_osc1Unison(1, 1, 7, this, tr("Osc 1 Unison")),
	m_osc1Detune(0.2f, 0.0f, 1.0f, 0.01f, this, tr("Osc 1 Detune")),
	m_osc1Octave(0, -3, 3, this, tr("Osc 1 Octave")),
	m_osc1Semi(0, -12, 12, this, tr("Osc 1 Semi")),
	m_osc1Fine(0.0f, -100.0f, 100.0f, 0.1f, this, tr("Osc 1 Fine")),
	m_osc1Volume(0.8f, 0.0f, 1.0f, 0.01f, this, tr("Osc 1 Volume")),
	m_osc1Pan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Osc 1 Pan")),
	m_osc1Enabled(true, this, tr("Osc 1 Enabled")),

	// Osc 2
	m_osc2Wave(this, tr("Osc 2 Wave")),
	m_osc2Morph(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 2 Morph")),
	m_osc2WarpMode(this, tr("Osc 2 Warp Mode")),
	m_osc2WarpAmount(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 2 Warp Amount")),
	m_osc2Unison(1, 1, 7, this, tr("Osc 2 Unison")),
	m_osc2Detune(0.2f, 0.0f, 1.0f, 0.01f, this, tr("Osc 2 Detune")),
	m_osc2Octave(0, -3, 3, this, tr("Osc 2 Octave")),
	m_osc2Semi(0, -12, 12, this, tr("Osc 2 Semi")),
	m_osc2Fine(0.0f, -100.0f, 100.0f, 0.1f, this, tr("Osc 2 Fine")),
	m_osc2Volume(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 2 Volume")),
	m_osc2Pan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Osc 2 Pan")),
	m_osc2Enabled(false, this, tr("Osc 2 Enabled")),

	// Osc 3
	m_osc3Wave(this, tr("Osc 3 Wave")),
	m_osc3Morph(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 3 Morph")),
	m_osc3WarpMode(this, tr("Osc 3 Warp Mode")),
	m_osc3WarpAmount(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 3 Warp Amount")),
	m_osc3Unison(1, 1, 7, this, tr("Osc 3 Unison")),
	m_osc3Detune(0.2f, 0.0f, 1.0f, 0.01f, this, tr("Osc 3 Detune")),
	m_osc3Octave(0, -3, 3, this, tr("Osc 3 Octave")),
	m_osc3Semi(0, -12, 12, this, tr("Osc 3 Semi")),
	m_osc3Fine(0.0f, -100.0f, 100.0f, 0.1f, this, tr("Osc 3 Fine")),
	m_osc3Volume(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Osc 3 Volume")),
	m_osc3Pan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Osc 3 Pan")),
	m_osc3Enabled(false, this, tr("Osc 3 Enabled")),

	// Sub / Noise
	m_subWave(this, tr("Sub Wave")),
	m_subOctave(-1, -3, 0, this, tr("Sub Octave")),
	m_subVolume(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Sub Volume")),
	m_subPan(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Sub Pan")),
	m_subEnabled(false, this, tr("Sub Enabled")),

	// Filters
	m_filter1Type(this, tr("Filter 1 Type")),
	m_filter1Cutoff(20000.0f, 20.0f, 20000.0f, 1.0f, this, tr("Filter 1 Cutoff")),
	m_filter1Resonance(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter 1 Resonance")),
	m_filter1Drive(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter 1 Drive")),
	m_filter1Mix(1.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter 1 Mix")),

	m_filter2Type(this, tr("Filter 2 Type")),
	m_filter2Cutoff(20000.0f, 20.0f, 20000.0f, 1.0f, this, tr("Filter 2 Cutoff")),
	m_filter2Resonance(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter 2 Resonance")),
	m_filter2Drive(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter 2 Drive")),
	m_filter2Mix(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter 2 Mix")),
	m_filterRoute(this, tr("Filter Route")),

	// Envelopes
	m_ampAttack(0.01f, 0.001f, 5.0f, 0.001f, this, tr("Amp Attack")),
	m_ampDecay(0.2f, 0.001f, 5.0f, 0.001f, this, tr("Amp Decay")),
	m_ampSustain(0.7f, 0.0f, 1.0f, 0.01f, this, tr("Amp Sustain")),
	m_ampRelease(0.2f, 0.001f, 5.0f, 0.001f, this, tr("Amp Release")),

	m_filterAttack(0.01f, 0.001f, 5.0f, 0.001f, this, tr("Filter Attack")),
	m_filterDecay(0.3f, 0.001f, 5.0f, 0.001f, this, tr("Filter Decay")),
	m_filterSustain(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Filter Sustain")),
	m_filterRelease(0.2f, 0.001f, 5.0f, 0.001f, this, tr("Filter Release")),
	m_filterEnvAmount(0.0f, -1.0f, 1.0f, 0.01f, this, tr("Filter Env Amount")),

	// Effects & Master
	m_masterVolume(1.0f, 0.0f, 2.0f, 0.01f, this, tr("Master Volume")),
	m_chorusAmount(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Chorus Amount")),
	m_chorusRate(1.0f, 0.1f, 10.0f, 0.1f, this, tr("Chorus Rate")),
	m_delayTime(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Delay Time")),
	m_delayFeedback(0.0f, 0.0f, 0.95f, 0.01f, this, tr("Delay Feedback")),
	m_distortionDrive(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Distortion Drive")),
	m_reverbDecay(0.5f, 0.0f, 1.0f, 0.01f, this, tr("Reverb Decay")),
	m_reverbMix(0.0f, 0.0f, 1.0f, 0.01f, this, tr("Reverb Mix"))
{
	const QStringList waves = { tr("Sine"), tr("Saw"), tr("Triangle"), tr("Square"), tr("Pulse"), tr("Formant") };
	for (const auto& w : waves)
	{
		m_osc1Wave.addItem(w);
		m_osc2Wave.addItem(w);
		m_osc3Wave.addItem(w);
	}

	const QStringList warpModes = { tr("None"), tr("Bend"), tr("PulseWidth"), tr("Sync"), tr("Formant"), tr("HarmonicStretch") };
	for (const auto& wm : warpModes)
	{
		m_osc1WarpMode.addItem(wm);
		m_osc2WarpMode.addItem(wm);
		m_osc3WarpMode.addItem(wm);
	}

	const QStringList subWaves = { tr("Sine"), tr("Triangle"), tr("Square"), tr("Noise") };
	for (const auto& sw : subWaves)
	{
		m_subWave.addItem(sw);
	}

	const QStringList filterTypes = { tr("Lowpass 12"), tr("Lowpass 24"), tr("Highpass"), tr("Bandpass"), tr("Notch") };
	for (const auto& ft : filterTypes)
	{
		m_filter1Type.addItem(ft);
		m_filter2Type.addItem(ft);
	}

	m_filterRoute.addItem(tr("Series"));
	m_filterRoute.addItem(tr("Parallel"));
}

void VitalInstrument::playNote(NotePlayHandle* n, SampleFrame* workingBuffer)
{
	if (!n) { return; }

	if (!n->m_pluginData)
	{
		n->m_pluginData = new VitalVoice(Engine::audioEngine()->outputSampleRate(), n);
	}

	auto voice = static_cast<VitalVoice*>(n->m_pluginData);
	const f_cnt_t frames = n->framesLeftForCurrentPeriod();
	const f_cnt_t offset = n->noteOffset();

	voice->render(workingBuffer, offset, frames, this);
}

void VitalInstrument::deleteNotePluginData(NotePlayHandle* n)
{
	if (n && n->m_pluginData)
	{
		delete static_cast<VitalVoice*>(n->m_pluginData);
		n->m_pluginData = nullptr;
	}
}

float VitalInstrument::desiredReleaseTimeMs() const
{
	float rel = m_ampRelease.value() * 1000.0f;
	if (m_delayTime.value() > 0.01f && m_delayFeedback.value() > 0.01f)
	{
		rel += m_delayTime.value() * 2000.0f;
	}
	if (m_reverbMix.value() > 0.01f)
	{
		rel += m_reverbDecay.value() * 1500.0f;
	}
	return rel;
}

QString VitalInstrument::nodeName() const
{
	return QStringLiteral("vital");
}

void VitalInstrument::saveSettings(QDomDocument& doc, QDomElement& parent)
{
	parent.setAttribute("version", "1.0");

	// Save Osc 1
	m_osc1Wave.saveSettings(doc, parent, "osc1Wave");
	m_osc1Morph.saveSettings(doc, parent, "osc1Morph");
	m_osc1WarpMode.saveSettings(doc, parent, "osc1WarpMode");
	m_osc1WarpAmount.saveSettings(doc, parent, "osc1WarpAmount");
	m_osc1Unison.saveSettings(doc, parent, "osc1Unison");
	m_osc1Detune.saveSettings(doc, parent, "osc1Detune");
	m_osc1Octave.saveSettings(doc, parent, "osc1Octave");
	m_osc1Semi.saveSettings(doc, parent, "osc1Semi");
	m_osc1Fine.saveSettings(doc, parent, "osc1Fine");
	m_osc1Volume.saveSettings(doc, parent, "osc1Volume");
	m_osc1Pan.saveSettings(doc, parent, "osc1Pan");
	m_osc1Enabled.saveSettings(doc, parent, "osc1Enabled");

	// Save Osc 2
	m_osc2Wave.saveSettings(doc, parent, "osc2Wave");
	m_osc2Morph.saveSettings(doc, parent, "osc2Morph");
	m_osc2WarpMode.saveSettings(doc, parent, "osc2WarpMode");
	m_osc2WarpAmount.saveSettings(doc, parent, "osc2WarpAmount");
	m_osc2Unison.saveSettings(doc, parent, "osc2Unison");
	m_osc2Detune.saveSettings(doc, parent, "osc2Detune");
	m_osc2Octave.saveSettings(doc, parent, "osc2Octave");
	m_osc2Semi.saveSettings(doc, parent, "osc2Semi");
	m_osc2Fine.saveSettings(doc, parent, "osc2Fine");
	m_osc2Volume.saveSettings(doc, parent, "osc2Volume");
	m_osc2Pan.saveSettings(doc, parent, "osc2Pan");
	m_osc2Enabled.saveSettings(doc, parent, "osc2Enabled");

	// Save Osc 3
	m_osc3Wave.saveSettings(doc, parent, "osc3Wave");
	m_osc3Morph.saveSettings(doc, parent, "osc3Morph");
	m_osc3WarpMode.saveSettings(doc, parent, "osc3WarpMode");
	m_osc3WarpAmount.saveSettings(doc, parent, "osc3WarpAmount");
	m_osc3Unison.saveSettings(doc, parent, "osc3Unison");
	m_osc3Detune.saveSettings(doc, parent, "osc3Detune");
	m_osc3Octave.saveSettings(doc, parent, "osc3Octave");
	m_osc3Semi.saveSettings(doc, parent, "osc3Semi");
	m_osc3Fine.saveSettings(doc, parent, "osc3Fine");
	m_osc3Volume.saveSettings(doc, parent, "osc3Volume");
	m_osc3Pan.saveSettings(doc, parent, "osc3Pan");
	m_osc3Enabled.saveSettings(doc, parent, "osc3Enabled");

	// Save Sub / Noise
	m_subWave.saveSettings(doc, parent, "subWave");
	m_subOctave.saveSettings(doc, parent, "subOctave");
	m_subVolume.saveSettings(doc, parent, "subVolume");
	m_subPan.saveSettings(doc, parent, "subPan");
	m_subEnabled.saveSettings(doc, parent, "subEnabled");

	// Save Filters
	m_filter1Type.saveSettings(doc, parent, "filter1Type");
	m_filter1Cutoff.saveSettings(doc, parent, "filter1Cutoff");
	m_filter1Resonance.saveSettings(doc, parent, "filter1Resonance");
	m_filter1Drive.saveSettings(doc, parent, "filter1Drive");
	m_filter1Mix.saveSettings(doc, parent, "filter1Mix");

	m_filter2Type.saveSettings(doc, parent, "filter2Type");
	m_filter2Cutoff.saveSettings(doc, parent, "filter2Cutoff");
	m_filter2Resonance.saveSettings(doc, parent, "filter2Resonance");
	m_filter2Drive.saveSettings(doc, parent, "filter2Drive");
	m_filter2Mix.saveSettings(doc, parent, "filter2Mix");
	m_filterRoute.saveSettings(doc, parent, "filterRoute");

	// Save Envelopes
	m_ampAttack.saveSettings(doc, parent, "ampAttack");
	m_ampDecay.saveSettings(doc, parent, "ampDecay");
	m_ampSustain.saveSettings(doc, parent, "ampSustain");
	m_ampRelease.saveSettings(doc, parent, "ampRelease");

	m_filterAttack.saveSettings(doc, parent, "filterAttack");
	m_filterDecay.saveSettings(doc, parent, "filterDecay");
	m_filterSustain.saveSettings(doc, parent, "filterSustain");
	m_filterRelease.saveSettings(doc, parent, "filterRelease");
	m_filterEnvAmount.saveSettings(doc, parent, "filterEnvAmount");

	// Save Effects
	m_masterVolume.saveSettings(doc, parent, "masterVolume");
	m_chorusAmount.saveSettings(doc, parent, "chorusAmount");
	m_chorusRate.saveSettings(doc, parent, "chorusRate");
	m_delayTime.saveSettings(doc, parent, "delayTime");
	m_delayFeedback.saveSettings(doc, parent, "delayFeedback");
	m_distortionDrive.saveSettings(doc, parent, "distortionDrive");
	m_reverbDecay.saveSettings(doc, parent, "reverbDecay");
	m_reverbMix.saveSettings(doc, parent, "reverbMix");
}

void VitalInstrument::loadSettings(const QDomElement& thisElement)
{
	m_osc1Wave.loadSettings(thisElement, "osc1Wave");
	m_osc1Morph.loadSettings(thisElement, "osc1Morph");
	m_osc1WarpMode.loadSettings(thisElement, "osc1WarpMode");
	m_osc1WarpAmount.loadSettings(thisElement, "osc1WarpAmount");
	m_osc1Unison.loadSettings(thisElement, "osc1Unison");
	m_osc1Detune.loadSettings(thisElement, "osc1Detune");
	m_osc1Octave.loadSettings(thisElement, "osc1Octave");
	m_osc1Semi.loadSettings(thisElement, "osc1Semi");
	m_osc1Fine.loadSettings(thisElement, "osc1Fine");
	m_osc1Volume.loadSettings(thisElement, "osc1Volume");
	m_osc1Pan.loadSettings(thisElement, "osc1Pan");
	m_osc1Enabled.loadSettings(thisElement, "osc1Enabled");

	m_osc2Wave.loadSettings(thisElement, "osc2Wave");
	m_osc2Morph.loadSettings(thisElement, "osc2Morph");
	m_osc2WarpMode.loadSettings(thisElement, "osc2WarpMode");
	m_osc2WarpAmount.loadSettings(thisElement, "osc2WarpAmount");
	m_osc2Unison.loadSettings(thisElement, "osc2Unison");
	m_osc2Detune.loadSettings(thisElement, "osc2Detune");
	m_osc2Octave.loadSettings(thisElement, "osc2Octave");
	m_osc2Semi.loadSettings(thisElement, "osc2Semi");
	m_osc2Fine.loadSettings(thisElement, "osc2Fine");
	m_osc2Volume.loadSettings(thisElement, "osc2Volume");
	m_osc2Pan.loadSettings(thisElement, "osc2Pan");
	m_osc2Enabled.loadSettings(thisElement, "osc2Enabled");

	m_osc3Wave.loadSettings(thisElement, "osc3Wave");
	m_osc3Morph.loadSettings(thisElement, "osc3Morph");
	m_osc3WarpMode.loadSettings(thisElement, "osc3WarpMode");
	m_osc3WarpAmount.loadSettings(thisElement, "osc3WarpAmount");
	m_osc3Unison.loadSettings(thisElement, "osc3Unison");
	m_osc3Detune.loadSettings(thisElement, "osc3Detune");
	m_osc3Octave.loadSettings(thisElement, "osc3Octave");
	m_osc3Semi.loadSettings(thisElement, "osc3Semi");
	m_osc3Fine.loadSettings(thisElement, "osc3Fine");
	m_osc3Volume.loadSettings(thisElement, "osc3Volume");
	m_osc3Pan.loadSettings(thisElement, "osc3Pan");
	m_osc3Enabled.loadSettings(thisElement, "osc3Enabled");

	m_subWave.loadSettings(thisElement, "subWave");
	m_subOctave.loadSettings(thisElement, "subOctave");
	m_subVolume.loadSettings(thisElement, "subVolume");
	m_subPan.loadSettings(thisElement, "subPan");
	m_subEnabled.loadSettings(thisElement, "subEnabled");

	m_filter1Type.loadSettings(thisElement, "filter1Type");
	m_filter1Cutoff.loadSettings(thisElement, "filter1Cutoff");
	m_filter1Resonance.loadSettings(thisElement, "filter1Resonance");
	m_filter1Drive.loadSettings(thisElement, "filter1Drive");
	m_filter1Mix.loadSettings(thisElement, "filter1Mix");

	m_filter2Type.loadSettings(thisElement, "filter2Type");
	m_filter2Cutoff.loadSettings(thisElement, "filter2Cutoff");
	m_filter2Resonance.loadSettings(thisElement, "filter2Resonance");
	m_filter2Drive.loadSettings(thisElement, "filter2Drive");
	m_filter2Mix.loadSettings(thisElement, "filter2Mix");
	m_filterRoute.loadSettings(thisElement, "filterRoute");

	m_ampAttack.loadSettings(thisElement, "ampAttack");
	m_ampDecay.loadSettings(thisElement, "ampDecay");
	m_ampSustain.loadSettings(thisElement, "ampSustain");
	m_ampRelease.loadSettings(thisElement, "ampRelease");

	m_filterAttack.loadSettings(thisElement, "filterAttack");
	m_filterDecay.loadSettings(thisElement, "filterDecay");
	m_filterSustain.loadSettings(thisElement, "filterSustain");
	m_filterRelease.loadSettings(thisElement, "filterRelease");
	m_filterEnvAmount.loadSettings(thisElement, "filterEnvAmount");

	m_masterVolume.loadSettings(thisElement, "masterVolume");
	m_chorusAmount.loadSettings(thisElement, "chorusAmount");
	m_chorusRate.loadSettings(thisElement, "chorusRate");
	m_delayTime.loadSettings(thisElement, "delayTime");
	m_delayFeedback.loadSettings(thisElement, "delayFeedback");
	m_distortionDrive.loadSettings(thisElement, "distortionDrive");
	m_reverbDecay.loadSettings(thisElement, "reverbDecay");
	m_reverbMix.loadSettings(thisElement, "reverbMix");
}

gui::PluginView* VitalInstrument::instantiateView(QWidget* parent)
{
	return new gui::VitalView(this, parent);
}

// ============================================================================
// VitalView GUI Implementation
// ============================================================================

namespace gui
{

VitalView::VitalView(Instrument* instrument, QWidget* parent) :
	InstrumentView(instrument, parent)
{
	setupUI();
}

void VitalView::setupUI()
{
	auto mainLayout = new QVBoxLayout(this);
	mainLayout->setContentsMargins(4, 4, 4, 4);

	auto tabWidget = new QTabWidget(this);

	// Tab 1: Oscillators
	auto oscTab = new QWidget(tabWidget);
	auto oscLayout = new QVBoxLayout(oscTab);

	auto createOscGroup = [this](int num, ComboBox*& waveBox, Knob*& morphKnob, ComboBox*& warpBox, Knob*& warpKnob,
		Knob*& unisonKnob, Knob*& detuneKnob, Knob*& octKnob, Knob*& semiKnob, Knob*& fineKnob,
		Knob*& volKnob, Knob*& panKnob, LedCheckBox*& enableLed) -> QGroupBox*
	{
		auto group = new QGroupBox(tr("Oscillator %1").arg(num));
		auto grid = new QGridLayout(group);

		enableLed = new LedCheckBox(tr("Active"), group);
		waveBox = new ComboBox(group);
		warpBox = new ComboBox(group);

		morphKnob = new Knob(KnobType::Styled, tr("Morph"), group);
		warpKnob = new Knob(KnobType::Styled, tr("Warp"), group);
		unisonKnob = new Knob(KnobType::Styled, tr("Unison"), group);
		detuneKnob = new Knob(KnobType::Styled, tr("Detune"), group);
		octKnob = new Knob(KnobType::Styled, tr("Oct"), group);
		semiKnob = new Knob(KnobType::Styled, tr("Semi"), group);
		fineKnob = new Knob(KnobType::Styled, tr("Fine"), group);
		volKnob = new Knob(KnobType::Styled, tr("Vol"), group);
		panKnob = new Knob(KnobType::Styled, tr("Pan"), group);

		grid->addWidget(enableLed, 0, 0);
		grid->addWidget(waveBox, 0, 1, 1, 2);
		grid->addWidget(warpBox, 0, 3, 1, 2);

		grid->addWidget(morphKnob, 1, 0);
		grid->addWidget(warpKnob, 1, 1);
		grid->addWidget(unisonKnob, 1, 2);
		grid->addWidget(detuneKnob, 1, 3);
		grid->addWidget(octKnob, 1, 4);

		grid->addWidget(semiKnob, 2, 1);
		grid->addWidget(fineKnob, 2, 2);
		grid->addWidget(volKnob, 2, 3);
		grid->addWidget(panKnob, 2, 4);

		return group;
	};

	oscLayout->addWidget(createOscGroup(1, m_osc1WaveBox, m_osc1MorphKnob, m_osc1WarpBox, m_osc1WarpKnob,
		m_osc1UnisonKnob, m_osc1DetuneKnob, m_osc1OctaveKnob, m_osc1SemiKnob, m_osc1FineKnob,
		m_osc1VolKnob, m_osc1PanKnob, m_osc1EnableLed));

	oscLayout->addWidget(createOscGroup(2, m_osc2WaveBox, m_osc2MorphKnob, m_osc2WarpBox, m_osc2WarpKnob,
		m_osc2UnisonKnob, m_osc2DetuneKnob, m_osc2OctaveKnob, m_osc2SemiKnob, m_osc2FineKnob,
		m_osc2VolKnob, m_osc2PanKnob, m_osc2EnableLed));

	oscLayout->addWidget(createOscGroup(3, m_osc3WaveBox, m_osc3MorphKnob, m_osc3WarpBox, m_osc3WarpKnob,
		m_osc3UnisonKnob, m_osc3DetuneKnob, m_osc3OctaveKnob, m_osc3SemiKnob, m_osc3FineKnob,
		m_osc3VolKnob, m_osc3PanKnob, m_osc3EnableLed));

	// Sub / Noise group
	auto subGroup = new QGroupBox(tr("Sub / Noise Generator"), oscTab);
	auto subGrid = new QGridLayout(subGroup);
	m_subEnableLed = new LedCheckBox(tr("Active"), subGroup);
	m_subWaveBox = new ComboBox(subGroup);
	m_subOctaveKnob = new Knob(KnobType::Styled, tr("Octave"), subGroup);
	m_subVolKnob = new Knob(KnobType::Styled, tr("Vol"), subGroup);
	m_subPanKnob = new Knob(KnobType::Styled, tr("Pan"), subGroup);

	subGrid->addWidget(m_subEnableLed, 0, 0);
	subGrid->addWidget(m_subWaveBox, 0, 1, 1, 2);
	subGrid->addWidget(m_subOctaveKnob, 0, 3);
	subGrid->addWidget(m_subVolKnob, 0, 4);
	subGrid->addWidget(m_subPanKnob, 0, 5);
	oscLayout->addWidget(subGroup);

	tabWidget->addTab(oscTab, tr("Oscillators"));

	// Tab 2: Filters & Envelopes
	auto feTab = new QWidget(tabWidget);
	auto feLayout = new QVBoxLayout(feTab);

	auto fGroup = new QGroupBox(tr("Dual Multimode Filters"), feTab);
	auto fGrid = new QGridLayout(fGroup);

	m_filter1TypeBox = new ComboBox(fGroup);
	m_filter1CutoffKnob = new Knob(KnobType::Styled, tr("F1 Cutoff"), fGroup);
	m_filter1ResKnob = new Knob(KnobType::Styled, tr("F1 Res"), fGroup);
	m_filter1DriveKnob = new Knob(KnobType::Styled, tr("F1 Drive"), fGroup);
	m_filter1MixKnob = new Knob(KnobType::Styled, tr("F1 Mix"), fGroup);

	m_filter2TypeBox = new ComboBox(fGroup);
	m_filter2CutoffKnob = new Knob(KnobType::Styled, tr("F2 Cutoff"), fGroup);
	m_filter2ResKnob = new Knob(KnobType::Styled, tr("F2 Res"), fGroup);
	m_filter2DriveKnob = new Knob(KnobType::Styled, tr("F2 Drive"), fGroup);
	m_filter2MixKnob = new Knob(KnobType::Styled, tr("F2 Mix"), fGroup);
	m_filterRouteBox = new ComboBox(fGroup);

	fGrid->addWidget(new QLabel(tr("Filter 1:"), fGroup), 0, 0);
	fGrid->addWidget(m_filter1TypeBox, 0, 1);
	fGrid->addWidget(m_filter1CutoffKnob, 0, 2);
	fGrid->addWidget(m_filter1ResKnob, 0, 3);
	fGrid->addWidget(m_filter1DriveKnob, 0, 4);
	fGrid->addWidget(m_filter1MixKnob, 0, 5);

	fGrid->addWidget(new QLabel(tr("Filter 2:"), fGroup), 1, 0);
	fGrid->addWidget(m_filter2TypeBox, 1, 1);
	fGrid->addWidget(m_filter2CutoffKnob, 1, 2);
	fGrid->addWidget(m_filter2ResKnob, 1, 3);
	fGrid->addWidget(m_filter2DriveKnob, 1, 4);
	fGrid->addWidget(m_filter2MixKnob, 1, 5);

	fGrid->addWidget(new QLabel(tr("Routing:"), fGroup), 2, 0);
	fGrid->addWidget(m_filterRouteBox, 2, 1);
	feLayout->addWidget(fGroup);

	auto envGroup = new QGroupBox(tr("Envelopes (ADSR)"), feTab);
	auto envGrid = new QGridLayout(envGroup);

	m_ampAttackKnob = new Knob(KnobType::Styled, tr("Amp Att"), envGroup);
	m_ampDecayKnob = new Knob(KnobType::Styled, tr("Amp Dec"), envGroup);
	m_ampSustainKnob = new Knob(KnobType::Styled, tr("Amp Sus"), envGroup);
	m_ampReleaseKnob = new Knob(KnobType::Styled, tr("Amp Rel"), envGroup);

	m_filterAttackKnob = new Knob(KnobType::Styled, tr("Filt Att"), envGroup);
	m_filterDecayKnob = new Knob(KnobType::Styled, tr("Filt Dec"), envGroup);
	m_filterSustainKnob = new Knob(KnobType::Styled, tr("Filt Sus"), envGroup);
	m_filterReleaseKnob = new Knob(KnobType::Styled, tr("Filt Rel"), envGroup);
	m_filterEnvAmtKnob = new Knob(KnobType::Styled, tr("Filt Env Amt"), envGroup);

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
	envGrid->addWidget(m_filterEnvAmtKnob, 1, 5);
	feLayout->addWidget(envGroup);

	tabWidget->addTab(feTab, tr("Filters & Envelopes"));

	// Tab 3: Effects & Master
	auto fxTab = new QWidget(tabWidget);
	auto fxGrid = new QGridLayout(fxTab);

	m_masterVolKnob = new Knob(KnobType::Styled, tr("Master Vol"), fxTab);
	m_distDriveKnob = new Knob(KnobType::Styled, tr("Distortion"), fxTab);
	m_chorusAmtKnob = new Knob(KnobType::Styled, tr("Chorus"), fxTab);
	m_chorusRateKnob = new Knob(KnobType::Styled, tr("Chorus Rate"), fxTab);
	m_delayTimeKnob = new Knob(KnobType::Styled, tr("Delay Time"), fxTab);
	m_delayFeedbackKnob = new Knob(KnobType::Styled, tr("Delay Feedback"), fxTab);
	m_reverbDecayKnob = new Knob(KnobType::Styled, tr("Reverb Decay"), fxTab);
	m_reverbMixKnob = new Knob(KnobType::Styled, tr("Reverb Mix"), fxTab);

	fxGrid->addWidget(m_masterVolKnob, 0, 0);
	fxGrid->addWidget(m_distDriveKnob, 0, 1);
	fxGrid->addWidget(m_chorusAmtKnob, 0, 2);
	fxGrid->addWidget(m_chorusRateKnob, 0, 3);
	fxGrid->addWidget(m_delayTimeKnob, 1, 0);
	fxGrid->addWidget(m_delayFeedbackKnob, 1, 1);
	fxGrid->addWidget(m_reverbDecayKnob, 1, 2);
	fxGrid->addWidget(m_reverbMixKnob, 1, 3);

	tabWidget->addTab(fxTab, tr("Effects & Master"));

	mainLayout->addWidget(tabWidget);
}

void VitalView::modelChanged()
{
	auto v = castModel<VitalInstrument>();
	if (!v) { return; }

	// Connect Osc 1
	m_osc1WaveBox->setModel(&v->m_osc1Wave);
	m_osc1MorphKnob->setModel(&v->m_osc1Morph);
	m_osc1WarpBox->setModel(&v->m_osc1WarpMode);
	m_osc1WarpKnob->setModel(&v->m_osc1WarpAmount);
	m_osc1UnisonKnob->setModel(&v->m_osc1Unison);
	m_osc1DetuneKnob->setModel(&v->m_osc1Detune);
	m_osc1OctaveKnob->setModel(&v->m_osc1Octave);
	m_osc1SemiKnob->setModel(&v->m_osc1Semi);
	m_osc1FineKnob->setModel(&v->m_osc1Fine);
	m_osc1VolKnob->setModel(&v->m_osc1Volume);
	m_osc1PanKnob->setModel(&v->m_osc1Pan);
	m_osc1EnableLed->setModel(&v->m_osc1Enabled);

	// Connect Osc 2
	m_osc2WaveBox->setModel(&v->m_osc2Wave);
	m_osc2MorphKnob->setModel(&v->m_osc2Morph);
	m_osc2WarpBox->setModel(&v->m_osc2WarpMode);
	m_osc2WarpKnob->setModel(&v->m_osc2WarpAmount);
	m_osc2UnisonKnob->setModel(&v->m_osc2Unison);
	m_osc2DetuneKnob->setModel(&v->m_osc2Detune);
	m_osc2OctaveKnob->setModel(&v->m_osc2Octave);
	m_osc2SemiKnob->setModel(&v->m_osc2Semi);
	m_osc2FineKnob->setModel(&v->m_osc2Fine);
	m_osc2VolKnob->setModel(&v->m_osc2Volume);
	m_osc2PanKnob->setModel(&v->m_osc2Pan);
	m_osc2EnableLed->setModel(&v->m_osc2Enabled);

	// Connect Osc 3
	m_osc3WaveBox->setModel(&v->m_osc3Wave);
	m_osc3MorphKnob->setModel(&v->m_osc3Morph);
	m_osc3WarpBox->setModel(&v->m_osc3WarpMode);
	m_osc3WarpKnob->setModel(&v->m_osc3WarpAmount);
	m_osc3UnisonKnob->setModel(&v->m_osc3Unison);
	m_osc3DetuneKnob->setModel(&v->m_osc3Detune);
	m_osc3OctaveKnob->setModel(&v->m_osc3Octave);
	m_osc3SemiKnob->setModel(&v->m_osc3Semi);
	m_osc3FineKnob->setModel(&v->m_osc3Fine);
	m_osc3VolKnob->setModel(&v->m_osc3Volume);
	m_osc3PanKnob->setModel(&v->m_osc3Pan);
	m_osc3EnableLed->setModel(&v->m_osc3Enabled);

	// Connect Sub / Noise
	m_subWaveBox->setModel(&v->m_subWave);
	m_subOctaveKnob->setModel(&v->m_subOctave);
	m_subVolKnob->setModel(&v->m_subVolume);
	m_subPanKnob->setModel(&v->m_subPan);
	m_subEnableLed->setModel(&v->m_subEnabled);

	// Connect Filters
	m_filter1TypeBox->setModel(&v->m_filter1Type);
	m_filter1CutoffKnob->setModel(&v->m_filter1Cutoff);
	m_filter1ResKnob->setModel(&v->m_filter1Resonance);
	m_filter1DriveKnob->setModel(&v->m_filter1Drive);
	m_filter1MixKnob->setModel(&v->m_filter1Mix);

	m_filter2TypeBox->setModel(&v->m_filter2Type);
	m_filter2CutoffKnob->setModel(&v->m_filter2Cutoff);
	m_filter2ResKnob->setModel(&v->m_filter2Resonance);
	m_filter2DriveKnob->setModel(&v->m_filter2Drive);
	m_filter2MixKnob->setModel(&v->m_filter2Mix);
	m_filterRouteBox->setModel(&v->m_filterRoute);

	// Connect Envelopes
	m_ampAttackKnob->setModel(&v->m_ampAttack);
	m_ampDecayKnob->setModel(&v->m_ampDecay);
	m_ampSustainKnob->setModel(&v->m_ampSustain);
	m_ampReleaseKnob->setModel(&v->m_ampRelease);

	m_filterAttackKnob->setModel(&v->m_filterAttack);
	m_filterDecayKnob->setModel(&v->m_filterDecay);
	m_filterSustainKnob->setModel(&v->m_filterSustain);
	m_filterReleaseKnob->setModel(&v->m_filterRelease);
	m_filterEnvAmtKnob->setModel(&v->m_filterEnvAmount);

	// Connect FX & Master
	m_masterVolKnob->setModel(&v->m_masterVolume);
	m_chorusAmtKnob->setModel(&v->m_chorusAmount);
	m_chorusRateKnob->setModel(&v->m_chorusRate);
	m_delayTimeKnob->setModel(&v->m_delayTime);
	m_delayFeedbackKnob->setModel(&v->m_delayFeedback);
	m_distDriveKnob->setModel(&v->m_distortionDrive);
	m_reverbDecayKnob->setModel(&v->m_reverbDecay);
	m_reverbMixKnob->setModel(&v->m_reverbMix);
}

} // namespace gui

extern "C"
{

PLUGIN_EXPORT Plugin * lmms_plugin_main( Model * m, void * )
{
	return new VitalInstrument( static_cast<InstrumentTrack *>( m ) );
}

}

} // namespace lmms

