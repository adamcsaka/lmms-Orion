/*
 * OxoSynth.h - Native Virtual Analog Synthesizer for LMMS
 * Inspired by Oxo Synth2 & Synth1 (Daichi / chab)
 *
 * This file is part of LMMS - https://lmms.io
 */

#ifndef OXO_SYNTH_H
#define OXO_SYNTH_H

#include <vector>
#include <cmath>

#include "AutomatableModel.h"
#include "ComboBoxModel.h"
#include "Instrument.h"
#include "InstrumentView.h"
#include "SampleFrame.h"

namespace lmms
{

class NotePlayHandle;

namespace gui
{
class OxoSynthView;
class Knob;
class ComboBox;
class LedCheckBox;
}

class OxoVoice
{
public:
	OxoVoice(sample_rate_t sampleRate, NotePlayHandle* nph);
	~OxoVoice() = default;

	void render(SampleFrame* buffer, f_cnt_t offset, f_cnt_t frames, class OxoSynthInstrument* inst);
	bool isFinished() const { return m_finished; }

private:
	sample_rate_t m_sampleRate;
	NotePlayHandle* m_nph;
	bool m_finished = false;

	// Oscillator phases
	float m_osc1Phase = 0.0f;
	float m_osc2Phase = 0.0f;
	float m_subPhase = 0.0f;

	// LFO states
	float m_lfo1Phase = 0.0f;
	float m_lfo1Value = 0.0f;
	float m_lfo2Phase = 0.0f;
	float m_lfo2Value = 0.0f;

	// Envelope states
	enum class EnvStage { Attack, Decay, Sustain, Release, Finished };
	EnvStage m_ampStage = EnvStage::Attack;
	float m_ampLevel = 0.0f;
	EnvStage m_filterStage = EnvStage::Attack;
	float m_filterLevel = 0.0f;

	// Resonant SVF filter states (stereo)
	float m_fL_ic1 = 0.0f, m_fL_ic2 = 0.0f;
	float m_fR_ic1 = 0.0f, m_fR_ic2 = 0.0f;

	// Delay effect state
	static constexpr int DELAY_BUFFER_SIZE = 96000;
	std::vector<SampleFrame> m_delayBuffer;
	int m_delayWritePos = 0;

	// Chorus LFO state
	float m_chorusPhase = 0.0f;

	float generateWave(int waveType, float phase, float pulseWidth);
	float processFilter(float input, float cutoffHz, float resonance, float saturation, int type, float& ic1, float& ic2);
};

class OxoSynthInstrument : public Instrument
{
	Q_OBJECT
public:
	OxoSynthInstrument(InstrumentTrack* instrumentTrack);
	~OxoSynthInstrument() override = default;

	void playNote(NotePlayHandle* n, SampleFrame* workingBuffer) override;
	void deleteNotePluginData(NotePlayHandle* n) override;

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& thisElement) override;

	QString nodeName() const override;

	float desiredReleaseTimeMs() const override;

	gui::PluginView* instantiateView(QWidget* parent) override;

	// Oscillator 1
	ComboBoxModel m_osc1Wave;
	FloatModel m_osc1PulseWidth;
	FloatModel m_osc1PWMDepth;
	ComboBoxModel m_subType;
	FloatModel m_subLevel;
	FloatModel m_fmAmount;
	FloatModel m_osc1Level;
	FloatModel m_osc1Pan;

	// Oscillator 2
	ComboBoxModel m_osc2Wave;
	IntModel m_osc2Octave;
	IntModel m_osc2Semi;
	FloatModel m_osc2Fine;
	BoolModel m_ringMod;
	BoolModel m_sync;
	FloatModel m_osc2Level;
	FloatModel m_osc2Pan;

	// Noise & Mixer
	FloatModel m_noiseLevel;
	FloatModel m_noiseColor;

	// Filter
	ComboBoxModel m_filterType;
	FloatModel m_filterCutoff;
	FloatModel m_filterResonance;
	FloatModel m_filterSaturation;
	FloatModel m_filterKeyFollow;
	FloatModel m_filterEnvAmount;

	// Envelopes
	FloatModel m_ampAttack;
	FloatModel m_ampDecay;
	FloatModel m_ampSustain;
	FloatModel m_ampRelease;

	FloatModel m_filterAttack;
	FloatModel m_filterDecay;
	FloatModel m_filterSustain;
	FloatModel m_filterRelease;

	// LFO 1 & LFO 2
	ComboBoxModel m_lfo1Wave;
	FloatModel m_lfo1Rate;
	FloatModel m_lfo1Depth;
	ComboBoxModel m_lfo1Dest;

	ComboBoxModel m_lfo2Wave;
	FloatModel m_lfo2Rate;
	FloatModel m_lfo2Depth;
	ComboBoxModel m_lfo2Dest;

	// Effects & Master
	FloatModel m_masterVolume;
	FloatModel m_masterPan;
	FloatModel m_chorusAmount;
	FloatModel m_chorusRate;
	FloatModel m_delayTime;
	FloatModel m_delayFeedback;
	FloatModel m_portamento;

	friend class gui::OxoSynthView;
};

namespace gui
{

class OxoSynthView : public InstrumentView
{
	Q_OBJECT
public:
	OxoSynthView(Instrument* instrument, QWidget* parent);
	~OxoSynthView() override = default;

	void modelChanged() override;

private:
	void setupUI();

	// Osc 1 controls
	ComboBox* m_osc1WaveBox = nullptr;
	Knob* m_osc1PWKnob = nullptr;
	Knob* m_osc1PWMKnob = nullptr;
	ComboBox* m_subTypeBox = nullptr;
	Knob* m_subLevelKnob = nullptr;
	Knob* m_fmAmountKnob = nullptr;
	Knob* m_osc1LevelKnob = nullptr;
	Knob* m_osc1PanKnob = nullptr;

	// Osc 2 controls
	ComboBox* m_osc2WaveBox = nullptr;
	Knob* m_osc2OctaveKnob = nullptr;
	Knob* m_osc2SemiKnob = nullptr;
	Knob* m_osc2FineKnob = nullptr;
	LedCheckBox* m_ringModLed = nullptr;
	LedCheckBox* m_syncLed = nullptr;
	Knob* m_osc2LevelKnob = nullptr;
	Knob* m_osc2PanKnob = nullptr;

	// Noise & Filter controls
	Knob* m_noiseLevelKnob = nullptr;
	Knob* m_noiseColorKnob = nullptr;
	ComboBox* m_filterTypeBox = nullptr;
	Knob* m_filterCutoffKnob = nullptr;
	Knob* m_filterResKnob = nullptr;
	Knob* m_filterSatKnob = nullptr;
	Knob* m_filterKeyFollowKnob = nullptr;
	Knob* m_filterEnvAmtKnob = nullptr;

	// Envelopes
	Knob* m_ampAttackKnob = nullptr;
	Knob* m_ampDecayKnob = nullptr;
	Knob* m_ampSustainKnob = nullptr;
	Knob* m_ampReleaseKnob = nullptr;

	Knob* m_filterAttackKnob = nullptr;
	Knob* m_filterDecayKnob = nullptr;
	Knob* m_filterSustainKnob = nullptr;
	Knob* m_filterReleaseKnob = nullptr;

	// LFOs
	ComboBox* m_lfo1WaveBox = nullptr;
	Knob* m_lfo1RateKnob = nullptr;
	Knob* m_lfo1DepthKnob = nullptr;
	ComboBox* m_lfo1DestBox = nullptr;

	ComboBox* m_lfo2WaveBox = nullptr;
	Knob* m_lfo2RateKnob = nullptr;
	Knob* m_lfo2DepthKnob = nullptr;
	ComboBox* m_lfo2DestBox = nullptr;

	// Effects & Master
	Knob* m_masterVolKnob = nullptr;
	Knob* m_masterPanKnob = nullptr;
	Knob* m_chorusAmtKnob = nullptr;
	Knob* m_chorusRateKnob = nullptr;
	Knob* m_delayTimeKnob = nullptr;
	Knob* m_delayFeedbackKnob = nullptr;
	Knob* m_portamentoKnob = nullptr;
};

} // namespace gui

} // namespace lmms

#endif // OXO_SYNTH_H
