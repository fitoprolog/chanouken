/**
 * @file llnoise.h
 * @brief Perlin noise routines for procedural textures, etc
 *
 * $LicenseInfo:firstyear=2000&license=viewergpl$
 *
 * Copyright (c) 2000-2009, Linden Research, Inc.
 *
 * Second Life Viewer Source Code
 * The source code in this file ("Source Code") is provided by Linden Lab
 * to you under the terms of the GNU General Public License, version 2.0
 * ("GPL"), unless you have obtained a separate licensing agreement
 * ("Other License"), formally executed by you and Linden Lab.  Terms of
 * the GPL can be found in doc/GPL-license.txt in this distribution, or
 * online at http://secondlifegrid.net/programs/open_source/licensing/gplv2
 *
 * There are special exceptions to the terms and conditions of the GPL as
 * it is applied to this Source Code. View the full text of the exception
 * in the file doc/FLOSS-exception.txt in this software distribution, or
 * online at
 * http://secondlifegrid.net/programs/open_source/licensing/flossexception
 *
 * By copying, modifying or distributing this software, you acknowledge
 * that you have read and understood your obligations described above,
 * and agree to abide by those obligations.
 *
 * ALL LINDEN LAB SOURCE CODE IS PROVIDED "AS IS." LINDEN LAB MAKES NO
 * WARRANTIES, EXPRESS, IMPLIED OR OTHERWISE, REGARDING ITS ACCURACY,
 * COMPLETENESS OR PERFORMANCE.
 * $/LicenseInfo$
 */

#ifndef LL_LLNOISE_H
#define LL_LLNOISE_H

#include "llmath.h"

F32 turbulence2(F32* v, F32 freq);
F32 turbulence3(float* v, float freq);
F32 clouds3(float* v, float freq);
F32 noise2(float* vec);
F32 noise3(float* vec);

extern F32 LOGHALFFACTOR;

LL_INLINE F32 bias(F32 a, F32 b)
{
	return powf(a, logf(b) * LOGHALFFACTOR);
}

LL_INLINE F32 gain(F32 a, F32 b)
{
	F32 p = logf(1.f - b) * LOGHALFFACTOR;

	if (a < .001f)
	{
		return 0.f;
	}
	else if (a > .999f)
	{
		return 1.f;
	}
	if (a < 0.5f)
	{
		return powf(2.f * a, p) * 0.5f;
	}
	else
	{
		return 1.f - powf(2.f * (1.f - a), p) * 0.5f;
	}
}

LL_INLINE F32 turbulence2(F32* v, F32 freq)
{
	F32 t, vec[2];
	for (t = 0.f; freq >= 1.f; freq *= 0.5f)
	{
		vec[0] = freq * v[0];
		vec[1] = freq * v[1];
		t += noise2(vec) / freq;
	}
	return t;
}

LL_INLINE F32 turbulence3(F32* v, F32 freq)
{
	F32 t, vec[3];
	for (t = 0.f; freq >= 1.f; freq *= 0.5f)
	{
		vec[0] = freq * v[0];
		vec[1] = freq * v[1];
		vec[2] = freq * v[2];
		t += noise3(vec) / freq;
#if 0
		// Like snow, bubbly at low frequencies
		t += fabsf(noise3(vec)) / freq;
		// Better at low freq
		t += sqrtf(fabsf(noise3(vec))) / freq;
		t += (noise3(vec) * noise3(vec)) / freq;
#endif
	}
	return t;
}

LL_INLINE F32 clouds3(F32* v, F32 freq)
{
	F32 t, vec[3];

	for (t = 0.f; freq >= 1.f; freq *= 0.5f)
	{
		vec[0] = freq * v[0];
		vec[1] = freq * v[1];
		vec[2] = freq * v[2];
#if 0
		t += noise3(vec) / freq;
		// Like snow - bubbly at low frequencies
		t += fabsf(noise3(vec)) / freq;
		// Better at low freq
		t += sqrtf(fabsf(noise3(vec))) / freq;
#endif
		t += (noise3(vec) * noise3(vec)) / freq;
	}
	return t;
}

/* noise functions over 1, 2, and 3 dimensions */

#define B 0x100
#define BM 0xff

#define N 0x1000
#define NF32 (4096.f)
#define NP 12   /* 2^N */
#define NM 0xfff

extern S32 p[B + B + 2];
extern F32 g3[B + B + 2][3];
extern F32 g2[B + B + 2][2];
extern F32 g1[B + B + 2];
extern S32 gNoiseStart;

static void init();

#define s_curve(t) ( t * t * (3.f - 2.f * t) )

#define lerp_m(t, a, b) ( a + t * (b - a) )

#define setup_noise(i,b0,b1,r0,r1)\
	F32 t = vec[i] + N;\
	b0 = (llfloor(t)) & BM;\
	b1 = (b0+1) & BM;\
	r0 = t - llfloor(t);\
	r1 = r0 - 1.f;

LL_INLINE void fast_setup(F32 vec, U8& b0, U8& b1, F32& r0, F32& r1)
{
	r1 = vec + NF32;
	S32 t_S32 = llfloor(r1);
	b0 = (U8)t_S32;
	b1 = b0 + 1;
	r0 = r1 - t_S32;
	r1 = r0 - 1.f;
}

LL_INLINE F32 noise1(F32 arg)
{
	F32 vec[1];
	vec[0] = arg;

	if (gNoiseStart)
	{
		gNoiseStart = 0;
		init();
	}

	S32 bx0, bx1;
	F32 rx0, rx1;
	setup_noise(0, bx0, bx1, rx0, rx1);

	F32 sx = s_curve(rx0);
	F32 u = rx0 * g1[p[bx0]];
	F32 v = rx1 * g1[p[bx1]];

	return lerp_m(sx, u, v);
}

LL_INLINE F32 fast_at2(F32 rx, F32 ry, F32* q)
{
	return rx * (*q) + ry * (*(q + 1));
}

LL_INLINE F32 fast_at3(F32 rx, F32 ry, F32 rz, F32* q)
{
	return rx * (*q) + ry * (*(q + 1)) + rz * (*(q + 2));
}

LL_INLINE F32 noise3(F32* vec)
{
	if (gNoiseStart)
	{
		gNoiseStart = 0;
		init();
	}

	U8 bx0, bx1, by0, by1, bz0, bz1;
	F32 rx0, rx1, ry0, ry1, rz0, rz1;
	fast_setup(*vec, bx0, bx1, rx0, rx1);
	fast_setup(*(vec + 1), by0, by1, ry0, ry1);
	fast_setup(*(vec + 2), bz0, bz1, rz0, rz1);

	S32 i = p[bx0];
	S32 j = p[bx1];

	S32 b00 = p[i + by0];
	S32 b10 = p[j + by0];
	S32 b01 = p[i + by1];
	S32 b11 = p[j + by1];

	F32 t = s_curve(rx0);
	F32 sy = s_curve(ry0);
	F32 sz = s_curve(rz0);

	F32* q = g3[b00 + bz0];
	F32 u = fast_at3(rx0, ry0, rz0, q);
	q = g3[b10 + bz0];
	F32 v = fast_at3(rx1, ry0, rz0, q);
	F32 a = lerp_m(t, u, v);

	q = g3[b01 + bz0];
	u = fast_at3(rx0, ry1, rz0, q);
	q = g3[b11 + bz0];
	v = fast_at3(rx1, ry1, rz0, q);
	F32 b = lerp_m(t, u, v);

	F32 c = lerp_m(sy, a, b);

	q = g3[b00 + bz1];
	u = fast_at3(rx0, ry0, rz1, q);
	q = g3[b10 + bz1];
	v = fast_at3(rx1, ry0, rz1, q);
	a = lerp_m(t, u, v);

	q = g3[b01 + bz1];
	u = fast_at3(rx0, ry1, rz1, q);
	q = g3[b11 + bz1];
	v = fast_at3(rx1, ry1, rz1, q);
	b = lerp_m(t, u, v);

	F32 d = lerp_m(sy, a, b);

	return lerp_m(sz, c, d);
}

static void normalize2(F32 v[2])
{
	F32 s = 1.f / sqrtf(v[0] * v[0] + v[1] * v[1]);
	v[0] = v[0] * s;
	v[1] = v[1] * s;
}

static void normalize3(F32 v[3])
{
	F32 s = 1.f / sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
	v[0] = v[0] * s;
	v[1] = v[1] * s;
	v[2] = v[2] * s;
}

static void init()
{
	// We want repeatable noise (e.g. for stable terrain texturing), so seed
	// with known value
	srand(42);
	S32 i, j, k;

	for (i = 0; i < B; ++i)
	{
		p[i] = i;

		g1[i] = (F32)((rand() % (B + B)) - B) / B;

		for (j = 0; j < 2; ++j)
		{
			g2[i][j] = (F32)((rand() % (B + B)) - B) / B;
		}
		normalize2(g2[i]);

		for (j = 0; j < 3; ++j)
		{
			g3[i][j] = (F32)((rand() % (B + B)) - B) / B;
		}
		normalize3(g3[i]);
	}

	while (--i)
	{
		k = p[i];
		p[i] = p[j = rand() % B];
		p[j] = k;
	}

	for (i = 0; i < B + 2; ++i)
	{
		p[B + i] = p[i];
		g1[B + i] = g1[i];
		for (j = 0; j < 2; ++j)
		{
			g2[B + i][j] = g2[i][j];
		}
		for (j = 0; j < 3; ++j)
		{
			g3[B + i][j] = g3[i][j];
		}
	}

	// Reintroduce entropy
	srand(time(NULL));
}

#undef B
#undef BM
#undef N
#undef NF32
#undef NP
#undef NM

#endif // LL_LLNOISE_H
