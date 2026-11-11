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

#include "gStuff.h"
#include "tSysTime.h"
#include "tDirectories.h"
#include "tLocale.h"
#include "rViewport.h"
#include "rConsole.h"
#include "gGame.h"
#include "gLogo.h"
#include "eSound.h"
#include "rScreen.h"
#include "rSysdep.h"
#include "uInputQueue.h"
#include "wdbg.h"
#include "tWasmLoop.h"
//#include "eTess.h"
#include "rTexture.h"
#include "tConfiguration.h"
#include "tRandom.h"
#include "tRecorder.h"
#include "tCommandLine.h"
#include "tToDo.h"
#include "eAdvWall.h"
#include "eGameObject.h"
#include "uMenu.h"
#include "ePlayer.h"
#include "gLanguageMenu.h"
#include "gAICharacter.h"
#include "gCycle.h"
//#include <unistd>
#include <stdio.h>
#include <stdlib.h>
#include <fstream>

#include "nServerInfo.h"
#include "nSocket.h"

#ifndef DEDICATED
#include "rRender.h"
#include "rSDL.h"
#endif

// data structure for command line parsing
class gMainCommandLineAnalyzer: public tCommandLineAnalyzer
{
public:
    bool     fullscreen_;
    bool     windowed_;
    bool     use_directx_;
    bool     dont_use_directx_;

    gMainCommandLineAnalyzer()
    {
        windowed_ = false;
        fullscreen_ = false;
        use_directx_ = false;
        dont_use_directx_ = false;
    }


private:
    bool DoAnalyze( tCommandLineParser & parser, int pass ) override
    {
        if(pass > 0)
            return false;

        if ( parser.GetSwitch( "-fullscreen", "-f" ) )
        {
            fullscreen_=true;
        }
        else if ( parser.GetSwitch( "-window", "-w" ) ||  parser.GetSwitch( "-windowed") )
        {
            windowed_=true;
        }
#ifdef WIN32
        else if ( parser.GetSwitch( "+directx") )
        {
            use_directx_=true;
        }
        else if ( parser.GetSwitch( "-directx") )
        {
            dont_use_directx_=true;
        }
#endif
        else
        {
            return false;
        }

        return true;
    }

    void DoHelp( std::ostream & s ) override
    {                                      //
#ifndef DEDICATED
        s << "-f, --fullscreen             : start in fullscreen mode\n";
        s << "-w, --window, --windowed     : start in windowed mode\n\n";
#ifdef WIN32
        s << "+directx, -directx           : enable/disable usage of DirectX for screen\n"
        << "                               initialisation under MS Windows\n\n";
        s << "\n\nYes, I know this looks ugly. Sorry about that.\n";
#endif
#endif
    }
};

static gMainCommandLineAnalyzer commandLineAnalyzer;

extern bool sr_useDirectX; // rScreen.cpp
#ifdef WIN32
static tConfItem<bool> udx("USE_DIRECTX","makes use of the DirectX input "
                           "fuctions; causes some graphic cards to fail to work (VooDoo 3,...)",
                           sr_useDirectX);
#endif

extern void exit_game_objects(eGrid *grid);

enum gConnection
{
    gLeave,
    gDialup,
    gISDN,
    gDSL
    // gT1
};

#ifdef __EMSCRIPTEN__
// Browser build: the startup flow (language menu -> first setup ->
// welcome message -> first game -> main menu) cannot run straight
// through - control is handed to the JS event loop at the first menu
// loop (tWasmLoop) and the C++ stack below it is never resumed.  The
// flow is re-run as a chain of steps (WasmStep_* below): each step
// ends by starting a trampoline loop and registering the next step
// with tWasmLoop::SetContinuation.
//
// The variables below are the durable storage the startup menu items
// write their selections into: the items keep pointers to the values,
// and the stack frame that owned the original locals is unwound when
// the first loop starts.
static bool s_chainTextOutBack = false;
static REAL s_splashTimeout = 0;

struct WasmFirstUseSettings
{
    bool autoNum;
    REAL speedFactor;
    REAL sizeFactor;
    REAL wallsLength;
    REAL rubber;
    REAL delayCycle;
};
static WasmFirstUseSettings s_firstUseSettings;

static gConnection s_playerMenuConnection = gLeave;
static tColor s_playerMenuColor( 0, 0, 0, 0 );
static tString s_playerMenuKeyboardTemplate;

static void WasmStep_LanguageMenu( void );
static void WasmStep_PlayerMenu( void );
static void WasmStep_WelcomeMessage( void );
static void WasmStep_FirstGame( void );
static void WasmStep_AfterGame( void );
static void WasmStep_MainTail( void );
#endif

void sg_StartupPlayerMenu()
{
    uMenu firstSetup_dummy(tOutput("$first_setup"), false);
    uMenu * firstSetup;
#ifdef __EMSCRIPTEN__
    firstSetup = uMenu::WasmNew( tOutput("$first_setup"), false );
    firstSetup->SetOnLoopExit( &uMenu::WasmFreeTree );
#else
    firstSetup = &firstSetup_dummy;
#endif
    firstSetup->SetBot(-.2);

    uMenuItemExit * e2 = new uMenuItemExit(firstSetup, "$menuitem_accept", "$menuitem_accept_help");

    ePlayer * player = ePlayer::PlayerConfig(0);
    tASSERT( player );

#ifdef __EMSCRIPTEN__
    // browser: the menu items write their selections through pointers,
    // and this stack frame is unwound when the menu loop starts - the
    // values must live in storage that outlives it (WasmStep_WelcomeMessage
    // reads them when the loop ends)
    s_playerMenuConnection = gDSL;
    gConnection & connection = s_playerMenuConnection;
#else
    gConnection connection = gDSL;
#endif

    uMenuItemSelection<gConnection> * net = new uMenuItemSelection<gConnection>(firstSetup, "$first_setup_net", "$first_setup_net_help", connection );
    if ( !st_FirstUse )
    {
        net->NewChoice( "$first_setup_leave", "$first_setup_leave_help", gLeave );
        connection = gLeave;
    }
    net->NewChoice( "$first_setup_net_dialup", "$first_setup_net_dialup_help", gDialup );
    net->NewChoice( "$first_setup_net_isdn", "$first_setup_net_isdn_help", gISDN );
    net->NewChoice( "$first_setup_net_dsl", "$first_setup_net_dsl_help", gDSL );

#ifdef __EMSCRIPTEN__
    s_playerMenuKeyboardTemplate = "keys_cursor.cfg";
    tString & keyboardTemplate = s_playerMenuKeyboardTemplate;
#else
    tString keyboardTemplate("keys_cursor.cfg");
#endif
    uMenuItemSelection<tString> * k = new uMenuItemSelection<tString>(firstSetup, "$first_setup_keys", "$first_setup_keys_help", keyboardTemplate );
    if ( !st_FirstUse )
    {
        k->NewChoice( "$first_setup_leave", "$first_setup_leave_help", tString("") );
        keyboardTemplate="";
    }
    k->NewChoice( "$first_setup_keys_cursor", "$first_setup_keys_cursor_help", tString("keys_cursor.cfg") );
    k->NewChoice( "$first_setup_keys_wasd", "$first_setup_keys_wasd_help", tString("keys_wasd.cfg") );
    k->NewChoice( "$first_setup_keys_zqsd", "$first_setup_keys_zqsd_help", tString("keys_zqsd.cfg") );
    k->NewChoice( "$first_setup_keys_cursor_single", "$first_setup_keys_cursor_single_help", tString("keys_cursor_single.cfg") );
    // k->NewChoice( "$first_setup_keys_both", "$first_setup_keys_both_help", tString("keys_twohand.cfg") );
    k->NewChoice( "$first_setup_keys_x", "$first_setup_keys_x_help", tString("keys_x.cfg") );

    tColor leave(0,0,0,0);
#ifdef __EMSCRIPTEN__
    s_playerMenuColor = tColor(1,0,0);
    tColor & color = s_playerMenuColor;
#else
    tColor color(1,0,0);
#endif
    uMenuItemSelection<tColor> * c = new uMenuItemSelection<tColor>(firstSetup,
                                 "$first_setup_color",
                                 "$first_setup_color_help",
                                 color);

    if ( !st_FirstUse )
    {
        color = leave;
        c->NewChoice( "$first_setup_leave", "$first_setup_leave_help", leave );
    }

    c->NewChoice( "$first_setup_color_red", "", tColor(1,0,0) );
    c->NewChoice( "$first_setup_color_blue", "", tColor(0,0,1) );
    c->NewChoice( "$first_setup_color_green", "", tColor(0,1,0) );
    c->NewChoice( "$first_setup_color_yellow", "", tColor(1,1,0) );
    c->NewChoice( "$first_setup_color_orange", "", tColor(1,.5,0) );
    c->NewChoice( "$first_setup_color_purple", "", tColor(.5,0,1) );
    c->NewChoice( "$first_setup_color_magenta", "", tColor(1,0,1) );
    c->NewChoice( "$first_setup_color_cyan", "", tColor(0,1,1) );
    c->NewChoice( "$first_setup_color_white", "", tColor(1,1,1) );
    c->NewChoice( "$first_setup_color_dark", "", tColor(0,0,0) );

    if ( st_FirstUse )
    {
        for(int i=tRandomizer::GetInstance().Get(4); i>=0; --i)
        {
            c->LeftRight(1);
        }
    }

    uMenuItemString * n = new uMenuItemString(firstSetup,
                      "$player_name_text",
                      "$player_name_help",
                      player->name, 16);

    uMenuItemExit * e = new uMenuItemExit(firstSetup, "$menuitem_accept", "$menuitem_accept_help");

#ifdef __EMSCRIPTEN__
    // browser: when this menu's loop ends, the tail below (network
    // rates, color, keyboard layout) runs as a continuation
    tWasmLoop::SetContinuation( &WasmStep_WelcomeMessage );
#endif

    firstSetup->Enter();

#ifndef __EMSCRIPTEN__
    // apply network rates
    switch(connection)
    {
    case gDialup:
        sn_maxRateIn  = 6;
        sn_maxRateOut = 4;
        break;
    case gISDN:
        sn_maxRateIn  = 8;
        sn_maxRateOut = 8;
        break;
    case gDSL:
        sn_maxRateIn  = 64;
        sn_maxRateOut = 16;
        break;
    case gLeave:
        break;
    }

    // store color
    if( ! (color == leave) )
    {
        player->rgb[0] = int(color.r_*15);
        player->rgb[1] = int(color.g_*15);
        player->rgb[2] = int(color.b_*15);
    }

    // load keyboard layout
    if( keyboardTemplate.Len() > 1 )
    {
        std::ifstream s;
        if( tConfItemBase::OpenFile( s, keyboardTemplate, tConfItemBase::Config ) )
        {
            tCurrentAccessLevel level( tAccessLevel_Owner, true );
            tConfItemBase::ReadFile( s );
        }
    }
#endif
}

#ifndef DEDICATED
#ifdef __EMSCRIPTEN__
// (the startup chain: storage and forward declarations are above,
// outside the DEDICATED guard; the step definitions live here next
// to welcome())

// one frame of the splash screen (non-first-run); returns false when
// the splash ends (key press or timeout)
static bool WasmSplashFrame( void * )
{
    static int iter = 0;
    SDL_Event tEvent;
    bool hasEvent = false;
    {
        uInputProcessGuard inputProcessGuard;
        hasEvent = su_GetSDLInput(tEvent);
    }
    if ( (hasEvent && tEvent.type==SDL_KEYDOWN) || tSysTimeFloat() >= s_splashTimeout )
    {
        if ( hasEvent && tEvent.type==SDL_KEYDOWN )
            wdbgi("splash loop exit: KEYDOWN scancode", tEvent.key.keysym.scancode);
        else
            wdbgi("splash loop exit: timeout", (int)s_splashTimeout);
        return false;
    }

    if ( sr_glOut )
    {
        sr_ResetRenderState(true);
        rViewport::s_viewportFullscreen.Select();

        rSysDep::ClearGL();

        uMenu::GenericBackground();

        rSysDep::SwapGL();
    }

    tAdvanceFrame();
    if ( iter < 5 || (iter % 120) == 0 )
        wdbgi("splash loop iter", iter);
    ++iter;
    return true;
}

// step 1: language selection menu
static void WasmStep_LanguageMenu( void )
{
    sr_textOut = s_chainTextOutBack;
    tWasmLoop::SetContinuation( &WasmStep_PlayerMenu );
    sg_StartupLanguageMenu();
}

// step 2: first-setup (player) menu
static void WasmStep_PlayerMenu( void )
{
    sr_textOut = s_chainTextOutBack;
    tWasmLoop::SetContinuation( &WasmStep_WelcomeMessage );
    sg_StartupPlayerMenu();
}

// step 3: the tail of sg_StartupPlayerMenu (which normally follows its
// menu), then the welcome message
static void WasmStep_WelcomeMessage( void )
{
    // apply network rates
    switch( s_playerMenuConnection )
    {
    case gDialup:
        sn_maxRateIn  = 6;
        sn_maxRateOut = 4;
        break;
    case gISDN:
        sn_maxRateIn  = 8;
        sn_maxRateOut = 8;
        break;
    case gDSL:
        sn_maxRateIn  = 64;
        sn_maxRateOut = 16;
        break;
    case gLeave:
        break;
    }

    // store color
    {
        ePlayer * player = ePlayer::PlayerConfig(0);
        tColor leave(0,0,0,0);
        if ( player && !( s_playerMenuColor == leave ) )
        {
            player->rgb[0] = int(s_playerMenuColor.r_*15);
            player->rgb[1] = int(s_playerMenuColor.g_*15);
            player->rgb[2] = int(s_playerMenuColor.b_*15);
        }
    }

    // load keyboard layout
    if( s_playerMenuKeyboardTemplate.Len() > 1 )
    {
        std::ifstream s;
        if( tConfItemBase::OpenFile( s, s_playerMenuKeyboardTemplate, tConfItemBase::Config ) )
        {
            tCurrentAccessLevel level( tAccessLevel_Owner, true );
            tConfItemBase::ReadFile( s );
        }
    }

    st_FirstUse = false;
#ifdef __EMSCRIPTEN__
    // persist FIRST_USE=0 (and the first-use choices) immediately; the
    // exit-time st_SaveConfig may never run in the browser (tab closed,
    // crash) and the IDBFS volume is only as fresh as the last save
    st_SaveConfig();
#endif

    sr_textOut = s_chainTextOutBack;
    tWasmLoop::SetContinuation( &WasmStep_FirstGame );
    uMenu::Message( tOutput("$welcome_message_heading"), tOutput("$welcome_message"), 300 );
}

// step 4: the first single-player game
static void WasmStep_FirstGame( void )
{
    s_firstUseSettings.speedFactor = sg_currentSettings->speedFactor;
    s_firstUseSettings.autoNum = sg_currentSettings->autoNum;
    s_firstUseSettings.sizeFactor = sg_currentSettings->sizeFactor;
    s_firstUseSettings.wallsLength = sg_currentSettings->wallsLength;
    s_firstUseSettings.rubber = sg_rubberCycle;
    s_firstUseSettings.delayCycle = sg_delayCycle;

    sg_currentSettings->speedFactor = -2;
    sg_currentSettings->autoNum = 0;
    sg_currentSettings->sizeFactor -= 2;
    sg_currentSettings->wallsLength = 400;
    sg_rubberCycle = 5;
    sg_delayCycle = 0.05;

    sr_textOut = s_chainTextOutBack;
    tWasmLoop::SetContinuation( &WasmStep_AfterGame );
    sg_SinglePlayerGame();
}

// step 5: after the first game: restore the settings, second welcome message
static void WasmStep_AfterGame( void )
{
    sg_currentSettings->autoNum = s_firstUseSettings.autoNum;
    sg_currentSettings->speedFactor = s_firstUseSettings.speedFactor;
    sg_currentSettings->sizeFactor = s_firstUseSettings.sizeFactor;
    sg_currentSettings->wallsLength = s_firstUseSettings.wallsLength;
    sg_rubberCycle = s_firstUseSettings.rubber;
    sg_delayCycle = s_firstUseSettings.delayCycle;

    sr_textOut = s_chainTextOutBack;
    tWasmLoop::SetContinuation( &WasmStep_MainTail );
    uMenu::Message( tOutput("$welcome_message_2_heading"), tOutput("$welcome_message_2"), 300 );
}

// step 6: the tail of the main flow (which normally sits between
// welcome() and MainMenu()), then the main menu.  Ending the main menu
// ends the chain (the pump then idles).
static void WasmStep_MainTail( void )
{
    sr_con.autoDisplayAtSwap=false;
    se_SoundPause(false);
    gLogo::SetBig(false);
    gLogo::SetSpinning(true);
    sn_bigBrotherString = renderer_identification + "VER=" + sn_programVersion + "\n\n";

    tWasmLoop::SetContinuation( 0 );
    MainMenu();
}
#endif

static void welcome(){
    bool textOutBack = sr_textOut;
    sr_textOut = false;

#ifdef DEBUG_XXXX
    {
        for (int i = 20; i>=0; i--)
        {
            sr_ClearGL();
            {
                rTextField c(-.8,.6, .1, .1);
                tString s;
                s << ColorString(1,1,1);
                s << "Test";
                s << ColorString(1,0,0);
                s << "bla bla blubb blaa blaa blubbb blaaa blaaa blubbbb blaaaa blaaaa blubbbbb blaaaaa blaaaaa blubbbbbb blaaaaaa\n";
                c << s;
            }
            sr_SwapGL();
        }
    }
#endif

    REAL timeout = tSysTimeFloat() + .2;
    SDL_Event tEvent;
    wdbgi("welcome: st_FirstUse", st_FirstUse ? 1 : 0);

    if (st_FirstUse)
    {
        sr_LoadDefaultConfig();
        textOutBack = sr_textOut;
        sr_textOut = false;
        gLogo::SetBig(false);
        gLogo::SetSpinning(true);
    }
    else
    {
        bool showSplash = true;
#ifdef DEBUG
        showSplash = false;
#endif

        // disable splash screen when recording (it's annoying)
        static const char * splashSection = "SPLASH";
        if ( tRecorder::IsRunning() )
        {
            showSplash = false;

            // but keep it for old recordings where the splash screen was always active
            if ( !tRecorder::Playback( splashSection, showSplash ) )
                showSplash = tRecorder::IsPlayingBack();
        }
        tRecorder::Record( splashSection, showSplash );

#ifdef __EMSCRIPTEN__
        // browser: the splash loop can't block the JS event loop; run
        // it as a trampoline loop, and the main flow's tail
        // (WasmStep_MainTail) runs when it ends
        s_chainTextOutBack = textOutBack;
        s_splashTimeout = showSplash ? tSysTimeFloat() + 6 : 0;
        tWasmLoop::SetContinuation( &WasmStep_MainTail );
        tWasmLoop::Start( &WasmSplashFrame );
        return;
#else
        if ( showSplash )
        {
            timeout = tSysTimeFloat() + 6;

            uInputProcessGuard inputProcessGuard;
            {
                int iter = 0;
                bool hasEvent = false;
                while (true)
                {
                    hasEvent = su_GetSDLInput(tEvent);
                    if ( (hasEvent && tEvent.type==SDL_KEYDOWN) || tSysTimeFloat() >= timeout )
                        break;

                    if ( sr_glOut )
                    {
                        sr_ResetRenderState(true);
                        rViewport::s_viewportFullscreen.Select();

                        rSysDep::ClearGL();

                        uMenu::GenericBackground();

                        rSysDep::SwapGL();
                    }

                    tAdvanceFrame();
                    if ( iter < 5 || (iter % 120) == 0 )
                        wdbgi("splash loop iter", iter);
                    ++iter;
                }
                // log why we exited (tEvent holds the event that broke the loop)
                if ( hasEvent && tEvent.type==SDL_KEYDOWN )
                    wdbgi("splash loop exit: KEYDOWN scancode", tEvent.key.keysym.scancode);
                else if ( tSysTimeFloat() >= timeout )
                    wdbgi("splash loop exit: timeout", (int)timeout);
                else
                    wdbgi("splash loop exit: other", (int)iter);
            }
        }

        // catch some keyboard input
        {
            uInputProcessGuard inputProcessGuard;
            while (su_GetSDLInput(tEvent)) ;
        }

        sr_textOut = textOutBack;
        return;
#endif
    }

    if ( sr_glOut )
    {
        rSysDep::ClearGL();
        rFont::s_defaultFont.Select();
        rFont::s_defaultFontSmall.Select();
        gLogo::Display();
        rSysDep::ClearGL();
    }
    rSysDep::SwapGL();

#ifdef __EMSCRIPTEN__
    // browser: the straight-line tail below can't run - control is
    // handed to the JS event loop at the first menu; the steps
    // re-run it as a continuation chain
    s_chainTextOutBack = textOutBack;
    WasmStep_LanguageMenu();
    return;
#else
    sr_textOut = textOutBack;
    sg_StartupLanguageMenu();

    sr_textOut = textOutBack;
    sg_StartupPlayerMenu();

    st_FirstUse=false;

    sr_textOut = textOutBack;
    uMenu::Message( tOutput("$welcome_message_heading"), tOutput("$welcome_message"), 300 );

    // start a first single player game
    auto speedFactor = sg_currentSettings->speedFactor;
    auto autoNum = sg_currentSettings->autoNum;
    auto sizeFactor = sg_currentSettings->sizeFactor;
    auto wallsLength = sg_currentSettings->wallsLength;
    auto rubber = sg_rubberCycle;
    auto delayCycle = sg_delayCycle;
    sg_currentSettings->speedFactor = -2;
    sg_currentSettings->autoNum = 0;
    sg_currentSettings->sizeFactor -= 2;
    sg_currentSettings->wallsLength = 400;
    sg_rubberCycle = 5;
    sg_delayCycle = 0.05;
    sr_textOut = textOutBack;
    sg_SinglePlayerGame();
    sg_currentSettings->autoNum = autoNum;
    sg_currentSettings->speedFactor = speedFactor;
    sg_currentSettings->sizeFactor= sizeFactor;
    sg_currentSettings->wallsLength = wallsLength;
    sg_rubberCycle = rubber;
    sg_delayCycle = delayCycle;

    sr_textOut = textOutBack;
    uMenu::Message( tOutput("$welcome_message_2_heading"), tOutput("$welcome_message_2"), 300 );

    sr_textOut = textOutBack;
#endif
}
#endif

void cleanup(eGrid *grid){
    static bool reentry=true;
    if (reentry){
        reentry=false;
        su_contInput=false;

        exit_game_objects(grid);
        /*
          for(int i=MAX_PLAYERS-1;i>=0;i--){
          if (playerConfig[i])
          destroy(playerConfig[i]->cam);
          }
          
          
          gNetPlayerWall::Clear();

          eFace::Clear();
          eEdge::Clear();
          ePoint::Clear();
          
          eFace::Clear();
          eEdge::Clear();
          ePoint::Clear();
          
          eGameObject::DeleteAll();


        */

#ifdef POWERPAK_DEB
        if (pp_out){
            PD_Quit();
            PP_Quit();
        }
#endif
        nNetObject::ClearAll();

        if (sr_glOut){
            rITexture::UnloadAll();
        }

        se_SoundExit();
        sr_glOut=false;
        sr_ExitDisplay();

#ifndef DEDICATED
        sr_RendererCleanup();
#endif

    }
}

#ifndef DEDICATED
static bool sg_active = true;
static void sg_DelayedActivation()
{
    Activate( sg_active );
}

#ifdef __EMSCRIPTEN__
int filter(void *userdata, SDL_Event *tEvent)
#else
int filter(const SDL_Event *tEvent)
#endif
{
    // recursion avoidance
    static bool recursion = false;
    if ( !recursion )
    {
        class RecursionGuard
        {
        public:
            RecursionGuard( bool& recursion )
                    :recursion_( recursion )
            {
                recursion = true;
            }

            ~RecursionGuard()
            {
                recursion_ = false;
            }

        private:
            bool& recursion_;
        };

        RecursionGuard guard( recursion );

        // boss key or OS X quit command
        if ((tEvent->type==SDL_KEYDOWN && tEvent->key.keysym.sym==27 &&
                tEvent->key.keysym.mod & KMOD_SHIFT) ||
                (tEvent->type==SDL_KEYDOWN && tEvent->key.keysym.sym==113 &&
                 tEvent->key.keysym.mod & KMOD_META) ||
                (tEvent->type==SDL_QUIT)){
            // sn_SetNetState(nSTANDALONE);
            // sn_Receive();

            // register end of recording
            tRecorder::Record("END");

            st_SaveConfig();
            uMenu::quickexit=uMenu::QuickExit_Total;
            return false;
        }

        if (tEvent->type==SDL_MOUSEMOTION)
            if (tEvent->motion.x==sr_screenWidth/2 && tEvent->motion.y==sr_screenHeight/2)
                return 0;
        if (su_mouseGrab &&
                tEvent->type!=SDL_MOUSEBUTTONDOWN &&
                tEvent->type!=SDL_MOUSEBUTTONUP &&
                ((tEvent->motion.x>=sr_screenWidth-10  || tEvent->motion.x<=10) ||
                 (tEvent->motion.y>=sr_screenHeight-10 || tEvent->motion.y<=10)))
            SDL_WarpMouse(sr_screenWidth/2,sr_screenHeight/2);

        // fetch alt-tab

        if (tEvent->type==SDL_ACTIVEEVENT)
        {
            // Jonathans fullscreen bugfix.
#ifdef MACOSX
            if (currentScreensetting.fullscreen ^ lastSuccess.fullscreen) return false;
#endif
            int flags = SDL_APPINPUTFOCUS;
            if ( tEvent->active.state & flags )
            {
                // con << tSysTimeFloat() << " " << "active: " << (tEvent->active.gain ? "on" : "off") << "\n";
                sg_active = tEvent->active.gain;
                st_ToDo(sg_DelayedActivation);
            }

            // reload GL stuff if application gets reactivated
            if ( tEvent->active.gain && tEvent->active.state & SDL_APPACTIVE )
            {
                // just treat it like a screen mode change, gets the job done
                st_ToDo(rCallbackBeforeScreenModeChange::Exec);
                st_ToDo(rCallbackAfterScreenModeChange::Exec);
            }
            return false;
        }

        if (su_prefetchInput){
            return su_StoreSDLEvent(*tEvent);
        }

    }

    return 1;
}
#endif

//from game.C
void Update_netPlayer();

void sg_SetIcon()
{
#ifndef DEDICATED
    rSurface tex( "textures/icon.png" );
    //    SDL_Surface *tex=IMG_Load( tDirectories::Data().GetReadPath( "textures/icon.png" ) );

    if (tex.GetSurface())
        SDL_WM_SetIcon(tex.GetSurface(),NULL);
#endif
}

class gAutoStringArray
{
public:
    ~gAutoStringArray()
    {
#ifdef HAVE_CLEARENV
        // Optional. Systems that don't have this function better make copies of putenv() arguments.
        clearenv();
#endif

        for ( std::vector< char * >::iterator i = strings.begin(); i != strings.end(); ++i )
        {
            free( *i );
        }
    }

    char * Store( char const * s )
    {
        char * ret = strdup( s );
        strings.push_back( ret );
        return ret;
    }
private:
    std::vector< char * > strings; // the stored raw C strings
};

// wrapper for putenv, taking care of the peculiarity that the argument
// is kept in use for the rest of the program's lifetime
void sg_PutEnv( char const * s )
{
    static gAutoStringArray store;
    putenv( store.Store( s ) );
}

namespace
{
tString sn_configurationSavedInVersion{"0.2.8"};
tConfItem<tString> sn_configurationSavedInVersionConf("SAVED_IN_VERSION",sn_configurationSavedInVersion);

#ifndef DEDICATED
struct SDLCleanup
{
    // no init, that requires parameters and gives a return
    // in the browser build the page (not main) owns the SDL lifetime;
    // tearing down here would kill the canvas/GL context while the
    // JS event loop is still driving the game
#ifndef __EMSCRIPTEN__
    ~SDLCleanup(){SDL_Quit();}
#endif
};
struct SDLSoundCleanup
{
    SDLSoundCleanup(){se_SoundInit();}
#ifndef __EMSCRIPTEN__
    ~SDLSoundCleanup(){se_SoundExit();}
#endif
};
#endif
}

int main(int argc,char **argv){
    //std::cout << "enter\n";
    //  net_test();

    bool dedicatedServer = false;

    //  std::cout << "Running " << argv[0] << "...\n";

    // tERR_MESSAGE( "Start!" );

    try
    {
        tCommandLineData commandLine;
        commandLine.programVersion_  = &sn_programVersion;

        // analyse command line
        // tERR_MESSAGE( "Analyzing command line." );
        if ( ! commandLine.Analyse(argc, argv) )
            return 0;


        {
            // embed version in recording
            const char * versionSection = "VERSION";
            tString version( sn_programVersion );
            tRecorder::Playback( versionSection, version );
            tRecorder::Record( versionSection, version );
#ifndef DEDICATED
            if(version != sn_programVersion)
            {
#ifdef DEBUG
                tERR_WARN( "Recording from a different version, consider at high risk of desync." );
#endif
                tRecorder::ActivateProbablyDesyncedPlayback();
            }
#endif
        }

        {
            // read/write server/client mode from/to recording
            const char * dedicatedSection = "DEDICATED";
            if ( !tRecorder::PlaybackStrict( dedicatedSection, dedicatedServer ) )
            {
#ifdef DEDICATED
                dedicatedServer = true;
#endif
            }
            tRecorder::Record( dedicatedSection, dedicatedServer );
        }

        // tERR_MESSAGE( "Initializing player data." );
        ePlayer::Init();

        // tERR_MESSAGE( "Loading configuration." );
        tLocale::Load("languages.txt");

        st_LoadConfig();

        // migrate user configuration from previous versions
        if(sn_configurationSavedInVersion != sn_programVersion)
        {
            if(st_FirstUse)
            {
                sn_configurationSavedInVersion = "0.0";
            }

            tConfigMigration::Migrate(sn_configurationSavedInVersion);
        }
        if(tConfigMigration::SavedBefore(sn_configurationSavedInVersion, sn_programVersion))
            sn_configurationSavedInVersion = sn_programVersion;

        // record and play back the recording debug level
        tRecorderSyncBase::GetDebugLevelPlayback();

        if ( commandLineAnalyzer.fullscreen_ )
            currentScreensetting.fullscreen   = true;
        if ( commandLineAnalyzer.windowed_ )
            currentScreensetting.fullscreen   = false;
        if ( commandLineAnalyzer.use_directx_ )
            sr_useDirectX                       = true;
        if ( commandLineAnalyzer.dont_use_directx_ )
            sr_useDirectX                       = false;

        gAICharacter::LoadAll(tString( "aiplayers.cfg" ) );

        sg_LanguageInit();
        atexit(tLocale::Clear);

        static eLadderLogWriter sg_encodingWriter( "ENCODING", true );
        sg_encodingWriter << st_internalEncoding;
        sg_encodingWriter.write();

        if ( commandLine.Execute() )
        {
            gCycle::PrivateSettings();

            {
                std::ifstream t;

                if ( !tDirectories::Config().Open( t, "settings.cfg" ) )
                {
                    //		#ifdef WIN32
                    //                    tERR_ERROR( "Data files not found. You have to run Armagetron from its own directory." );
                    //		#else
                    tERR_ERROR( "Configuration files not found. Check your installation." );
                    //		#endif
                }
            }

            {
                std::ofstream s;
                if (! tDirectories::Var().Open( s, "scorelog.txt", std::ios::app ) )
                {
                    char const * error = "var directory not writable or does not exist. It should reside inside your user data directory and should have been created automatically on first start, but something must have gone wrong."
#ifdef WIN32
                                         " You can access your user data directory over one of the start menu entries we installed."
#else
                                         " Your user data directory is subdirectory named .armagetronad in your home directory."
#endif
                                         ;

                    tERR_ERROR( error );
                }
            }

            {
                std::ifstream t;

                if ( tDirectories::Data().Open( t, "moviepack/settings.cfg" ) )
                {
                    sg_moviepackInstalled=true;
                }
            }

#ifndef DEDICATED
            sr_glOut=1;
#endif

            //std::cout << "checked mp\n";

            // while DGA mouse is buggy in XFree 4.0:
#ifdef linux
            // Sam 5/23 - Don't ever use DGA, we don't need it for this game.
            if ( ! getenv("SDL_VIDEO_X11_DGAMOUSE") ) {
                sg_PutEnv( "SDL_VIDEO_X11_DGAMOUSE=0" );
            }
#endif

#ifdef WIN32
            // disable DirectX by default; it causes problems with some boards.
            if (!getenv( "SDL_VIDEODRIVER") ) {
                if (sr_useDirectX) {
                    // NO. Causes crashes with SDL1.2 on Windows on window deactivation/alt-tabbing away
                    // sg_PutEnv( "SDL_VIDEODRIVER=directx" );
                }
                else
                {
                    sg_PutEnv( "SDL_VIDEODRIVER=windib" );
                }
            }
#endif

#ifdef MACOSX
            {
                // defaults for playing well with retina displays
                if (!getenv("SDL12COMPAT_HIGHDPI"))
                    sg_PutEnv("SDL12COMPAT_HIGHDPI=1");
                if (!getenv("SDL12COMPAT_OPENGL_SCALING"))
                    sg_PutEnv("SDL12COMPAT_OPENGL_SCALING=1");
            }
#endif

            // atexit(ANET_Shutdown);

#ifndef WIN32
#ifdef DEBUG
#define NOSOUND
#endif
#endif

#ifndef DEDICATED
            if (
#ifndef NOSOUND
#ifndef DEFAULT_SDL_AUDIODRIVER
                SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0 &&
#endif
#endif
                SDL_Init(SDL_INIT_VIDEO) < 0 )            {
                tERR_ERROR("Couldn't initialize SDL: " << SDL_GetError());
            }
            SDLCleanup sdlCleanup; // call SDL_Quit later

            sr_glRendererInit();

#ifdef __EMSCRIPTEN__
            SDL_SetEventFilter(&filter, NULL);
#else
            SDL_SetEventFilter(&filter);
#endif

            //std::cout << "set filter\n";

            sg_SetIcon();

            tConsole::RegisterMessageCallback(&uMenu::Message);
            tConsole::RegisterIdleCallback(&uMenu::IdleInput);

#ifndef NOSOUND
            SDLSoundCleanup soundInitAndCleanup; // se_SoundInit() now, se_SoundExit() later
#ifndef DEBUG
#if false // this was added a long time ago to work around problems. Now it occasionally causes startup freezes because
          // SDL does not like it when you init sound, then shut it down right away, some kind of race condition.
          // We leave it around in case we get the same idea again.
          
            // double sound initialisation for dodgy cards
            se_SoundExit();
            se_SoundInit();
#endif
#endif
#endif

            if (sr_InitDisplay()){

                try
                {
                    //std::cout << "init disp\n";

                    //std::cout << "init sound\n";

                    welcome();
                    wdbg("welcome() returned, about to call MainMenu()");
#ifndef __EMSCRIPTEN__
                    //std::cout << "atexit\n";

                    sr_con.autoDisplayAtSwap=false;

                    se_SoundPause(false);

                    //std::cout << "sound started\n";

                    gLogo::SetBig(false);
                    gLogo::SetSpinning(true);

                    sn_bigBrotherString = renderer_identification + "VER=" + sn_programVersion + "\n\n";

                    MainMenu();
#else
                    // browser: welcome() never returns - the startup
                    // chain (WasmStep_*) runs the tail above and the
                    // main menu as trampoline loops; when the main menu
                    // ends the chain ends (continuation cleared) and the
                    // pump idles.  main() returns and the page stays
                    // alive (Module.noExitRuntime), so nothing may be
                    // torn down here.
#endif
#ifndef __EMSCRIPTEN__
                    // remove all players
                    for ( int i = se_PlayerNetIDs.Len()-1; i>=0; --i )
                        se_PlayerNetIDs(i)->RemoveFromGame();

                    nNetObject::ClearAll();

                    rITexture::UnloadAll();
#endif
                }
                catch (tException const & e)
                {
                    gLogo::SetDisplayed(true);
                    uMenu::SetIdle(NULL);
                    sr_con.autoDisplayAtSwap=false;

                    // inform user of generic errors
                    tConsole::Message( e.GetName(), e.GetDescription(), 20 );
                    // wasm: the dialog runs on the JS event loop; main returns
                }

#ifndef __EMSCRIPTEN__
                sr_ExitDisplay();
                sr_RendererCleanup();

                //std::cout << "exit\n";

                st_SaveConfig();

                //std::cout << "saved\n";

                //    cleanup(grid);
                SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif
            }
#else
            sr_glOut=0;

            //  nServerInfo::TellMasterAboutMe();

            while (!uMenu::quickexit)
                sg_HostGame();
#endif
#ifndef __EMSCRIPTEN__
            nNetObject::ClearAll();
            nServerInfo::DeleteAll();
#endif
        }

#ifndef __EMSCRIPTEN__
        ePlayer::Exit();
#endif


        //	tLocale::Clear();
    }
    catch ( tCleanQuit const & e )
    {
        return 0;
    }
    catch ( tException const & e )
    {
        try
        {
            st_PresentError( e.GetName(), e.GetDescription() );
        }
        catch (...)
        {
        }

        return 1;
    }
#ifdef _MSC_VER
#pragma warning ( disable : 4286 )
    // GRR. Visual C++ dones not handle generic exceptions with the above general statement.
    // A specialized version is needed. The best part: it warns about the code below being redundant.
    catch ( tGenericException const & e )
    {
        try
        {
            st_PresentError( e.GetName(), e.GetDescription() );
        }
        catch (...)
        {
        }

        return 1;
    }
#endif
    catch (...)
    {
        return 1;
    }

    return 0;
}

#ifdef DEDICATED
// settings missing in the dedicated server
static void st_Dummy(std::istream &s){
    tString rest;
    rest.ReadLine(s);
}
static tConfItemFunc st_Dummy10("MASTER_QUERY_INTERVAL", &st_Dummy);
static tConfItemFunc st_Dummy11("MASTER_SAVE_INTERVAL", &st_Dummy);
static tConfItemFunc st_Dummy12("MASTER_IDLE", &st_Dummy);
static tConfItemFunc st_Dummy13("MASTER_PORT", &st_Dummy);
#endif



