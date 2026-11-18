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

// Self check for the 360 view: every lens, every globe, make check.
//
// Three independent checks run over all seven lenses (panini, mercator,
// equirectangular, stereographic, winkel tripel, fisheye, cylinder;
// "standard" is the feature-off setting and never builds a map).
//
// 1. Lens map against a world space plate model.  wv_buildLensmap() stores,
//    for every lens pixel, the plate whose frustum contains the pixel's ray
//    and where inside that plate image to sample.  The check rebuilds the
//    same information from the plate tables using gluLookAt semantics
//    (right = up x -forward, up = -forward x right) with only the plate field
//    of view and the pixel aspect.  The two models share no cross products:
//    the map works in the lens basis, the model never leaves world space.
//    This is what catches a mirrored plate axis (forward x up instead of
//    up x forward, the lens basis being left handed with respect to the world
//    because a camera basis is right handed as (right, up, -forward)) and a
//    single shared x/y sample bound (rViewport::Perspective builds
//    ymul = xmul/aspect, so a square plate viewport with non square pixels
//    samples v outside the plate frustum).  The mirrored hypothesis
//    u' = 1 - u is measured as well and must stay away from zero.
//
// 2. Lens algebra.  The module only has inverse lens maps (plus the x
//    component used to pick the lens scale); the forward maps written here
//    come from the projection definitions.  Forward then inverse must return
//    the input ray, and the module's wv_forwardX must agree with these
//    forwards along the equator - that is the value the lens scale is taken
//    from, so a disagreement scales the image wrong.
//
// 3. Map shape.  Rays are unit length, the lens is odd in longitude and in
//    latitude, the centre pixel looks forward, the angular radius grows
//    monotonically outward, sample positions stay inside the plate image, and
//    a pixel is black exactly when its lens rejects the point.
//
// CPU only, no GL context, a few seconds.  What it cannot cover is what the
// plates actually look like once rendered: that needs a GLX display.

#include "eWideView.cpp"

#include <cstdio>
#include <cmath>

// ---------------------------------------------------------------- vectors

struct Vec
{
    double x, y, z;
    Vec() : x(0), y(0), z(0) {}
    Vec(double a, double b, double c) : x(a), y(b), z(c) {}
};

static Vec operator + (Vec a, Vec b) { return Vec(a.x+b.x, a.y+b.y, a.z+b.z); }
static Vec operator - (Vec a, Vec b) { return Vec(a.x-b.x, a.y-b.y, a.z-b.z); }
static Vec operator * (Vec a, double k) { return Vec(a.x*k, a.y*k, a.z*k); }
static double dot (Vec a, Vec b) { return a.x*b.x + a.y*b.y + a.z*b.z; }
static Vec cross (Vec a, Vec b)
{
    return Vec(a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x);
}
static Vec norm (Vec a)
{
    double l = sqrt(dot(a,a));
    if (l < 1e-9) l = 1e-9;
    return a * (1.0/l);
}

static double magnitude ( Vec a ) { return sqrt(dot(a,a)); }

//! one plate camera, axes in world space, gluLookAt semantics
struct PlateCam
{
    Vec fwd, right, up;   // world axes of the rendered plate image
    double tan;           // frustum half width in x (aspect 1 pixels)
    double tanY;          // and in y: xmul/aspect, as rViewport::Perspective does it
};

static PlateCam makeCam (Vec fwdW, Vec upW, double fovDeg, double aspect)
{
    PlateCam c;
    c.fwd = norm(fwdW);
    Vec z = c.fwd * -1.0;
    c.right = norm(cross(upW, z));      // gluLookAt: s = up x z
    c.up = cross(z, c.right);           // gluLookAt: u = z x s
    double a = aspect / 1.5;            // the engine widens a square viewport by aspect/1.5
    if (a < 1.0) a = 1.0;
    double xmul = a * tan(fovDeg * WV_PI / 360.0);
    c.tan = xmul;
    c.tanY = xmul / aspect;
    return c;
}

// ------------------------------------------------------------- lens table

struct LensCase
{
    int lens;
    const char * name;
    bool (*inverse)( double, double, double* );   // the module's
    // the forward map written here: lat,lon -> lens x,y; false if the sample
    // is outside the region this lens is expected to cover
    bool (*forward)( double, double, double* );
    double rayTol;                                // forward/inverse round trip
};

static const double DEG = WV_PI / 180.0;

// blinky's panini.lua with d = 1: S = 2/(1+cos lon)
static bool fwdPanini ( double phi, double lam, double * xy )
{
    if ( fabs(lam) > 2.5 || fabs(phi) > 75*DEG )
        return false;
    double S = 2.0 / (1.0 + cos( lam ));
    xy[0] = S * sin( lam );
    xy[1] = S * tan( phi );
    return true;
}

// conformal cylindrical (mercator): y = asinh(tan phi)
static bool fwdMercator ( double phi, double lam, double * xy )
{
    if ( fabs(lam) > 3.10 || fabs(phi) > 75*DEG )
        return false;
    double t = tan( phi );
    xy[0] = lam;
    xy[1] = log( t + sqrt( 1.0 + t*t ) );
    return true;
}

// plain lat/lon plate carree
static bool fwdEquirect ( double phi, double lam, double * xy )
{
    if ( fabs(lam) > 3.10 || fabs(phi) > 85*DEG )
        return false;
    xy[0] = lam;
    xy[1] = phi;
    return true;
}

// stereographic with angleScale 1/2: radius tan(theta/2)
static bool fwdStereographic ( double phi, double lam, double * xy )
{
    double ray[3];
    wv_latlon_to_ray( phi, lam, ray );
    double theta = acos( ray[2] );
    if ( theta > 3.0 )
        return false;
    double xy2 = sqrt( ray[0]*ray[0] + ray[1]*ray[1] );
    double r = tan( theta * .5 );
    if ( xy2 < 1e-12 )
    {
        xy[0] = 0; xy[1] = 0;
        return true;
    }
    xy[0] = ray[0] * r / xy2;
    xy[1] = ray[1] * r / xy2;
    return true;
}

// winkel tripel: mean of the equirectangular and the Aitoff projection
static bool fwdWinkelTripel ( double phi, double lam, double * xy )
{
    if ( fabs(lam) > 2.5 || fabs(phi) > 75*DEG )
        return false;
    double temp = cos( phi ) * cos( lam * .5 );
    double D = acos( temp );
    double C = 1 - temp*temp;
    if ( C <= 0 )
        return false;
    double E = D / sqrt( C );
    xy[0] = .5 * ( 2*E*cos( phi )*sin( lam * .5 ) + lam / (WV_PI/2) );
    xy[1] = .5 * ( E*sin( phi ) + phi );
    return xy[1] < WV_PI/2;
}

// equidistant fisheye: radius theta
static bool fwdFisheye ( double phi, double lam, double * xy )
{
    double ray[3];
    wv_latlon_to_ray( phi, lam, ray );
    double theta = acos( ray[2] );
    if ( theta > 3.05 )
        return false;
    double xy2 = sqrt( ray[0]*ray[0] + ray[1]*ray[1] );
    if ( xy2 < 1e-12 )
    {
        xy[0] = 0; xy[1] = 0;
        return true;
    }
    xy[0] = ray[0] * theta / xy2;
    xy[1] = ray[1] * theta / xy2;
    return true;
}

// cylindrical with a tangent vertical axis (blinky's cylinder.lua)
static bool fwdCylinder ( double phi, double lam, double * xy )
{
    if ( fabs(lam) > 3.10 || fabs(phi) > 75*DEG )
        return false;
    xy[0] = lam;
    xy[1] = tan( phi );
    return true;
}

static const LensCase lensCases[] =
{
    { eWideView::LENS_PANINI,         "panini",    wv_inversePanini,         fwdPanini,         1e-6 },
    { eWideView::LENS_MERCATOR,       "mercator",  wv_inverseMercator,       fwdMercator,       1e-6 },
    { eWideView::LENS_EQUIRECT,       "equi",      wv_inverseEquirect,       fwdEquirect,       1e-6 },
    { eWideView::LENS_STEREOGRAPHIC,  "stereo",    wv_inverseStereographic,  fwdStereographic,  1e-6 },
    { eWideView::LENS_WINKELTRIPEL,   "winkel",    wv_inverseWinkelTripel,   fwdWinkelTripel,   3e-4 },
    { eWideView::LENS_FISHEYE,        "fisheye",   wv_inverseFisheye,        fwdFisheye,        1e-6 },
    { eWideView::LENS_CYLINDER,       "cylinder",  wv_inverseCylinder,       fwdCylinder,       1e-6 },
};
static const int lensCount = sizeof(lensCases) / sizeof(lensCases[0]);

static int fails = 0;

static void Fail ( const char * what )
{
    printf("  FAIL: %s\n", what);
    fails++;
}

// ------------------------------------------------- 1. map vs world model

static void CheckMapAgainstWorld ( const LensCase & lc, int globe,
                                   double fov, double aspect )
{
    const WVPlate * plates = globe == eWideView::GLOBE_TRISM ? wv_trismPlates : wv_cubePlates;
    int plateCount = globe == eWideView::GLOBE_TRISM ? 5 : 6;

    // a deliberately non-axis-aligned camera: looking along world +X, head up
    // along world +Z -> the true right is world -Y.
    Vec fW(1, 0, 0);
    Vec upH(0, 0, 1);
    Vec sW = norm(cross(fW, upH));       // camera right
    Vec uW = cross(sW, fW);              // camera up

    currentScreensetting.aspect = (float) aspect;

    const int w = 73, h = 41;
    wv_mapLens = -1;                     // force a rebuild
    wv_buildLensmap( lc.lens, globe, fov, w, h );

    double scale = wv_mapScale;
    int mism = 0, total = 0, black = 0, plateUse[8] = {0,0,0,0,0,0,0,0};
    int uvOut = 0, blackMismatch = 0, blackLens = 0, blackNoPlate = 0;
    double sumErrU = 0, sumErrUflip = 0, sumErrV = 0;
    double maxErrU = 0, maxErrV = 0;

    for (int ly = 0; ly < h; ly++)
    {
        for (int lx = 0; lx < w; lx++)
        {
            const WVMapPixel &mp = wv_lensmap[(size_t) ly * w + lx];
            total++;
            double x = (lx + .5 - w * .5) * scale;
            double y = (h * .5 - ly - .5) * scale;
            double lr[3];
            bool inside = lc.inverse( x, y, lr );

            Vec rayW = sW * lr[0] + uW * lr[1] + fW * lr[2];

            // independent model: first plate in table order whose frustum
            // holds the ray
            int wantPlate = -1;
            double wantU = .5, wantV = .5;
            for (int i = 0; i < plateCount; i++ )
            {
                Vec fw(plates[i].fwd[0], plates[i].fwd[1], plates[i].fwd[2]);
                Vec uw(plates[i].up[0], plates[i].up[1], plates[i].up[2]);
                // lens vector -> world
                Vec fwdW = sW * fw.x + uW * fw.y + fW * fw.z;
                Vec upW  = sW * uw.x + uW * uw.y + fW * uw.z;
                PlateCam pc = makeCam(fwdW, upW, plates[i].fov, aspect);
                double pz = dot(rayW, pc.fwd);
                if (pz <= 1e-6) continue;
                double px = dot(rayW, pc.right);
                double py = dot(rayW, pc.up);
                if (fabs(px / pz) <= pc.tan * 1.001 &&
                    fabs(py / pz) <= pc.tanY * 1.001)
                {
                    wantPlate = i;
                    wantU = .5 * (1 + (px / pz) / pc.tan);
                    wantV = .5 * (1 - (py / pz) / pc.tanY);
                    break;
                }
            }

            if (!inside)
            {
                // the lens rejects this point, so it has to be black
                if (mp.plate >= 0)
                {
                    if (blackMismatch < 3)
                        printf("  pixel %3d,%3d lens (%.3f,%.3f) is outside the lens "
                            "but the map samples plate %d\n", lx, ly, x, y, mp.plate);
                    blackMismatch++;
                }
                blackLens++;
                black++;
                continue;
            }

            if (mp.plate >= plateCount)
            {
                Fail("map references a plate the globe does not have");
                continue;
            }

            if (wantPlate != mp.plate)
            {
                if (mism < 3)
                    printf("  pixel %3d,%3d ray %.3f %.3f %.3f: map plate %d, model plate %d\n",
                        lx, ly, rayW.x, rayW.y, rayW.z, mp.plate, wantPlate);
                mism++;
                if (mp.plate < 0)
                    continue;
            }
            if (mp.plate < 0)
            {
                // inside the lens, but no plate of this globe covers this ray:
                // a hole in the globe rather than a map error
                blackNoPlate++;
                black++;
                continue;
            }
            plateUse[mp.plate]++;
            if (mp.u < -1e-3 || mp.u > 1+1e-3 || mp.v < -1e-3 || mp.v > 1+1e-3)
                uvOut++;

            double du = fabs(mp.u - wantU), dv = fabs(mp.v - wantV);
            sumErrU += du; sumErrV += dv;
            sumErrUflip += fabs(mp.u - (1.0 - wantU));
            if (du > maxErrU) maxErrU = du;
            if (dv > maxErrV) maxErrV = dv;
        }
    }

    int lit = total - black;
    if (lit < 8)
    {
        printf("  %s globe %-4s fov %.0f aspect %.3f: nothing to sample (%d black)\n",
            lc.name, eWideView::GlobeNames[globe], fov, aspect, black);
        return;
    }
    double meanU = sumErrU / lit, meanUflip = sumErrUflip / lit;

    if (mism)
    {
        printf("  %s globe %-4s fov %.0f aspect %.3f: %d plate mismatches "
            "(mean |du| %.5f, mean |dv| %.5f)\n",
            lc.name, eWideView::GlobeNames[globe], fov, aspect, mism, meanU, sumErrV / lit);
        fails++;
    }
    if (maxErrU > 1e-3 || maxErrV > 1e-3)
    {
        printf("  %s globe %-4s fov %.0f aspect %.3f: sample position off by "
            "max |du| %.5f, max |dv| %.5f\n",
            lc.name, eWideView::GlobeNames[globe], fov, aspect, maxErrU, maxErrV);
        fails++;
    }
    // a map that agrees with the mirrored hypothesis is a mirrored map
    if (meanUflip < meanU + 1e-3)
    {
        printf("  %s globe %-4s fov %.0f aspect %.3f: the map matches u' = 1 - u, "
            "plate axes are mirrored (mean |du| %.5f)\n",
            lc.name, eWideView::GlobeNames[globe], fov, aspect, meanU);
        fails++;
    }
    if (blackMismatch)
    {
        printf("  %s globe %-4s fov %.0f aspect %.3f: %d pixels disagree with the "
            "lens about being inside the lens\n",
            lc.name, eWideView::GlobeNames[globe], fov, aspect, blackMismatch);
        fails++;
    }
    if (uvOut)
    {
        printf("  %s globe %-4s fov %.0f aspect %.3f: %d sample positions outside "
            "the plate image\n",
            lc.name, eWideView::GlobeNames[globe], fov, aspect, uvOut);
        fails++;
    }

    int used = 0;
    for (int i = 0; i < plateCount; i++)
        if (plateUse[i])
            used++;
    printf("  %-8s globe %-4s fov %3.0f aspect %.3f: %4d px, %3d black (%3d lens, "
        "%3d hole), %d plates used, max |du| %.5f, max |dv| %.5f, mirrored |du| %.5f\n",
        lc.name, eWideView::GlobeNames[globe], fov, aspect, total, black, blackLens,
        blackNoPlate, used, maxErrU, maxErrV, meanUflip);
}

// --------------------------------------------------- 2. lens algebra

static void CheckLensAlgebra ( const LensCase & lc )
{
    int rejected = 0, samples = 0;
    double worstRay = 0, worstFwdX = 0;
    double worstAt = 0, worstAtLon = 0;

    for (int lat = -80; lat <= 80; lat += 5)
    {
        for (int lon = -175; lon <= 175; lon += 5)
        {
            double phi = lat * DEG, lam = lon * DEG;
            double xy[3];
            if (!lc.forward( phi, lam, xy ))
                continue;
            samples++;

            // the module's x component must agree with this forward: the
            // lens scale is taken from it
            double xy0[3];
            if (lc.forward( 0, lam, xy0 ))
            {
                double d = fabs( wv_forwardX( lc.lens, 0, lam ) - xy0[0] );
                if (d > worstFwdX)
                    worstFwdX = d;
            }

            double ray[3];
            if (!lc.inverse( xy[0], xy[1], ray ))
            {
                if (rejected < 3)
                    printf("  %s rejects its own point (%.3f,%.3f) at lat %d lon %d\n",
                        lc.name, xy[0], xy[1], lat, lon);
                rejected++;
                continue;
            }
            Vec in = Vec(ray[0], ray[1], ray[2]);
            // the ray the forward map was built from
            double want[3];
            wv_latlon_to_ray( phi, lam, want );
            Vec wantV = Vec(want[0], want[1], want[2]);
            double d = magnitude(in - wantV);
            if (d > worstRay)
            {
                worstRay = d;
                worstAt = phi;
                worstAtLon = lam;
            }
            // rays must be unit length
            if (fabs(dot(in,in) - 1.0) > 1e-9)
            {
                Fail("a lens ray is not unit length");
            }
        }
    }

    if (samples < 100)
        Fail("too few samples inside a lens to judge it");
    if (rejected)
    {
        printf("  %s: %d of %d in-domain points rejected\n", lc.name, rejected, samples);
        fails++;
    }
    if (worstRay > lc.rayTol)
    {
        printf("  %s: forward then inverse off by %.6f (tolerance %.6f) at lat %.1f lon %.1f\n",
            lc.name, worstRay, lc.rayTol, worstAt/DEG, worstAtLon/DEG);
        fails++;
    }
    if (worstFwdX > 1e-9)
    {
        printf("  %s: wv_forwardX disagrees with the forward map by %.9f, "
            "the lens scale would be wrong\n", lc.name, worstFwdX);
        fails++;
    }
    printf("  %-8s %4d samples, worst round trip %.6f, wv_forwardX off by %.12f\n",
        lc.name, samples, worstRay, worstFwdX);
}

// ------------------------------------------------------- 3. lens shape

static void CheckLensShape ( const LensCase & lc )
{
    // the centre looks forward
    double r0[3];
    if (!lc.inverse( 0, 0, r0 ) ||
        fabs(r0[0]) > 1e-9 || fabs(r0[1]) > 1e-9 || fabs(r0[2] - 1) > 1e-9)
    {
        printf("  %s: the centre of the lens is not forward\n", lc.name);
        fails++;
        return;
    }

    // odd in longitude and in latitude, using a point safely inside
    double a[3], b[3], c[3], d[3];
    if (lc.inverse( .3, .2, a ) && lc.inverse( -.3, .2, b ) &&
        lc.inverse( .3, -.2, c ) && lc.inverse( -.3, -.2, d ))
    {
        double tol = lc.rayTol;
        if (fabs(a[0]+b[0]) > tol || fabs(a[1]-b[1]) > tol || fabs(a[2]-b[2]) > tol ||
            fabs(a[0]-c[0]) > tol || fabs(a[1]+c[1]) > tol || fabs(a[2]-c[2]) > tol ||
            fabs(a[0]+d[0]) > tol || fabs(a[1]+d[1]) > tol || fabs(a[2]-d[2]) > tol)
        {
            printf("  %s: the lens is not symmetric about its axes\n", lc.name);
            fails++;
        }
    }
    else
    {
        printf("  %s: (0.3,0.2) should be inside the lens\n", lc.name);
        fails++;
    }

    // angular radius grows with the image radius
    double prev = -1;
    for (int i = 1; i <= 40; i++)
    {
        double t = i / 40.0 * 2.5;
        double r[3];
        if (!lc.inverse( t, 0, r ))
            break;
        double ang = acos( r[2] < -1 ? -1 : (r[2] > 1 ? 1 : r[2]) );
        if (ang <= prev + 1e-6)
        {
            printf("  %s: the view direction stops turning at lens x %.3f\n", lc.name, t);
            fails++;
            break;
        }
        prev = ang;
    }
}

// ------------------------------------------------------------- main

int main()
{
    printf("lens map check: %d lenses, globes cube and trism\n", lensCount);

    double aspects[2] = {1.0, 4.0 / 3};
    double fovs[2] = {360, 180};

    for (int li = 0; li < lensCount; li++)
    {
        const LensCase & lc = lensCases[li];
        for (int fi = 0; fi < 2; fi++)
            for (int gi = 0; gi < 2; gi++)
                for (int ai = 0; ai < 2; ai++)
                    CheckMapAgainstWorld( lc,
                        gi == 0 ? eWideView::GLOBE_CUBE : eWideView::GLOBE_TRISM,
                        fovs[fi], aspects[ai] );
    }

    for (int li = 0; li < lensCount; li++)
        CheckLensAlgebra( lensCases[li] );

    for (int li = 0; li < lensCount; li++)
        CheckLensShape( lensCases[li] );

    printf("%s\n", fails == 0
        ? "EVERY LENS AGREES WITH THE WORLD-SPACE MODEL"
        : "A LENS DISAGREES WITH THE WORLD-SPACE MODEL");
    return fails == 0 ? 0 : 1;
}
