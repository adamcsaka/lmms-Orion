/*
 * Vital.h - Native Spectral Wavetable Synthesizer for LMMS
 * Inspired by Vital (Matt Tytel)
 *
 * This file is part of LMMS - https://lmms.io
 */

#ifndef VITAL_H
#define VITAL_H

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
class VitalView;
class Knob;
class ComboBox;
class LedCheckBox;
}

class VitalVoice
{
public:
	VitalVoice(sample_rate_t sampleRate, NotePlayHandle* nph);
	~VitalVoice() = default;

	void render(SampleFrame* buffer, f_cnt_t offset, f_cnt_t frames, class VitalInstrument* inst);
	bool isFinished() const { return m_finished; }

private:
	sample_rate_t m_sampleRate;
	NotePlayHandle* m_nph;
	bool m_finished = false;

	// Oscillator phase accumulators (support up to 7 unison voices per osc)
	static constexpr int MAX_UNISON = 7;
	float m_osc1Phase[MAX_UNISON] = {0.0f};
	float m_osc2Phase[MAX_UNISON] = {0.0f};
	float m_osc3Phase[MAX_UNISON] = {0.0f};
	float m_subPhase = 0.0f;

	// Envelope state
	enum class EnvStage { Attack, Decay, Sustain, Release, Finished };
	EnvStage m_ampStage = EnvStage::Attack;
	float m_ampLevel = 0.0f;
	EnvStage m_filterStage = EnvStage::Attack;
	float m_filterLevel = 0.0f;

	// Resonant SVF filter states (Filter 1 and Filter 2, stereo)
	float m_f1L_ic1 = 0.0f, m_f1L_ic2 = 0.0f;
	float m_f1R_ic1 = 0.0f, m_f1R_ic2 = 0.0f;
	float m_f2L_ic1 = 0.0f, m_f2L_ic2 = 0.0f;
	float m_f2R_ic1 = 0.0f, m_f2R_ic2 = 0.0f;

	// Voice effects state (chorus, delay, reverb)
	float m_chorusPhase = 0.0f;
	static constexpr int DELAY_BUFFER_SIZE = 96000;
	std::vector<SampleFrame> m_delayBuffer;
	int m_delayWritePos = 0;

	// Simple reverb comb/allpass state
	static constexpr int REVERB_COMBS = 4;
	std::vector<float> m_reverbBuffer[REVERB_COMBS];
	int m_reverbPos[REVERB_COMBS] = {0};

	float processWavetable(int waveType, float phase, float morph, int warpMode, float warpAmount);
	float processSVF(float input, float cutoffHz, float resonance, float drive, int type, float& ic1, float& ic2);
};

class VitalInstrument : public Instrument
{
	Q_OBJECT
public:
	VitalInstrument(InstrumentTrack* instrumentTrack);
	~VitalInstrument() override = default;

	void playNote(NotePlayHandle* n, SampleFrame* workingBuffer) override;
	void deleteNotePluginData(NotePlayHandle* n) override;

	void saveSettings(QDomDocument& doc, QDomElement& parent) override;
	void loadSettings(const QDomElement& thisElement) override;

	QString nodeName() const override;

	float desiredReleaseTimeMs() const override;

	gui::PluginView* instantiateView(QWidget* parent) override;

	// Models - Osc 1
	ComboBoxModel m_osc1Wave;
	FloatModel m_osc1Morph;
	ComboBoxModel m_osc1WarpMode;
	FloatModel m_osc1WarpAmount;
	IntModel m_osc1Unison;
	FloatModel m_osc1Detune;
	IntModel m_osc1Octave;
	IntModel m_osc1Semi;
	FloatModel m_osc1Fine;
	FloatModel m_osc1Volume;
	FloatModel m_osc1Pan;
	BoolModel m_osc1Enabled;

	// Models - Osc 2
	ComboBoxModel m_osc2Wave;
	FloatModel m_osc2Morph;
	ComboBoxModel m_osc2WarpMode;
	FloatModel m_osc2WarpAmount;
	IntModel m_osc2Unison;
	FloatModel m_osc2Detune;
	IntModel m_osc2Octave;
	IntModel m_osc2Semi;
	FloatModel m_osc2Fine;
	FloatModel m_osc2Volume;
	FloatModel m_osc2Pan;
	BoolModel m_osc2Enabled;

	// Models - Osc 3
	ComboBoxModel m_osc3Wave;
	FloatModel m_osc3Morph;
	ComboBoxModel m_osc3WarpMode;
	FloatModel m_osc3WarpAmount;
	IntModel m_osc3Unison;
	FloatModel m_osc3Detune;
	IntModel m_osc3Octave;
	IntModel m_osc3Semi;
	FloatModel m_osc3Fine;
	FloatModel m_osc3Volume;
	FloatModel m_osc3Pan;
	BoolModel m_osc3Enabled;

	// Models - Sub / Noise
	ComboBoxModel m_subWave;
	IntModel m_subOctave;
	FloatModel m_subVolume;
	FloatModel m_subPan;
	BoolModel m_subEnabled;

	// Models - Filters
	ComboBoxModel m_filter1Type;
	FloatModel m_filter1Cutoff;
	FloatModel m_filter1Resonance;
	FloatModel m_filter1Drive;
	FloatModel m_filter1Mix;

	ComboBoxModel m_filter2Type;
	FloatModel m_filter2Cutoff;
	FloatModel m_filter2Resonance;
	FloatModel m_filter2Drive;
	FloatModel m_filter2Mix;
	ComboBoxModel m_filterRoute;

	// Models - Envelopes
	FloatModel m_ampAttack;
	FloatModel m_ampDecay;
	FloatModel m_ampSustain;
	FloatModel m_ampRelease;

	FloatModel m_filterAttack;
	FloatModel m_filterDecay;
	FloatModel m_filterSustain;
	FloatModel m_filterRelease;
	FloatModel m_filterEnvAmount;

	// Models - Effects & Master
	FloatModel m_masterVolume;
	FloatModel m_chorusAmount;
	FloatModel m_chorusRate;
	FloatModel m_delayTime;
	FloatModel m_delayFeedback;
	FloatModel m_distortionDrive;
	FloatModel m_reverbDecay;
	FloatModel m_reverbMix;

	friend class gui::VitalView;
};

namespace gui
{

class VitalView : public InstrumentView
{
	Q_OBJECT
public:
	VitalView(Instrument* instrument, QWidget* parent);
	~VitalView() override = default;

	void modelChanged() override;

private:
	void setupUI();

	// Osc 1 controls
	ComboBox* m_osc1WaveBox = nullptr;
	Knob* m_osc1MorphKnob = nullptr;
	ComboBox* m_osc1WarpBox = nullptr;
	Knob* m_osc1WarpKnob = nullptr;
	Knob* m_osc1UnisonKnob = nullptr;
	Knob* m_osc1DetuneKnob = nullptr;
	Knob* m_osc1OctaveKnob = nullptr;
	Knob* m_osc1SemiKnob = nullptr;
	Knob* m_osc1FineKnob = nullptr;
	Knob* m_osc1VolKnob = nullptr;
	Knob* m_osc1PanKnob = nullptr;
	LedCheckBox* m_osc1EnableLed = nullptr;

	// Osc 2 controls
	ComboBox* m_osc2WaveBox = nullptr;
	Knob* m_osc2MorphKnob = nullptr;
	ComboBox* m_osc2WarpBox = nullptr;
	Knob* m_osc2WarpKnob = nullptr;
	Knob* m_osc2UnisonKnob = nullptr;
	Knob* m_osc2DetuneKnob = nullptr;
	Knob* m_osc2OctaveKnob = nullptr;
	Knob* m_osc2SemiKnob = nullptr;
	Knob* m_osc2FineKnob = nullptr;
	Knob* m_osc2VolKnob = nullptr;
	Knob* m_osc2PanKnob = nullptr;
	LedCheckBox* m_osc2EnableLed = nullptr;

	// Osc 3 controls
	ComboBox* m_osc3WaveBox = nullptr;
	Knob* m_osc3MorphKnob = nullptr;
	ComboBox* m_osc3WarpBox = nullptr;
	Knob* m_osc3WarpKnob = nullptr;
	Knob* m_osc3UnisonKnob = nullptr;
	Knob* m_osc3DetuneKnob = nullptr;
	Knob* m_osc3OctaveKnob = nullptr;
	Knob* m_osc3SemiKnob = nullptr;
	Knob* m_osc3FineKnob = nullptr;
	Knob* m_osc3VolKnob = nullptr;
	Knob* m_osc3PanKnob = nullptr;
	LedCheckBox* m_osc3EnableLed = nullptr;

	// Sub / Noise controls
	ComboBox* m_subWaveBox = nullptr;
	Knob* m_subOctaveKnob = nullptr;
	Knob* m_subVolKnob = nullptr;
	Knob* m_subPanKnob = nullptr;
	LedCheckBox* m_subEnableLed = nullptr;

	// Filter controls
	ComboBox* m_filter1TypeBox = nullptr;
	Knob* m_filter1CutoffKnob = nullptr;
	Knob* m_filter1ResKnob = nullptr;
	Knob* m_filter1DriveKnob = nullptr;
	Knob* m_filter1MixKnob = nullptr;

	ComboBox* m_filter2TypeBox = nullptr;
	Knob* m_filter2CutoffKnob = nullptr;
	Knob* m_filter2ResKnob = nullptr;
	Knob* m_filter2DriveKnob = nullptr;
	Knob* m_filter2MixKnob = nullptr;
	ComboBox* m_filterRouteBox = nullptr;

	// Envelopes
	Knob* m_ampAttackKnob = nullptr;
	Knob* m_ampDecayKnob = nullptr;
	Knob* m_ampSustainKnob = nullptr;
	Knob* m_ampReleaseKnob = nullptr;

	Knob* m_filterAttackKnob = nullptr;
	Knob* m_filterDecayKnob = nullptr;
	Knob* m_filterSustainKnob = nullptr;
	Knob* m_filterReleaseKnob = nullptr;
	Knob* m_filterEnvAmtKnob = nullptr;

	// Effects & Master
	Knob* m_masterVolKnob = nullptr;
	Knob* m_chorusAmtKnob = nullptr;
	Knob* m_chorusRateKnob = nullptr;
	Knob* m_delayTimeKnob = nullptr;
	Knob* m_delayFeedbackKnob = nullptr;
	Knob* m_distDriveKnob = nullptr;
	Knob* m_reverbDecayKnob = nullptr;
	Knob* m_reverbMixKnob = nullptr;
};

} // namespace gui

} // namespace lmms

#endif // VITAL_H
