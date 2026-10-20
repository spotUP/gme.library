// YM2413 FM sound chip emulator interface, backed by emu2413 (gme/ext).
// Upstream GME ships this file as a stub that rejects YM2413; gme.library
// replaces it so VGM files using the YM2413 play.
// Licensed like the rest of GME: LGPL-2.1 or (at your option) any later
// version; see COPYING.

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Ym2413_Emu.h"

#include "ext/emu2413.h"

Ym2413_Emu::Ym2413_Emu() { opll = 0; }

Ym2413_Emu::~Ym2413_Emu()
{
	if ( opll )
		OPLL_delete( (OPLL*) opll );
}

int Ym2413_Emu::set_rate( double sample_rate, double clock_rate )
{
	if ( opll )
	{
		OPLL_delete( (OPLL*) opll );
		opll = 0;
	}

	opll = OPLL_new( (uint32_t) clock_rate, (uint32_t) sample_rate );
	if ( !opll )
		return 1;

	reset();
	return 0;
}

void Ym2413_Emu::reset()
{
	OPLL_reset( (OPLL*) opll );
	OPLL_setMask( (OPLL*) opll, 0 );
}

void Ym2413_Emu::write( int addr, int data )
{
	OPLL_writeReg( (OPLL*) opll, addr, data );
}

void Ym2413_Emu::mute_voices( int mask )
{
	OPLL_setMask( (OPLL*) opll, mask );
}

void Ym2413_Emu::run( int pair_count, sample_t* out )
{
	while ( pair_count-- )
	{
		int s = OPLL_calc( (OPLL*) opll );
		out [0] = s;
		out [1] = s;
		out += 2;
	}
}
