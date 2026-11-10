/*
 * LfoGraph.cpp - Displays LFO graphs
 *
 * Copyright (c) 2004-2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2024-     Michael Gregorius
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

#include "LfoGraph.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <cmath>
#include <random>

#include "EnvelopeAndLfoParameters.h"
#include "Oscillator.h"
#include "ColorHelper.h"
#include "Engine.h"
#include "FontHelper.h"
#include "Song.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace lmms
{

extern const float SECS_PER_LFO_OSCILLATION;

namespace gui
{

LfoGraph::LfoGraph(QWidget* parent) :
	QWidget(parent),
	ModelView(nullptr, this),
	m_noAmountColor(96, 91, 96),
	m_fullAmountColor(0, 255, 128)
{
	setMinimumSize(m_lfoGraph.size());
}

void LfoGraph::mousePressEvent(QMouseEvent* me)
{
	if (me->button() == Qt::LeftButton)
	{
		toggleAmountModel();
	}
}

void LfoGraph::contextMenuEvent(QContextMenuEvent* ce)
{
	QMenu menu(this);
	menu.addAction(tr("Generate Random Harmonics Wave"), [this]{ randomizeUserWave(0); });
	menu.addAction(tr("Generate Smooth Random Spline"), [this]{ randomizeUserWave(2); });
	menu.addAction(tr("Generate Stepped Random (S&H)"), [this]{ randomizeUserWave(1); });
	menu.addSeparator();
	menu.addAction(tr("Smooth Current Waveform"), [this]{ randomizeUserWave(4); });
	menu.addAction(tr("Invert Current Waveform"), [this]{ randomizeUserWave(3); });
	menu.addSeparator();
	menu.addAction(tr("Toggle LFO Amount (0% / 100%)"), [this]{ toggleAmountModel(); update(); });
	menu.exec(ce->globalPos());
}

void LfoGraph::randomizeUserWave(int type)
{
	auto* params = castModel<EnvelopeAndLfoParameters>();
	if (!params) { return; }

	const int numFrames = 512;
	std::vector<SampleFrame> data(numFrames);

	if (type == 3) // Invert
	{
		auto currentWave = params->getLfoUserWave();
		if (currentWave && currentWave->size() >= 2)
		{
			data.resize(currentWave->size());
			for (size_t i = 0; i < currentWave->size(); ++i)
			{
				data[i][0] = -currentWave->data()[i][0];
				data[i][1] = -currentWave->data()[i][1];
			}
		}
		else
		{
			for (int i = 0; i < numFrames; ++i)
			{
				float phase = static_cast<float>(i) / numFrames * 2.0f * static_cast<float>(M_PI);
				float v = -std::sin(phase);
				data[i][0] = v;
				data[i][1] = v;
			}
		}
	}
	else if (type == 4) // Smooth
	{
		auto currentWave = params->getLfoUserWave();
		if (currentWave && currentWave->size() >= 4)
		{
			size_t sz = currentWave->size();
			data.resize(sz);
			for (size_t i = 0; i < sz; ++i)
			{
				size_t prev = (i + sz - 1) % sz;
				size_t next = (i + 1) % sz;
				float v0 = 0.25f * currentWave->data()[prev][0] + 0.5f * currentWave->data()[i][0] + 0.25f * currentWave->data()[next][0];
				float v1 = 0.25f * currentWave->data()[prev][1] + 0.5f * currentWave->data()[i][1] + 0.25f * currentWave->data()[next][1];
				data[i][0] = v0;
				data[i][1] = v1;
			}
		}
		else
		{
			return;
		}
	}
	else if (type == 1) // Stepped Random (Sample & Hold)
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
		const int steps = 16;
		const int stepSize = numFrames / steps;
		for (int s = 0; s < steps; ++s)
		{
			float v = dis(gen);
			for (int i = 0; i < stepSize; ++i)
			{
				int idx = s * stepSize + i;
				if (idx < numFrames)
				{
					data[idx][0] = v;
					data[idx][1] = v;
				}
			}
		}
	}
	else if (type == 2) // Smooth Random Spline
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> dis(-1.0f, 1.0f);
		const int numPoints = 8;
		float pts[9];
		for (int p = 0; p < numPoints; ++p)
		{
			pts[p] = dis(gen);
		}
		pts[numPoints] = pts[0]; // seamless periodic loop

		for (int i = 0; i < numFrames; ++i)
		{
			float pos = static_cast<float>(i) / numFrames * numPoints;
			int idx = static_cast<int>(pos);
			float frac = pos - idx;
			// cosine interpolation
			float mu2 = (1.0f - std::cos(frac * static_cast<float>(M_PI))) / 2.0f;
			float v = pts[idx] * (1.0f - mu2) + pts[idx + 1] * mu2;
			data[i][0] = v;
			data[i][1] = v;
		}
	}
	else // Type 0: Random Harmonics
	{
		std::random_device rd;
		std::mt19937 gen(rd());
		std::uniform_real_distribution<float> distAmp(0.1f, 1.0f);
		std::uniform_real_distribution<float> distPhase(0.0f, 2.0f * static_cast<float>(M_PI));

		const int numHarmonics = 6;
		float amps[6];
		float phases[6];
		float maxVal = 0.001f;

		for (int h = 0; h < numHarmonics; ++h)
		{
			amps[h] = distAmp(gen) / (h + 1.0f);
			phases[h] = distPhase(gen);
		}

		for (int i = 0; i < numFrames; ++i)
		{
			float phase = static_cast<float>(i) / numFrames * 2.0f * static_cast<float>(M_PI);
			float v = 0.0f;
			for (int h = 0; h < numHarmonics; ++h)
			{
				v += amps[h] * std::sin((h + 1) * phase + phases[h]);
			}
			data[i][0] = v;
			data[i][1] = v;
			maxVal = std::max(maxVal, std::abs(v));
		}

		for (int i = 0; i < numFrames; ++i)
		{
			data[i][0] /= maxVal;
			data[i][1] /= maxVal;
		}
	}

	auto newBuffer = std::make_shared<SampleBuffer>(std::move(data), Engine::audioEngine()->outputSampleRate(), "");
	params->setUserWave(newBuffer);
	params->getLfoWaveModel().setValue(static_cast<int>(EnvelopeAndLfoParameters::LfoShape::UserDefinedWave));
	if (Engine::getSong())
	{
		Engine::getSong()->setModified();
	}
	update();
}

void LfoGraph::paintEvent(QPaintEvent*)
{
	QPainter p{this};
	p.setRenderHint(QPainter::Antialiasing);

	// Draw the graph background
	p.drawPixmap(rect(), m_lfoGraph);

	const auto* params = castModel<EnvelopeAndLfoParameters>();
	if (!params) { return; }

	const float amount = params->getLfoAmountModel().value();
	const float lfoSpeed = params->getLfoSpeedModel().value();
	const f_cnt_t predelayFrames = params->getLfoPredelayFrames();
	const f_cnt_t attackFrames = params->getLfoAttackFrames();
	const f_cnt_t oscillationFrames = params->getLfoOscillationFrames();
	const bool x100 = params->getX100Model().value();
	const int lfoWaveModel = params->getLfoWaveModel().value();
	const auto * userWave = params->getLfoUserWave().get();

	const int margin = 3;
	const int lfoGraphWidth = width() - margin; // subtract margin
	const int lfoGraphHeight = height() - 2 * margin; // subtract margin
	int graphBaseX = 2;
	int graphBaseY = margin + lfoGraphHeight / 2;

	const float framesForGraph =
		SECS_PER_LFO_OSCILLATION * Engine::audioEngine()->baseSampleRate() / 10;

	float oscFrames = oscillationFrames * (x100 ? 100. : 1.);

	QPolygonF polyLine;
	polyLine << QPointF(graphBaseX - 1, graphBaseY);

	// Collect the points for the poly line by sampling the LFO according to its shape
	for (int x = 0; x <= lfoGraphWidth; ++x)
	{
		float value = 0.0;
		float currentSample = x * framesForGraph / lfoGraphWidth;
		const auto sampleAsFrameCount = static_cast<f_cnt_t>(currentSample);
		if (sampleAsFrameCount > predelayFrames)
		{
			currentSample -= predelayFrames;
			const float phase = currentSample / oscFrames;

			const auto lfoShape = static_cast<EnvelopeAndLfoParameters::LfoShape>(lfoWaveModel);
			switch (lfoShape)
			{
				case EnvelopeAndLfoParameters::LfoShape::SineWave:
				default:
					value = Oscillator::sinSample(phase);
					break;
				case EnvelopeAndLfoParameters::LfoShape::TriangleWave:
					value = Oscillator::triangleSample(phase);
					break;
				case EnvelopeAndLfoParameters::LfoShape::SawWave:
					value = Oscillator::sawSample(phase);
					break;
				case EnvelopeAndLfoParameters::LfoShape::SquareWave:
					value = Oscillator::squareSample(phase);
					break;
				case EnvelopeAndLfoParameters::LfoShape::RandomWave:
					if (x % (int)(900 * lfoSpeed + 1) == 0)
					{
						m_randomGraph = Oscillator::noiseSample(0.0);
					}
					value = m_randomGraph;
					break;
				case EnvelopeAndLfoParameters::LfoShape::UserDefinedWave:
					value = Oscillator::userWaveSample(userWave, phase);
					break;
			}

			if (sampleAsFrameCount <= attackFrames)
			{
				value *= currentSample / attackFrames;
			}
		}

		const float currentY = -lfoGraphHeight / 2.0f * value;

		polyLine << QPointF(graphBaseX + x, graphBaseY + currentY);
	}

	// Compute the color of the lines based on the amount of the LFO
	const float absAmount = std::abs(amount);
	const QColor lineColor{ColorHelper::interpolateInRgb(m_noAmountColor, m_fullAmountColor, absAmount)};

	p.setPen(QPen(lineColor, 1.5));

	p.drawPolyline(polyLine);

	drawInfoText(*params);
}

void LfoGraph::drawInfoText(const EnvelopeAndLfoParameters& params)
{
	QPainter p(this);

	const float lfoSpeed = params.getLfoSpeedModel().value();
	const bool x100 = params.getX100Model().value();

	const float hertz = 1. / (SECS_PER_LFO_OSCILLATION * lfoSpeed) * (x100 ? 100. : 1.);
	const auto infoText = tr("%1 Hz").arg(hertz, 0, 'f', 3);

	// First configure the font so that we get correct results for the font metrics used below
	QFont f = p.font();
	p.setFont(adjustedToPixelSize(f, height() * 0.2));

	// This is the position where the text and its rectangle will be rendered
	const QPoint textPosition(4, height() - 6);

	// Draw a slightly transparent black rectangle underneath the text to keep it legible
	const QFontMetrics fontMetrics(f);
	// This gives the bounding rectangle if the text was rendered at the origin ...
	const auto boundingRect = fontMetrics.boundingRect(infoText);
	// ... so we translate it to the actual position where the text will be rendered.
	p.fillRect(boundingRect.translated(textPosition), QColor{0, 0, 0, 192});

	// Now draw the actual info text
	p.setPen(QColor(201, 201, 225));
	p.drawText(textPosition, infoText);
}

void LfoGraph::toggleAmountModel()
{
	auto* params = castModel<EnvelopeAndLfoParameters>();
	auto& lfoAmountModel = params->getLfoAmountModel();

	lfoAmountModel.setValue(lfoAmountModel.value() < 1.0 ? 1.0 : 0.0);
}

} // namespace gui

} // namespace lmms
