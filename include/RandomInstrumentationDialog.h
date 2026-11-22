/*
 * RandomInstrumentationDialog.h - dialog and generator for random instruments with effects
 *
 * This file is part of LMMS - https://lmms.io
 */

#ifndef LMMS_GUI_RANDOM_INSTRUMENTATION_DIALOG_H
#define LMMS_GUI_RANDOM_INSTRUMENTATION_DIALOG_H

#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QSpinBox>
#include <QVBoxLayout>
#include <cstdlib>

#include "AudioBusHandle.h"
#include "ConfigManager.h"
#include "Effect.h"
#include "EffectChain.h"
#include "embed.h"
#include "InstrumentTrack.h"
#include "TextFloat.h"
#include "Track.h"
#include "TrackContainer.h"

namespace lmms::gui
{

class RandomInstrumentationDialog : public QDialog
{
public:
	explicit RandomInstrumentationDialog(QWidget* parent = nullptr) :
		QDialog(parent)
	{
		setWindowTitle(tr("Generate Random Instrumentation"));
		auto mainLayout = new QVBoxLayout(this);

		int initialInstruments = ConfigManager::inst()->value("ui", "random_instrument_count", "4").toInt();
		if (initialInstruments < 1 || initialInstruments > 32) { initialInstruments = 4; }
		int initialEffects = ConfigManager::inst()->value("ui", "random_effect_count", "2").toInt();
		if (initialEffects < 0 || initialEffects > 10) { initialEffects = 2; }
		int initialPolyphony = ConfigManager::inst()->value("ui", "random_polyphony_count", "16").toInt();
		if (initialPolyphony < 1 || initialPolyphony > 64) { initialPolyphony = 16; }

		auto formLayout = new QFormLayout();
		m_numInstrumentsSpin = new QSpinBox(this);
		m_numInstrumentsSpin->setRange(1, 32);
		m_numInstrumentsSpin->setValue(initialInstruments);
		formLayout->addRow(tr("Number of instruments to generate:"), m_numInstrumentsSpin);

		m_numEffectsSpin = new QSpinBox(this);
		m_numEffectsSpin->setRange(0, 10);
		m_numEffectsSpin->setValue(initialEffects);
		formLayout->addRow(tr("Number of DSP effects per instrument:"), m_numEffectsSpin);

		m_polyphonySpin = new QSpinBox(this);
		m_polyphonySpin->setRange(1, 32);
		m_polyphonySpin->setValue(initialPolyphony);
		m_polyphonySpin->setToolTip(tr("Maximum voices per instrument to protect CPU (1 = Mono, 16 = standard polyphonic)"));
		formLayout->addRow(tr("Max polyphony voices per track:"), m_polyphonySpin);

		mainLayout->addLayout(formLayout);

		auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
			ConfigManager::inst()->setValue("ui", "random_instrument_count", QString::number(m_numInstrumentsSpin->value()));
			ConfigManager::inst()->setValue("ui", "random_effect_count", QString::number(m_numEffectsSpin->value()));
			ConfigManager::inst()->setValue("ui", "random_polyphony_count", QString::number(m_polyphonySpin->value()));
			accept();
		});
		connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
		mainLayout->addWidget(buttonBox);
	}

	int instrumentCount() const { return m_numInstrumentsSpin->value(); }
	int effectCount() const { return m_numEffectsSpin->value(); }
	int polyphonyCount() const { return m_polyphonySpin->value(); }

private:
	QSpinBox* m_numInstrumentsSpin;
	QSpinBox* m_numEffectsSpin;
	QSpinBox* m_polyphonySpin;
};

inline void generateRandomInstrumentation(TrackContainer* container, int numTracks, int numEffects, int maxPolyphony = 16)
{
	if (!container || numTracks <= 0) { return; }

	// Only rock-solid, fully native LMMS synthesizers (excluding experimental or crash-prone plugins)
	const char* synthPlugins[] = {
		"tripleoscillator", "bitinvader", "monstro", "watsyn",
		"organic", "freeboy", "nes", "kicker", "sfxr", "vibed", "lb302"
	};
	const int numPlugins = sizeof(synthPlugins) / sizeof(synthPlugins[0]);

	const char* fxPlugins[] = {
		"delay", "dualfilter", "flanger", "bitcrush", "reverbsc", "stereomatrix", "waveshaper",
		"amplifier", "bassbooster", "compressor", "crossovereq"
	};
	const int numFx = sizeof(fxPlugins) / sizeof(fxPlugins[0]);

	for (int i = 0; i < numTracks; ++i)
	{
		auto track = Track::create(Track::Type::Instrument, container);
		auto it = dynamic_cast<InstrumentTrack*>(track);
		if (!it) { continue; }

		const char* chosenSynth = synthPlugins[rand() % numPlugins];
		it->loadInstrument(chosenSynth);
		it->setName(QString("%1 (Synth %2)").arg(it->instrumentName()).arg(i + 1));

		if (numEffects > 0 && it->audioBusHandle() && it->audioBusHandle()->effects())
		{
			for (int eIdx = 0; eIdx < numEffects; ++eIdx)
			{
				const char* chosenFx = fxPlugins[rand() % numFx];
				Effect* e = Effect::instantiate(QString::fromUtf8(chosenFx), it->audioBusHandle()->effects(), nullptr);
				if (e && e->isOkay())
				{
					it->audioBusHandle()->effects()->appendEffect(e);
				}
			}
		}

		it->randomizeParameters(false);
		it->maxPolyphonyModel()->setValue(maxPolyphony);
		it->setMuted(false);
		it->setSolo(false);
		it->volumeModel()->setValue(75.0f + static_cast<float>(rand() % 25));
	}

	TextFloat::displayMessage(
		QObject::tr("Random Instrumentation"),
		QObject::tr("Generated %1 randomized instrument(s) with %2 effect(s) each!").arg(numTracks).arg(numEffects),
		embed::getIconPixmap("arp_random"),
		3000
	);
}

} // namespace lmms::gui

#endif // LMMS_GUI_RANDOM_INSTRUMENTATION_DIALOG_H
