/*
 * MixHelpers.cpp - helper functions for mixing buffers
 *
 * Copyright (c) 2014 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "MixHelpers.h"

#include <algorithm>
#include <cmath>

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#include <immintrin.h>
#ifdef _MSC_VER
#include <intrin.h>
#endif
#endif

#include "ValueBuffer.h"
#include "SampleFrame.h"

namespace lmms::MixHelpers
{

namespace {

struct CpuFeatures
{
	bool hasAVX2 = false;

	CpuFeatures()
	{
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
		int cpuInfo[4] = {0};
		__cpuid(cpuInfo, 0);
		int nIds = cpuInfo[0];
		if (nIds >= 7)
		{
			__cpuidex(cpuInfo, 7, 0);
			hasAVX2 = (cpuInfo[1] & (1 << 5)) != 0;
		}
#endif
	}
};

static const CpuFeatures s_cpu;

constexpr auto SilenceThreshold = 0.000001f; // -120 dBFS

/*! \brief Function for applying MIXOP on all sample frames */
template<typename MIXOP>
inline void run(SampleFrame* dst, const SampleFrame* src, int frames, const MIXOP& OP)
{
	for( int i = 0; i < frames; ++i )
	{
		OP( dst[i], src[i] );
	}
}

/*! \brief Function for applying MIXOP on all sample frames - split source */
template<typename MIXOP>
inline void run(SampleFrame* dst, const sample_t* srcLeft, const sample_t* srcRight, int frames, const MIXOP& OP)
{
	for( int i = 0; i < frames; ++i )
	{
		const SampleFrame src = { srcLeft[i], srcRight[i] };
		OP( dst[i], src );
	}
}

} // namespace

bool isSilent(const SampleFrame* src, int frames)
{
	return isSilent({&src[0][0], static_cast<std::size_t>(frames * 2)});
}

bool isSilent(std::span<const sample_t> buffer)
{
	return std::ranges::all_of(buffer, [&](const sample_t s) { return std::abs(s) < SilenceThreshold; });
}

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
#if defined(__GNUC__) || defined(__clang__)
#define LMMS_AVX2_TARGET __attribute__((target("avx2")))
#else
#define LMMS_AVX2_TARGET
#endif

LMMS_AVX2_TARGET
static inline void add_loop_avx2(float* d, const float* s, int& i, int totalFloats)
{
	for (; i + 8 <= totalFloats; i += 8)
	{
		__m256 vd = _mm256_loadu_ps(d + i);
		__m256 vs = _mm256_loadu_ps(s + i);
		_mm256_storeu_ps(d + i, _mm256_add_ps(vd, vs));
	}
}

LMMS_AVX2_TARGET
static inline void add_multiplied_loop_avx2(float* d, const float* s, float coeffSrc, int& i, int totalFloats)
{
	__m256 vc = _mm256_set1_ps(coeffSrc);
	for (; i + 8 <= totalFloats; i += 8)
	{
		__m256 vd = _mm256_loadu_ps(d + i);
		__m256 vs = _mm256_loadu_ps(s + i);
		_mm256_storeu_ps(d + i, _mm256_add_ps(vd, _mm256_mul_ps(vs, vc)));
	}
}

LMMS_AVX2_TARGET
static inline void multiply_loop_avx2(float* d, float coeff, int& i, int totalFloats)
{
	__m256 vc = _mm256_set1_ps(coeff);
	for (; i + 8 <= totalFloats; i += 8)
	{
		__m256 vd = _mm256_loadu_ps(d + i);
		_mm256_storeu_ps(d + i, _mm256_mul_ps(vd, vc));
	}
}
#endif

struct AddOp
{
	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst += src;
	}
} ;

void add( SampleFrame* dst, const SampleFrame* src, int frames )
{
	const int totalFloats = frames * 2;
	auto* d = reinterpret_cast<float*>(dst);
	const auto* s = reinterpret_cast<const float*>(src);
	int i = 0;

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
	if (s_cpu.hasAVX2)
	{
		add_loop_avx2(d, s, i, totalFloats);
	}
	else
	{
		for (; i + 4 <= totalFloats; i += 4)
		{
			__m128 vd = _mm_loadu_ps(d + i);
			__m128 vs = _mm_loadu_ps(s + i);
			_mm_storeu_ps(d + i, _mm_add_ps(vd, vs));
		}
	}
#endif

	for (; i < totalFloats; ++i)
	{
		d[i] += s[i];
	}
}


void add(PlanarBufferView<sample_t> dst, PlanarBufferView<const sample_t> src)
{
	assert(dst.channels() == src.channels());
	assert(dst.frames() == src.frames());

	const auto channels = dst.channels();
	const auto frames = dst.frames();
	for (ch_cnt_t channel = 0; channel < channels; ++channel)
	{
		auto* dstPtr = dst.bufferPtr(channel);
		const auto* srcPtr = src.bufferPtr(channel);
		for (f_cnt_t frame = 0; frame < frames; ++frame)
		{
			dstPtr[frame] += srcPtr[frame];
		}
	}
}


struct AddMultipliedOp
{
	AddMultipliedOp( float coeff ) : m_coeff( coeff ) { }

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst += src * m_coeff;
	}

	const float m_coeff;
} ;


void addMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffSrc, int frames )
{
	const int totalFloats = frames * 2;
	auto* d = reinterpret_cast<float*>(dst);
	const auto* s = reinterpret_cast<const float*>(src);
	int i = 0;

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
	if (s_cpu.hasAVX2)
	{
		add_multiplied_loop_avx2(d, s, coeffSrc, i, totalFloats);
	}
	else
	{
		__m128 vc = _mm_set1_ps(coeffSrc);
		for (; i + 4 <= totalFloats; i += 4)
		{
			__m128 vd = _mm_loadu_ps(d + i);
			__m128 vs = _mm_loadu_ps(s + i);
			_mm_storeu_ps(d + i, _mm_add_ps(vd, _mm_mul_ps(vs, vc)));
		}
	}
#endif

	for (; i < totalFloats; ++i)
	{
		d[i] += s[i] * coeffSrc;
	}
}


struct AddSwappedMultipliedOp
{
	AddSwappedMultipliedOp( float coeff ) : m_coeff( coeff ) { }

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst[0] += src[1] * m_coeff;
		dst[1] += src[0] * m_coeff;
	}

	const float m_coeff;
};

void multiply(SampleFrame* dst, float coeff, int frames)
{
	const int totalFloats = frames * 2;
	auto* d = reinterpret_cast<float*>(dst);
	int i = 0;

#if defined(_M_X64) || defined(__x86_64__) || defined(_M_IX86) || defined(__i386__)
	if (s_cpu.hasAVX2)
	{
		multiply_loop_avx2(d, coeff, i, totalFloats);
	}
	else
	{
		__m128 vc = _mm_set1_ps(coeff);
		for (; i + 4 <= totalFloats; i += 4)
		{
			__m128 vd = _mm_loadu_ps(d + i);
			_mm_storeu_ps(d + i, _mm_mul_ps(vd, vc));
		}
	}
#endif

	for (; i < totalFloats; ++i)
	{
		d[i] *= coeff;
	}
}

void addSwappedMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffSrc, int frames )
{
	run<>( dst, src, frames, AddSwappedMultipliedOp(coeffSrc) );
}


void addMultipliedByBuffer( SampleFrame* dst, const SampleFrame* src, float coeffSrc, ValueBuffer * coeffSrcBuf, int frames )
{
	for( int f = 0; f < frames; ++f )
	{
		dst[f][0] += src[f][0] * coeffSrc * coeffSrcBuf->values()[f];
		dst[f][1] += src[f][1] * coeffSrc * coeffSrcBuf->values()[f];
	}
}

void addMultipliedByBuffers( SampleFrame* dst, const SampleFrame* src, ValueBuffer * coeffSrcBuf1, ValueBuffer * coeffSrcBuf2, int frames )
{
	for( int f = 0; f < frames; ++f )
	{
		dst[f][0] += src[f][0] * coeffSrcBuf1->values()[f] * coeffSrcBuf2->values()[f];
		dst[f][1] += src[f][1] * coeffSrcBuf1->values()[f] * coeffSrcBuf2->values()[f];
	}

}

struct AddMultipliedStereoOp
{
	AddMultipliedStereoOp( float coeffLeft, float coeffRight )
	{
		m_coeffs[0] = coeffLeft;
		m_coeffs[1] = coeffRight;
	}

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst[0] += src[0] * m_coeffs[0];
		dst[1] += src[1] * m_coeffs[1];
	}

	std::array<float, 2> m_coeffs;
} ;


void addMultipliedStereo( SampleFrame* dst, const SampleFrame* src, float coeffSrcLeft, float coeffSrcRight, int frames )
{

	run<>( dst, src, frames, AddMultipliedStereoOp(coeffSrcLeft, coeffSrcRight) );
}





struct MultiplyAndAddMultipliedOp
{
	MultiplyAndAddMultipliedOp( float coeffDst, float coeffSrc )
	{
		m_coeffs[0] = coeffDst;
		m_coeffs[1] = coeffSrc;
	}

	void operator()( SampleFrame& dst, const SampleFrame& src ) const
	{
		dst[0] = dst[0]*m_coeffs[0] + src[0]*m_coeffs[1];
		dst[1] = dst[1]*m_coeffs[0] + src[1]*m_coeffs[1];
	}

	std::array<float, 2> m_coeffs;
} ;


void multiplyAndAddMultiplied( SampleFrame* dst, const SampleFrame* src, float coeffDst, float coeffSrc, int frames )
{
	run<>( dst, src, frames, MultiplyAndAddMultipliedOp(coeffDst, coeffSrc) );
}



void multiplyAndAddMultipliedJoined( SampleFrame* dst,
										const sample_t* srcLeft,
										const sample_t* srcRight,
										float coeffDst, float coeffSrc, int frames )
{
	run<>( dst, srcLeft, srcRight, frames, MultiplyAndAddMultipliedOp(coeffDst, coeffSrc) );
}

} // namespace lmms::MixHelpers

