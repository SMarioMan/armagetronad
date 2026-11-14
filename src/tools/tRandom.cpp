/*

*************************************************************************

ArmageTron -- Just another Tron Lightcycle Game in 3D.
Copyright (C) 2000  Manuel Moos (manuel@moosnet.de)

**************************************************************************

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
  
***************************************************************************

*/

// declaration
#ifndef		TRANDOM_H_INCLUDED
#include	"tRandom.h"
#endif

#ifdef      HAVE_STDLIB
#include    <stdlib.h>
#endif

#include    "tError.h"
#include    "tRecorder.h"
#include    <cstdlib>
#include    <cstring>

#undef 	INLINE_DEF
#define INLINE_DEF

// *******************************************************************************************
// *
// *	tRandomizer
// *
// *******************************************************************************************
//!
//!
// *******************************************************************************************

tRandomizer::tRandomizer( void )
        :randMax_( RAND_MAX )
{
}

// *******************************************************************************************
// *
// *	Get
// *
// *******************************************************************************************
//!
//!		@return		random value between 0 and 1
//!
// *******************************************************************************************

REAL tRandomizer::Get( void )
{
    // Grr. VisualC 6 can't handle this in one line.
    REAL a = GetRawRand();
    REAL b = randMax_;

    return a / b;
}

// *******************************************************************************************
// *
// *	Get
// *
// *******************************************************************************************
//!
//!		@param	max	upper limit of the return value
//!		@return		random integer value between 0 and max - 1
//!
// *******************************************************************************************

int tRandomizer::Get( int max )
{
    int ret = int( floorf( Get() * max ) );
    tASSERT( 0 <= ret );
    tASSERT( ret < max );

    return ret;
}

// *******************************************************************************************
// *
// *	GetInstance
// *
// *******************************************************************************************
//!
//!		@return
//!
// *******************************************************************************************

tRandomizer & tRandomizer::GetInstance( void )
{
    static tRandomizer randomizer;
    return randomizer;
}

// *******************************************************************************************
// *
// *	~tRandomizer
// *
// *******************************************************************************************
//!
//!
// *******************************************************************************************

tRandomizer::~tRandomizer( void )
{
}

// *******************************************************************************************
// *
// *	tRandomizer
// *
// *******************************************************************************************
//!
//!		@param	other	copy source
//!
// *******************************************************************************************

tRandomizer::tRandomizer( tRandomizer const & other )
{
    tASSERT( 0 ); // not implemented
}

// *******************************************************************************************
// *
// *	operator =
// *
// *******************************************************************************************
//!
//!		@param	other	copy source
//!		@return		    reference to this for chaining
//!
// *******************************************************************************************

tRandomizer & tRandomizer::operator =( tRandomizer const & other )
{
    tASSERT( 0 ); // not implemented

    return *this;
}

// *******************************************************************************************
// *
// *	GetRawRand
// *
// *******************************************************************************************
//!
//!		@return		random value between 0 and randMax_
//!
// *******************************************************************************************

unsigned int tRandomizer::GetRawRand( void )
{
    return rand();
}


// *******************************************************************************************
// *
// *   tReproducibleRandomizer
// *
// *******************************************************************************************
//!
//!
// *******************************************************************************************
// Which generator number this is among the ones using the same archive section.
// Nothing else about the run - how many other generators exist, or in which
// order they were constructed - is allowed to move this number, because that is
// exactly what made the desktop build and the wasm build hand the computer
// players different seeds while replaying the same recording.
static unsigned int sectionInstance(char const *section)
{
    struct entry { char const *name; unsigned int count; };
    static entry instances[16];
    static int used = 0;

    for (int i = 0; i < used; i++)
        if (strcmp(instances[i].name, section) == 0)
            return instances[i].count++;

    if (used < 16)
    {
        instances[used].name = section;
        instances[used].count = 1;
        int const index = used;
        used++;
        return index;
    }

    return 0;
}

// Fallback seed for a recording that has nothing archived under this section:
// FNV-1a over the section name, salted with the instance number, then an
// avalanche step so two generators of the same section do not start from
// related states.  It depends on nothing but the section name and the instance,
// so every build derives the same one.
static int sectionSeed(char const *section, unsigned int instance, unsigned int salt)
{
    unsigned int h = 2166136261u ^ salt;

    for (char const *p = section; *p != '\0'; ++p)
    {
        h ^= static_cast<unsigned char>(*p);
        h *= 16777619u;
    }

    h ^= (instance + 1u) * 2654435761u;
    h ^= h >> 16;
    h *= 2246822519u;
    h ^= h >> 13;

    return static_cast<int>(h);
}


tReproducibleRandomizer::tReproducibleRandomizer( char const *section )
{
    unsigned int const instance = sectionInstance(section);
    bool const replaying = tRecorder::IsPlayingBack();

    randMax_ = 0xffffffff;

    // Archive seed: every generator keeps its seed under its own section, so
    // constructing another generator somewhere else cannot shift the values
    // this one reads back.  When the archive has nothing for this section - a
    // recording made before the section existed - a playback takes the
    // deterministic seed derived from the section name and the instance number
    // below rather than drawing from the standard generator, whose position in
    // its sequence depends on how many generators were built beforehand and is
    // therefore not the same in two different builds.  Outside a playback the
    // standard generator still provides the seed, and recording it is what lets
    // a later playback read it back instead of deriving it.
    bool archived_z = tRecorder::PlaybackStrict( section, z_ );
    if (!archived_z)
    {
        z_ = replaying ? sectionSeed(section, instance, 0x5a17u) : rand();
        tRecorder::Record( section, z_ );
    }

    bool archived_w = tRecorder::PlaybackStrict( section, w_ );
    if (!archived_w)
    {
        w_ = replaying ? sectionSeed(section, instance, 0x9e37u) : rand();
        tRecorder::Record( section, w_ );
    }

    /*
        // read RAND_MAX from recording or write it to recording
        if ( !tRecorder::PlaybackStrict( recordingSection, randMax_ ) )
            tRecorder::Record( recordingSection, randMax_ );
    */
}

// *******************************************************************************************
// *
// *	~tReproducibleRandomizer
// *
// *******************************************************************************************
//!
//!
// *******************************************************************************************

tReproducibleRandomizer::~tReproducibleRandomizer( void )
{
}

// *******************************************************************************************
// *
// *	GetInstance
// *
// *******************************************************************************************
//!
//!		@return		the default reproducible randomizer
//!
// *******************************************************************************************

tReproducibleRandomizer & tReproducibleRandomizer::GetInstance( void )
{
    static tReproducibleRandomizer randomizer;
    return randomizer;
}

// *******************************************************************************************
// *
// *	GetRawRand
// *
// *******************************************************************************************
//!
//!		@return		raw random value between 0 and randMax_
//!		todo use own, fixed random algorithm so the random values don't have to be stored
// *******************************************************************************************

unsigned int tReproducibleRandomizer::GetRawRand( void )
{
    // advance pseudo random numbers
    z_ = ( 36969 * ( z_ & 0xffff ) + ( z_ >> 16 ) ) & 0xffffffff;
    w_ = ( 18000 * ( w_ & 0xffff ) + ( w_ >> 16 ) ) & 0xffffffff;

    return ( (z_ << 16) + w_ ) & 0xffffffff;

    /*
        unsigned int r = 0;

        // read random value from recording
        if ( !tRecorder::PlaybackStrict( recordingSection, r ) )
        {
            // not found. generate random value
            r = rand();

            // store it
            tRecorder::Record( recordingSection, r );
        }

        return r;
        */
}

