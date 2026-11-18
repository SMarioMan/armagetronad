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

360 degree projection camera.

Ported from the fisheye/globe/lens concepts of the "blinky" Quake mod
(engine/NQ/fisheye.c and game/lua-scripts/globes, game/lua-scripts/lenses)
and the lens set of the flex-FOV Minecraft mod (Flex.fs):

- a "globe" is a set of plates (viewpoints) arranged around the camera:
  the cube globe (6 plates, 90 degrees, the standard cubemap) and the
  trism globe (5 plates, 120/128 degrees);
- the scene is rendered once per plate into a square region of the
  viewport (the plate's own perspective view);
- a "lens" projects the plates onto the screen: for each lens pixel the
  inverse lens mapping yields a ray, the ray is matched against the
  plates and the plate image is bilinearly sampled.
  Lenses: standard (off), panini, mercator, equirectangular,
  stereographic, winkel tripel, fisheye (equidistant), cylinder.
- WIDE_VIEW_FOV is the angular width of the lens image, in the same sense
  as blinky's f_fov (360 means the full panorama around the camera).

The lens mapping is precomputed into a lookup table (one entry per lens
pixel) whenever the lens, fov, globe or resolution changes, mirroring
blinky's lens builder as a one-shot build.

*/

#include "eWideView.h"
#include "eCamera.h"
#include "eGrid.h"
#include "eAdvWall.h"
#include "eGameObject.h"
#include "tConfiguration.h"

#include "rGL.h"
#include "rScreen.h"

#include <cmath>
#include <vector>

using std::vector;

#ifndef DEDICATED

int eWideView::se_wideViewLens = eWideView::LENS_STANDARD;
REAL eWideView::se_wideViewFOV = 360;
int eWideView::se_wideViewGlobe = eWideView::GLOBE_CUBE;
REAL eWideView::se_wideViewPlateRes = .5;
REAL eWideView::se_wideViewLensRes = .5;

static tConfItem<int> se_confWideViewLens("WIDE_VIEW_LENS", eWideView::se_wideViewLens);
static tConfItem<REAL> se_confWideViewFOV("WIDE_VIEW_FOV", eWideView::se_wideViewFOV);
static tConfItem<int> se_confWideViewGlobe("WIDE_VIEW_GLOBE", eWideView::se_wideViewGlobe);
static tConfItem<REAL> se_confWideViewPlateRes("WIDE_VIEW_PLATE_RES", eWideView::se_wideViewPlateRes);
static tConfItem<REAL> se_confWideViewLensRes("WIDE_VIEW_LENS_RES", eWideView::se_wideViewLensRes);


const char * eWideView::LensNames[LENS_COUNT] =
{
    "standard",
    "panini",
    "mercator",
    "equirectangular",
    "stereographic",
    "winkel tripel",
    "fisheye",
    "cylinder"
};

const char * eWideView::GlobeNames[GLOBE_COUNT] =
{
    "cube",
    "trism"
};

namespace
{

const double WV_PI = 3.14159265358979323846;

// ----------------------------------------------------------------- helpers
// lens space convention (as in blinky): forward=+Z, up=+Y, right=+X.

inline void wv_latlon_to_ray( double lat, double lon, double out[3] )
{
    double clat = std::cos( lat );
    out[0] = std::sin( lon ) * clat;
    out[1] = std::sin( lat );
    out[2] = std::cos( lon ) * clat;
}

// --------------------------------------------- lens forward map x components
// used to determine the lens scale for a given horizontal FOV:
// the x coordinate of the forward map at the ray with the given
// latitude/longitude.

double wv_forwardX( int lens, double lat, double lon )
{
    double clat = cos( lat );
    double rayx = sin( lon ) * clat;
    double rayy = sin( lat );
    double rayz = cos( lon ) * clat;
    double xy = sqrt( rayx*rayx + rayy*rayy );

    switch ( lens )
    {
    case eWideView::LENS_STANDARD:
        {
            double theta = acos( rayz );
            double r = tan( theta );
            if ( xy < 1e-9 )
                return 0;
            return rayx * r / xy;
        }
    case eWideView::LENS_PANINI:
        {
            const double d = 1;
            double S = ( d+1 ) / ( d + cos( lon ) );
            return S * sin( lon );
        }
    case eWideView::LENS_MERCATOR:
        return lon;
    case eWideView::LENS_EQUIRECT:
        return lon;
    case eWideView::LENS_STEREOGRAPHIC:
        {
            const double angleScale = .5;
            double theta = acos( rayz );
            double r = tan( theta * angleScale );
            if ( xy < 1e-9 )
                return 0;
            return rayx * r / xy;
        }
    case eWideView::LENS_WINKELTRIPEL:
        {
            double temp = clat * cos( lon * .5 );
            double D = acos( temp );
            double C = 1 - temp * temp;
            temp = D / sqrt( C );
            return .5 * ( 2 * temp * clat * sin( lon * .5 ) + lon * 2 / WV_PI );
        }
    case eWideView::LENS_FISHEYE:
        {
            double theta = acos( rayz );
            if ( xy < 1e-9 )
                return 0;
            return rayx * theta / xy;
        }
    case eWideView::LENS_CYLINDER:
        return lon;
    }
    return 0;
}

// ------------------------------------------------- lens inverse mappings
// map a point of the lens image (in lens units) to a ray; return false if
// the point lies outside the lens (pixel is blacked out).

bool wv_inverseStandard( double x, double y, double out[3] )
{
    double r = sqrt( x*x + y*y );
    if ( r < 1e-9 )
    {
        out[0]=0; out[1]=0; out[2]=1;
        return true;
    }
    double theta = atan( r );
    double s = sin( theta );
    out[0] = x/r * s;
    out[1] = y/r * s;
    out[2] = cos( theta );
    return true;
}

// from blinky's panini.lua (d=1)
bool wv_inversePanini( double x, double y, double out[3] )
{
    const double d = 1;
    double k = x*x / ((d+1)*(d+1));
    double dscr = k*k*d*d - (k+1)*(k*d*d-1);
    if ( dscr < 0 )
        dscr = 0;
    double clon = ( -k*d + sqrt( dscr ) ) / ( k+1 );
    double S = ( d+1 ) / ( d + clon );
    double lon = atan2( x, S * clon );
    double lat = atan2( y, S );
    wv_latlon_to_ray( lat, lon, out );
    return true;
}

// from blinky's mercator.lua
bool wv_inverseMercator( double x, double y, double out[3] )
{
    if ( std::fabs( x ) > WV_PI )
        return false;
    double lon = x;
    double lat = atan( sinh( y ) );
    wv_latlon_to_ray( lat, lon, out );
    return true;
}

// from blinky's equirect.lua
bool wv_inverseEquirect( double x, double y, double out[3] )
{
    if ( std::fabs( x ) > WV_PI || std::fabs( y ) > WV_PI * .5 )
        return false;
    wv_latlon_to_ray( y, x, out );
    return true;
}

// from blinky's stereographic.lua (angleScale=0.5)
bool wv_inverseStereographic( double x, double y, double out[3] )
{
    const double angleScale = .5;
    double r = sqrt( x*x + y*y );
    if ( r < 1e-9 )
    {
        out[0]=0; out[1]=0; out[2]=1;
        return true;
    }
    double theta = atan( r ) / angleScale;
    double s = sin( theta );
    out[0] = x/r * s;
    out[1] = y/r * s;
    out[2] = cos( theta );
    return true;
}

// from blinky's winkeltripel.lua, following
// https://github.com/d3/d3-geo-projection/blob/master/src/winkel3.js
bool wv_inverseWinkelTripel( double x, double y, double out[3] )
{
    const double halfpi = WV_PI / 2;
    const double eps = 1e-4;

    // the lens image's half height (y of the pole) is pi/2
    if ( std::fabs( y ) >= halfpi )
        return false;

    double lambda = x;
    double phi = y;

    for ( int iter = 0; iter < 25; iter++ )
    {
        double cosphi = cos( phi );
        double sinphi = sin( phi );
        double sin_2phi = sin( 2 * phi );
        double sin2phi = sinphi * sinphi;
        double cos2phi = cosphi * cosphi;
        double sinlambda = sin( lambda );
        double coslambda_2 = cos( lambda * .5 );
        double sinlambda_2 = sin( lambda * .5 );
        double sin2lambda_2 = sinlambda_2 * sinlambda_2;
        double C = 1 - cos2phi * coslambda_2 * coslambda_2;
        double E, F;
        if ( C != 0 )
        {
            F = 1 / C;
            E = acos( cosphi * coslambda_2 ) * sqrt( F );
        }
        else
        {
            E = 0;
            F = 0;
        }
        double fx = .5 * ( 2 * E * cosphi * sinlambda_2 + lambda / halfpi ) - x;
        double fy = .5 * ( E * sinphi + phi ) - y;
        double sigxsiglambda = .5 * F * ( cos2phi * sin2lambda_2 +
                                          E * cosphi * coslambda_2 * sin2phi )
                              + .5 / halfpi;
        double sigxsigphi = F * ( sinlambda * sin_2phi / 4
                                  - E * sinphi * sinlambda_2 );
        double sigysiglambda = .125 * F * ( sin_2phi * sinlambda_2
                                            - E * sinphi * cos2phi * sinlambda );
        double sigysigphi = .5 * F * ( sin2phi * coslambda_2
                                       + E * sin2lambda_2 * cosphi ) + .5;
        double denominator = sigxsigphi * sigysiglambda
                            - sigysigphi * sigxsiglambda;
        double siglambda = ( fy * sigxsigphi - fx * sigysigphi ) / denominator;
        double sigphi = ( fx * sigysiglambda - fy * sigxsiglambda ) / denominator;
        lambda -= siglambda;
        phi -= sigphi;
        if ( std::fabs( siglambda ) < eps && std::fabs( sigphi ) < eps )
            break;
    }

    // reject points outside the projected silhouette
    double x0 = wv_forwardX( eWideView::LENS_WINKELTRIPEL, phi, WV_PI );
    if ( std::fabs( x ) >= std::fabs( x0 ) )
        return false;

    wv_latlon_to_ray( phi, lambda, out );
    return true;
}

// from blinky's fisheye1.lua (equidistant)
bool wv_inverseFisheye( double x, double y, double out[3] )
{
    double r = sqrt( x*x + y*y );
    if ( r > WV_PI )
        return false;
    if ( r < 1e-9 )
    {
        out[0]=0; out[1]=0; out[2]=1;
        return true;
    }
    double s = sin( r );
    out[0] = x/r * s;
    out[1] = y/r * s;
    out[2] = cos( r );
    return true;
}

// from blinky's cylinder.lua
bool wv_inverseCylinder( double x, double y, double out[3] )
{
    if ( std::fabs( x ) > WV_PI )
        return false;
    double lon = x;
    double lat = atan( y );
    wv_latlon_to_ray( lat, lon, out );
    return true;
}

// ------------------------------------------------------------------ globes
// plate definitions in lens space (from blinky's globes/cube.lua and
// globes/trism.lua): forward and up of the plate view, and the plate's
// vertical field of view in degrees (square plates).

struct WVPlate
{
    float fwd[3];
    float up[3];
    REAL fov;
};

const WVPlate wv_cubePlates[6] =
{
    { {  0,  0,  1 }, { 0, 1,  0 }, 90 },  // front
    { {  1,  0,  0 }, { 0, 1,  0 }, 90 },  // right
    { { -1,  0,  0 }, { 0, 1,  0 }, 90 },  // left
    { {  0,  0, -1 }, { 0, 1,  0 }, 90 },  // back
    { {  0,  1,  0 }, { 0, 0, -1 }, 90 },  // top
    { {  0, -1,  0 }, { 0, 0,  1 }, 90 }   // bottom
};

const WVPlate wv_trismPlates[5] =
{
    { { -cos( WV_PI/6 ), 0, sin( WV_PI/6 ) }, { 0, 1, 0 }, 120 },  // left
    { {  cos( WV_PI/6 ), 0, sin( WV_PI/6 ) }, { 0, 1, 0 }, 120 },  // right
    { {  0, 0, -1 },                      { 0, 1, 0 }, 120 },      // back
    { {  0, 1, 0 },                       { 0, 0, -1 }, 128 },     // top
    { {  0, -1, 0 },                      { 0, 0, -1 }, 128 }      // bottom
};

// ------------------------------------------------------------ lens map table
// one entry per lens pixel: which plate the pixel's ray falls into and the
// sample position inside the plate image (normalized 0..1).
// Rebuilt whenever lens, fov, globe or the lens image size change.

struct WVMapPixel
{
    int plate;   // -1 if the ray falls outside all plates
    float u, v;  // sample position, 0..1 (v grows downwards)
};

vector<WVMapPixel> wv_lensmap;
int wv_mapLens = -1, wv_mapGlobe = -1, wv_mapW = 0, wv_mapH = 0;
double wv_mapFOV = -1;
double wv_mapScale = 1;

void wv_buildLensmap( int lens, int globe, double fov, int w, int h )
{
    if ( wv_mapLens == lens && wv_mapGlobe == globe &&
         wv_mapW == w && wv_mapH == h && wv_mapFOV == fov )
        return;

    const WVPlate * plates = globe == eWideView::GLOBE_TRISM ?
        wv_trismPlates : wv_cubePlates;
    const int plateCount = globe == eWideView::GLOBE_TRISM ? 5 : 6;

    // lens scale: lens units per lens pixel.  As in blinky's calc_zoom, the
    // forward map's x at half the requested width spans half the lens image.
    // blinky bounds that per lens with the max_fov/max_vfov each lens script
    // declares and refuses a wider setting; his panini and stereographic
    // scripts both load at "f_fov 180", because both projections run off to
    // infinity at the point straight behind the camera.  flex-fov never
    // carries those two that far either: its shader crossfades
    // standard -> panini/stereographic -> mercator and uses mercator alone
    // from fovx 300 up.  The port follows flex-fov's handoff and stops
    // scaling panini and stereographic at 300 degrees of width, the widest
    // setting at which they still fill the window; a wider request then
    // shows about +-150 deg instead of collapsing the scene into a few
    // pixels at the centre.  The fisheye stops just below 360 to dodge the
    // sin(pi) ~ 0 artifact in its forward map; equirectangular, mercator,
    // cylinder and winkel tripel reach the full antipode.
    double fovr = fov * WV_PI / 180;
    double fovCap;
    switch ( lens )
    {
    case eWideView::LENS_PANINI:
    case eWideView::LENS_STEREOGRAPHIC:
        fovCap = 300 * WV_PI / 180;
        break;
    case eWideView::LENS_FISHEYE:
        fovCap = 2 * 3.10;
        break;
    default:
        fovCap = 2 * WV_PI;
        break;
    }
    if ( fovr > fovCap )
        fovr = fovCap;
    double lonEval = fovr * .5;
    double x_edge = std::fabs( wv_forwardX( lens, 0, lonEval ) );
    if ( x_edge <= 0 || !std::isfinite( x_edge ) )
        x_edge = 1;
    wv_mapScale = x_edge / ( w * .5 );

    wv_lensmap.resize( (size_t) w * h );

    // plate frames in lens space.  The lens basis (right, up, forward) is
    // left handed with respect to the world: a camera basis is right handed
    // as (right, up, -forward), so the algebraic cross product of two
    // lens-space vectors is the negative of the world space cross product.
    // The screen right axis of a plate camera is forward x up in world space
    // (gluLookAt: s = up x z with z = -forward), hence up x forward - not
    // forward x up - in lens coordinates.
    double plateRight[6][3], plateFwd[6][3], plateUp[6][3];
    double plateTan[6], plateTanY[6];
    for ( int i = 0; i < plateCount; i++ )
    {
        double f[3] = { plates[i].fwd[0], plates[i].fwd[1], plates[i].fwd[2] };
        double u[3] = { plates[i].up[0], plates[i].up[1], plates[i].up[2] };
        plateFwd[i][0]=f[0]; plateFwd[i][1]=f[1]; plateFwd[i][2]=f[2];
        plateUp[i][0]=u[0]; plateUp[i][1]=u[1]; plateUp[i][2]=u[2];
        // screen right of the GL camera built below: up x fwd
        plateRight[i][0] = u[1]*f[2] - u[2]*f[1];
        plateRight[i][1] = u[2]*f[0] - u[0]*f[2];
        plateRight[i][2] = u[0]*f[1] - u[1]*f[0];
        // frustum half widths: the plates are rendered through
        // rViewport::PerspectiveSquare, so the plate field of view applies
        // to both axes and the plates tile the sphere at any window aspect,
        // like blinky's square globe plates and flex-fov's six 90 degree
        // textures.
        plateTan[i] = tan( plates[i].fov * WV_PI / 360 );
        plateTanY[i] = plateTan[i];
    }

    bool (*inverse)( double, double, double* ) = 0;
    switch ( lens )
    {
    case eWideView::LENS_PANINI: inverse = wv_inversePanini; break;
    case eWideView::LENS_MERCATOR: inverse = wv_inverseMercator; break;
    case eWideView::LENS_EQUIRECT: inverse = wv_inverseEquirect; break;
    case eWideView::LENS_STEREOGRAPHIC: inverse = wv_inverseStereographic; break;
    case eWideView::LENS_WINKELTRIPEL: inverse = wv_inverseWinkelTripel; break;
    case eWideView::LENS_FISHEYE: inverse = wv_inverseFisheye; break;
    case eWideView::LENS_CYLINDER: inverse = wv_inverseCylinder; break;
    default: inverse = wv_inverseStandard; break;
    }

    for ( int ly = 0; ly < h; ly++ )
    {
        double y = ( h * .5 - ly - .5 ) * wv_mapScale;
        for ( int lx = 0; lx < w; lx++ )
        {
            double x = ( lx + .5 - w * .5 ) * wv_mapScale;
            WVMapPixel & mp = wv_lensmap[(size_t) ly * w + lx];
            double ray[3];
            if ( !inverse( x, y, ray ) )
            {
                mp.plate = -1;
                mp.u = 0; mp.v = 0;
                continue;
            }
            mp.plate = -1;
            mp.u = .5f; mp.v = .5f;
            for ( int i = 0; i < plateCount; i++ )
            {
                double pz = ray[0]*plateFwd[i][0] + ray[1]*plateFwd[i][1] +
                            ray[2]*plateFwd[i][2];
                if ( pz <= 1e-6 )
                    continue;
                double px = ray[0]*plateRight[i][0] + ray[1]*plateRight[i][1] +
                            ray[2]*plateRight[i][2];
                double py = ray[0]*plateUp[i][0] + ray[1]*plateUp[i][1] +
                            ray[2]*plateUp[i][2];
                if ( std::fabs( px/pz ) <= plateTan[i] * 1.001 &&
                     std::fabs( py/pz ) <= plateTanY[i] * 1.001 )
                {
                    mp.plate = i;
                    mp.u = (float) ( .5 * ( 1 + ( px/pz ) / plateTan[i] ) );
                    mp.v = (float) ( .5 * ( 1 - ( py/pz ) / plateTanY[i] ) );
                    break;
                }
            }
        }
    }

    wv_mapLens = lens;
    wv_mapGlobe = globe;
    wv_mapW = w;
    wv_mapH = h;
    wv_mapFOV = fov;
}

// --------------------------------------------------------------- wide view
// cached scratch space
vector<unsigned char> wv_plateBuf;
vector<unsigned char> wv_lensBuf;
unsigned int wv_lensTex = 0;
int wv_lensTexW = 0, wv_lensTexH = 0;

} // anonymous namespace

bool eWideView::Active()
{
    return se_wideViewLens > LENS_STANDARD && se_wideViewLens < LENS_COUNT;
}

void eWideView::Render( eCamera * cam, eCoord const & pos_diff,
                        eCoord const & glancedir )
{
    rViewport * vp = cam->vp;
    if ( !vp )
        return;

    int vpLeft = (int) ( vp->GetLeft() * sr_screenWidth );
    int vpBottom = (int) ( vp->GetBottom() * sr_screenHeight );
    int vpW = (int) ( vp->GetWidth() * sr_screenWidth );
    int vpH = (int) ( vp->GetHeight() * sr_screenHeight );
    if ( vpW < 64 || vpH < 64 )
        return;

    int lens = se_wideViewLens;
    int globe = se_wideViewGlobe;
    if ( lens <= LENS_STANDARD || lens >= LENS_COUNT ||
         globe < 0 || globe >= GLOBE_COUNT )
        return;

    double fov = se_wideViewFOV;
    if ( fov < 10 )
        fov = 10;
    if ( fov > 360 )
        fov = 360;

    // plate and lens image sizes
    int minDim = vpW < vpH ? vpW : vpH;
    int plateSize = (int) ( minDim * se_wideViewPlateRes );
    if ( plateSize < 64 )
        plateSize = 64;
    if ( plateSize > 1024 )
        plateSize = 1024;
    // keep the plate inside the viewport
    if ( plateSize > vpW )
        plateSize = vpW;
    if ( plateSize > vpH )
        plateSize = vpH;

    int lensW = (int) ( vpW * se_wideViewLensRes );
    int lensH = (int) ( vpH * se_wideViewLensRes );
    if ( lensW < 64 )
        lensW = 64;
    if ( lensH < 64 )
        lensH = 64;
    if ( lensW > 1024 )
        lensW = 1024;
    if ( lensH > 1024 )
        lensH = 1024;

    // the function is fully self-contained: remember the caller's GL
    // state and restore it before returning
    GLfloat oldProj[16], oldModel[16];
    GLint oldTexEnabled = 0, oldFogEnabled = 0, oldDepthEnabled = 0;
    glMatrixMode( GL_PROJECTION );
    glGetFloatv( GL_PROJECTION_MATRIX, oldProj );
    glMatrixMode( GL_MODELVIEW );
    glGetFloatv( GL_MODELVIEW_MATRIX, oldModel );
    glGetIntegerv( GL_TEXTURE_2D, &oldTexEnabled );
    glGetIntegerv( GL_FOG, &oldFogEnabled );
    glGetIntegerv( GL_DEPTH_TEST, &oldDepthEnabled );

    const WVPlate * plates = globe == GLOBE_TRISM ? wv_trismPlates : wv_cubePlates;
    const int plateCount = globe == GLOBE_TRISM ? 5 : 6;

    wv_plateBuf.resize( (size_t) plateCount * plateSize * plateSize * 3 );

    // camera view frame in world space: forward, right, up
    // (the same frame gluLookAt would build for the normal view)
    double f[3] = { (double) glancedir.x, (double) glancedir.y, (double) cam->rise };
    double flen = sqrt( f[0]*f[0] + f[1]*f[1] + f[2]*f[2] );
    if ( flen < 1e-6 )
        flen = 1e-6;
    f[0] /= flen; f[1] /= flen; f[2] /= flen;

    double uph[3] = { (double) cam->top.x, (double) cam->top.y, 1.0 };
    // s = right = normalize(cross(f, uph))
    double s[3] =
    {
        f[1]*uph[2] - f[2]*uph[1],
        f[2]*uph[0] - f[0]*uph[2],
        f[0]*uph[1] - f[1]*uph[0]
    };
    double slen = sqrt( s[0]*s[0] + s[1]*s[1] + s[2]*s[2] );
    if ( slen < 1e-6 )
    {
        // degenerate (camera looking straight along the up hint)
        s[0]=1; s[1]=0; s[2]=0;
    }
    else
    {
        s[0] /= slen; s[1] /= slen; s[2] /= slen;
    }
    // u = up = cross(s, f)
    double u[3] =
    {
        s[1]*f[2] - s[2]*f[1],
        s[2]*f[0] - s[0]*f[2],
        s[0]*f[1] - s[1]*f[0]
    };

    bool draw_center = ( ( cam->CenterPos() - cam->pos ).NormSquared() > 1 ||
                         std::fabs( cam->CenterZ() - cam->z ) > 1 );
    tJUST_CONTROLLED_PTR< eGameObject > c = cam->Center();
    if ( !draw_center && c )
        c->RemoveFromList();

    eCoord poscopy = cam->pos;
    REAL zNear = - eWallRim::Bound( poscopy, 0.0f );
    if ( zNear < -.1 )
        zNear = .1;

    eCoord oldDir = cam->dir;

    // ------------------------------------------------------------- capture
    // the plate sub-viewport sits at the top-right corner of the camera
    // viewport; read exactly the region it renders to
    int plateX = vpLeft + vpW - plateSize;
    int plateY = vpBottom + vpH - plateSize;

    if ( cam->CenterCockpitFixedBefore() )
    {
        for ( int i = 0; i < plateCount; i++ )
        {
            // plate world frame: a lens-space vector v maps to
            // v.x*right + v.y*up + v.z*forward in world space
            double fp[3], up[3];
            fp[0] = plates[i].fwd[0]*s[0] + plates[i].fwd[1]*u[0] + plates[i].fwd[2]*f[0];
            fp[1] = plates[i].fwd[0]*s[1] + plates[i].fwd[1]*u[1] + plates[i].fwd[2]*f[1];
            fp[2] = plates[i].fwd[0]*s[2] + plates[i].fwd[1]*u[2] + plates[i].fwd[2]*f[2];
            up[0] = plates[i].up[0]*s[0] + plates[i].up[1]*u[0] + plates[i].up[2]*f[0];
            up[1] = plates[i].up[0]*s[1] + plates[i].up[1]*u[1] + plates[i].up[2]*f[1];
            up[2] = plates[i].up[0]*s[2] + plates[i].up[1]*u[2] + plates[i].up[2]*f[2];

            // modelview: look along fp with world up `up` (exact lookAt)
            double z[3] = { -fp[0], -fp[1], -fp[2] };
            double zl = sqrt( z[0]*z[0] + z[1]*z[1] + z[2]*z[2] );
            z[0]/=zl; z[1]/=zl; z[2]/=zl;
            double x[3] =
            {
                up[1]*z[2] - up[2]*z[1],
                up[2]*z[0] - up[0]*z[2],
                up[0]*z[1] - up[1]*z[0]
            };
            double xl = sqrt( x[0]*x[0] + x[1]*x[1] + x[2]*x[2] );
            if ( xl < 1e-6 )
            {
                // up parallel to the view direction: pick any side vector
                x[0]=1; x[1]=0; x[2]=0;
                xl=1;
            }
            x[0]/=xl; x[1]/=xl; x[2]/=xl;
            double y[3] =
            {
                z[1]*x[2] - z[2]*x[1],
                z[2]*x[0] - z[0]*x[2],
                z[0]*x[1] - z[1]*x[0]
            };

            // display_simple reads the camera's 2D direction for the floor
            // grid and the wall proximity sensors.  A plate looking straight
            // up or straight down has no horizontal direction of its own, so
            // it keeps the camera's own direction rather than the direction
            // of whichever plate was rendered before it.
            double hlen = sqrt( fp[0]*fp[0] + fp[1]*fp[1] );
            if ( hlen > .01 )
                cam->dir = eCoord( (REAL) ( fp[0]/hlen ), (REAL) ( fp[1]/hlen ) );
            else
                cam->dir = oldDir;

            // plate viewport: the same viewport machinery as the normal
            // view, but with the square frustum PerspectiveSquare() so that
            // a square plate keeps its full field of view vertically too.
            rViewport plateSub( (REAL) ( ( vpW - plateSize ) / (double) vpW ),
                                (REAL) ( ( vpH - plateSize ) / (double) vpH ),
                                (REAL) ( plateSize / (double) vpW ),
                                (REAL) ( plateSize / (double) vpH ) );
            rViewport plateVP( *vp, plateSub );
            plateVP.Select();
            glMatrixMode( GL_PROJECTION );
            glLoadIdentity();
            plateVP.PerspectiveSquare( plates[i].fov, cam->zNear, 1e20 );

            glMatrixMode( GL_MODELVIEW );
            glLoadIdentity();
            GLfloat m[16] =
            {
                (GLfloat) x[0], (GLfloat) y[0], (GLfloat) z[0], 0,
                (GLfloat) x[1], (GLfloat) y[1], (GLfloat) z[1], 0,
                (GLfloat) x[2], (GLfloat) y[2], (GLfloat) z[2], 0,
                0, 0, 0, 1
            };
            glMultMatrixf( m );
            glTranslatef( -pos_diff.x, -pos_diff.y, -cam->z );

            glClear( GL_DEPTH_BUFFER_BIT );

            cam->grid->Render( cam, cam->id, zNear );

            if ( c )
                c->RenderCockpitVirtual();

            glReadPixels( plateX, plateY, plateSize, plateSize,
                          GL_RGB, GL_UNSIGNED_BYTE,
                          &wv_plateBuf[(size_t) i * plateSize * plateSize * 3] );
        }
        cam->CenterCockpitFixedAfter();
    }
    cam->dir = oldDir;

    if ( !draw_center && c )
        c->AddToList();

    // ------------------------------------------------------------- lens pass
    wv_buildLensmap( lens, globe, fov, lensW, lensH );
    wv_lensBuf.resize( (size_t) lensW * lensH * 3 );

    for ( int ly = 0; ly < lensH; ly++ )
    {
        for ( int lx = 0; lx < lensW; lx++ )
        {
            const WVMapPixel & mp = wv_lensmap[(size_t) ly * lensW + lx];
            unsigned char * out = &wv_lensBuf[((size_t) ly * lensW + lx) * 3];
            if ( mp.plate < 0 )
            {
                out[0]=0; out[1]=0; out[2]=0;
                continue;
            }
            const unsigned char * plate =
                &wv_plateBuf[(size_t) mp.plate * plateSize * plateSize * 3];
            double sx = mp.u * plateSize - .5;
            double sy = ( 1.0 - mp.v ) * plateSize - .5;
            int x0 = (int) sx;
            int y0 = (int) sy;
            double dx = sx - x0;
            double dy = sy - y0;
            if ( x0 < 0 )
                x0 = 0;
            if ( y0 < 0 )
                y0 = 0;
            if ( x0 > plateSize - 2 )
                x0 = plateSize - 2;
            if ( y0 > plateSize - 2 )
                y0 = plateSize - 2;
            const unsigned char * p00 = &plate[( (size_t) y0 * plateSize + x0 ) * 3];
            const unsigned char * p10 = p00 + 3;
            const unsigned char * p01 = &plate[( (size_t) ( y0 + 1 ) * plateSize + x0 ) * 3];
            const unsigned char * p11 = p01 + 3;
            for ( int ch = 0; ch < 3; ch++ )
            {
                double a = p00[ch] * ( 1 - dx ) + p10[ch] * dx;
                double b = p01[ch] * ( 1 - dx ) + p11[ch] * dx;
                out[ch] = (unsigned char) ( a * ( 1 - dy ) + b * dy + .5 );
            }
        }
    }

    // ------------------------------------------------------------- composite
    if ( !wv_lensTex || wv_lensTexW != lensW || wv_lensTexH != lensH )
    {
        if ( wv_lensTex )
            glDeleteTextures( 1, &wv_lensTex );
        glGenTextures( 1, &wv_lensTex );
        wv_lensTexW = lensW;
        wv_lensTexH = lensH;
    }
    glBindTexture( GL_TEXTURE_2D, wv_lensTex );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP );
    GLint oldAlign = 0;
    glGetIntegerv( GL_UNPACK_ALIGNMENT, &oldAlign );
    glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    glTexImage2D( GL_TEXTURE_2D, 0, 3, lensW, lensH, 0,
                  GL_RGB, GL_UNSIGNED_BYTE, &wv_lensBuf[0] );
    glPixelStorei( GL_UNPACK_ALIGNMENT, oldAlign );

    glViewport( vpLeft, vpBottom, vpW, vpH );
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    glDisable( GL_DEPTH_TEST );
    glDisable( GL_FOG );
    glEnable( GL_TEXTURE_2D );
    glColor4f( 1, 1, 1, 1 );

    // the lens buffer's first row is the top of the image; the first
    // texture row is at v=0, so map v=0 to the top of the screen
    glBegin( GL_QUADS );
    glTexCoord2f( 0, 0 ); glVertex2f( -1, 1 );
    glTexCoord2f( 1, 0 ); glVertex2f( 1, 1 );
    glTexCoord2f( 1, 1 ); glVertex2f( 1, -1 );
    glTexCoord2f( 0, 1 ); glVertex2f( -1, -1 );
    glEnd();

    if ( oldTexEnabled )
        glEnable( GL_TEXTURE_2D );
    else
        glDisable( GL_TEXTURE_2D );
    if ( oldFogEnabled )
        glEnable( GL_FOG );
    else
        glDisable( GL_FOG );
    if ( oldDepthEnabled )
        glEnable( GL_DEPTH_TEST );
    else
        glDisable( GL_DEPTH_TEST );
    glMatrixMode( GL_PROJECTION );
    glLoadMatrixf( oldProj );
    glMatrixMode( GL_MODELVIEW );
    glLoadMatrixf( oldModel );
}

#endif // DEDICATED
