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

#ifndef Armagetron_eWideView_H
#define Armagetron_eWideView_H

#include "defs.h"
#include "eCoord.h"

class eCamera;

// 360 degree projection camera: captures the environment from a set of
// viewpoints (a "globe" of plates around the camera, à la the blinky Quake
// mod's fisheye/globe feature) and projects it onto the screen with a
// configurable "lens" (panini, mercator, equirectangular, stereographic,
// winkel tripel, fisheye, cylindrical), à la the flex-FOV lens set.
//
// The capture is done by re-rendering the scene once per plate into a
// square region of the viewport, the projection is a per-pixel lookup that
// maps each lens pixel to a ray, finds the plate that ray falls into and
// bilinearly samples the plate image.
class eWideView
{
    friend class eCamera;
public:
    //! globes: sets of plates around the camera (from blinky's globes)
    enum Globe
    {
        GLOBE_CUBE=0,    // 6 plates, 90 degrees (standard cubemap)
        GLOBE_TRISM=1,   // 5 plates, 120/128 degrees
        GLOBE_COUNT
    };

    //! lenses: projections from lens image space to rays
    // (from blinky's lenses and the flex-FOV shader)
    enum Lens
    {
        LENS_STANDARD=0, // no capture, normal view (feature off)
        LENS_PANINI,
        LENS_MERCATOR,
        LENS_EQUIRECT,
        LENS_STEREOGRAPHIC,
        LENS_WINKELTRIPEL,
        LENS_FISHEYE,    // equidistant
        LENS_CYLINDER,
        LENS_COUNT
    };

    //! is the wide view capture active (lens other than standard)?
    static bool Active();

    //! render the wide view into the camera's viewport, replacing the normal
    //! view. pos_diff and glancedir are the camera's glanced position and
    //! direction, as computed by eCamera::Render.
    static void Render( eCamera * cam, eCoord const & pos_diff,
                        eCoord const & glancedir );

    static const char * LensNames[LENS_COUNT];
    static const char * GlobeNames[GLOBE_COUNT];

    //! cvars
    static int   se_wideViewLens;    // WIDE_VIEW_LENS: lens selection
    static REAL  se_wideViewFOV;     // WIDE_VIEW_FOV: horizontal field of view in degrees
    static int   se_wideViewGlobe;   // WIDE_VIEW_GLOBE: plate layout
    static REAL  se_wideViewPlateRes;// WIDE_VIEW_PLATE_RES: plate resolution (fraction of the smaller viewport dimension)
    static REAL  se_wideViewLensRes; // WIDE_VIEW_LENS_RES: lens image resolution (fraction of the viewport)
};

#endif
