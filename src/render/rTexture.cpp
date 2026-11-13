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

#include "rSDL.h"

#include "config.h"

#include "rTexture.h"
#include "rDisplayList.h"
#include "tString.h"
#include "rScreen.h"
#include "tDirectories.h"
#include "tLocale.h"
#include "tException.h"
#include "tResourceManager.h"

#include <sstream>
#ifdef __EMSCRIPTEN__
#include <stdlib.h>
#include <vector>
#endif

#ifndef DEDICATED
#include "rRender.h"
#include "rGL.h"

// Load the right SDL_IMAGE header

#ifdef _MSC_VER
#include <SDL_image.h>
#else
#ifdef __MINGW32__
#include <SDL_image.h>
#else
#ifdef HAVE_SDL_IMG_H
#include <SDL_image.h>
#else
#ifdef HAVE_SDL_SDL_IMAGE_H
#include <SDL/SDL_image.h>
#else
#ifdef HAVE_IMG_H
#include <IMG.h>
#else
#ifdef HAVE_SDL_IMG_H
#include <SDL/IMG.h>
#else
#ifdef HAVE_LIBSDL
#include <SDL_image.h>
#else
#ifdef HAVE_LIBIMG
#include <IMG.h>
#else
// if the following include ( or one of the earlier ones ) fails, you don't have SDL_image properly installed.
#include <SDL_image.h>
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif
#endif


// ******************************************************************************************
// *
// *	rSurface
// *
// ******************************************************************************************
//!
//!		@param	fileName	name of the file to load sufrace from
//!
// ******************************************************************************************

rSurface::rSurface( char const * fileName )
{
    Init();
    Create( fileName );
}

// ******************************************************************************************
// *
// *	~rSurface
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rSurface::~rSurface( void )
{
    Clear();
}

// ******************************************************************************************
// *
// *	rSurface
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rSurface::rSurface( void )
{
    Init();
}

// ******************************************************************************************
// *
// *   rSurface
// *
// ******************************************************************************************
//!
//!        @param  other   source to copy from
//!
// ******************************************************************************************

rSurface::rSurface( rSurface const & other )
{
    Init();
    CopyFrom( other );
}

// ******************************************************************************************
// *
// *   operator =
// *
// ******************************************************************************************
//!
//!        @param  other
//!        @return
//!
// ******************************************************************************************

rSurface & rSurface::operator =( rSurface const & other )
{
    if ( &other != this )
    {
        Clear();
        CopyFrom( other );
    }

    return *this;
}

// ******************************************************************************************
// *
// *	Init
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rSurface::Init( void )
{
    surface_ = 0;
    format_ = 0;
}

// ******************************************************************************************
// *
// *   Clear
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rSurface::Clear( void )
{
#ifndef DEDICATED
    // delete surface
    if ( surface_ )
        SDL_FreeSurface( surface_ );

#endif
    surface_ = 0;
}

// ******************************************************************************************
// *
// *	Create
// *
// ******************************************************************************************
//!
//!		@param	fileName	name of the image file to load
//!
// ******************************************************************************************

void rSurface::Create( char const * fileName )
{
#ifndef DEDICATED
    sr_LockSDL();

    // find path of image
    // tString s = tResourceManager::locateResource("", fileName);
    tString s = tDirectories::Data().GetReadPath( fileName );

    // Load image
#ifdef __EMSCRIPTEN__
    // The emscripten IMG port (stb_image) decodes with the standard
    // alpha convention (255 = opaque), exactly like native SDL_image:
    // IMG_InvertAlpha() has been a no-op since SDL_image 1.0.10, so the
    // "inversion" the game asks for never actually happens on the native
    // path either. Do NOT invert the alpha channel here.
    FILE *fp = fopen((const char*)s, "rb");
    SDL_Surface *surface = NULL;
    if (fp) {
        fseek(fp, 0, SEEK_END);
        long size = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        unsigned char *buffer = (unsigned char *)malloc(size);
        if (buffer) {
            fread(buffer, 1, size, fp);
            SDL_RWops *rw = SDL_RWFromMem(buffer, size);
            surface = IMG_Load_RW(rw, 1);
            free(buffer);
        }
        fclose(fp);
    }
    Create( surface );
#else
    IMG_InvertAlpha(true);
    Create( IMG_Load(s) );
#endif

    //if ( surface_ )
    //    std::cerr << "loaded surface " << fileName << "\n";

    sr_UnlockSDL();
#endif
}

// ******************************************************************************************
// *
// *	Create
// *
// ******************************************************************************************
//!
//!		@param	surface
//!
// ******************************************************************************************

void rSurface::Create( SDL_Surface * surface )
{
#ifndef DEDICATED
    // clear previous surface
    Clear();

    // take ownership
    surface_ = surface;

    // determine texture format
    if ( surface_ )
    {
        switch (surface_->format->BytesPerPixel){
        case 1:
            format_ = GL_LUMINANCE;
            break;

        case 2:
#ifdef __EMSCRIPTEN__
            format_ = GL_LUMINANCE_ALPHA;
#else
            format_ = GL_LUMINANCE8_ALPHA8;
#endif
            break;

        case 3:
            format_ = GL_RGB;
            break;

        case 4:
            format_ = GL_RGBA;
            break;

        default:
            {
                // fallback: convert the texture into a known format.

                SDL_Surface *dummy =
                    SDL_CreateRGBSurface(SDL_SWSURFACE, 1, 1,
                                         32,
                                         0x0000FF, 0x00FF00,
                                         0xFF0000 ,0xFF000000);

                SDL_Surface *convtex =
                    SDL_ConvertSurface(surface_, dummy->format, SDL_SWSURFACE);

                SDL_FreeSurface(surface_);
                surface_ = convtex;
                SDL_FreeSurface(dummy);

                format_ = GL_RGBA;
            }
            break;
        }
    }
#endif
}

// ******************************************************************************************
// *
// *	CopyFrom
// *
// ******************************************************************************************
//!
//!		@param	other
//!
// ******************************************************************************************

void rSurface::CopyFrom( rSurface const & other )
{
#ifndef DEDICATED
    tASSERT( 0 == surface_ );
    tASSERT( other.surface_ );

    // copy surface
    surface_ = SDL_ConvertSurface(other.surface_, other.surface_->format, SDL_SWSURFACE);

    // copy flags
    format_ = other.format_;
#endif
}

// ******************************************************************************************
// *
// *	~rITexture
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rITexture::~rITexture( void )
{
    s_textures_.Remove(this,id_);
}

// ******************************************************************************************
// *
// *	UnloadAll
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rITexture::UnloadAll( void )
{
    for(int i=s_textures_.Len()-1;i>=0;i--)
    {
        s_textures_(i)->Unload();
    }
}

// ******************************************************************************************
// *
// *	LoadAll
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rITexture::LoadAll( void )
{
    // s_reportErrors=false;
    for(int i=s_textures_.Len()-1;i>=0;i--)
    {
        s_textures_(i)->Select();
        //if (i>=s_textures.Len())
        //    i=s_textures.Len()-1;
    }
    // s_reportErrors=true;
}

// ******************************************************************************************
// *
// *	rITexture
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rITexture::rITexture( void )
        : id_( -1 )
{
}

// ******************************************************************************************
// *
// *	OnSelect
// *
// ******************************************************************************************
//!
//!		@param	enforce enforce when set to true, the texture should be loaded even if the configuration says it should not
//!
// ******************************************************************************************

void rITexture::OnSelect( bool enforce )
{
    if ( id_ < 0 )
        s_textures_.Add(this,id_);
}

// ******************************************************************************************
// *
// *	OnUnload
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rITexture::OnUnload( void )
{
}

// ******************************************************************************************
// *
// *	rISurfaceTexture
// *
// ******************************************************************************************
//!
//!		@param	group	texture group ( floor/wall)
//!		@param	repx    flag indicating the x repeat mode
//!		@param	repy    flag indicating the y repeat mode
//!		@param	storeAlpha flag indicating whether the alpha channel should be stored
//!
// ******************************************************************************************

rISurfaceTexture::rISurfaceTexture( int group, bool repx, bool repy, bool storeAlpha )
        : group_( group ), textureModeLast_( -1), tint_( 0 ), repx_( repx ), repy_( repy ), storeAlpha_( storeAlpha )
#ifdef __EMSCRIPTEN__
        , mipmapped_( false )
#endif // __EMSCRIPTEN__
{
}

// ******************************************************************************************
// *
// *	~rISurfaceTexture
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rISurfaceTexture::~rISurfaceTexture( void )
{
#ifndef DEDICATED
    if (tint_ > 0)
    {
        rDisplayList::ClearAll();

        glDeleteTextures(1,&tint_);
        tint_ = 0;
    }
#endif
}

// ******************************************************************************************
// *
// *	ProcessImage
// *
// ******************************************************************************************
//!
//!		@param	surface the surface to process
//!
// ******************************************************************************************

void rISurfaceTexture::ProcessImage( SDL_Surface * surface )
{
}

// ******************************************************************************************
// *
// *	Upload
// *
// ******************************************************************************************
//!
//!		@param	surface
//!
// ******************************************************************************************

#ifdef __EMSCRIPTEN__
// WebGL1 has no gluBuild2DMipmaps (or glGenerateMipmap in the legacy GL
// emulation), so build the mipmap chain in software and upload each level:
// every level is the 2x2 box average of the level above, mirroring what
// gluBuild2DMipmaps does in the native build. Only POT textures get a
// chain; WebGL1 refuses mipmaps for anything else.
static bool UploadMipLevels( const SDL_Surface * tex )
{
    const int w = tex->w;
    const int h = tex->h;
    const int bpp = tex->format->BytesPerPixel;

    if ( (w & (w - 1)) || (h & (h - 1)) || bpp < 1 || bpp > 4 )
        return false;

    const int internalFormat = bpp == 4 ? GL_RGBA
                               : bpp == 3 ? GL_RGB
                               : bpp == 2 ? GL_LUMINANCE_ALPHA
                               : GL_LUMINANCE;
    const int pitch = tex->pitch > 0 ? tex->pitch : w * bpp;
    const unsigned char * src = (const unsigned char *)tex->pixels;

    std::vector< unsigned char > cur, nxt;
    cur.assign( src, src + (size_t)pitch * h );

    int level = 1;
    int cw = w, ch = h, cp = pitch;
    while ( cw > 1 || ch > 1 )
    {
        const int nw = cw > 1 ? cw / 2 : 1;
        const int nh = ch > 1 ? ch / 2 : 1;
        nxt.resize( (size_t)nw * nh * bpp );

        for ( int y = 0; y < nh; y++ )
        {
            // A level that is already one texel tall cannot be halved
            // vertically, so the second row is the first row again -- exactly
            // the box gluBuild2DMipmaps applies to a texture like floor_b.png
            // (256x1, one set of grid lines): it averages two texels, not four.
            // Reading row 1 of a one-row image instead ran off the end of the
            // buffer and every odd mip level of that texture turned into
            // uninitialised bytes, which is what made one family of far-away
            // grid lines go black and the other family stay bright.
            const int y0 = (2 * y < ch) ? 2 * y : ch - 1;
            const int y1 = (2 * y + 1 < ch) ? 2 * y + 1 : y0;
            const unsigned char * src0 = &cur[(size_t)y0 * cp];
            const unsigned char * src1 = &cur[(size_t)y1 * cp];
            for ( int x = 0; x < nw; x++ )
            {
                const unsigned char * a = &src0[2 * x * bpp];
                const unsigned char * b = 2 * x + 1 < cw ? &src0[(2 * x + 1) * bpp] : a;
                const unsigned char * c = &src1[2 * x * bpp];
                const unsigned char * d = 2 * x + 1 < cw ? &src1[(2 * x + 1) * bpp] : c;
                // The output texel moves along the row; it used to stay put, so
                // every level wider than one texel kept one correct pixel and
                // whatever the reused buffer happened to hold in the rest.  For
                // the floor grid textures that meant one family of grid lines
                // (floor_b.png, 256x1) sampled black at most distances while
                // the other family (floor_a.png, 1x256, one texel wide, so
                // unaffected) stayed bright.
                unsigned char * dst = &nxt[((size_t)y * nw + x) * bpp];

                for ( int i = 0; i < bpp; i++ )
                    dst[i] = (unsigned char)((a[i] + b[i] + c[i] + d[i]) / 4);
            }
        }

        glTexImage2D(GL_TEXTURE_2D, level, internalFormat, nw, nh, 0,
                     internalFormat, GL_UNSIGNED_BYTE, &nxt[0]);

        cur.swap(nxt);
        cw = nw;
        ch = nh;
        cp = nw * bpp;
        level++;
    }

    return level > 1;
}
#endif
void rISurfaceTexture::Upload( rSurface & surface )
{
#ifndef DEDICATED
#ifndef GL_CLAMP_TO_EDGE
// GL_CLAMP_TO_EDGE was observed undefined in Windows; should be the same as GL_CLAMP.
#define GL_CLAMP_TO_EDGE GL_CLAMP
#endif

    sr_LockSDL();
    GLenum texformat = surface.GetFormat();
    SDL_Surface * tex = surface.GetSurface();
    tASSERT( tex );

    bool texalpha=tex->format->Amask;

    ProcessImage(tex);

    if(repx_)
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);
    else
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);
    if(repy_)
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_REPEAT);
    else
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);

#ifdef __EMSCRIPTEN__
    int format;
    if ( storageHack_ || ( storeAlpha_ && texalpha ) )
        format=GL_RGBA;
    else
        format=GL_RGB;
#else
    int format;
    if (sr_texturesTruecolor)
        if (storageHack_ || ( storeAlpha_ && texalpha ) )
            format=GL_RGBA8;
        else
            format=GL_RGB8;
    else
        if (storageHack_ || ( storeAlpha_ && texalpha ) )
            format=GL_RGBA4;
        else
            format=GL_RGB5;
#endif

#ifdef __EMSCRIPTEN__
    // The emscripten IMG port (stb_image) always produces 32-bit RGBA
    // surfaces, so upload them directly. (SDL_BlitSurface is unavailable
    // in the emscripten SDL port, so no intermediate surface.)
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    switch (tex->format->BytesPerPixel) {
    case 4:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->w, tex->h, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, tex->pixels);
        break;
    case 3:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tex->w, tex->h, 0,
                      GL_RGB, GL_UNSIGNED_BYTE, tex->pixels);
        break;
    case 2:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, tex->w, tex->h, 0,
                      GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, tex->pixels);
        break;
    case 1:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, tex->w, tex->h, 0,
                      GL_LUMINANCE, GL_UNSIGNED_BYTE, tex->pixels);
        break;
    default:
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tex->w, tex->h, 0,
                      GL_RGBA, GL_UNSIGNED_BYTE, tex->pixels);
        break;
    }

    // WebGL1: NPOT textures must use CLAMP_TO_EDGE (and never mipmap filters,
    // which OnSelect remaps). POT textures keep the game's wrap settings and
    // get a mipmap chain so minification filters like the native build.
    if ( (tex->w & (tex->w - 1)) || (tex->h & (tex->h - 1)) )
    {
        mipmapped_ = false;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    else
    {
        mipmapped_ = UploadMipLevels( tex );
    }
#else
    gluBuild2DMipmaps(GL_TEXTURE_2D,format,tex->w,tex->h,
                      texformat,GL_UNSIGNED_BYTE,tex->pixels);
#endif

    sr_UnlockSDL();
 #endif
}

// ******************************************************************************************
// *
// *	OnSelect
// *
// ******************************************************************************************
//!
//!		@param	enforce enforce when set to true, the texture should be loaded even if the configuration says it should not
//!
// ******************************************************************************************

void rISurfaceTexture::OnSelect( bool enforce )
{
#ifndef DEDICATED
    if(sr_glOut)
    {
        RenderEnd(true);

        int texmod=rTextureGroups::TextureMode[group_];

        if (enforce && texmod<0) texmod=GL_NEAREST_MIPMAP_NEAREST;

        if(textureModeLast_!=texmod)
        {
            // unload texture if the mode changed
            if ( tint_ > 0 )
                Unload();
            tASSERT( tint_ == 0 );
            // std::cerr << "loading texture " << fileName << ':' << tint << "\n";

            if (texmod>0){
                // don't generate textures inside display lists
                rDisplayList::Cancel();

                glGenTextures(1, &tint_);
                glBindTexture(GL_TEXTURE_2D,tint_);

                if (textureModeLast_<0)
                {
                    // delegate core loading work to derived class
                    OnSelect();
                }

                //glEnable(GL_TEXTURE);
                glEnable(GL_TEXTURE_2D);

#ifdef __EMSCRIPTEN__
                // WebGL1: a mipmapped min filter needs a complete mip chain,
                // which Upload only builds for POT textures (mipmapped_).
                // NPOT textures fall back to the equivalent non-mipmapped
                // filter; textureModeLast_ still tracks the requested mode,
                // so no reload loop can result.
                int minfilter = texmod;
                if ( !mipmapped_ )
                {
                    if (minfilter == GL_NEAREST_MIPMAP_NEAREST) minfilter = GL_NEAREST;
                    else if (minfilter == GL_LINEAR_MIPMAP_NEAREST) minfilter = GL_LINEAR;
                    else if (minfilter == GL_NEAREST_MIPMAP_LINEAR) minfilter = GL_NEAREST;
                    else if (minfilter == GL_LINEAR_MIPMAP_LINEAR) minfilter = GL_LINEAR;
                }
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,minfilter);
#else
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,texmod);
#endif

                switch(texmod)
                {
                case GL_NEAREST:
                case GL_NEAREST_MIPMAP_NEAREST:
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,
                                    GL_NEAREST);
                    break;
                default:
                    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,
                                    GL_LINEAR);
                    break;
                }
                #ifdef __EMSCRIPTEN__
                // ANGLE switches a trilinear-mipmapped sampler to its
                // anisotropic path on its own; nothing here asks for it and
                // desktop GL never sees an anisotropy request because the game
                // makes none.  Left alone the port samples grazing surfaces
                // softer than the desktop: over x 1200-1270, y 20-200 (the rim
                // wall seen along its length) the detail measure is 0.500
                // against the desktop's 0.622, and pinning the hint to 1 moves
                // it to 0.593 while leaving face-on floor sampling unchanged.
                #ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
                #define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
                #endif
                glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
                #endif

            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }
        }
        else
        {
            glBindTexture(GL_TEXTURE_2D,tint_);
            if (texmod>0)
            {
                glEnable(GL_TEXTURE_2D);
            }
            else
            {
                glDisable(GL_TEXTURE_2D);
            }
        }
        textureModeLast_=texmod;
    }
    rITexture::OnSelect(enforce);
#endif
}

// ******************************************************************************************
// *
// * OnSelect
// *
// ******************************************************************************************
//!
//!  In derived classes, this routine is supposed to do the work of loading the texture
//!  into memory and using the Upload() function to upload it to OpenGL.
//!
// ******************************************************************************************

void rISurfaceTexture::OnSelect()
{
}

// ******************************************************************************************
// *
// *	OnUnload
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rISurfaceTexture::OnUnload( void )
{
#ifndef DEDICATED
    if (tint_ > 0)
    {
        rDisplayList::ClearAll();

        // std::cerr << "unloading texture " << fileName << ':' << tint_ << "\n";
        glDeleteTextures(1,&tint_);
        tint_ = 0;
    }

    textureModeLast_=-100;
    rITexture::OnUnload();
#endif
}

// ******************************************************************************************
// *
// *	StoreAlpha
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rISurfaceTexture::StoreAlpha( void )
{
    storeAlpha_ = true;
}

// ******************************************************************************************
// *
// *	rFileTexture
// *
// ******************************************************************************************
//!
//!		@param	group	texture group ( floor/wall)
//!		@param	fileName the filename of the picture to load
//!		@param	repx    flag indicating the x repeat mode
//!		@param	repy    flag indicating the y repeat mode
//!		@param	storeAlpha flag indicating whether the alpha channel should be stored
//!
// ******************************************************************************************

rFileTexture::rFileTexture( int group, char const * fileName, bool repx, bool repy, bool storeAlpha )
        : rISurfaceTexture( group, repx, repy, storeAlpha )
        ,  fileName_( fileName )
{
}

// ******************************************************************************************
// *
// *	~rFileTexture
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rFileTexture::~rFileTexture( void )
{
}

// ******************************************************************************************
// *
// *	OnSelect
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rFileTexture::OnSelect()
{
#ifndef DEDICATED
    // std::cerr << "loading texture " << fileName_ << "\n";
    rSurface surface( fileName_ );
    if ( surface.GetSurface() )
    {
        this->Upload( surface );
    }
    else if (s_reportErrors_)
    {
        throw tGenericException( tOutput( "$texture_error_filenotfound", fileName_ ), tOutput("$texture_error_filenotfound_title") );
    }
    rISurfaceTexture::OnSelect();
#endif
}

// ******************************************************************************************
// *
// *	rSurfaceTexture
// *
// ******************************************************************************************
//!
//!		@param	group	texture group ( floor/wall)
//!		@param	surface
//!		@param	repx    flag indicating the x repeat mode
//!		@param	repy    flag indicating the y repeat mode
//!		@param	storeAlpha flag indicating whether the alpha channel should be stored
//!
// ******************************************************************************************

rSurfaceTexture::rSurfaceTexture( int group, rSurface const & surface, bool repx, bool repy, bool storeAlpha )
        : rISurfaceTexture( group, repx, repy, storeAlpha )
        , surface_( surface )
{
}

// ******************************************************************************************
// *
// *	~rSurfaceTexture
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

rSurfaceTexture::~rSurfaceTexture( void )
{
}

// ******************************************************************************************
// *
// *	OnSelect
// *
// ******************************************************************************************
//!
//!
// ******************************************************************************************

void rSurfaceTexture::OnSelect()
{
#ifndef DEDICATED
    // upload a copy of the surface ( it may get modified )
    if ( surface_.GetSurface() )
    {
        rSurface copy( surface_ );
        this->Upload( copy );
        rISurfaceTexture::OnSelect();
    }
#endif
}


bool rISurfaceTexture::s_reportErrors_=false;
tList<rITexture> rITexture::s_textures_;

int rTextureGroups::TextureMode[rTextureGroups::TEX_GROUPS]
#ifndef DEDICATED
={GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR_MIPMAP_LINEAR, GL_LINEAR }
#endif
;

char const * rTextureGroups::TextureGroupDescription[rTextureGroups::TEX_GROUPS]=
    {
        "$texture_mode_0_help",
        "$texture_mode_1_help",
        "$texture_mode_2_help",
        "$texture_mode_3_help",
    };

bool rISurfaceTexture::storageHack_ = false;

//rTexture ArmageTron_eWall("wWall.png",1,0);
//rTexture ArmageTron_dir_eWall("wall.png",1,0);


static rCallbackBeforeScreenModeChange unload(&rITexture::UnloadAll);

// static rCallbackAfterScreenModeChange load(&rITexture::LoadAll);
