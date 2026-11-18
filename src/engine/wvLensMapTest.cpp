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

// Self check for the 360 view lens map (make check).
//
// wv_buildLensmap() stores, for every lens pixel, the plate whose frustum
// contains the pixel's ray and where inside that plate image to sample.  The
// check rebuilds the same information with a second, independent model: it
// takes the plate axes straight out of the globe tables, turns them into
// world space with the camera basis, and projects the ray with gluLookAt
// semantics (right = up x -forward, up = -forward x right) using nothing but
// the plate field of view and the pixel aspect.  The two models share no
// cross products: the map builds rays from a lens basis, the model works in
// world space only.
//
// What this catches: the map's plate right axis is up x forward.  Forward x up
// is the negative of it - the lens basis (right, up, forward) is left handed
// with respect to the world, because a camera basis is right handed as
// (right, up, -forward) - and sampling with that axis mirrors every plate, so
// the panorama shows a mirrored world.  The mirrored hypothesis is measured
// too and must stay away from zero, so the mirror cannot come back unnoticed.
// It also catches a single shared x/y sample bound: rViewport::Perspective
// builds xmul and ymul = xmul/aspect, so a square plate viewport with non
// square pixels samples v outside the plate frustum.
//
// Run: make check (in src).  CPU only, no GL context.  Measured on the
// corrected map: 0 plate mismatches and max |du| = max |dv| = 0.00000 for
// both globes at pixel aspect 1 and 4/3; the mirrored hypothesis sits at
// mean |du| around 0.35, and with the old axis the cube globe showed mean
// |du| 0.357 / max 0.993 at aspect 1 plus 358 plate mismatches and mean
// |dv| 0.043 at aspect 4/3.

#include "eWideView.cpp"

#include <cstdio>
#include <cmath>

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

// sample tolerance: u,v are REAL and the model is double, so anything above
// float rounding is a real disagreement
static const double TOL = 1e-3;

int main()
{
    // a deliberately non-axis-aligned camera: looking along world +X, head up
    // along world +Z -> the true right is world -Y.
    Vec fW(1, 0, 0);
    Vec upH(0, 0, 1);
    Vec sW = norm(cross(fW, upH));       // camera right
    Vec uW = cross(sW, fW);              // camera up

    printf("cam: fwd %.3f %.3f %.3f  right %.3f %.3f %.3f  up %.3f %.3f %.3f\n",
        fW.x,fW.y,fW.z, sW.x,sW.y,sW.z, uW.x,uW.y,uW.z);

    double aspects[2] = {1.0, 4.0 / 3};
    int fails = 0;

    for (int ai = 0; ai < 2; ai++)
    {
        double asp = aspects[ai];
        currentScreensetting.aspect = (float) asp;

        for (int gi = 0; gi < 2; gi++)
        {
            int globe = gi == 0 ? eWideView::GLOBE_CUBE : eWideView::GLOBE_TRISM;
            const WVPlate * plates = gi == 0 ? wv_cubePlates : wv_trismPlates;
            int plateCount = gi == 0 ? 6 : 5;

            const int w = 73, h = 41;
            wv_mapLens = -1;                 // force a rebuild
            wv_buildLensmap(eWideView::LENS_EQUIRECT, globe, 360, w, h);

            double scale = wv_mapScale;
            int mism = 0, total = 0, black = 0;
            double sumErrU = 0, sumErrUflip = 0, sumErrV = 0;
            double maxErrU = 0, maxErrV = 0;

            for (int ly = 0; ly < h; ly++)
            {
                for (int lx = 0; lx < w; lx++)
                {
                    const WVMapPixel &mp = wv_lensmap[(size_t) ly * w + lx];
                    total++;
                    if (mp.plate < 0)
                    {
                        black++;
                        continue;
                    }
                    // lens pixel -> lens ray
                    double x = (lx + .5 - w * .5) * scale;
                    double y = (h * .5 - ly - .5) * scale;
                    double lr[3];
                    if (!wv_inverseEquirect(x, y, lr))
                    {
                        printf("map says plate %d but the lens rejects (%.3f,%.3f)\n",
                            mp.plate, x, y);
                        mism++;
                        continue;
                    }
                    Vec rayW = sW * lr[0] + uW * lr[1] + fW * lr[2];

                    // independent model: first plate in table order whose
                    // frustum holds the ray
                    int wantPlate = -1;
                    double wantU = .5, wantV = .5;
                    for (int i = 0; i < plateCount; i++ )
                    {
                        Vec fw(plates[i].fwd[0], plates[i].fwd[1], plates[i].fwd[2]);
                        Vec uw(plates[i].up[0], plates[i].up[1], plates[i].up[2]);
                        // lens vector -> world
                        Vec fwdW = sW * fw.x + uW * fw.y + fW * fw.z;
                        Vec upW  = sW * uw.x + uW * uw.y + fW * uw.z;
                        PlateCam pc = makeCam(fwdW, upW, plates[i].fov, asp);
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
                    if (wantPlate != mp.plate)
                    {
                        if (mism < 6)
                            printf("pixel %3d,%3d ray %.3f %.3f %.3f: map plate %d, model plate %d\n",
                                lx, ly, rayW.x, rayW.y, rayW.z, mp.plate, wantPlate);
                        mism++;
                        continue;
                    }
                    double du = fabs(mp.u - wantU), dv = fabs(mp.v - wantV);
                    sumErrU += du; sumErrV += dv;
                    sumErrUflip += fabs(mp.u - (1.0 - wantU));
                    if (du > maxErrU) maxErrU = du;
                    if (dv > maxErrV) maxErrV = dv;
                }
            }

            int lit = total - black;
            double meanU = sumErrU / lit, meanUflip = sumErrUflip / lit;
            printf("aspect %.3f globe %-4s: %d px, %d black, %d mismatch, "
                "mean |du| %.5f, mean mirrored |du| %.5f, mean |dv| %.5f, "
                "max |du| %.5f, max |dv| %.5f\n",
                asp, eWideView::GlobeNames[globe], total, black, mism,
                meanU, meanUflip, sumErrV / lit, maxErrU, maxErrV);

            fails += mism;
            if (maxErrU > TOL || maxErrV > TOL)
            {
                printf("sample position off by more than %g\n", TOL);
                fails++;
            }
            // a map that agrees with the mirrored hypothesis is a mirrored map
            if (meanUflip < meanU + 1e-3)
            {
                printf("the map matches u' = 1 - u: plate axes are mirrored\n");
                fails++;
            }
        }
    }

    printf("%s\n", fails == 0
        ? "LENSMAP AGREES WITH THE WORLD-SPACE MODEL"
        : "LENSMAP DISAGREES WITH THE WORLD-SPACE MODEL");
    return fails == 0 ? 0 : 1;
}
