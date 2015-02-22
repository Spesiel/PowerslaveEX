//
// Copyright(C) 2014-2015 Samuel Villarreal
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//      Play loop (in-game) logic
//

#include "kexlib.h"
#include "renderMain.h"
#include "renderView.h"
#include "game.h"

//
// kexPlayLoop::kexPlayLoop
//

kexPlayLoop::kexPlayLoop(void)
{
}

//
// kexPlayLoop::~kexPlayLoop
//

kexPlayLoop::~kexPlayLoop(void)
{
}

//
// kexPlayLoop::Init
//

void kexPlayLoop::Init(void)
{
    hud.Init();
}

//
// kexPlayLoop::Start
//

void kexPlayLoop::Start(void)
{
    ticks = 0;
    
    if(kexGame::cLocal->Player()->Actor() == NULL)
    {
        kex::cSystem->Warning("No player starts present\n");
        kexGame::cLocal->SetGameState(GS_TITLE);
        return;
    }
    
    hud.SetPlayer(kexGame::cLocal->Player());
    renderScene.SetView(&renderView);
    renderScene.SetWorld(kexGame::cLocal->World());

    kexGame::cLocal->Player()->Ready();
    InitWater();
}

//
// kexPlayLoop::Stop
//

void kexPlayLoop::Stop(void)
{
    kexGame::cLocal->World()->UnloadMap();
}

//
// kexPlayLoop::Draw
//

void kexPlayLoop::Draw(void)
{
    kexPlayer *p = kexGame::cLocal->Player();
    kexWorld *world = kexGame::cLocal->World();
    
    renderView.SetupFromPlayer(p);
    
    world->FindVisibleSectors(renderView, p->Actor()->Sector());
    
    renderScene.Draw();
    
    if(world->MapLoaded())
    {
        kexCpuVertList *vl = kexRender::cVertList;

        {
            static spriteAnim_t *anim = p->Weapon().Anim();
            const kexGameLocal::weaponInfo_t *weaponInfo = kexGame::cLocal->WeaponInfo(p->CurrentWeapon());
            
            kexRender::cScreen->SetOrtho();
            kexRender::cBackend->SetState(GLSTATE_DEPTHTEST, false);
            kexRender::cBackend->SetState(GLSTATE_SCISSOR, true);
            kexRender::cBackend->SetBlend(GLSRC_SRC_ALPHA, GLDST_ONE_MINUS_SRC_ALPHA);

            if(anim)
            {
                spriteFrame_t *frame = p->Weapon().Frame();
                spriteSet_t *spriteSet;
                kexSprite *sprite;
                spriteInfo_t *info;

                vl->BindDrawPointers();

                for(unsigned int i = 0; i < frame->spriteSet[0].Length(); ++i)
                {
                    spriteSet = &frame->spriteSet[0][i];
                    sprite = spriteSet->sprite;
                    info = &sprite->InfoList()[spriteSet->index];

                    float x = (float)spriteSet->x;
                    float y = (float)spriteSet->y;
                    float w = (float)info->atlas.w;
                    float h = (float)info->atlas.h;
                    word c = 0xff;

                    float u1, u2, v1, v2;
                    
                    u1 = info->u[0 ^ spriteSet->bFlipped];
                    u2 = info->u[1 ^ spriteSet->bFlipped];
                    v1 = info->v[0];
                    v2 = info->v[1];

                    kexRender::cScreen->SetAspectDimentions(x, y, w, h);

                    sprite->Texture()->Bind();

                    x += p->Weapon().BobX() + weaponInfo->offsetX;
                    y += p->Weapon().BobY() + weaponInfo->offsetY;

                    if(!(frame->flags & SFF_FULLBRIGHT))
                    {
                        c = (p->Actor()->Sector()->lightLevel << 1);

                        if(c > 255)
                        {
                            c = 255;
                        }
                    }

                    vl->AddQuad(x, y + 8, 0, w, h, u1, v1, u2, v2, (byte)c, (byte)c, (byte)c, 255);
                    vl->DrawElements();
                }
            }
        }
        
        kexRender::cBackend->SetState(GLSTATE_SCISSOR, false);
        hud.Display();
    }
}

//
// kexPlayLoop::Tick
//

void kexPlayLoop::Tick(void)
{
    if(ticks > 4)
    {
        kexGame::cLocal->UpdateGameObjects();
        kexGame::cLocal->Player()->Tick();
        UpdateWater();
    }
    
    ticks++;
}

//
// kexPlayLoop::ProcessInput
//

bool kexPlayLoop::ProcessInput(inputEvent_t *ev)
{
    return false;
}

//
// kexPlayLoop::InitWater
//

void kexPlayLoop::InitWater(void)
{
    waterMaxMagnitude = 0;
    
    for(int i = 0; i < 16; i++)
    {
        for(int j = 0; j < 16; j++)
        {
            int r = (kexRand::Max(255) - 128) << 12;

            waterAccelPoints[i][j] = 0;
            waterVelocityPoints[i][j] = r;
            
            if(waterMaxMagnitude < kexMath::Abs(r))
            {
                waterMaxMagnitude = kexMath::Abs(r);
            }
        }
    }
}

//
// kexPlayLoop::UpdateWater
//

void kexPlayLoop::UpdateWater(void)
{
    for(int i = 0; i < 16; i++)
    {
        for(int j = 0; j < 16; j++)
        {
            int v1 = waterVelocityPoints[(i + 1) & 15][j];
            
            v1 += waterVelocityPoints[(i - 1) & 15][j];
            v1 += waterVelocityPoints[i][(j + 1) & 15];
            v1 += waterVelocityPoints[i][(j - 1) & 15];
            v1 -= (waterVelocityPoints[i][j] << 2);
            v1 +=  waterAccelPoints[i][j];
            
            waterAccelPoints[i][j] = v1;
        }
    }
    
    for(int i = 0; i < 16; i++)
    {
        for(int j = 0; j < 16; j++)
        {
            waterVelocityPoints[i][j] += (waterAccelPoints[i][j] >> 9);
        }
    }
}

//
// kexPlayLoop::GetWaterVelocityPoint
//

const int kexPlayLoop::GetWaterVelocityPoint(const float x, const float y)
{
    int *vel = (int*)waterVelocityPoints;
    int ix = (int)x;
    int iy = (int)y;
    int index = ((((ix + iy) >> 4) & 60) + (ix & 960)) >> 2;

    return vel[index & 0xff];
}
