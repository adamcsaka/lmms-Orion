/*
 * LfoControllerDialog.cpp - per-controller-specific view for changing a
 *                           controller's settings
 *
 * Copyright (c) 2008-2009 Paul Giblock <drfaygo/at/gmail.com>
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


#include "FileDialog.h"
#include "embed.h"

#include <cmath>
#include <functional>
#include <random>
#include <QDialog>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

#include "FontHelper.h"
#include "LfoController.h"
#include "Knob.h"
#include "Oscillator.h"
#include "TempoSyncKnob.h"
#include "PixmapButton.h"

namespace lmms::gui
{


const int CD_ENV_KNOBS_LBL_Y = 11;
const int CD_KNOB_X_SPACING = 32;

const int CD_LFO_SHAPES_X = 6;
const int CD_LFO_SHAPES_Y = CD_ENV_KNOBS_LBL_Y + 4;

const int CD_LFO_GRAPH_Y = CD_ENV_KNOBS_LBL_Y+3;
const int CD_LFO_CD_KNOB_Y = CD_LFO_GRAPH_Y-2;
const int CD_LFO_BASE_CD_KNOB_X = CD_LFO_SHAPES_X + 68;
const int CD_LFO_SPEED_CD_KNOB_X = CD_LFO_BASE_CD_KNOB_X+CD_KNOB_X_SPACING;
const int CD_LFO_AMOUNT_CD_KNOB_X = CD_LFO_SPEED_CD_KNOB_X+CD_KNOB_X_SPACING;
const int CD_LFO_PHASE_CD_KNOB_X = CD_LFO_AMOUNT_CD_KNOB_X+CD_KNOB_X_SPACING;
const int CD_LFO_MULTIPLIER_X = CD_LFO_PHASE_CD_KNOB_X+CD_KNOB_X_SPACING+3;

class WaveDrawWidget : public QWidget
{
public:
	std::function<void()> onWaveChanged;

	explicit WaveDrawWidget(const std::vector<float>& initSamples, QWidget* parent = nullptr) :
		QWidget(parent),
		m_samples(initSamples),
		m_lastIdx(-1),
		m_lastVal(0.0f)
	{
		if (m_samples.size() != 256)
		{
			m_samples.resize(256, 0.0f);
		}
		setMinimumSize(360, 150);
		setCursor(Qt::CrossCursor);
	}

	const std::vector<float>& samples() const { return m_samples; }

	void setSamples(const std::vector<float>& s)
	{
		m_samples = s;
		if (m_samples.size() != 256) { m_samples.resize(256, 0.0f); }
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void clearWave()
	{
		std::fill(m_samples.begin(), m_samples.end(), 0.0f);
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void smoothWave()
	{
		std::vector<float> smoothed = m_samples;
		const int n = static_cast<int>(m_samples.size());
		for (int pass = 0; pass < 2; ++pass)
		{
			for (int i = 0; i < n; ++i)
			{
				int prev = (i - 1 + n) % n;
				int next = (i + 1) % n;
				smoothed[i] = 0.25f * m_samples[prev] + 0.5f * m_samples[i] + 0.25f * m_samples[next];
			}
			m_samples = smoothed;
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void invertWave()
	{
		for (float& s : m_samples) { s = -s; }
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void setSine()
	{
		for (int i = 0; i < 256; ++i)
		{
			m_samples[i] = std::sin(2.0f * 3.1415926535f * i / 256.0f);
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void setTriangle()
	{
		for (int i = 0; i < 256; ++i)
		{
			float phase = static_cast<float>(i) / 256.0f;
			m_samples[i] = phase < 0.25f ? 4.0f * phase : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void setSaw()
	{
		for (int i = 0; i < 256; ++i)
		{
			m_samples[i] = 1.0f - 2.0f * (static_cast<float>(i) / 256.0f);
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void setSquare()
	{
		for (int i = 0; i < 256; ++i)
		{
			m_samples[i] = i < 128 ? 1.0f : -1.0f;
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void randomizeCurve()
	{
		std::mt19937 rng(1337 + static_cast<unsigned int>(std::time(nullptr)));
		std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
		std::uniform_real_distribution<float> phaseDist(0.0f, 6.2831853f);

		std::vector<float> harmAmp(5);
		std::vector<float> harmPhase(5);
		float sumAmp = 0.0f;
		for (int h = 0; h < 5; ++h)
		{
			harmAmp[h] = dist(rng) / (h + 1);
			harmPhase[h] = phaseDist(rng);
			sumAmp += std::abs(harmAmp[h]);
		}
		if (sumAmp < 0.001f) { sumAmp = 1.0f; }

		for (int i = 0; i < 256; ++i)
		{
			float val = 0.0f;
			for (int h = 0; h < 5; ++h)
			{
				val += harmAmp[h] * std::sin((h + 1) * 6.2831853f * i / 256.0f + harmPhase[h]);
			}
			m_samples[i] = std::clamp(val / sumAmp, -1.0f, 1.0f);
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	void randomizeSteps()
	{
		std::mt19937 rng(42 + static_cast<unsigned int>(std::time(nullptr)));
		std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
		const int stepSize = 16;
		for (int i = 0; i < 256; i += stepSize)
		{
			float level = dist(rng);
			for (int j = 0; j < stepSize && (i + j) < 256; ++j)
			{
				m_samples[i + j] = level;
			}
		}
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

protected:
	void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		p.setRenderHint(QPainter::Antialiasing);

		const int w = width();
		const int h = height();
		const float midY = h / 2.0f;

		// Background
		p.fillRect(rect(), QColor(26, 28, 34));

		// Grid lines
		p.setPen(QPen(QColor(50, 54, 66), 1, Qt::DashLine));
		p.drawLine(0, static_cast<int>(midY), w, static_cast<int>(midY));
		p.drawLine(w / 4, 0, w / 4, h);
		p.drawLine(w / 2, 0, w / 2, h);
		p.drawLine(3 * w / 4, 0, 3 * w / 4, h);

		// Waveform path
		QPainterPath path;
		QPainterPath fillPath;
		fillPath.moveTo(0, midY);

		const int n = static_cast<int>(m_samples.size());
		for (int i = 0; i < n; ++i)
		{
			float x = static_cast<float>(i) * w / (n - 1);
			float y = midY - (m_samples[i] * (midY - 4.0f));
			if (i == 0)
			{
				path.moveTo(x, y);
				fillPath.lineTo(x, y);
			}
			else
			{
				path.lineTo(x, y);
				fillPath.lineTo(x, y);
			}
		}
		fillPath.lineTo(w, midY);
		fillPath.closeSubpath();

		// Fill
		p.fillPath(fillPath, QColor(0, 210, 255, 35));

		// Line
		p.setPen(QPen(QColor(0, 210, 255), 2.0f));
		p.drawPath(path);
	}

	void mousePressEvent(QMouseEvent* me) override
	{
		if (me->button() == Qt::LeftButton)
		{
			setPointFromMouse(me->pos(), true);
		}
	}

	void mouseMoveEvent(QMouseEvent* me) override
	{
		if (me->buttons() & Qt::LeftButton)
		{
			setPointFromMouse(me->pos(), false);
		}
	}

	void mouseReleaseEvent(QMouseEvent*) override
	{
		m_lastIdx = -1;
	}

private:
	void setPointFromMouse(const QPoint& pt, bool initial)
	{
		const int w = width();
		const int h = height();
		if (w <= 0 || h <= 0) { return; }

		const float midY = h / 2.0f;
		int idx = std::clamp(static_cast<int>(pt.x() * 256 / w), 0, 255);
		float val = std::clamp((midY - pt.y()) / (midY - 4.0f), -1.0f, 1.0f);

		if (initial || m_lastIdx < 0)
		{
			m_samples[idx] = val;
		}
		else
		{
			int start = std::min(m_lastIdx, idx);
			int end = std::max(m_lastIdx, idx);
			float startVal = (m_lastIdx <= idx) ? m_lastVal : val;
			float endVal = (m_lastIdx <= idx) ? val : m_lastVal;

			for (int i = start; i <= end; ++i)
			{
				float t = (start == end) ? 0.0f : static_cast<float>(i - start) / (end - start);
				m_samples[i] = startVal + t * (endVal - startVal);
			}
		}

		m_lastIdx = idx;
		m_lastVal = val;
		update();
		if (onWaveChanged) { onWaveChanged(); }
	}

	std::vector<float> m_samples;
	int m_lastIdx;
	float m_lastVal;
};

class WaveDrawDialog : public QDialog
{
public:
	explicit WaveDrawDialog(LfoController* lfo, QWidget* parent = nullptr) :
		QDialog(parent),
		m_lfo(lfo)
	{
		setWindowTitle(tr("LFO Controller - Waveform Drawer"));
		setWindowIcon(embed::getIconPixmap("controller"));
		setMinimumSize(420, 260);

		auto layout = new QVBoxLayout(this);

		std::vector<float> initSamples(256, 0.0f);
		if (m_lfo && m_lfo->userDefSampleBuffer() && !m_lfo->userDefSampleBuffer()->empty())
		{
			auto buf = m_lfo->userDefSampleBuffer();
			for (size_t i = 0; i < 256; ++i)
			{
				float phase = static_cast<float>(i) / 256.0f;
				initSamples[i] = Oscillator::userWaveSample(buf.get(), phase);
			}
		}
		else
		{
			for (int i = 0; i < 256; ++i)
			{
				initSamples[i] = std::sin(2.0f * 3.1415926535f * i / 256.0f);
			}
		}

		m_drawer = new WaveDrawWidget(initSamples, this);
		layout->addWidget(m_drawer);

		m_drawer->onWaveChanged = [this]() {
			if (m_lfo)
			{
				m_lfo->setCustomWave(m_drawer->samples());
			}
		};

		// Button bar 1: Presets
		auto btnLayout1 = new QHBoxLayout();
		auto sineBtn = new QPushButton(tr("Sine"), this);
		auto triBtn = new QPushButton(tr("Triangle"), this);
		auto sawBtn = new QPushButton(tr("Saw"), this);
		auto sqrBtn = new QPushButton(tr("Square"), this);
		auto randBtn = new QPushButton(tr("Random"), this);
		auto stepBtn = new QPushButton(tr("Steps"), this);

		connect(sineBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::setSine);
		connect(triBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::setTriangle);
		connect(sawBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::setSaw);
		connect(sqrBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::setSquare);
		connect(randBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::randomizeCurve);
		connect(stepBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::randomizeSteps);

		btnLayout1->addWidget(sineBtn);
		btnLayout1->addWidget(triBtn);
		btnLayout1->addWidget(sawBtn);
		btnLayout1->addWidget(sqrBtn);
		btnLayout1->addWidget(randBtn);
		btnLayout1->addWidget(stepBtn);
		layout->addLayout(btnLayout1);

		// Button bar 2: Operations
		auto btnLayout2 = new QHBoxLayout();
		auto smoothBtn = new QPushButton(tr("Smooth"), this);
		auto invertBtn = new QPushButton(tr("Invert"), this);
		auto clearBtn = new QPushButton(tr("Clear"), this);
		auto loadWavBtn = new QPushButton(tr("Load .WAV..."), this);
		auto closeBtn = new QPushButton(tr("Close"), this);

		connect(smoothBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::smoothWave);
		connect(invertBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::invertWave);
		connect(clearBtn, &QPushButton::clicked, m_drawer, &WaveDrawWidget::clearWave);
		connect(loadWavBtn, &QPushButton::clicked, [this]() {
			const auto fileName = FileDialog::openWaveformFile();
			if (!fileName.isEmpty() && m_lfo)
			{
				auto buf = SampleBuffer::fromFile(fileName);
				std::vector<float> s(256);
				for (size_t i = 0; i < 256; ++i)
				{
					s[i] = Oscillator::userWaveSample(buf.get(), static_cast<float>(i) / 256.0f);
				}
				m_drawer->setSamples(s);
			}
		});
		connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);

		btnLayout2->addWidget(smoothBtn);
		btnLayout2->addWidget(invertBtn);
		btnLayout2->addWidget(clearBtn);
		btnLayout2->addWidget(loadWavBtn);
		btnLayout2->addWidget(closeBtn);
		layout->addLayout(btnLayout2);
	}

private:
	LfoController* m_lfo;
	WaveDrawWidget* m_drawer;
};

LfoControllerDialog::LfoControllerDialog( Controller * _model, QWidget * _parent ) :
	ControllerDialog( _model, _parent )
{
	QString title = tr( "LFO" );
	title.append( " (" );
	title.append( _model->name() );
	title.append( ")" );
	setWindowTitle( title );
	setWindowIcon( embed::getIconPixmap( "controller" ) );
	setFixedSize( 285, 58 );
	
	m_baseKnob = new Knob(KnobType::Bright26, tr("BASE"), SMALL_FONT_SIZE, this);
	m_baseKnob->move( CD_LFO_BASE_CD_KNOB_X, CD_LFO_CD_KNOB_Y );
	m_baseKnob->setHintText( tr( "Base:" ), "" );

	m_speedKnob = new TempoSyncKnob(KnobType::Bright26, tr("FREQ"), SMALL_FONT_SIZE, this);
	m_speedKnob->move( CD_LFO_SPEED_CD_KNOB_X, CD_LFO_CD_KNOB_Y );
	m_speedKnob->setHintText( tr( "LFO frequency:" ), "" );

	m_amountKnob = new Knob(KnobType::Bright26, tr("AMNT"), SMALL_FONT_SIZE, this);
	m_amountKnob->move( CD_LFO_AMOUNT_CD_KNOB_X, CD_LFO_CD_KNOB_Y );
	m_amountKnob->setHintText( tr( "Modulation amount:" ), "" );

	m_phaseKnob = new Knob(KnobType::Bright26, tr("PHS"), SMALL_FONT_SIZE, this);
	m_phaseKnob->move( CD_LFO_PHASE_CD_KNOB_X, CD_LFO_CD_KNOB_Y );
	m_phaseKnob->setHintText( tr( "Phase offset:" ) , "" + tr( " degrees" ) );

	auto sin_wave_btn = new PixmapButton(this, nullptr);
	sin_wave_btn->move( CD_LFO_SHAPES_X, CD_LFO_SHAPES_Y );
	sin_wave_btn->setActiveGraphic( embed::getIconPixmap(
						"sin_wave_active" ) );
	sin_wave_btn->setInactiveGraphic( embed::getIconPixmap(
						"sin_wave_inactive" ) );
	sin_wave_btn->setToolTip(
			tr( "Sine wave" ) );

	auto triangle_wave_btn = new PixmapButton(this, nullptr);
	triangle_wave_btn->move( CD_LFO_SHAPES_X + 15, CD_LFO_SHAPES_Y );
	triangle_wave_btn->setActiveGraphic(
		embed::getIconPixmap( "triangle_wave_active" ) );
	triangle_wave_btn->setInactiveGraphic(
		embed::getIconPixmap( "triangle_wave_inactive" ) );
	triangle_wave_btn->setToolTip(
			tr( "Triangle wave" ) );

	auto saw_wave_btn = new PixmapButton(this, nullptr);
	saw_wave_btn->move( CD_LFO_SHAPES_X + 30, CD_LFO_SHAPES_Y );
	saw_wave_btn->setActiveGraphic( embed::getIconPixmap(
						"saw_wave_active" ) );
	saw_wave_btn->setInactiveGraphic( embed::getIconPixmap(
						"saw_wave_inactive" ) );
	saw_wave_btn->setToolTip(
			tr( "Saw wave" ) );

	auto sqr_wave_btn = new PixmapButton(this, nullptr);
	sqr_wave_btn->move( CD_LFO_SHAPES_X + 45, CD_LFO_SHAPES_Y );
	sqr_wave_btn->setActiveGraphic( embed::getIconPixmap(
					"square_wave_active" ) );
	sqr_wave_btn->setInactiveGraphic( embed::getIconPixmap(
					"square_wave_inactive" ) );
	sqr_wave_btn->setToolTip(
			tr( "Square wave" ) );

	auto moog_saw_wave_btn = new PixmapButton(this, nullptr);
	moog_saw_wave_btn->move( CD_LFO_SHAPES_X, CD_LFO_SHAPES_Y + 15 );
	moog_saw_wave_btn->setActiveGraphic(
		embed::getIconPixmap( "moog_saw_wave_active" ) );
	moog_saw_wave_btn->setInactiveGraphic(
		embed::getIconPixmap( "moog_saw_wave_inactive" ) );
	moog_saw_wave_btn->setToolTip(
			tr( "Moog saw wave" ) );

	auto exp_wave_btn = new PixmapButton(this, nullptr);
	exp_wave_btn->move( CD_LFO_SHAPES_X + 15, CD_LFO_SHAPES_Y + 15 );
	exp_wave_btn->setActiveGraphic( embed::getIconPixmap(
						"exp_wave_active" ) );
	exp_wave_btn->setInactiveGraphic( embed::getIconPixmap(
						"exp_wave_inactive" ) );
	exp_wave_btn->setToolTip(
			tr( "Exponential wave" ) );

	auto white_noise_btn = new PixmapButton(this, nullptr);
	white_noise_btn->move( CD_LFO_SHAPES_X + 30, CD_LFO_SHAPES_Y + 15 );
	white_noise_btn->setActiveGraphic(
		embed::getIconPixmap( "white_noise_wave_active" ) );
	white_noise_btn->setInactiveGraphic(
		embed::getIconPixmap( "white_noise_wave_inactive" ) );
	white_noise_btn->setToolTip(
				tr( "White noise" ) );

	m_userWaveBtn = new PixmapButton( this, nullptr );
	m_userWaveBtn->move( CD_LFO_SHAPES_X + 45, CD_LFO_SHAPES_Y + 15 );
	m_userWaveBtn->setActiveGraphic( embed::getIconPixmap(
						"usr_wave_active" ) );
	m_userWaveBtn->setInactiveGraphic( embed::getIconPixmap(
						"usr_wave_inactive" ) );
	connect( m_userWaveBtn,
					SIGNAL(doubleClicked()),
			this, SLOT(askUserDefWave()));
	m_userWaveBtn->setToolTip(
				tr( "User-defined shape.\nDouble click to pick a file." ) );
	
	m_waveBtnGrp = new AutomatableButtonGroup( this );
	m_waveBtnGrp->addButton( sin_wave_btn );
	m_waveBtnGrp->addButton( triangle_wave_btn );
	m_waveBtnGrp->addButton( saw_wave_btn );
	m_waveBtnGrp->addButton( sqr_wave_btn );
	m_waveBtnGrp->addButton( moog_saw_wave_btn );
	m_waveBtnGrp->addButton( exp_wave_btn );
	m_waveBtnGrp->addButton( white_noise_btn );
	m_waveBtnGrp->addButton( m_userWaveBtn );

	auto x1 = new PixmapButton(this, nullptr);
	x1->move(CD_LFO_MULTIPLIER_X, CD_LFO_SHAPES_Y + 7);
	x1->setActiveGraphic(embed::getIconPixmap("lfo_x1_active"));
	x1->setInactiveGraphic(embed::getIconPixmap("lfo_x1_inactive"));
	x1->setToolTip(tr("Multiply modulation frequency by 1"));

	auto x100 = new PixmapButton(this, nullptr);
	x100->move(CD_LFO_MULTIPLIER_X, CD_LFO_SHAPES_Y - 8);
	x100->setActiveGraphic(embed::getIconPixmap("lfo_x100_active"));
	x100->setInactiveGraphic(embed::getIconPixmap("lfo_x100_inactive"));
	x100->setToolTip(tr("Multiply modulation frequency by 100"));

	auto d100 = new PixmapButton(this, nullptr);
	d100->move(CD_LFO_MULTIPLIER_X, CD_LFO_SHAPES_Y + 22);
	d100->setActiveGraphic(embed::getIconPixmap("lfo_d100_active"));
	d100->setInactiveGraphic(embed::getIconPixmap("lfo_d100_inactive"));
	d100->setToolTip(tr("Divide modulation frequency by 100"));

	m_multiplierBtnGrp = new AutomatableButtonGroup( this );
	m_multiplierBtnGrp->addButton( x1 );
	m_multiplierBtnGrp->addButton( x100 );
	m_multiplierBtnGrp->addButton( d100 );

	m_freeRunBtn = new QPushButton( tr("FREE"), this );
	m_freeRunBtn->move( 228, 5 );
	m_freeRunBtn->setFixedSize( 52, 22 );
	m_freeRunBtn->setCheckable( true );
	m_freeRunBtn->setToolTip( tr("Free run mode: oscillate continuously without sync to playback") );

	m_drawWaveBtn = new QPushButton( tr("DRAW"), this );
	m_drawWaveBtn->move( 228, 30 );
	m_drawWaveBtn->setFixedSize( 52, 22 );
	m_drawWaveBtn->setToolTip( tr("Draw custom LFO waveform shape") );

	connect( m_drawWaveBtn, &QPushButton::clicked, this, &LfoControllerDialog::openWaveDrawer );
	connect( m_freeRunBtn, &QPushButton::toggled, [this](bool checked) {
		if (m_lfo) { m_lfo->freeRunModel()->setValue(checked); }
	});


	setModel( _model );

	setAutoFillBackground( true );
	QPalette pal;
	pal.setBrush( backgroundRole(), embed::getIconPixmap( "lfo_controller_artwork" ) );
	setPalette( pal );

}



LfoControllerDialog::~LfoControllerDialog()
{
	m_userWaveBtn->disconnect( this );
}



void LfoControllerDialog::askUserDefWave()
{
	openWaveDrawer();
}

void LfoControllerDialog::openWaveDrawer()
{
	if (!m_lfo) { m_lfo = castModel<LfoController>(); }
	if (!m_lfo) { return; }
	WaveDrawDialog dlg(m_lfo, this);
	dlg.exec();
}



void LfoControllerDialog::contextMenuEvent( QContextMenuEvent * )
{
	/*
	QPointer<captionMenu> contextMenu = new captionMenu(
						getEffect()->publicName() );
		// ^ if this ever gets activated again:
		//   a47a36c86e4473af862df58d2b59f6cb8620b9ee says that publicName
		//   should be replaced by Model::displayName
	contextMenu->addAction( embed::getIconPixmap( "arp_up_on" ),
						tr( "Move &up" ),
						this, SLOT(moveUp()));
	contextMenu->addAction( embed::getIconPixmap( "arp_down_on" ),
						tr( "Move &down" ),
						this, SLOT(moveDown()));
	contextMenu->addSeparator();
	contextMenu->addAction( embed::getIconPixmap( "cancel" ),
						tr( "&Remove this plugin" ),
						this, SLOT(deletePlugin()));
	contextMenu->exec( QCursor::pos() );
	delete contextMenu;
	*/
}



void LfoControllerDialog::modelChanged()
{
	m_lfo = castModel<LfoController>();

	m_baseKnob->setModel( &m_lfo->m_baseModel );
	m_speedKnob->setModel( &m_lfo->m_speedModel );
	m_amountKnob->setModel( &m_lfo->m_amountModel );
	m_phaseKnob->setModel( &m_lfo->m_phaseModel );
	m_waveBtnGrp->setModel( &m_lfo->m_waveModel );
	m_multiplierBtnGrp->setModel( &m_lfo->m_multiplierModel );
	m_freeRunBtn->setChecked( m_lfo->freeRunModel()->value() );
}


} // namespace lmms::gui
