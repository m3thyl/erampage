//-------------------------------------------------------------------------
/*
Copyright (C) 1996, 2003 - 3D Realms Entertainment
Copyright (C) 2000, 2003 - Matt Saettler (EDuke Enhancements)
Copyright (C) 2004, 2007 - EDuke32 developers
Copyright (C) 2009 - dimag0g

This file is part of eRampage port for Redneck Rampage series, derived from EDuke32

eRampage is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License version 2
as published by the Free Software Foundation.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/
//-------------------------------------------------------------------------

//#include <conio.h>
#include <stdio.h>
#include <string.h>

#include "fx_man.h"
#include "music.h"
#include "duke3d.h"
#include "util_lib.h"
#include "osd.h"

#ifdef _WIN32
#ifdef USE_OPENAL
#include "openal.h"
#endif
#endif

#define LOUDESTVOLUME 150

int32_t backflag,g_numEnvSoundsPlaying;

/*
===================
=
= SoundStartup
=
===================
*/

void S_SoundStartup(void) {
    int32_t status, err = 0;

    // if they chose None lets return
    if (ud.config.FXDevice < 0) return;

RETRY:
    status = FX_Init(ud.config.FXDevice, ud.config.NumVoices, ud.config.NumChannels, ud.config.NumBits, ud.config.MixRate);
    if (status == FX_Ok) {
        FX_SetVolume(ud.config.FXVolume);
        if (ud.config.ReverseStereo == 1) {
            FX_SetReverseStereo(!FX_GetReverseStereo());
        }
        status = FX_SetCallBack(S_TestSoundCallback);
    }

    if (status != FX_Ok) {
        if (!err) {
#if defined(_WIN32)
            ud.config.MixRate = 44100;
#else
            ud.config.MixRate = 48000;
#endif
            ud.config.NumBits = 16;
            ud.config.NumChannels = 2;
            ud.config.NumVoices = 32;
            ud.config.ReverseStereo = 0;
            err = 1;
            goto RETRY;
        }
        Bsprintf(tempbuf, "Sound startup error: %s", FX_ErrorString(FX_Error));
        G_GameExit(tempbuf);
    }
}

/*
===================
=
= SoundShutdown
=
===================
*/

void S_SoundShutdown(void) {
    int32_t status;

    // if they chose None lets return
    if (ud.config.FXDevice < 0)
        return;

    status = FX_Shutdown();
    if (status != FX_Ok) {
        Bsprintf(tempbuf, "Sound shutdown error: %s", FX_ErrorString(FX_Error));
        G_GameExit(tempbuf);
    }
}

/*
===================
=
= MusicStartup
=
===================
*/

void S_MusicStartup(void) {
    int32_t status;

    // if they chose None lets return
    if (ud.config.MusicDevice < 0)
        return;

    status = MUSIC_Init(ud.config.MusicDevice, 0);

    if (status == MUSIC_Ok) {
        MUSIC_SetVolume(ud.config.MusicVolume);
    } else {
        ud.config.MusicDevice = 0;

        status = MUSIC_Init(ud.config.MusicDevice, 0);

        if (status == MUSIC_Ok) {
            MUSIC_SetVolume(ud.config.MusicVolume);
        }
        /*
                initprintf("Couldn't find selected sound card, or, error w/ sound card itself.\n");

                S_SoundShutdown();
                uninittimer();
                uninitengine();
                CONTROL_Shutdown();
                CONFIG_WriteSetup();
                KB_Shutdown();
                uninitgroupfile();
                //unlink("duke3d.tmp");
                exit(-1);
        */
    }
}

/*
===================
=
= MusicShutdown
=
===================
*/

void S_MusicShutdown(void) {
    int32_t status;

    // if they chose None lets return
    if (ud.config.MusicDevice < 0)
        return;

    status = MUSIC_Shutdown();
    if (status != MUSIC_Ok) {
        Error(MUSIC_ErrorString(MUSIC_ErrorCode));
    }
}

void S_MenuSound(void) {
    static int32_t SoundNum=0;
    static int16_t menusnds[] = {
        LASERTRIP_EXPLODE,
        DUKE_GRUNT,
        DUKE_LAND_HURT,
        CHAINGUN_FIRE,
        SQUISHED,
        KICK_HIT,
        PISTOL_RICOCHET,
        PISTOL_BODYHIT,
        PISTOL_FIRE,
        SHOTGUN_FIRE,
        BOS1_WALK,
        RPG_EXPLODE,
        PIPEBOMB_BOUNCE,
        PIPEBOMB_EXPLODE,
        NITEVISION_ONOFF,
        RPG_SHOOT,
        SELECT_WEAPON
    };
    S_PlaySound(menusnds[SoundNum++]);
    SoundNum %= (sizeof(menusnds)/sizeof(menusnds[0]));
}

void _playmusic(const char *fn) {
    int32_t        fp, l, i;

    if (fn == NULL) return;

    if (ud.config.MusicToggle == 0) return;
    if (ud.config.MusicDevice < 0) return;

    fp = kopen4loadfrommod((char *)fn,0);

    if (fp == -1) {
        OSD_Printf(OSD_ERROR "S_PlayMusic(): error: can't open '%s' for playback!",fn);
        return;
    }

    l = kfilelength(fp);
    MUSIC_StopSong();

    MusicPtr = Brealloc(MusicPtr, l);
    if ((i = kread(fp, (char *)MusicPtr, l)) != l) {
        OSD_Printf(OSD_ERROR "S_PlayMusic(): error: read %d bytes from '%s', needed %d\n",i, fn, l);
        kclose(fp);
        return;
    }

    kclose(fp);

    g_musicSize=l;

    // FIXME: I need this to get the music volume initialized (not sure why) -- Jim Bentler
    MUSIC_SetVolume(ud.config.MusicVolume);
    MUSIC_PlaySong((char *)MusicPtr, MUSIC_LoopSong);
}

int32_t S_PlayMusic(const char *fn, const int32_t sel) {
    g_musicSize=0;
    if (MapInfo[sel].musicfn1 != NULL)
        _playmusic(MapInfo[sel].musicfn1);
    if (!g_musicSize) {
        _playmusic(fn);
        return 0;
    }
    return 1;
}

int32_t S_LoadSound(uint32_t num) {
    int32_t   fp = -1, l;

    if (num >= MAXSOUNDS || ud.config.SoundToggle == 0) return 0;
    if (ud.config.FXDevice < 0) return 0;

    if (g_sounds[num].filename == NULL) {
        OSD_Printf(OSD_ERROR "Sound (#%d) not defined!\n",num);
        return 0;
    }

    if (g_sounds[num].filename1) fp = kopen4loadfrommod(g_sounds[num].filename1,g_loadFromGroupOnly);
    if (fp == -1)fp = kopen4loadfrommod(g_sounds[num].filename,g_loadFromGroupOnly);
    if (fp == -1) {
//        Bsprintf(ScriptQuotes[113],"g_sounds %s(#%d) not found.",sounds[num],num);
//        P_DoQuote(113,g_player[myconnectindex].ps);
        OSD_Printf(OSDTEXT_RED "Sound %s(#%d) not found!\n",g_sounds[num].filename,num);
        return 0;
    }

    l = kfilelength(fp);
    g_sounds[num].soundsiz = l;

    g_sounds[num].lock = 200;

    allocache((intptr_t *)&g_sounds[num].ptr,l,(char *)&g_sounds[num].lock);
    kread(fp, g_sounds[num].ptr , l);
    kclose(fp);
    return 1;
}

int32_t S_PlaySoundXYZ(int32_t num, int32_t i, const vec3_t *pos) {
    int32_t sndist, cx, cy, cz, j,k;
    int32_t pitche,pitchs,cs;
    int32_t voice, sndang, ca, pitch;

    //    if(num != 358) return 0;

    if (num >= MAXSOUNDS ||
            ud.config.FXDevice < 0 ||
            ((g_sounds[num].m&8) && ud.lockout) ||
            ud.config.SoundToggle == 0 ||
            g_sounds[num].num > 3 ||
            FX_VoiceAvailable(g_sounds[num].pr) == 0 ||
            (g_player[myconnectindex].ps->timebeforeexit > 0 && g_player[myconnectindex].ps->timebeforeexit <= GAMETICSPERSEC*3) ||
            g_player[myconnectindex].ps->gm&MODE_MENU) return -1;

    if (g_sounds[num].m&128) {
        S_PlaySound(num);
        return 0;
    }

    if (g_sounds[num].m&4) {
        if (ud.multimode > 1 && PN == APLAYER && sprite[i].yvel != screenpeek) { // other player sound
            if (!(ud.config.VoiceToggle&4))
                return -1;
        } else if (!(ud.config.VoiceToggle&1))
            return -1;
        for (j=0; j<MAXSOUNDS; j++)
            for (k=0; k<g_sounds[j].num; k++)
                if ((g_sounds[j].num > 0) && (g_sounds[j].m&4))
                    return -1;
    }

    cx = g_player[screenpeek].ps->oposx;
    cy = g_player[screenpeek].ps->oposy;
    cz = g_player[screenpeek].ps->oposz;
    cs = g_player[screenpeek].ps->cursectnum;
    ca = g_player[screenpeek].ps->ang+g_player[screenpeek].ps->look_ang;

    sndist = FindDistance3D((cx-pos->x),(cy-pos->y),(cz-pos->z)>>4);

    if (i >= 0 && (g_sounds[num].m&16) == 0 && PN == MUSICANDSFX && SLT < 999 && (sector[SECT].lotag&0xff) < 9)
        sndist = divscale14(sndist,(SHT+1));

    pitchs = g_sounds[num].ps;
    pitche = g_sounds[num].pe;
    cx = klabs(pitche-pitchs);

    if (cx) {
        if (pitchs < pitche)
            pitch = pitchs + (rand()%cx);
        else pitch = pitche + (rand()%cx);
    } else pitch = pitchs;

    sndist += g_sounds[num].vo;
    if (sndist < 0) sndist = 0;
    if (cs > -1 && sndist && PN != MUSICANDSFX && !cansee(cx,cy,cz-(24<<8),cs,SX,SY,SZ-(24<<8),SECT))
        sndist += sndist>>5;

    switch (num) {
    case PIPEBOMB_EXPLODE:
    case LASERTRIP_EXPLODE:
    case RPG_EXPLODE:
        if (sndist > (6144))
            sndist = 6144;
        if (g_player[screenpeek].ps->cursectnum > -1 && sector[g_player[screenpeek].ps->cursectnum].lotag == 2)
            pitch -= 1024;
        break;
    default:
        if (g_player[screenpeek].ps->cursectnum > -1 && sector[g_player[screenpeek].ps->cursectnum].lotag == 2 && (g_sounds[num].m&4) == 0)
            pitch = -768;
        if (sndist > 31444 && PN != MUSICANDSFX)
            return -1;
        break;
    }

    if (g_player[screenpeek].ps->sound_pitch) pitch += g_player[screenpeek].ps->sound_pitch;

    if (g_sounds[num].num > 0 && PN != MUSICANDSFX) {
        if (g_sounds[num].SoundOwner[0].i == i) S_StopSound(num);
        else if (g_sounds[num].num > 1) S_StopSound(num);
        else if (A_CheckEnemySprite(&sprite[i]) && sprite[i].extra <= 0) S_StopSound(num);
    }

    if (PN == APLAYER && sprite[i].yvel == screenpeek) {
        sndang = 0;
        sndist = 0;
    } else {
        sndang = 2048 + ca - getangle(cx-pos->x,cy-pos->y);
        sndang &= 2047;
    }

    if (g_sounds[num].ptr == 0) {
        if (S_LoadSound(num) == 0) return 0;
    } else {
        if (g_sounds[num].lock < 200)
            g_sounds[num].lock = 200;
        else g_sounds[num].lock++;
    }

    if (g_sounds[num].m&16) sndist = 0;

    if (sndist < ((255-LOUDESTVOLUME)<<6))
        sndist = ((255-LOUDESTVOLUME)<<6);

    if (g_sounds[num].m&1) {
        uint16_t start;

        if (g_sounds[num].num > 0) return -1;

        start = *(uint16_t *)(g_sounds[num].ptr + 0x14);

        if (*g_sounds[num].ptr == 'C')
            voice = FX_PlayLoopedVOC(g_sounds[num].ptr, start, start + g_sounds[num].soundsiz,
                                     pitch,sndist>>6,sndist>>6,0,g_sounds[num].pr,num);
        else if (*g_sounds[num].ptr == 'O')
            voice = FX_PlayLoopedOGG(g_sounds[num].ptr, start, start + g_sounds[num].soundsiz,
                                     pitch,sndist>>6,sndist>>6,0,g_sounds[num].pr,num);
        else
            voice = FX_PlayLoopedWAV(g_sounds[num].ptr, start, start + g_sounds[num].soundsiz,
                                     pitch,sndist>>6,sndist>>6,0,g_sounds[num].pr,num);
    } else {
        if (*g_sounds[num].ptr == 'C')
            voice = FX_PlayVOC3D(g_sounds[ num ].ptr,pitch,sndang>>6,sndist>>6, g_sounds[num].pr, num);
        else if (*g_sounds[num].ptr == 'O')
            voice = FX_PlayOGG3D(g_sounds[ num ].ptr,pitch,sndang>>6,sndist>>6, g_sounds[num].pr, num);
        else
            voice = FX_PlayWAV3D(g_sounds[ num ].ptr,pitch,sndang>>6,sndist>>6, g_sounds[num].pr, num);
    }

    if (voice > FX_Ok) {
        g_sounds[num].SoundOwner[g_sounds[num].num].i = i;
        g_sounds[num].SoundOwner[g_sounds[num].num].voice = voice;
        g_sounds[num].num++;
    } else g_sounds[num].lock--;
    return (voice);
}

void S_PlaySound(int32_t num) {
    int32_t pitch,pitche,pitchs,cx;
    int32_t voice;
    int32_t start;

    if (ud.config.FXDevice < 0) return;
    if (ud.config.SoundToggle==0) return;
    if (!(ud.config.VoiceToggle&1) && (g_sounds[num].m&4)) return;
    if ((g_sounds[num].m&8) && ud.lockout) return;
    if (FX_VoiceAvailable(g_sounds[num].pr) == 0) return;
    if (num > MAXSOUNDS-1 || !g_sounds[num].filename) {
        OSD_Printf("WARNING: invalid sound #%d\n",num);
        return;
    }

    pitchs = g_sounds[num].ps;
    pitche = g_sounds[num].pe;
    cx = klabs(pitche-pitchs);

    if (cx) {
        if (pitchs < pitche)
            pitch = pitchs + (rand()%cx);
        else pitch = pitche + (rand()%cx);
    } else pitch = pitchs;

    if (g_sounds[num].ptr == 0) {
        if (S_LoadSound(num) == 0) return;
    } else {
        if (g_sounds[num].lock < 200)
            g_sounds[num].lock = 200;
        else g_sounds[num].lock++;
    }

    if (g_sounds[num].m&1) {
        if (*g_sounds[num].ptr == 'C') {
            start = (int32_t)*(uint16_t *)(g_sounds[num].ptr + 0x14);
            voice = FX_PlayLoopedVOC(g_sounds[num].ptr, start, start + g_sounds[num].soundsiz,
                                     pitch,LOUDESTVOLUME,LOUDESTVOLUME,LOUDESTVOLUME,g_sounds[num].pr,num);
        } else if (*g_sounds[num].ptr == 'O') {
            start = (int32_t)*(uint16_t *)(g_sounds[num].ptr + 0x14);
            voice = FX_PlayLoopedOGG(g_sounds[num].ptr, start, start + g_sounds[num].soundsiz,
                                     pitch,LOUDESTVOLUME,LOUDESTVOLUME,LOUDESTVOLUME,g_sounds[num].pr,num);
        } else {
            start = (int32_t)*(uint16_t *)(g_sounds[num].ptr + 0x14);
            voice = FX_PlayLoopedWAV(g_sounds[num].ptr, start, start + g_sounds[num].soundsiz,
                                     pitch,LOUDESTVOLUME,LOUDESTVOLUME,LOUDESTVOLUME,g_sounds[num].pr,num);
        }
    } else {
        if (*g_sounds[num].ptr == 'C')
            voice = FX_PlayVOC3D(g_sounds[ num ].ptr, pitch,0,255-LOUDESTVOLUME,g_sounds[num].pr, num);
        else if (*g_sounds[num].ptr == 'O')
            voice = FX_PlayOGG3D(g_sounds[ num ].ptr, pitch,0,255-LOUDESTVOLUME,g_sounds[num].pr, num);
        else
            voice = FX_PlayWAV3D(g_sounds[ num ].ptr, pitch,0,255-LOUDESTVOLUME,g_sounds[num].pr, num);
    }

    if (voice > FX_Ok) return;
    g_sounds[num].lock--;
}

int32_t A_PlaySound(uint32_t num, int32_t i) {
    if (num >= MAXSOUNDS) return -1;
    if (i < 0) {
        S_PlaySound(num);
        return 0;
    }
    {
        /*
                        vec3_t davector;
                        Bmemcpy(&davector,&sprite[i],sizeof(intptr_t) * 3); */
//        OSD_Printf("x: %d y: %d z: %d\n",davector.x,davector.y,davector.z);
        return S_PlaySoundXYZ(num,i, (vec3_t *)&sprite[i]);
    }
}

void A_StopSound(int32_t num, int32_t i) {
    UNREFERENCED_PARAMETER(i);
    if (num >= 0 && num < MAXSOUNDS) S_StopSound(num);
}

void S_StopSound(int32_t num) {
    if (num >= 0 && num < MAXSOUNDS)
        if (g_sounds[num].num > 0) {
            FX_StopSound(g_sounds[num].SoundOwner[g_sounds[num].num-1].voice);
            S_TestSoundCallback(num);
        }
}

void S_StopEnvSound(int32_t num,int32_t i) {
    int32_t j, k;

    if (num >= 0 && num < MAXSOUNDS)
        if (g_sounds[num].num > 0) {
            k = g_sounds[num].num;
            for (j=0; j<k; j++)
                if (g_sounds[num].SoundOwner[j].i == i) {
                    FX_StopSound(g_sounds[num].SoundOwner[j].voice);
                    break;
                }
        }
}

void S_Pan3D(void) {
    int32_t sndist, sx, sy, sz, cx, cy, cz;
    int32_t sndang,ca,j,k,i,cs;

    g_numEnvSoundsPlaying = 0;

    if (ud.camerasprite == -1) {
        cx = g_player[screenpeek].ps->oposx;
        cy = g_player[screenpeek].ps->oposy;
        cz = g_player[screenpeek].ps->oposz;
        cs = g_player[screenpeek].ps->cursectnum;
        ca = g_player[screenpeek].ps->ang+g_player[screenpeek].ps->look_ang;
    } else {
        cx = sprite[ud.camerasprite].x;
        cy = sprite[ud.camerasprite].y;
        cz = sprite[ud.camerasprite].z;
        cs = sprite[ud.camerasprite].sectnum;
        ca = sprite[ud.camerasprite].ang;
    }

    for (j=0; j<MAXSOUNDS; j++) for (k=0; k<g_sounds[j].num; k++) {
            i = g_sounds[j].SoundOwner[k].i;

            sx = sprite[i].x;
            sy = sprite[i].y;
            sz = sprite[i].z;

            if (PN == APLAYER && sprite[i].yvel == screenpeek) {
                sndang = 0;
                sndist = 0;
            } else {
                sndang = 2048 + ca - getangle(cx-sx,cy-sy);
                sndang &= 2047;
                sndist = FindDistance3D((cx-sx),(cy-sy),(cz-sz)>>4);
                if (i >= 0 && (g_sounds[j].m&16) == 0 && PN == MUSICANDSFX && SLT < 999 && (sector[SECT].lotag&0xff) < 9)
                    sndist = divscale14(sndist,(SHT+1));
            }

            sndist += g_sounds[j].vo;
            if (sndist < 0) sndist = 0;

            if (cs > -1 && sndist && PN != MUSICANDSFX && !cansee(cx,cy,cz-(24<<8),cs,sx,sy,sz-(24<<8),SECT))
                sndist += sndist>>5;

            if (PN == MUSICANDSFX && SLT < 999)
                g_numEnvSoundsPlaying++;

            switch (j) {
            case PIPEBOMB_EXPLODE:
            case LASERTRIP_EXPLODE:
            case RPG_EXPLODE:
                if (sndist > (6144)) sndist = (6144);
                break;
            default:
                if (sndist > 31444 && PN != MUSICANDSFX) {
                    S_StopSound(j);
                    continue;
                }
            }

            if (g_sounds[j].ptr == 0 && S_LoadSound(j) == 0) continue;
            if (g_sounds[j].m&16) sndist = 0;

            if (sndist < ((255-LOUDESTVOLUME)<<6))
                sndist = ((255-LOUDESTVOLUME)<<6);

            FX_Pan3D(g_sounds[j].SoundOwner[k].voice,sndang>>6,sndist>>6);
        }
}

void S_TestSoundCallback(uint32_t num) {
    int32_t tempi,tempj,tempk;

    if ((int32_t)num < 0) {
        if (lumplockbyte[num] >= 200)
            lumplockbyte[num]--;
        return;
    }

    tempk = g_sounds[num].num;

    if (tempk > 0) {
        if ((g_sounds[num].m&16) == 0)
            for (tempj=0; tempj<tempk; tempj++) {
                tempi = g_sounds[num].SoundOwner[tempj].i;
                if (sprite[tempi].picnum == MUSICANDSFX && sector[sprite[tempi].sectnum].lotag < 3 && sprite[tempi].lotag < 999) {
                    ActorExtra[tempi].temp_data[0] = 0;
                    if ((tempj + 1) < tempk) {
                        g_sounds[num].SoundOwner[tempj].voice = g_sounds[num].SoundOwner[tempk-1].voice;
                        g_sounds[num].SoundOwner[tempj].i     = g_sounds[num].SoundOwner[tempk-1].i;
                    }
                    break;
                }
            }

        g_sounds[num].num--;
        g_sounds[num].SoundOwner[tempk-1].i = -1;
    }

    g_sounds[num].lock--;
}

void S_ClearSoundLocks(void) {
    int32_t i;

    for (i=0; i<MAXSOUNDS; i++)
        if (g_sounds[i].lock >= 200)
            g_sounds[i].lock = 199;

    for (i=0; i<11; i++)
        if (lumplockbyte[i] >= 200)
            lumplockbyte[i] = 199;
}

int32_t A_CheckSoundPlaying(int32_t i, int32_t num) {
    UNREFERENCED_PARAMETER(i);
    if (num < 0) num=0;	// FIXME
    return (g_sounds[num].num > 0);
}

int32_t S_CheckSoundPlaying(int32_t i, int32_t num) {
    if (i == -1) {
        if (g_sounds[num].lock == 200)
            return 1;
        return 0;
    }
    return(g_sounds[num].num);
}
