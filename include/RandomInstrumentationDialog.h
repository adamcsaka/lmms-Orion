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

		auto formLayout = new QFormLayout();
		m_numInstrumentsSpin = new QSpinBox(this);
		m_numInstrumentsSpin->setRange(1, 32);
		m_numInstrumentsSpin->setValue(4);
		formLayout->addRow(tr("Number of instruments to generate:"), m_numInstrumentsSpin);

		m_numEffectsSpin = new QSpinBox(this);
		m_numEffectsSpin->setRange(0, 10);
		m_numEffectsSpin->setValue(2);
		formLayout->addRow(tr("Number of DSP effects per instrument:"), m_numEffectsSpin);

		mainLayout->addLayout(formLayout);

		auto buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
		connect(buttonBox, &QDialogButtonBox::accepted, this, &QDialog::accept);
		connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
		mainLayout->addWidget(buttonBox);
	}

	int instrumentCount() const { return m_numInstrumentsSpin->value(); }
	int effectCount() const { return m_numEffectsSpin->value(); }

private:
	QSpinBox* m_numInstrumentsSpin;
	QSpinBox* m_numEffectsSpin;
};

inline void generateRandomInstrumentation(TrackContainer* container, int numTracks, int numEffects)
{
	if (!container || numTracks <= 0) { return; }

	const char* synthPlugins[] = {
		"vital", "oxosynth", "tripleoscillator", "bitinvader", "monstro", "watsyn",
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
