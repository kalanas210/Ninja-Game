// ============================================================================
//  SHADOW NINJA  -  a 2D silhouette ninja action-platformer
// ----------------------------------------------------------------------------
//  A complete game written from scratch in C++17 with OpenGL (immediate mode)
//  and freeglut.  Every visual is generated procedurally in code - there are no
//  image, texture, audio or font asset files.  The whole game ships as one
//  self-contained .exe.
//
//  Features: 5 themed worlds, a procedurally skeletal-animated ninja, shuriken
//  + katana + a chakra Ultimate, a kill-streak combo system, 6 enemy types and
//  a 3-phase boss, breakable walls, hazards, and full game "juice" (hitstop,
//  slow-motion, screen shake, particles, victory cinematics).
//
//  CONTROLS
//    A / D  or  Left / Right .... move
//    W / Space / Up ............. jump (press again in the air = double jump)
//    L .......................... dash
//    J  (or left mouse) ......... throw shuriken
//    K .......................... katana slash
//    U .......................... ULTIMATE (when the chakra bar is full)
//    S .......................... shrink (fit through low tunnels)
//    Down + W ................... drop through a one-way platform
//    R .......................... restart from the last checkpoint
//    Enter ...................... start / confirm / next
//
//  BUILD (MinGW-w64 / msys2):
//    g++ ninja.cpp -o ninja.exe -O2 -std=c++17 -DFREEGLUT_STATIC \
//      -DGLUT_DISABLE_ATEXIT_HACK -static -static-libgcc -static-libstdc++ \
//      -lfreeglut -lopengl32 -lglu32 -lwinmm -lgdi32 -luser32 -lkernel32 -lole32
//
//  Source layout: this single file is organised top-to-bottom into clearly
//  labelled sections - constants, math/GL helpers, themes, background, the
//  ninja skeleton & animation, platforms/collision, world objects, the player,
//  enemies, projectiles/hazards, levels, the update loop & state machine, and
//  rendering/HUD.
//
//  License: MIT (see LICENSE).
// ============================================================================
#include <GL/freeglut.h>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <random>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef GL_MULTISAMPLE
#define GL_MULTISAMPLE 0x809D
#endif

// ---------------------------------------------------------------------------
//  Tunable constants  (physics, combat, sizing)
// ---------------------------------------------------------------------------
static const int   WINW=1280, WINH=720;
static const float TILE=40.f, DT=1.f/60.f;
static const float GRAVITY=2600, MAXFALL=1500, GROUND_ACCEL=6000, AIR_ACCEL=3600, MOVE_MAX=360;
static const float GROUND_FRICTION=4600, AIR_FRICTION=500;
static const float JUMP_VEL=860, DJUMP_VEL=760; static const int MAX_JUMPS=2;
static const float COYOTE=0.10f, JUMP_BUFFER=0.12f, CUT_JUMP=0.45f;
static const float WALL_SLIDE=150, WALL_JUMP_VX=470, WALL_JUMP_VY=800, WALL_LOCK=0.12f;
static const float DASH_SPEED=1200, DASH_TIME=0.15f, DASH_CD=0.40f; static const int MAX_DASH=1;
static const float NW=30, NH=54, SW=24, SH=26, DEATH_Y=-120;
static const float CRUMBLE_DELAY=0.40f, CRUMBLE_RESPAWN=2.2f;
static const float PLAYER_MAXHP=100;
static const float SHURIKEN_DMG=12, SHURIKEN_SPEED=900, SHURIKEN_CD=0.30f, SHURIKEN_LIFE=1.6f;
static const float SLASH_DMG=18, SLASH_REACH=62, SLASH_HALFH=34, SLASH_TIME=0.18f, SLASH_CD=0.30f;
static const float IFRAMES=0.55f;

// ---------------------------------------------------------------------------
//  Math / util
// ---------------------------------------------------------------------------
static std::mt19937 rng(20240611u);
static float frand(float a,float b){ return a+(b-a)*(float)(rng()/(double)rng.max()); }
static float clampf(float v,float lo,float hi){ return v<lo?lo:(v>hi?hi:v); }
static float lerpf(float a,float b,float t){ return a+(b-a)*t; }
static float smooth(float t){ return t*t*(3-2*t); }
static float deg2rad(float d){ return d*(float)M_PI/180.f; }
static float sgn(float v){ return v<0?-1.f:(v>0?1.f:0.f); }
static float hash1(float n){ float s=sinf(n)*43758.5453f; return s-floorf(s); }
struct Rect{ float x,y,w,h; };
static bool overlap(const Rect&a,const Rect&b){ return a.x<b.x+b.w&&a.x+a.w>b.x&&a.y<b.y+b.h&&a.y+a.h>b.y; }

// ---------------------------------------------------------------------------
//  GL helpers
// ---------------------------------------------------------------------------
static void setProj(float ox,float oy){
    glMatrixMode(GL_PROJECTION); glLoadIdentity();
    glOrtho(ox,ox+WINW,oy,oy+WINH,-1,1);
    glMatrixMode(GL_MODELVIEW); glLoadIdentity();
}
static void fillRect(float x,float y,float w,float h){ glBegin(GL_QUADS); glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h); glEnd(); }
static void disc(float cx,float cy,float r,int seg){ glBegin(GL_TRIANGLE_FAN); glVertex2f(cx,cy); for(int i=0;i<=seg;i++){float a=2*M_PI*i/seg; glVertex2f(cx+cosf(a)*r,cy+sinf(a)*r);} glEnd(); }
static void ring(float cx,float cy,float r,int seg){ glBegin(GL_LINE_LOOP); for(int i=0;i<seg;i++){float a=2*M_PI*i/seg; glVertex2f(cx+cosf(a)*r,cy+sinf(a)*r);} glEnd(); }
static void glowDisc(float cx,float cy,float r,float cr,float cg,float cb,float a){
    glBlendFunc(GL_SRC_ALPHA,GL_ONE);
    glBegin(GL_TRIANGLE_FAN); glColor4f(cr,cg,cb,a); glVertex2f(cx,cy); glColor4f(cr,cg,cb,0);
    int seg=36; for(int i=0;i<=seg;i++){float ang=2*M_PI*i/seg; glVertex2f(cx+cosf(ang)*r,cy+sinf(ang)*r);} glEnd();
    glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
}
static void bone(float x0,float y0,float x1,float y1,float w0,float w1){
    float dx=x1-x0,dy=y1-y0,len=sqrtf(dx*dx+dy*dy); if(len<0.001f)return;
    float nx=-dy/len,ny=dx/len;
    glBegin(GL_QUADS); glVertex2f(x0+nx*w0,y0+ny*w0);glVertex2f(x0-nx*w0,y0-ny*w0);glVertex2f(x1-nx*w1,y1-ny*w1);glVertex2f(x1+nx*w1,y1+ny*w1); glEnd();
    disc(x0,y0,w0,9); disc(x1,y1,w1,9);
}
static void text(float x,float y,void*f,const char*s){ glRasterPos2f(x,y); for(;*s;s++) glutBitmapCharacter(f,*s); }
static float textW(void*f,const char*s){ return (float)glutBitmapLength(f,(const unsigned char*)s); }
static void textC(float cx,float y,void*f,const char*s){ text(cx-textW(f,s)*0.5f,y,f,s); }

// ---------------------------------------------------------------------------
//  Themes
// ---------------------------------------------------------------------------
enum ThemeId{ TH_BAMBOO,TH_SUNSET,TH_MOONLIT,TH_TWILIGHT,TH_VOLCANIC,TH_COUNT };
struct Theme{ const char*name;
 float skyTop[3],skyMid[3],skyBot[3]; bool band3; float fog[3]; float fogA; int fogBands;
 int discKind; float discX,discY,discR,discGlow; float discCol[3];
 float ridge[3]; float accent[3]; int ambient; float ambientRate; int midShape; };
static const Theme THEMES[TH_COUNT]={
 {"Whispering Bamboo",{.45f,.68f,.52f},{0,0,0},{.83f,.92f,.74f},false,{.78f,.89f,.78f},.22f,4,0,922,520,64,.55f,{.97f,.99f,.86f},{.34f,.52f,.43f},{1,.95f,.45f},0,1.6f,1},
 {"Embers of the Pass",{.45f,.13f,.34f},{.85f,.42f,.40f},{1,.62f,.20f},true,{.95f,.55f,.30f},.30f,5,0,640,418,96,.90f,{1,.83f,.42f},{.62f,.27f,.32f},{1,.55f,.15f},1,3.2f,3},
 {"Pale Moon Ruins",{.04f,.06f,.16f},{0,0,0},{.16f,.27f,.45f},false,{.40f,.52f,.72f},.20f,4,1,358,547,88,.75f,{.86f,.92f,1},{.12f,.18f,.34f},{.6f,.85f,1},2,1.1f,4},
 {"Veil of Twilight",{.13f,.06f,.27f},{.42f,.18f,.55f},{.62f,.32f,.60f},true,{.55f,.36f,.65f},.26f,5,1,845,504,74,.68f,{.95f,.78f,.95f},{.32f,.18f,.42f},{.85f,.50f,1},3,1.8f,5},
 {"The Crimson Shogun",{.18f,.02f,.04f},{.70f,.10f,.05f},{.85f,.18f,.08f},true,{.70f,.20f,.10f},.34f,6,0,640,475,110,1.0f,{1,.42f,.18f},{.50f,.12f,.10f},{1,.35f,.10f},4,5.0f,3},
};

// ---------------------------------------------------------------------------
//  Particles + floaters
// ---------------------------------------------------------------------------
struct Particle{ float x,y,vx,vy,life,max,size,r,g,b,a; bool additive; };
static std::vector<Particle> parts;
static void spawnP(float x,float y,float vx,float vy,float life,float size,float r,float g,float b,float a,bool add){
    Particle p{x,y,vx,vy,life,life,size,r,g,b,a,add}; parts.push_back(p);
}
static void burst(float x,float y,int n,float r,float g,float b,float spd,float life,bool add){
    for(int i=0;i<n;i++){ float a=frand(0,6.2832f),s=frand(spd*0.3f,spd); spawnP(x,y,cosf(a)*s,sinf(a)*s,frand(life*0.5f,life),frand(2,5),r,g,b,1,add);} }
static void dust(float x,float y,int n){ for(int i=0;i<n;i++) spawnP(x+frand(-8,8),y,frand(-60,60),frand(30,140),frand(.2f,.45f),frand(2,4),.8f,.85f,.95f,.9f,false); }
struct Floater{ float x,y,vy,life; char t[12]; float r,g,b; };
static std::vector<Floater> floaters;
static void floater(float x,float y,const char*s,float r,float g,float b){ Floater f; f.x=x;f.y=y;f.vy=70;f.life=0.8f; strncpy(f.t,s,11); f.t[11]=0; f.r=r;f.g=g;f.b=b; floaters.push_back(f); }

// ---------------------------------------------------------------------------
//  Globals / camera / state
// ---------------------------------------------------------------------------
enum GameState{ ST_MENU,ST_INTRO,ST_PLAY,ST_CLEAR,ST_OVER,ST_COMPLETE };
static int gameState=ST_MENU;
static int curLevel=0, curTheme=0;
static float camX=0,camY=0,shake=0,gTime=0,timeScale=1.f,hitstop=0,flashWhite=0;
static float worldWidth=4000;
static int score=0,lives=3,coinsGot=0,coinsTotal=0,kills=0,killsTotal=0;
static float energy=0; static const float ENERGY_MAX=100;   // chakra meter -> ultimate
static int streak=0; static float streakT=0;                // kill-streak multiplier
static float levelTime=0, introT=0, clearT=0, stateT=0;
static bool key[256]={false},spec[512]={false};
static bool pJump=false,pShoot=false,pSlash=false,pDash=false,pEnter=false,pShrink=false,pUlti=false;
// platforms + breakables declared early (used by background grass + colliders)
struct Plat{ Rect box; float ax,ay,range,speed; int axis; bool oneWay,crumble; int cstate; float ct; float dx,dy; };
static std::vector<Plat> plats;
struct Breakable{ Rect box; bool alive; int gold; };
static std::vector<Breakable> breakables;

// ===========================================================================
//  BACKGROUND
// ===========================================================================
static float terrainH(float wx,float seed,float amp,bool sharp){
    float n=sinf(wx*0.012f+seed*1.7f)*0.55f+sinf(wx*0.027f+seed*3.3f)*0.30f+sinf(wx*0.061f+seed*5.1f)*0.15f;
    if(sharp){ float s=n<0?-1.f:1.f; n=s*powf(fabsf(n),0.55f);} return amp*n;
}
static void drawSky(const Theme&T){
    setProj(0,0);
    if(T.band3){
        glBegin(GL_QUADS); glColor3fv(T.skyBot);glVertex2f(0,0);glVertex2f(WINW,0); glColor3fv(T.skyMid);glVertex2f(WINW,WINH*0.5f);glVertex2f(0,WINH*0.5f); glEnd();
        glBegin(GL_QUADS); glColor3fv(T.skyMid);glVertex2f(0,WINH*0.5f);glVertex2f(WINW,WINH*0.5f); glColor3fv(T.skyTop);glVertex2f(WINW,WINH);glVertex2f(0,WINH); glEnd();
    } else { glBegin(GL_QUADS); glColor3fv(T.skyBot);glVertex2f(0,0);glVertex2f(WINW,0); glColor3fv(T.skyTop);glVertex2f(WINW,WINH);glVertex2f(0,WINH); glEnd(); }
}
static void drawCelestial(const Theme&T){
    float cx=T.discX,cy=T.discY,R=T.discR;
    glowDisc(cx,cy,R*3.4f,T.discCol[0],T.discCol[1],T.discCol[2],T.discGlow*0.5f);
    glowDisc(cx,cy,R*1.8f,T.discCol[0],T.discCol[1],T.discCol[2],T.discGlow*0.7f);
    glColor3fv(T.discCol); disc(cx,cy,R,48);
    if(T.discKind==1){ glColor4f(T.skyTop[0],T.skyTop[1],T.skyTop[2],0.35f); disc(cx-R*0.3f,cy+R*0.25f,R*0.18f,16); disc(cx+R*0.25f,cy-R*0.15f,R*0.12f,14); }
    else { glBlendFunc(GL_SRC_ALPHA,GL_ONE); glColor4f(T.discCol[0],T.discCol[1],T.discCol[2],0.10f); fillRect(0,cy-R*0.12f,WINW,R*0.24f); fillRect(0,cy-R*0.04f,WINW,R*0.08f); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);}
}
static void drawFog(const Theme&T,float yc,int bands,float spread){
    for(int i=0;i<bands;i++){ float y=yc+i*spread,h=90;
        glBegin(GL_QUADS); glColor4f(T.fog[0],T.fog[1],T.fog[2],0);glVertex2f(0,y+h);glVertex2f(WINW,y+h);
        glColor4f(T.fog[0],T.fog[1],T.fog[2],T.fogA);glVertex2f(WINW,y);glVertex2f(0,y); glEnd(); }
}
static void drawTerrain(float factor,float seed,float baseFrac,float amp,bool sharp,float r,float g,float b,float a){
    float baseY=baseFrac*WINH; glColor4f(r,g,b,a);
    glBegin(GL_QUAD_STRIP);
    for(int sx=-20;sx<=WINW+20;sx+=8){ float wx=sx+camX*factor; float top=baseY+terrainH(wx,seed,amp,sharp); glVertex2f(sx,0); glVertex2f(sx,top);} glEnd();
}
static void drawBamboo(float factor,float baseFrac,float r,float g,float b,float a,float spacing,float wdt,float lean){
    float top=baseFrac*WINH; glColor4f(r,g,b,a); float start=camX*factor-100; int k0=(int)floorf(start/spacing);
    for(int k=k0;k<k0+(int)(WINW/spacing)+4;k++){ float wx=k*spacing+hash1(k*1.7f)*spacing*0.6f; float sx=wx-camX*factor;
        float h=top*(0.7f+hash1(k*2.3f)*0.5f); float lw=wdt*(0.8f+hash1(k*3.1f)*0.5f); float ln=lean*(hash1(k*4.7f)*2-1);
        glBegin(GL_QUADS); glVertex2f(sx-lw,0);glVertex2f(sx+lw,0);glVertex2f(sx+lw+ln,h);glVertex2f(sx-lw+ln,h); glEnd();
        for(int l=0;l<3;l++){ float ly=h*(0.7f+l*0.1f); float dir=(l%2?1:-1);
            glBegin(GL_TRIANGLES); glVertex2f(sx+ln*0.8f,ly);glVertex2f(sx+ln*0.8f+dir*40,ly+25);glVertex2f(sx+ln*0.8f+dir*10,ly+5); glEnd(); } }
}
static void drawPillars(float factor,float baseFrac,float r,float g,float b,float a,float spacing,bool ruins){
    float top=baseFrac*WINH; glColor4f(r,g,b,a); float start=camX*factor-100; int k0=(int)floorf(start/spacing);
    for(int k=k0;k<k0+(int)(WINW/spacing)+4;k++){ float wx=k*spacing+hash1(k*1.3f)*spacing*0.4f; float sx=wx-camX*factor;
        float w=32+hash1(k*2.1f)*16; float hh=top*(0.5f+hash1(k*3.7f)*0.5f);
        fillRect(sx-w*0.5f,0,w,hh); fillRect(sx-w*0.85f,0,w*1.7f,22);
        if(!ruins) fillRect(sx-w*0.7f,hh-18,w*1.4f,18); else if(hash1(k*5.3f)>0.5f) fillRect(sx-w*0.5f,hh-14,spacing*0.7f,16); }
}
static void drawPines(float factor,float baseFrac,float r,float g,float b,float a,float spacing){
    float top=baseFrac*WINH; glColor4f(r,g,b,a); float start=camX*factor-100; int k0=(int)floorf(start/spacing);
    for(int k=k0;k<k0+(int)(WINW/spacing)+4;k++){ float wx=k*spacing+hash1(k*1.9f)*spacing*0.5f; float sx=wx-camX*factor; float h=top*(0.7f+hash1(k*2.7f)*0.5f);
        fillRect(sx-4,0,8,h*0.4f); float ti[4]={0.95f,0.72f,0.5f,0.32f};
        for(int t=0;t<4;t++){ float ty=h*(0.35f+t*0.18f); float wd=h*0.32f*ti[t]; glBegin(GL_TRIANGLES); glVertex2f(sx-wd,ty);glVertex2f(sx+wd,ty);glVertex2f(sx,ty+h*0.28f); glEnd(); } }
}
static void drawGrass(float baseY){
    glColor3f(0,0,0); float start=camX-40; int k0=(int)floorf(start/16.f);
    for(int k=k0;k<k0+(int)(WINW/16)+6;k++){ float wx=k*16.f;
        bool ground=false; for(auto&p:plats){ if(p.crumble&&p.cstate==2)continue; if(wx>=p.box.x-2&&wx<=p.box.x+p.box.w+2 && p.box.y<=82 && p.box.y+p.box.h>=78){ground=true;break;} }
        if(!ground) continue;   // no grass floating over pits
        float sx=wx-camX; float h=40+hash1(k*1.3f)*90; float sway=sinf(gTime*1.5f+wx*0.05f)*6;
        glBegin(GL_TRIANGLES); glVertex2f(sx-3,baseY);glVertex2f(sx+3,baseY);glVertex2f(sx+sway,baseY+h); glEnd(); }
}
static void drawLanternsBehind(const Theme&T,float factor){
    float start=camX*factor-100,spacing=520; int k0=(int)floorf(start/spacing);
    for(int k=k0;k<k0+4;k++){ float wx=k*spacing+200; float sx=wx-camX*factor; float y=180+hash1(k*2.1f)*120; float fl=0.6f+0.12f*sinf(gTime*6+k);
        glowDisc(sx,y,30,T.accent[0],T.accent[1],T.accent[2],fl); glColor3f(0,0,0); fillRect(sx-7,y-10,14,20); fillRect(sx-1,y+10,2,30); }
}
static std::vector<Particle> ambient;
static void updateAmbient(const Theme&T){
    static float acc=0; acc+=T.ambientRate*DT;
    while(acc>=1){ acc-=1; Particle p; p.x=camX+frand(0,WINW); p.size=frand(2,4); p.r=T.accent[0];p.g=T.accent[1];p.b=T.accent[2]; p.a=frand(.4f,.9f); p.life=p.max=frand(6,12); p.max=p.life;
        if(T.ambient==1||T.ambient==4){ p.vy=frand(20,50);p.vx=frand(-10,10);p.additive=true;p.y=camY-10; } else { p.vy=-frand(15,30);p.vx=frand(-20,20);p.additive=false;p.y=camY+WINH+10; }
        ambient.push_back(p); }
    for(size_t i=0;i<ambient.size();){ Particle&p=ambient[i]; p.life-=DT; p.x+=p.vx*DT; p.y+=p.vy*DT; if(!p.additive)p.x+=sinf(gTime*1.2f+p.y*0.05f)*0.6f;
        if(p.life<=0||p.y<camY-40||p.y>camY+WINH+40){ ambient[i]=ambient.back(); ambient.pop_back(); } else i++; }
}
static void drawAmbient(){ setProj(camX,camY); for(auto&p:ambient){ float a=p.a*clampf(p.life/1.0f,0,1); if(p.additive) glowDisc(p.x,p.y,p.size*3,p.r,p.g,p.b,a*0.8f); else { glColor4f(p.r,p.g,p.b,a); fillRect(p.x-p.size*0.5f,p.y-p.size*0.5f,p.size,p.size);} } }
static void drawBackground(){
    const Theme&T=THEMES[curTheme]; float horizon=WINH*0.42f;
    drawSky(T); drawCelestial(T); drawFog(T,horizon-20,2,26);
    drawTerrain(0.10f,curTheme*3+1,0.30f,26,false,T.ridge[0],T.ridge[1],T.ridge[2],0.55f);
    drawFog(T,horizon-6,T.fogBands/2+1,22);
    float f=0.32f,r=T.ridge[0]*0.45f,g=T.ridge[1]*0.45f,b=T.ridge[2]*0.45f;
    switch(T.midShape){ case 1:drawBamboo(f,0.62f,r,g,b,0.85f,120,9,4);break; case 2:drawPines(f,0.55f,r,g,b,0.85f,150);break;
        case 3:drawTerrain(f,curTheme*3+2,0.26f,190,true,r,g,b,0.85f);break; case 4:drawPillars(f,0.62f,r,g,b,0.85f,170,false);break;
        case 5:drawPillars(f,0.60f,r,g,b,0.82f,175,true);break; default:drawTerrain(f,curTheme*3+2,0.30f,120,false,r,g,b,0.85f);break; }
    drawLanternsBehind(T,0.32f); drawFog(T,horizon-30,1,0);
}
static void drawForeground(){ setProj(camX,camY); drawGrass(80); }

// ===========================================================================
//  NINJA SKELETON + ANIMATION
// ===========================================================================
enum Joint{ J_ROOT,J_TORSO,J_HEAD,J_SHF,J_ELF,J_SHB,J_ELB,J_HIPF,J_KNF,J_HIPB,J_KNB,J_N };
struct KF{ float t; float a[J_N]; };
struct Clip{ const KF* kf; int n; float dur; bool loop; };
static const KF K_IDLE[]={{0,{0,0,0,152,14,208,-14,8,-10,-8,12}},{0.5f,{0,2,-2,150,12,210,-12,8,-8,-8,10}},{1,{0,0,0,152,14,208,-14,8,-10,-8,12}}};
static const KF K_RUN[]={{0,{0,12,-6,130,35,235,-20,38,-18,-30,-55}},{0.125f,{0,13,-6,120,28,250,-28,14,-62,6,-22}},{0.25f,{0,14,-7,235,-18,128,32,-28,-45,40,-16}},{0.375f,{0,13,-6,248,-26,122,26,10,-28,16,-64}},{0.5f,{0,12,-6,235,-20,130,35,-30,-55,38,-18}},{0.625f,{0,13,-6,250,-28,120,28,6,-22,14,-62}},{0.75f,{0,14,-7,128,32,235,-18,40,-16,-28,-45}},{0.875f,{0,13,-6,122,26,248,-26,16,-64,10,-28}},{1,{0,12,-6,130,35,235,-20,38,-18,-30,-55}}};
static const KF K_RISE[]={{0,{0,18,-4,100,70,260,-70,70,-90,60,-85}},{0.35f,{0,6,6,200,-10,160,10,30,-50,100,-20}},{1,{0,4,8,215,-25,145,25,55,-75,25,-18}}};
static const KF K_FALL[]={{0,{0,-2,4,235,-30,125,30,22,-30,-10,-36}},{0.5f,{0,0,6,240,-36,120,36,26,-24,-6,-42}},{1,{0,-2,4,235,-30,125,30,22,-30,-10,-36}}};
static const KF K_FLIP[]={{0,{0,8,-2,210,-20,150,20,40,-40,-30,-45}},{0.30f,{180,30,-20,95,120,265,-120,110,-130,95,-125}},{0.55f,{320,24,-16,120,90,240,-90,90,-110,80,-110}},{0.80f,{355,10,-4,180,10,180,-10,50,-60,40,-55}},{1,{360,4,4,230,-28,130,28,30,-36,-12,-40}}};
static const KF K_DASH[]={{0,{0,16,-2,150,30,210,-30,30,-30,-20,-35}},{0.25f,{0,26,-8,250,-40,110,50,-10,-10,70,-20}},{1,{0,24,-6,245,-36,115,44,-6,-14,64,-16}}};
static const KF K_THROW[]={{0,{0,8,-4,152,14,208,-14,10,-12,-10,14}},{0.30f,{0,-6,10,95,95,230,-30,10,-12,-10,14}},{0.45f,{0,14,-10,250,-50,175,5,10,-12,-10,14}},{0.70f,{0,10,-6,215,-20,160,-20,10,-12,-10,14}},{1,{0,8,-4,152,14,208,-14,10,-12,-10,14}}};
static const KF K_SLASH[]={{0,{0,10,-4,150,20,205,-16,8,-10,-8,12}},{0.30f,{0,-8,14,70,60,110,40,8,-10,-8,12}},{0.50f,{0,20,-14,255,-20,280,-50,8,-10,-8,12}},{0.70f,{0,16,-10,300,-10,320,-30,8,-10,-8,12}},{1,{0,10,-4,150,20,205,-16,8,-10,-8,12}}};
static const KF K_HURT[]={{0,{0,0,0,152,14,208,-14,8,-10,-8,12}},{0.25f,{0,-22,22,110,80,250,-80,-20,-40,30,-55}},{1,{0,0,0,152,14,208,-14,8,-10,-8,12}}};
static const KF K_DEATH[]={{0,{0,0,0,152,14,208,-14,8,-10,-8,12}},{0.2f,{0,-22,22,110,80,250,-80,-20,-40,30,-55}},{0.55f,{0,40,-12,120,40,220,-40,-30,-60,20,-50}},{1,{0,78,18,140,20,200,-20,-60,-20,-50,-30}}};
static const KF K_VICT[]={{0,{0,-4,-6,300,-30,120,70,25,-20,-25,20}},{0.5f,{0,-2,-8,302,-34,122,72,25,-20,-25,20}},{1,{0,-4,-6,300,-30,120,70,25,-20,-25,20}}};
enum AnimState{ AN_IDLE,AN_RUN,AN_RISE,AN_FALL,AN_FLIP,AN_DASH,AN_THROW,AN_SLASH,AN_HURT,AN_DEATH,AN_VICTORY,AN_COUNT };
static Clip CLIPS[AN_COUNT]={{K_IDLE,3,2.4f,true},{K_RUN,9,0.5f,true},{K_RISE,3,0.32f,false},{K_FALL,3,0.6f,true},{K_FLIP,5,0.55f,false},{K_DASH,3,0.22f,false},{K_THROW,5,0.34f,false},{K_SLASH,5,0.30f,false},{K_HURT,3,0.28f,false},{K_DEATH,4,0.9f,false},{K_VICT,3,2.0f,true}};
static void sampleClip(const Clip&c,float u,float out[J_N]){
    u=clampf(u,0,1); int i=0; while(i<c.n-1&&c.kf[i+1].t<u)i++; int j=(i+1<c.n)?i+1:i;
    float t0=c.kf[i].t,t1=c.kf[j].t; float f=(t1>t0)?(u-t0)/(t1-t0):0; f=smooth(f);
    for(int k=0;k<J_N;k++) out[k]=lerpf(c.kf[i].a[k],c.kf[j].a[k],f);
}
static void drawNinja(float px,float py,int facing,const float ang[J_N],float sqx,float sqy,const float col[3],float alpha,float scale){
    const float L_torso=34,L_head=16,L_sh=20,L_el=19,L_hip=24,L_kn=23; float pelvisOff=(L_hip+L_kn)*0.90f; float fx=(float)facing; float root=ang[J_ROOT];
    glPushMatrix(); glTranslatef(px,py+pelvisOff*scale*sqy,0); glScalef(scale*sqx,scale*sqy,1); glColor4f(col[0],col[1],col[2],alpha);
    float ta=deg2rad(90+root+ang[J_TORSO]); float Sx=cosf(ta)*L_torso*fx,Sy=sinf(ta)*L_torso;
    auto leg=[&](float hipA,float knA){ float ha=deg2rad(-90+root+hipA); float kx=cosf(ha)*L_hip*fx,ky=sinf(ha)*L_hip; float ka=ha+deg2rad(knA); float ex=kx+cosf(ka)*L_kn*fx,ey=ky+sinf(ka)*L_kn; bone(0,0,kx,ky,5,4); bone(kx,ky,ex,ey,4,3); bone(ex,ey,ex+9*fx,ey,3,2); };
    auto arm=[&](float shA,float elA){ float sa=ta+deg2rad(shA); float ex=Sx+cosf(sa)*L_sh*fx,ey=Sy+sinf(sa)*L_sh; bone(Sx,Sy,ex,ey,5,4); float ea=sa+deg2rad(elA); float hx=ex+cosf(ea)*L_el*fx,hy=ey+sinf(ea)*L_el; bone(ex,ey,hx,hy,4,3); };
    leg(ang[J_HIPB],ang[J_KNB]); arm(ang[J_SHB],ang[J_ELB]);
    { float qx0=Sx*0.45f-7*fx,qy0=Sy*0.45f; float qx1=qx0+cosf(ta-deg2rad(32))*44*fx,qy1=qy0+sinf(ta-deg2rad(32))*44; bone(qx0,qy0,qx1,qy1,2.6f,1.1f); }
    bone(0,0,Sx,Sy,9,7);
    float headA=ta+deg2rad(ang[J_HEAD]); float Hx=Sx+cosf(headA)*L_head*0.55f*fx,Hy=Sy+sinf(headA)*L_head*0.55f; disc(Hx,Hy,9.5f,18);
    { float cp=gTime*4; float cx=Hx-fx*7,cy=Hy+3; for(int s=0;s<3;s++){ float nx=cx-fx*(11+s*9)+sinf(cp+s)*3,ny=cy-3-s*5+cosf(cp+s)*2; bone(cx,cy,nx,ny,4.5f-s*1.2f,3.0f-s*1.0f); cx=nx;cy=ny; } }
    bone(-2*fx,-2,-fx*12+sinf(gTime*3)*3,-16,4,2);
    leg(ang[J_HIPF],ang[J_KNF]); arm(ang[J_SHF],ang[J_ELF]);
    glPopMatrix();
}

// ===========================================================================
//  PLATFORMS / COLLISION
// ===========================================================================
struct Col{ Rect r; int plat; bool oneWay; int crum; };
static std::vector<Col> cols;
static void buildColliders(){
    cols.clear();
    for(size_t i=0;i<plats.size();i++){ Plat&p=plats[i]; if(p.crumble&&p.cstate==2) continue; cols.push_back({p.box,(int)i,p.oneWay, p.crumble?(int)i:-1}); }
    for(auto&b:breakables){ if(b.alive) cols.push_back({b.box,-1,false,-1}); }   // intact breakable = solid
}
static bool fitsSolid(float x,float y,float w,float h){ Rect b{x,y,w,h}; for(auto&p:plats){ if(p.oneWay||(p.crumble&&p.cstate==2))continue; if(overlap(b,p.box))return false; } return true; }

// ===========================================================================
//  COINS / CHECKS / HAZARDS / PROJECTILES / ENEMIES
// ===========================================================================
struct Coin{ float x,y; bool got; };           static std::vector<Coin> coins;
struct Check{ float x,y; bool active; };        static std::vector<Check> checks;
enum HazardType{ HZ_SPIKES,HZ_SAW,HZ_LAVA,HZ_CRUSHER,HZ_FIREJET };
struct Hazard{ HazardType type; Rect box; float r,ax,ay,range,speed; int axis; float spin,phase,timer; int state; };
static std::vector<Hazard> hazards;
struct Proj{ float x,y,vx,vy,life,dmg,grav,spin,rot; int owner; bool alive; int kind; }; // owner 0=player 1=enemy
static std::vector<Proj> projs;
static void spawnProj(float x,float y,float vx,float vy,float dmg,int owner,int kind,float grav){
    Proj p; p.x=x;p.y=y;p.vx=vx;p.vy=vy;p.life=(owner==0?SHURIKEN_LIFE:2.2f);p.dmg=dmg;p.grav=grav;p.spin=18;p.rot=0;p.owner=owner;p.alive=true;p.kind=kind; projs.push_back(p);
}
static void breakWall(Breakable&b){ if(!b.alive)return; b.alive=false;
    float cx=b.box.x+b.box.w*0.5f, cy=b.box.y+b.box.h*0.5f;
    burst(cx,cy,18,0.25f,0.30f,0.45f,360,0.5f,false);      // stone shards
    burst(cx,cy,12,1,0.85f,0.4f,300,0.5f,true);            // gold sparkle
    for(int i=0;i<b.gold;i++){ coins.push_back({cx+frand(-b.box.w*0.32f,b.box.w*0.32f), cy+frand(-12,22), false}); coinsTotal++; }
    shake=fmaxf(shake,0.2f); hitstop=fmaxf(hitstop,0.05f);
}
enum EnemyType{ EN_STALKER,EN_ARCHER,EN_DASHER,EN_BRUTE,EN_MINE,EN_BOSS,EN_TCOUNT };
struct EArch{ float w,h; int hp; float speed; int pts; float col[3]; };
static const EArch EARCH[EN_TCOUNT]={
 {34,50,30,175,100,{.05f,.05f,.07f}},{32,48,20,120,120,{.06f,.06f,.09f}},{30,46,18,240,150,{.04f,.04f,.06f}},
 {58,72,110,70,300,{.03f,.03f,.05f}},{30,22,8,200,60,{.04f,.04f,.05f}},{90,120,560,110,2000,{.02f,.02f,.04f}}};
enum EState{ ES_IDLE,ES_PATROL,ES_CHASE,ES_WINDUP,ES_ATTACK,ES_RECOVER,ES_HURT,ES_DEAD };
struct Enemy{ EnemyType type; Rect box; float vx,vy; int hp,maxhp,facing; bool alive,onGround; float ax,patrol; int st; float stT,atkT,windup,hurtFlash,stagger; AnimState anim; float animT; float sqx,sqy; int phase; float misc; float deadT; bool aggro; };
static std::vector<Enemy> enemies;

// ===========================================================================
//  PLAYER
// ===========================================================================
struct Player{ float x,y,w,h,vx,vy; bool onGround,wallL,wallR; int jumpsLeft,facing,dashLeft; float coyote,jumpBuf,wallLock;
 float dashTime,dashCD; bool dashing; float dashDir; AnimState anim; float animT; float sqx,sqy;
 float hp,iframe,hurtBlink; float slashTime,slashCD,fireCD; bool slashHit; bool alive; float deadT; float spawnX,spawnY; int carrier; bool small; bool blockedFlash; float ultiTime; bool ulting; };
static Player P;
static void setAnim(AnimState a){ if(P.anim!=a){P.anim=a;P.animT=0;} }
static void playerSpawn(){ P.x=P.spawnX;P.y=P.spawnY;P.vx=P.vy=0;P.w=NW;P.h=NH;P.onGround=false;P.wallL=P.wallR=false;P.jumpsLeft=MAX_JUMPS;P.facing=1;P.dashLeft=MAX_DASH;P.coyote=P.jumpBuf=P.wallLock=0;P.dashTime=0;P.dashCD=0;P.dashing=false;P.anim=AN_IDLE;P.animT=0;P.sqx=P.sqy=1;P.hp=PLAYER_MAXHP;P.iframe=0;P.slashTime=0;P.slashCD=0;P.fireCD=0;P.slashHit=false;P.alive=true;P.deadT=0;P.carrier=-1;P.small=false;P.blockedFlash=false;P.ulting=false;P.ultiTime=0; }

// forward
static void damageEnemy(Enemy&e,float dmg,float kbx);

static void killPlayer(){
    if(!P.alive)return; P.alive=false; P.deadT=1.1f; lives--; shake=0.55f; timeScale=0.35f; setAnim(AN_DEATH);
    burst(P.x+P.w*0.5f,P.y+P.h*0.5f,30,1,.35f,.3f,420,0.8f,false);
}
static void hurtPlayer(float dmg,float fromX,bool instakill){
    if(!P.alive||((P.iframe>0||P.dashing)&&!instakill)) return;   // instakill (lava/fall) pierces i-frames
    P.hp-=instakill?9999:dmg; P.iframe=IFRAMES; P.hurtBlink=IFRAMES; P.vx+=sgn(P.x+P.w*0.5f-fromX)*260; P.vy=200; shake=0.30f; hitstop=0.12f; streak=0; streakT=0;
    burst(P.x+P.w*0.5f,P.y+P.h*0.5f,10,1,.3f,.25f,300,0.4f,false); setAnim(AN_HURT);
    if(P.hp<=0) killPlayer();
}
static void throwShuriken(){
    if(P.fireCD>0||!P.alive)return; P.fireCD=SHURIKEN_CD; setAnim(AN_THROW);
    spawnProj(P.x+P.w*0.5f+P.facing*18,P.y+P.h*0.62f,P.facing*SHURIKEN_SPEED,0,SHURIKEN_DMG,0,0,0); // straight & long-range
}
static void doSlash(){ if(P.slashCD>0||!P.alive)return; P.slashCD=SLASH_CD; P.slashTime=SLASH_TIME; P.slashHit=false; setAnim(AN_SLASH); }

static void updatePlayer(){
    bool L=key['a']||spec[GLUT_KEY_LEFT], R=key['d']||spec[GLUT_KEY_RIGHT];
    bool jumpH=key['w']||key[' ']||spec[GLUT_KEY_UP];
    bool shootH=key['j'], slashH=key['k'], dashH=key['l'], shrinkH=key['s'], ultiH=key['u'];
    bool jumpE=jumpH&&!pJump, jumpRel=!jumpH&&pJump, shootE=shootH&&!pShoot, slashE=slashH&&!pSlash, dashE=dashH&&!pDash, shrinkE=shrinkH&&!pShrink, ultiE=ultiH&&!pUlti;
    pJump=jumpH;pShoot=shootH;pSlash=slashH;pDash=dashH;pShrink=shrinkH;pUlti=ultiH;
    if(streakT>0){ streakT-=DT; if(streakT<=0)streak=0; }

    if(!P.alive){ // death: freeze, just animate, gravity
        P.vy-=GRAVITY*DT; P.y+=P.vy*DT; if(P.y<0)P.y=0;
        P.animT=clampf(P.animT+DT,0,CLIPS[AN_DEATH].dur);
        P.deadT-=DT; if(P.deadT<=0){ if(lives>0){ playerSpawn(); } else { gameState=ST_OVER; stateT=0; } }
        return;
    }
    if(P.iframe>0)P.iframe-=DT; if(P.hurtBlink>0)P.hurtBlink-=DT;
    if(P.fireCD>0)P.fireCD-=DT; if(P.slashCD>0)P.slashCD-=DT;

    float desire=(R?1.f:0)-(L?1.f:0); if(desire!=0)P.facing=(desire>0?1:-1);
    // carry by moving platform
    if(P.onGround&&P.carrier>=0&&P.carrier<(int)plats.size()){ P.x+=plats[P.carrier].dx; P.y+=plats[P.carrier].dy; }

    if(P.ulting){ P.vx=P.facing*1950; P.vy=0; P.iframe=fmaxf(P.iframe,0.1f); P.ultiTime-=DT;
        Rect ub{P.x-26,P.y-8,P.w+52,P.h+16};
        for(auto&e:enemies){ if(e.alive&&overlap(ub,e.box)) damageEnemy(e,SLASH_DMG*2,P.facing*320); }
        for(auto&b:breakables){ if(b.alive&&overlap(ub,b.box)) breakWall(b); }
        burst(P.x+P.w*0.5f,P.y+P.h*0.5f,2,.6f,.95f,1,140,.25f,true);
        if(P.ultiTime<=0){ P.ulting=false; P.vx*=0.35f; } }
    else if(P.dashing){ P.vx=P.dashDir*DASH_SPEED; P.vy=0; P.dashTime-=DT; if(P.dashTime<=0){P.dashing=false;P.vx*=0.45f;} }
    else { float accel=P.onGround?GROUND_ACCEL:AIR_ACCEL; if(P.wallLock>0){P.wallLock-=DT;accel*=0.25f;}
        P.vx+=desire*accel*DT; P.vx=clampf(P.vx,-MOVE_MAX,MOVE_MAX);
        if(desire==0){ float fr=(P.onGround?GROUND_FRICTION:AIR_FRICTION)*DT; if(P.vx>0)P.vx=(P.vx>fr?P.vx-fr:0); else P.vx=(P.vx<-fr?P.vx+fr:0);} }

    if(jumpE)P.jumpBuf=JUMP_BUFFER; if(P.jumpBuf>0)P.jumpBuf-=DT; if(P.coyote>0)P.coyote-=DT;
    bool wallC=(P.wallL&&L)||(P.wallR&&R);
    if(P.jumpBuf>0&&!P.dashing){
        if(P.coyote>0){ P.vy=JUMP_VEL;P.jumpsLeft=MAX_JUMPS-1;P.coyote=0;P.jumpBuf=0;dust(P.x+P.w*0.5f,P.y,8);P.sqx=0.7f;P.sqy=1.35f; }
        else if(wallC&&!P.onGround){ int d=P.wallL?1:-1;P.vy=WALL_JUMP_VY;P.vx=d*WALL_JUMP_VX;P.wallLock=WALL_LOCK;P.jumpsLeft=MAX_JUMPS-1;P.facing=d;P.jumpBuf=0; }
        else if(P.jumpsLeft>0){ P.vy=DJUMP_VEL;P.jumpsLeft--;P.jumpBuf=0;burst(P.x+P.w*0.5f,P.y+4,12,.6f,.85f,1,240,.35f,true);setAnim(AN_FLIP); }
    }
    if(jumpRel&&P.vy>0)P.vy*=CUT_JUMP;
    if(dashE&&!P.dashing&&P.dashCD<=0&&P.dashLeft>0){ P.dashing=true;P.dashTime=DASH_TIME;P.dashDir=(desire!=0?desire:P.facing);P.dashCD=DASH_CD;P.dashLeft--;P.iframe=fmaxf(P.iframe,DASH_TIME+0.10f);P.sqx=1.4f;P.sqy=0.7f;burst(P.x+P.w*0.5f,P.y+P.h*0.5f,12,.5f,.9f,1,260,.3f,true); }
    if(P.dashCD>0)P.dashCD-=DT;
    if(ultiE && !P.ulting && energy>=ENERGY_MAX){ P.ulting=true;P.ultiTime=0.5f;energy=0;P.dashing=false;flashWhite=1.f;P.iframe=0.7f;shake=fmaxf(shake,0.4f);setAnim(AN_DASH);burst(P.x+P.w*0.5f,P.y+P.h*0.5f,24,.6f,.95f,1,400,.5f,true); }
    if(shootE&&!P.ulting)throwShuriken();
    if(slashE&&!P.ulting)doSlash();
    // shrink toggle (fit through low tunnels). Grow only if there's headroom.
    if(shrinkE){ if(!P.small){ P.small=true; P.x+=(NW-SW)*0.5f; P.w=SW;P.h=SH; burst(P.x+P.w*0.5f,P.y+P.h*0.5f,12,1,.85f,.4f,200,.3f,true); }
        else { float nx=P.x-(NW-SW)*0.5f; if(fitsSolid(nx,P.y,NW,NH)){ P.small=false;P.x=nx;P.w=NW;P.h=NH; } else P.blockedFlash=true; } }
    // slash hitbox at impact
    if(P.slashTime>0){ P.slashTime-=DT; if(!P.slashHit&&P.slashTime<=SLASH_TIME*0.5f){ P.slashHit=true; hitstop=fmaxf(hitstop,0.07f);
        Rect sb{ P.facing>0?P.x+P.w:P.x-SLASH_REACH, P.y+P.h*0.5f-SLASH_HALFH, SLASH_REACH, SLASH_HALFH*2 };
        for(auto&e:enemies){ if(e.alive&&overlap(sb,e.box)) damageEnemy(e,SLASH_DMG,P.facing*220); }
        for(auto&b:breakables){ if(b.alive&&overlap(sb,b.box)) breakWall(b); }
        burst(P.x+P.w*0.5f+P.facing*40,P.y+P.h*0.6f,6,.7f,.9f,1,200,.25f,true);
    } }

    if(!P.dashing&&!P.ulting){ P.vy-=GRAVITY*DT; if(P.vy<-MAXFALL)P.vy=-MAXFALL; if(wallC&&P.vy<0&&!P.onGround&&P.vy<-WALL_SLIDE)P.vy=-WALL_SLIDE; }

    // integrate vs cols
    bool dropReq=spec[GLUT_KEY_DOWN]&&jumpE;   // down + jump = drop through one-way
    float total=fabsf(P.vx*DT)+fabsf(P.vy*DT); int steps=(int)ceilf(total/18.f); if(steps<1)steps=1; if(steps>10)steps=10; float sdt=DT/steps;
    bool landed=false; int carrier=-1; P.wallL=P.wallR=false;
    for(int s=0;s<steps;s++){
        P.x+=P.vx*sdt;
        for(auto&c:cols){ if(c.oneWay)continue; Rect pr{P.x,P.y,P.w,P.h}; if(overlap(pr,c.r)){ if(P.vx>0){P.x=c.r.x-P.w;P.wallR=true;} else if(P.vx<0){P.x=c.r.x+c.r.w;P.wallL=true;} if(P.dashing)P.dashing=false; P.vx=0; } }
        float prevBot=P.y; P.y+=P.vy*sdt;
        for(auto&c:cols){ Rect pr{P.x,P.y,P.w,P.h}; if(!overlap(pr,c.r))continue;
            if(c.oneWay){ if(P.vy<=0 && prevBot>=c.r.y+c.r.h-2 && !dropReq){ P.y=c.r.y+c.r.h;landed=true;P.vy=0; carrier=c.plat; if(c.crum>=0&&plats[c.crum].cstate==0){plats[c.crum].cstate=1;plats[c.crum].ct=CRUMBLE_DELAY;} } }
            else { if(P.vy<=0){ P.y=c.r.y+c.r.h;landed=true;P.vy=0;carrier=c.plat; if(c.crum>=0&&plats[c.crum].cstate==0){plats[c.crum].cstate=1;plats[c.crum].ct=CRUMBLE_DELAY;} } else { P.y=c.r.y-P.h;P.vy=0; } } }
    }
    if(landed){ if(!P.onGround){dust(P.x+P.w*0.5f,P.y,8);P.sqx=1.3f;P.sqy=0.7f;} P.onGround=true;P.jumpsLeft=MAX_JUMPS;P.dashLeft=MAX_DASH;P.coyote=COYOTE;P.carrier=carrier; }
    else { P.onGround=false; P.carrier=-1; }
    if(P.y<DEATH_Y) killPlayer();
    P.sqx=lerpf(P.sqx,1,0.2f);P.sqy=lerpf(P.sqy,1,0.2f);

    // anim selection
    bool busy=(P.anim==AN_THROW||P.anim==AN_SLASH||P.anim==AN_HURT)&&P.animT<CLIPS[P.anim].dur;
    if(!busy){ AnimState want;
        if(P.dashing)want=AN_DASH;
        else if(!P.onGround){ if(P.anim==AN_FLIP&&P.animT<CLIPS[AN_FLIP].dur)want=AN_FLIP; else want=(P.vy>0?AN_RISE:AN_FALL);}
        else if(fabsf(P.vx)>40)want=AN_RUN; else want=AN_IDLE;
        if(want!=AN_FLIP)setAnim(want);
    }
    float spd=1.f; if(P.anim==AN_RUN)spd=clampf(fabsf(P.vx)/MOVE_MAX,0.4f,1.6f);
    P.animT+=DT*spd; if(CLIPS[P.anim].loop)P.animT=fmodf(P.animT,CLIPS[P.anim].dur); else if(P.animT>CLIPS[P.anim].dur)P.animT=CLIPS[P.anim].dur;
}
static void drawPlayer(){
    setProj(camX,camY); float ang[J_N]; sampleClip(CLIPS[P.anim],P.animT/CLIPS[P.anim].dur,ang);
    float black[3]={0,0,0}; const float*acc=THEMES[curTheme].accent;
    if(P.hurtBlink>0 && fmodf(gTime*30,2)<1 && P.alive) return; // blink only when hurt (not during dash)
    if(P.dashing) for(int i=1;i<=3;i++){ float a=0.18f-i*0.05f; drawNinja(P.x+P.w*0.5f-P.dashDir*i*9,P.y,P.facing,ang,P.sqx,P.sqy,acc,a,1.0f); }
    if(P.ulting){ for(int i=1;i<=6;i++){ float a=0.5f-i*0.07f; drawNinja(P.x+P.w*0.5f-P.facing*i*16,P.y,P.facing,ang,P.sqx,P.sqy,acc,a,1.06f); }
        glBlendFunc(GL_SRC_ALPHA,GL_ONE); glColor4f(.7f,.95f,1,0.5f); fillRect(P.x-P.facing*120,P.y+P.h*0.4f,P.facing*120,8); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); }
    drawNinja(P.x+P.w*0.5f,P.y,P.facing,ang,P.sqx,P.sqy,acc,0.28f,1.16f);
    drawNinja(P.x+P.w*0.5f,P.y,P.facing,ang,P.sqx,P.sqy,black,1.0f,1.0f);
    // slash: visible sword blade sweeping through a glowing crescent trail
    if(P.slashTime>0){ float u=1-P.slashTime/SLASH_TIME; float fx=(float)P.facing;
        float cx=P.x+P.w*0.5f, cy=P.y+P.h*0.55f; float curA=lerpf(-78.f,78.f,u);
        // swept trail (additive), from start angle to current
        glBlendFunc(GL_SRC_ALPHA,GL_ONE);
        glBegin(GL_TRIANGLE_STRIP);
        for(int i=0;i<=16;i++){ float aa=deg2rad(lerpf(-78.f,curA,i/16.f)); float fade=(float)i/16.f;
            glColor4f(.75f,.92f,1,0.f); glVertex2f(cx+cosf(aa)*64*fx,cy+sinf(aa)*64);
            glColor4f(.85f,.95f,1,0.55f*fade); glVertex2f(cx+cosf(aa)*32*fx,cy+sinf(aa)*32); }
        glEnd(); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        // the katana blade itself at the current angle
        float a=deg2rad(curA); float hx=cx+fx*9, hy=cy;
        float gx=hx+cosf(a)*12*fx, gy=hy+sinf(a)*12;            // guard
        float tx=hx+cosf(a)*64*fx, ty=hy+sinf(a)*64;           // tip
        glColor3f(0.18f,0.16f,0.20f); bone(hx,hy,gx,gy,3.2f,2.4f);     // dark grip
        glColor3f(0.90f,0.95f,1.0f); bone(gx,gy,tx,ty,3.0f,0.4f);     // bright blade
        glColor3f(1,1,1); bone(gx,gy, hx+cosf(a)*44*fx, hy+sinf(a)*44, 1.2f,0.4f); // hot edge
    }
}

// ===========================================================================
//  ENEMIES
// ===========================================================================
static void damageEnemy(Enemy&e,float dmg,float kbx){
    if(!e.alive||e.st==ES_DEAD)return; e.hp-=(int)dmg; e.hurtFlash=0.10f; e.sqx=1.3f;e.sqy=0.7f; hitstop=fmaxf(hitstop,0.07f);
    e.stagger=(e.type==EN_BRUTE||e.type==EN_BOSS)?0.07f:0.16f;   // brief freeze => melee breathing room
    burst(e.box.x+e.box.w*0.5f,e.box.y+e.box.h*0.6f,10,1,.22f,.18f,360,0.35f,false);
    burst(e.box.x+e.box.w*0.5f,e.box.y+e.box.h*0.6f,3,1,.95f,.55f,300,0.3f,true);
    char b[8]; snprintf(b,8,"-%d",(int)dmg); floater(e.box.x+e.box.w*0.5f,e.box.y+e.box.h,b,1,.5f,.4f);
    if(e.type!=EN_BRUTE&&e.type!=EN_BOSS) e.vx+=kbx;
    energy=fminf(ENERGY_MAX,energy+6);                          // build chakra on hit
    if(e.hp<=0){ e.st=ES_DEAD; e.deadT=0.30f; e.alive=false; kills++;
        streak++; streakT=2.6f; energy=fminf(ENERGY_MAX,energy+18);
        int mult = 1 + streak/3; score += EARCH[e.type].pts * mult;   // kill-streak multiplier
        if(streak>=2){ char cb[16]; snprintf(cb,16,"COMBO x%d",mult); floater(e.box.x+e.box.w*0.5f,e.box.y+e.box.h+18,cb,1,.85f,.35f); }
        hitstop=fmaxf(hitstop,0.09f); timeScale=fminf(timeScale,0.5f);
        burst(e.box.x+e.box.w*0.5f,e.box.y+e.box.h*0.5f,22,0,0,0,420,0.6f,false); burst(e.box.x+e.box.w*0.5f,e.box.y+e.box.h*0.5f,12,1,.3f,.2f,380,0.5f,true);
        if(e.type==EN_MINE){ for(auto&o:enemies){ if(&o!=&e&&o.alive){ float dx=o.box.x-e.box.x,dy=o.box.y-e.box.y; if(dx*dx+dy*dy<70*70) damageEnemy(o,14,sgn(dx)*200);} } }
    }
}
static void enemyWorldCollide(Enemy&e,bool dropOff){
    // x
    for(auto&c:cols){ if(c.oneWay)continue; if(overlap(e.box,c.r)){ if(e.vx>0)e.box.x=c.r.x-e.box.w; else if(e.vx<0)e.box.x=c.r.x+c.r.w; e.vx=0; } }
    // y
    e.onGround=false;
    for(auto&c:cols){ if(overlap(e.box,c.r)){ if(e.vy<=0){ if(c.oneWay && e.box.y < c.r.y+c.r.h-6) continue; e.box.y=c.r.y+c.r.h; e.vy=0; e.onGround=true; } else if(!c.oneWay){ e.box.y=c.r.y-e.box.h; e.vy=0; } } }
    (void)dropOff;
}
static void updateEnemies(){
    float pcx=P.x+P.w*0.5f, pcy=P.y+P.h*0.5f;
    for(auto&e:enemies){
        if(e.hurtFlash>0)e.hurtFlash-=DT; e.sqx=lerpf(e.sqx,1,0.2f);e.sqy=lerpf(e.sqy,1,0.2f);
        if(!e.alive){ if(e.deadT>0){e.deadT-=DT;} e.animT+=DT; continue; }
        float ecx=e.box.x+e.box.w*0.5f, ecy=e.box.y+e.box.h*0.5f;
        float dx=pcx-ecx, dy=pcy-ecy, dist=sqrtf(dx*dx+dy*dy);
        const EArch&A=EARCH[e.type];
        bool grav=true;
        if(e.stagger>0){ e.stagger-=DT; e.vx=0; }
        else switch(e.type){
        case EN_STALKER:{
            if(!e.aggro){ if(dist<360&&fabsf(dy)<70)e.aggro=true; else { e.vx=e.facing*90; if(e.box.x<e.ax-e.patrol){e.facing=1;} if(e.box.x>e.ax+e.patrol){e.facing=-1;} e.st=ES_PATROL; } }
            if(e.aggro){ e.stT+=DT; if(dist<360)e.stT=0; if(e.stT>2.5f)e.aggro=false; e.facing=(dx>0?1:-1);
                if(e.st==ES_WINDUP){ e.windup-=DT; e.vx=0; if(e.windup<=0){ Rect hb{ e.facing>0?e.box.x+e.box.w:e.box.x-46, e.box.y, 46,30 }; if(overlap(hb,Rect{P.x,P.y,P.w,P.h}))hurtPlayer(12,ecx,false); e.st=ES_RECOVER; e.atkT=0.5f; } }
                else if(e.st==ES_RECOVER){ e.atkT-=DT; e.vx=0; if(e.atkT<=0)e.st=ES_CHASE; }
                else { if(fabsf(dx)>54){ e.vx=sgn(dx)*A.speed; e.st=ES_CHASE; } else { e.st=ES_WINDUP; e.windup=0.38f; } }
            }
        }break;
        case EN_ARCHER:{
            e.facing=(dx>0?1:-1);
            if(dist<220){ e.vx=-sgn(dx)*120; } else if(dist>560){ e.vx=sgn(dx)*90; } else { e.vx=0; e.atkT-=DT; if(e.atkT<=0&&fabsf(dy)<160){ e.st=ES_WINDUP; e.windup=0.45f; e.atkT=1.6f; } }
            if(e.st==ES_WINDUP){ e.windup-=DT; if(e.windup<=0){ float ang=atan2f(dy+60,dx); spawnProj(ecx,ecy,cosf(ang)*520,sinf(ang)*520,10,1,1,380); e.st=ES_IDLE; } }
        }break;
        case EN_DASHER:{
            if(e.st==ES_WINDUP){ e.windup-=DT;e.vx=0; if(e.windup<=0){e.st=ES_ATTACK;e.atkT=0.22f;e.misc=e.facing;} }
            else if(e.st==ES_ATTACK){ e.atkT-=DT; e.vx=e.misc*720; if(overlap(e.box,Rect{P.x,P.y,P.w,P.h}))hurtPlayer(16,ecx,false); burst(ecx,ecy,1,.4f,.9f,1,40,.2f,true); if(e.atkT<=0){e.st=ES_RECOVER;e.atkT=0.6f;} }
            else if(e.st==ES_RECOVER){ e.atkT-=DT;e.vx*=0.9f; if(e.atkT<=0){e.st=ES_IDLE;e.misc=1.3f;} }
            else { e.facing=(dx>0?1:-1); if(dist<300&&e.misc<=0){ e.st=ES_WINDUP;e.windup=0.30f; } else { e.vx=-sgn(dx)*110; if(e.misc>0)e.misc-=DT; } }
        }break;
        case EN_BRUTE:{
            e.facing=(dx>0?1:-1);
            if(e.st==ES_WINDUP){ e.windup-=DT;e.vx=0; if(e.windup<=0){ Rect hb{ e.facing>0?e.box.x+e.box.w:e.box.x-90,e.box.y,90,46}; if(overlap(hb,Rect{P.x,P.y,P.w,P.h}))hurtPlayer(28,ecx,false); spawnProj(ecx+e.facing*40,e.box.y+10,e.facing*600,0,8,1,2,0); shake=fmaxf(shake,0.55f); e.st=ES_RECOVER;e.atkT=0.9f; burst(e.box.x+e.box.w*0.5f,e.box.y,16,.5f,.4f,.4f,300,0.4f,false);} }
            else if(e.st==ES_RECOVER){ e.atkT-=DT;e.vx=0; if(e.atkT<=0)e.st=ES_CHASE; }
            else { if(fabsf(dx)>80){ e.vx=sgn(dx)*A.speed; } else { e.st=ES_WINDUP;e.windup=0.85f; } }
        }break;
        case EN_MINE:{
            e.facing=(dx>0?1:-1); e.vx=sgn(dx)*A.speed;
            if(e.st==ES_WINDUP){ e.windup-=DT; e.sqx=1+sinf(gTime*40)*0.2f; if(e.windup<=0){ if(dist<70)hurtPlayer(14,ecx,false); burst(ecx,ecy,20,1,.4f,.2f,400,0.5f,true); e.hp=0;e.st=ES_DEAD;e.alive=false;e.deadT=0.2f; if(killsTotal>0)killsTotal--; } }
            else if(dist<34){ e.st=ES_WINDUP; e.windup=0.4f; }
        }break;
        case EN_BOSS:{
            e.facing=(dx>0?1:-1); float hpFrac=(float)e.hp/e.maxhp;
            e.phase = hpFrac>0.66f?0:(hpFrac>0.33f?1:2);
            float spdMul = e.phase==0?1.f:(e.phase==1?1.3f:1.55f);
            if(e.st==ES_WINDUP){ e.windup-=DT; e.vx=0; if(e.windup<=0){ Rect hb{ e.facing>0?e.box.x+e.box.w:e.box.x-120,e.box.y,120,60}; if(overlap(hb,Rect{P.x,P.y,P.w,P.h}))hurtPlayer(e.phase>=1?34:22,ecx,false); shake=fmaxf(shake,0.55f); burst(e.box.x+e.box.w*0.5f,e.box.y,24,1,.4f,.2f,380,0.5f,true); if(e.phase>=1)spawnProj(ecx,e.box.y+20,e.facing*460,0,18,1,2,0); e.st=ES_RECOVER;e.atkT=(e.phase==2?0.6f:1.0f); } }
            else if(e.st==ES_RECOVER){ e.atkT-=DT;e.vx=0; if(e.atkT<=0)e.st=ES_CHASE; }
            else { if(fabsf(dx)>110){ e.vx=sgn(dx)*A.speed*spdMul; } else { e.st=ES_WINDUP; e.windup=(e.phase==2?0.55f:0.8f); } }
        }break;
        default:break;
        }
        // ledge guard: ground enemies (incl. boss) must not walk off into pits
        if(e.onGround && e.vx!=0){
            float aheadX = e.vx>0 ? e.box.x+e.box.w+4 : e.box.x-4;
            bool g=false; for(auto&c:cols){ if(aheadX>=c.r.x&&aheadX<=c.r.x+c.r.w && fabsf((c.r.y+c.r.h)-e.box.y)<12){ g=true; break; } }
            if(!g){ e.vx=0; if(e.st==ES_PATROL) e.facing=-e.facing; }
        }
        if(grav){ e.vy-=GRAVITY*DT; if(e.vy<-MAXFALL)e.vy=-MAXFALL; }
        // separated-axis resolution (X then Y) so wide ground isn't mistaken for a wall
        e.box.x+=e.vx*DT;
        for(auto&c:cols){ if(c.oneWay)continue; if(overlap(e.box,c.r)){ if(e.vx>0)e.box.x=c.r.x-e.box.w; else if(e.vx<0)e.box.x=c.r.x+c.r.w; e.vx=0; } }
        e.onGround=false; float pby=e.box.y; e.box.y+=e.vy*DT;
        for(auto&c:cols){ if(overlap(e.box,c.r)){ if(e.vy<=0){ if(c.oneWay && pby < c.r.y+c.r.h-6) continue; e.box.y=c.r.y+c.r.h; e.vy=0; e.onGround=true; } else if(!c.oneWay){ e.box.y=c.r.y-e.box.h; e.vy=0; } } }
        if(e.box.y<DEATH_Y){ e.alive=false; e.st=ES_DEAD; }
        // anim
        AnimState wa = (e.st==ES_WINDUP||e.st==ES_ATTACK)?AN_SLASH : (fabsf(e.vx)>30?AN_RUN:AN_IDLE);
        if(e.hurtFlash>0)wa=AN_HURT;
        if(e.anim!=wa){e.anim=wa;e.animT=0;} float es=1.f; if(e.anim==AN_RUN)es=clampf(fabsf(e.vx)/150,0.5f,1.6f);
        e.animT+=DT*es; if(CLIPS[e.anim].loop)e.animT=fmodf(e.animT,CLIPS[e.anim].dur); else if(e.animT>CLIPS[e.anim].dur)e.animT=CLIPS[e.anim].dur;
        // contact damage for stalker/brute touching
        if((e.type==EN_STALKER||e.type==EN_BRUTE||e.type==EN_BOSS)&&overlap(e.box,Rect{P.x,P.y,P.w,P.h})) hurtPlayer(e.type==EN_BOSS?10:(e.type==EN_BRUTE?8:6),ecx,false);
    }
}
static void drawEnemies(){
    setProj(camX,camY);
    for(auto&e:enemies){
        if(!e.alive&&e.deadT<=0&&e.type!=EN_BOSS) continue;
        float ang[J_N]; sampleClip(CLIPS[e.anim],e.animT/CLIPS[e.anim].dur,ang);
        float sc = e.box.h/54.f; // scale by height vs base 54
        float col[3]={EARCH[e.type].col[0],EARCH[e.type].col[1],EARCH[e.type].col[2]};
        float alpha=1.f; if(!e.alive){ alpha=clampf(e.deadT/0.30f,0,1); }
        // hurt flash -> white
        if(e.hurtFlash>0){ col[0]=1;col[1]=1;col[2]=1; }
        float cx=e.box.x+e.box.w*0.5f, fy=e.box.y;
        // per-archetype menace rim so silhouettes pop against black geometry
        float rimcol[3];
        switch(e.type){
            case EN_ARCHER: rimcol[0]=0.45f;rimcol[1]=0.70f;rimcol[2]=1.0f; break;   // cool blue
            case EN_DASHER: rimcol[0]=0.30f;rimcol[1]=0.95f;rimcol[2]=1.0f; break;   // cyan (lunge tell)
            case EN_BRUTE:  rimcol[0]=1.0f; rimcol[1]=0.35f;rimcol[2]=0.10f; break;  // hot orange, heavy
            case EN_MINE:   rimcol[0]=1.0f; rimcol[1]=0.30f;rimcol[2]=0.20f; break;
            case EN_BOSS:   rimcol[0]=1.0f; rimcol[1]=0.25f;rimcol[2]=0.10f; break;
            default:        rimcol[0]=0.95f;rimcol[1]=0.22f;rimcol[2]=0.15f; break;  // stalker red
        }
        // windup telegraph glow (brighter pulse during attacks)
        if(e.st==ES_WINDUP){ glowDisc(cx,e.box.y+e.box.h*0.6f,e.box.w*1.6f,rimcol[0],rimcol[1],rimcol[2],0.45f+0.35f*sinf(gTime*20)); }
        else if(e.type==EN_DASHER){ glowDisc(cx,e.box.y+e.box.h*0.6f,e.box.w*0.9f,rimcol[0],rimcol[1],rimcol[2],0.25f); } // dasher always faintly lit
        float rimA=(0.5f+0.18f*sinf(gTime*3+cx*0.01f))*alpha;
        drawNinja(cx,fy,e.facing,ang,e.sqx,e.sqy,rimcol,rimA, sc*(e.type==EN_BRUTE||e.type==EN_BOSS?1.18f:1.22f));
        drawNinja(cx,fy,e.facing,ang,e.sqx,e.sqy,col,alpha,sc);
        if(e.type==EN_BOSS&&e.st==ES_WINDUP){ // directional strike telegraph
            glowDisc(cx+e.facing*e.box.w*0.55f, e.box.y+e.box.h*0.45f, 78, 1,.2f,.1f, 0.45f+0.30f*sinf(gTime*18)); }
        if(e.type==EN_BOSS&&e.alive){ // boss HP bar above
            float w=160,hh=8,bx=cx-w*0.5f,by=e.box.y+e.box.h+18; glColor4f(0,0,0,0.6f);fillRect(bx-2,by-2,w+4,hh+4);
            glColor3f(0.2f,0.2f,0.2f);fillRect(bx,by,w,hh); glColor3f(0.9f,0.15f,0.12f);fillRect(bx,by,w*clampf((float)e.hp/e.maxhp,0,1),hh); }
    }
}

// ===========================================================================
//  PROJECTILES / HAZARDS update + draw
// ===========================================================================
static void updateProjectiles(){
    for(size_t i=0;i<projs.size();){ Proj&p=projs[i]; p.life-=DT; p.rot+=p.spin*DT; p.vy-=p.grav*DT; p.x+=p.vx*DT; p.y+=p.vy*DT;
        bool dead=p.life<=0;
        for(auto&c:cols){ if(c.oneWay)continue; if(p.x>c.r.x&&p.x<c.r.x+c.r.w&&p.y>c.r.y&&p.y<c.r.y+c.r.h){ dead=true; burst(p.x,p.y,5,1,.9f,.6f,150,0.2f,true);} }
        if(!dead){ if(p.owner==0){ for(auto&b:breakables){ if(b.alive&&overlap(Rect{p.x-6,p.y-6,12,12},b.box)){ breakWall(b); dead=true; break; } }
            if(!dead) for(auto&e:enemies){ if(e.alive&&overlap(Rect{p.x-6,p.y-6,12,12},e.box)){ damageEnemy(e,p.dmg,sgn(p.vx)*150); dead=true; break; } } }
            else { if(overlap(Rect{p.x-6,p.y-6,12,12},Rect{P.x,P.y,P.w,P.h})){ hurtPlayer(p.dmg,p.x,false); dead=true; } } }
        if(dead){ projs[i]=projs.back(); projs.pop_back(); } else i++;
    }
}
static void updateHazards(){
    for(auto&h:hazards){ h.phase+=h.speed*DT;
        if(h.type==HZ_SAW){ float off=sinf(h.phase)*h.range; h.box.x=(h.axis==0?h.ax+off:h.ax); h.box.y=(h.axis==1?h.ay+off:h.ay); h.spin+=12*DT; }
        if(h.type==HZ_FIREJET){ h.timer+=DT; if(h.timer>3.0f)h.timer=0; }   // 0-0.7 warn, 0.7-1.7 fire, rest off
        // hazard vs player
        bool hit=false; Rect pr{P.x,P.y,P.w,P.h};
        if(h.type==HZ_SAW){ float cx=h.box.x,cy=h.box.y; float nx=clampf(cx,P.x,P.x+P.w),ny=clampf(cy,P.y,P.y+P.h); float dxx=cx-nx,dyy=cy-ny; if(dxx*dxx+dyy*dyy<h.r*h.r)hit=true; }
        else if(h.type==HZ_FIREJET){ if(h.timer>=0.7f&&h.timer<1.7f && overlap(pr,h.box))hit=true; }
        else if(overlap(pr,h.box)) hit=true;
        if(hit){ if(h.type==HZ_LAVA) hurtPlayer(0,P.x+P.w*0.5f,true); else hurtPlayer(h.type==HZ_SAW?18:14, h.box.x+h.box.w*0.5f,false); }
    }
}
static void drawHazards(){
    setProj(camX,camY); const Theme&T=THEMES[curTheme];
    for(auto&h:hazards){
        if(h.type==HZ_SPIKES){ glColor3f(0.6f,0.88f,1.f); int n=(int)(h.box.w/20); for(int i=0;i<n;i++){ float x=h.box.x+i*20; glBegin(GL_TRIANGLES);glVertex2f(x,h.box.y);glVertex2f(x+20,h.box.y);glVertex2f(x+10,h.box.y+h.box.h);glEnd(); } }
        else if(h.type==HZ_SAW){ glowDisc(h.box.x,h.box.y,h.r*1.5f,.7f,.9f,1,.4f); glColor3f(.75f,.92f,1.f); int p=8; glBegin(GL_TRIANGLE_FAN);glVertex2f(h.box.x,h.box.y); for(int i=0;i<=p*2;i++){float a=h.spin+M_PI*i/p; float rr=(i%2==0)?h.r:h.r*0.5f; glVertex2f(h.box.x+cosf(a)*rr,h.box.y+sinf(a)*rr);} glEnd(); glColor3f(.95f,1,1);disc(h.box.x,h.box.y,h.r*0.25f,12); }
        else if(h.type==HZ_LAVA){ glBlendFunc(GL_SRC_ALPHA,GL_ONE); float pul=0.3f+0.1f*sinf(gTime*3); glColor4f(1,.35f,.1f,pul); fillRect(h.box.x,h.box.y,h.box.w,h.box.h); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA); glColor4f(1,.5f,.15f,0.9f); fillRect(h.box.x,h.box.y+h.box.h-6,h.box.w,6); }
        else if(h.type==HZ_FIREJET){ if(h.timer>=0.7f&&h.timer<1.7f){ glBlendFunc(GL_SRC_ALPHA,GL_ONE); glColor4f(1,.5f,.15f,0.8f); fillRect(h.box.x,h.box.y,h.box.w,h.box.h); glColor4f(1,.85f,.4f,0.7f); fillRect(h.box.x+h.box.w*0.3f,h.box.y,h.box.w*0.4f,h.box.h); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);} else if(h.timer<0.7f){ float fl=0.2f+0.25f*fabsf(sinf(gTime*18)); glColor4f(1,.5f,.15f,fl); fillRect(h.box.x+h.box.w*0.25f,h.box.y,h.box.w*0.5f,h.box.h); glColor4f(1,.8f,.3f,fl*0.8f); fillRect(h.box.x+h.box.w*0.35f,h.box.y,h.box.w*0.3f,28);} }
        (void)T;
    }
}
static void drawProjectiles(){
    setProj(camX,camY);
    for(auto&p:projs){ const float*acc=THEMES[curTheme].accent;
        if(p.owner==0){ glowDisc(p.x,p.y,14,acc[0],acc[1],acc[2],0.6f); glColor3f(.9f,.97f,1.f);
            glPushMatrix();glTranslatef(p.x,p.y,0);glRotatef(p.rot*57.3f,0,0,1); int pt=4; glBegin(GL_TRIANGLE_FAN);glVertex2f(0,0); for(int i=0;i<=pt*2;i++){float a=M_PI*i/pt; float rr=(i%2==0)?9:3; glVertex2f(cosf(a)*rr,sinf(a)*rr);} glEnd(); glPopMatrix(); }
        else { glowDisc(p.x,p.y,12,1,.5f,.2f,0.6f); glColor3f(.15f,.05f,.05f); glPushMatrix();glTranslatef(p.x,p.y,0);glRotatef(atan2f(p.vy,p.vx)*57.3f,0,0,1); glBegin(GL_TRIANGLES);glVertex2f(8,0);glVertex2f(-6,4);glVertex2f(-6,-4);glEnd(); glPopMatrix(); }
    }
}

// ===========================================================================
//  COINS / CHECKS / GOAL
// ===========================================================================
static Rect goalRect; static bool isBossLevel=false; static bool bossAlive=false;
static void drawCoins(){ setProj(camX,camY); const float*acc=THEMES[curTheme].accent;
    for(auto&c:coins){ if(c.got)continue; float yo=sinf(gTime*2.2f+c.x*0.02f)*3; float s=10; glowDisc(c.x,c.y+yo,16,1,.85f,.4f,.5f);
        glColor3f(1,.85f,.35f); glBegin(GL_QUADS);glVertex2f(c.x,c.y+yo+s);glVertex2f(c.x+s,c.y+yo);glVertex2f(c.x,c.y+yo-s);glVertex2f(c.x-s,c.y+yo);glEnd();
        glColor3f(1,.97f,.7f); float s2=s*0.45f; glBegin(GL_QUADS);glVertex2f(c.x,c.y+yo+s2);glVertex2f(c.x+s2,c.y+yo);glVertex2f(c.x,c.y+yo-s2);glVertex2f(c.x-s2,c.y+yo);glEnd(); (void)acc; }
}
static void drawChecks(){ setProj(camX,camY); for(auto&c:checks){ float cx=c.x; glColor3f(0.45f,0.72f,0.95f); fillRect(cx-2,c.y,3,72);
    if(c.active)glColor3f(0.55f,0.92f,1.f); else glColor3f(0.22f,0.40f,0.58f); float wav=c.active?sinf(gTime*5)*3:0;
    glBegin(GL_TRIANGLES);glVertex2f(cx+1,c.y+72);glVertex2f(cx+1,c.y+52);glVertex2f(cx+24+wav,c.y+62);glEnd(); } }
static void drawGoal(){ if(isBossLevel)return; setProj(camX,camY); float x=goalRect.x,y=goalRect.y,w=goalRect.w,h=goalRect.h; float cx=x+w*0.5f,cy=y+h*0.5f;
    glowDisc(cx,cy,90,0.6f,0.95f,1,0.5f); for(int r=0;r<4;r++){ float rad=44-r*8+sinf(gTime*2+r)*3; glColor4f(0.5f,0.9f,1,0.3f+r*0.12f); ring(cx,cy,rad,24); } glColor4f(.9f,1,1,.9f); disc(cx,cy,9+sinf(gTime*4)*2,14); }
static void updatePickups(){
    Rect pb{P.x,P.y,P.w,P.h};
    for(auto&c:coins){ if(!c.got&&overlap(pb,Rect{c.x-14,c.y-14,28,28})){ c.got=true;coinsGot++;score+=10; burst(c.x,c.y,10,1,.9f,.4f,200,0.4f,false);} }
    for(auto&c:checks){ if(!c.active&&overlap(pb,Rect{c.x-10,c.y,40,80})){ c.active=true;P.spawnX=c.x;P.spawnY=c.y+4; burst(c.x,c.y+40,14,.5f,1,.7f,240,0.5f,true);} }
    if(!isBossLevel){ if(overlap(pb,goalRect)){ gameState=ST_CLEAR; stateT=0; setAnim(AN_FLIP); flashWhite=1.f; hitstop=0.15f; timeScale=0.4f; P.vx=P.vy=0;
        burst(P.x+P.w*0.5f,P.y+P.h*0.6f,30,1,.85f,.4f,360,0.7f,true); } }
    else { if(bossAlive){ bool anyBoss=false; for(auto&e:enemies)if(e.type==EN_BOSS&&e.alive)anyBoss=true; if(!anyBoss){ bossAlive=false; gameState=ST_COMPLETE; stateT=0; setAnim(AN_VICTORY); flashWhite=1.f; timeScale=0.25f; } } }
}

// ===========================================================================
//  WORLD draw
// ===========================================================================
static void drawWorld(){
    setProj(camX,camY); const Theme&T=THEMES[curTheme];
    for(auto&p:plats){ if(p.crumble&&p.cstate==2)continue; float jx=0,jy=0; if(p.crumble&&p.cstate==1){jx=frand(-2,2);jy=frand(-2,2);}
        glColor3f(0,0,0); fillRect(p.box.x+jx,p.box.y+jy,p.box.w,p.box.h);
        glColor4f(T.accent[0],T.accent[1],T.accent[2],p.crumble?0.7f:0.45f); glBegin(GL_LINES);glVertex2f(p.box.x+jx,p.box.y+p.box.h+jy);glVertex2f(p.box.x+p.box.w+jx,p.box.y+p.box.h+jy);glEnd();
        if(p.oneWay){ glColor4f(T.accent[0],T.accent[1],T.accent[2],0.25f); for(float gx=p.box.x+6;gx<p.box.x+p.box.w;gx+=14){ glBegin(GL_LINES);glVertex2f(gx,p.box.y+2);glVertex2f(gx+6,p.box.y+2);glEnd(); } }
    }
}
static void drawBreakables(){ setProj(camX,camY); const Theme&T=THEMES[curTheme];
    for(auto&b:breakables){ if(!b.alive)continue; float x=b.box.x,y=b.box.y,w=b.box.w,h=b.box.h;
        glColor3f(0.11f,0.11f,0.14f); fillRect(x,y,w,h);                              // cracked stone (lighter than platforms)
        glColor4f(T.accent[0],T.accent[1],T.accent[2],0.55f); glBegin(GL_LINE_LOOP);glVertex2f(x,y);glVertex2f(x+w,y);glVertex2f(x+w,y+h);glVertex2f(x,y+h);glEnd();
        glColor4f(0.55f,0.55f,0.65f,0.5f); glBegin(GL_LINES);                          // cracks
            glVertex2f(x+w*0.5f,y+h);glVertex2f(x+w*0.42f,y+h*0.45f); glVertex2f(x+w*0.42f,y+h*0.45f);glVertex2f(x+w*0.62f,y);
            glVertex2f(x,y+h*0.6f);glVertex2f(x+w*0.42f,y+h*0.5f); glEnd();
        float gx=x+w*0.5f,gy=y+h*0.5f,s=8+sinf(gTime*3+x*0.01f)*2;                     // gold pocket inside
        glowDisc(gx,gy,18,1,.85f,.4f,0.5f); glColor3f(1,.85f,.35f);
        glBegin(GL_QUADS);glVertex2f(gx,gy+s);glVertex2f(gx+s,gy);glVertex2f(gx,gy-s);glVertex2f(gx-s,gy);glEnd(); }
}
static void drawFloaters(){ setProj(camX,camY); for(auto&f:floaters){ float a=clampf(f.life/0.8f,0,1); glColor4f(f.r,f.g,f.b,a); text(f.x-6,f.y,GLUT_BITMAP_9_BY_15,f.t);} }

// ===========================================================================
//  LEVELS (procedural builders)
// ===========================================================================
static void addPlat(float x,float y,float w,float h,bool ow=false,bool cr=false,float range=0,float speed=0,int axis=0){ Plat p; p.box={x,y,w,h};p.ax=x;p.ay=y;p.range=range;p.speed=speed;p.axis=axis;p.oneWay=ow;p.crumble=cr;p.cstate=0;p.ct=0;p.dx=p.dy=0; plats.push_back(p); }
static void addCoin(float x,float y){ coins.push_back({x,y,false}); coinsTotal++; }
static void addCoinRow(float x,float y,int n,float dx){ for(int i=0;i<n;i++)addCoin(x+i*dx,y); }
static void addCheck(float x,float y){ checks.push_back({x,y,false}); }
static void addSpikes(float x,float y,float w){ Hazard h{}; h.type=HZ_SPIKES; h.box={x,y,w,26}; hazards.push_back(h); }
static void addSaw(float x,float y,float r,float range,float speed,int axis){ Hazard h{}; h.type=HZ_SAW; h.box={x,y,0,0}; h.ax=x;h.ay=y;h.r=r;h.range=range;h.speed=speed;h.axis=axis; hazards.push_back(h); }
static void addLava(float x,float y,float w,float hh){ Hazard h{}; h.type=HZ_LAVA; h.box={x,y,w,hh}; hazards.push_back(h); }
static void addFirejet(float x,float y,float w,float hh,float ph){ Hazard h{}; h.type=HZ_FIREJET; h.box={x,y,w,hh}; h.timer=ph; hazards.push_back(h); }
static void addEnemy(EnemyType t,float x,float patrol,int facing){ Enemy e{}; e.type=t; const EArch&A=EARCH[t]; e.box={x,90,A.w,A.h}; e.hp=e.maxhp=A.hp; e.facing=facing; e.alive=true; e.ax=x; e.patrol=patrol; e.st=ES_PATROL; e.anim=AN_IDLE; e.sqx=e.sqy=1; e.misc=1.0f; if(t==EN_BOSS){isBossLevel=true;bossAlive=true;} enemies.push_back(e); }
static void addBreakable(float x,float y,float w,float h,int gold=4){ breakables.push_back({{x,y,w,h},true,gold}); }

static void clearLevel(){ plats.clear();coins.clear();checks.clear();hazards.clear();enemies.clear();projs.clear();parts.clear();floaters.clear();ambient.clear();breakables.clear(); coinsTotal=0; isBossLevel=false;bossAlive=false; }

static void buildL1(){ // Bamboo tutorial
    worldWidth=3200; addPlat(0,0,1920,80); addPlat(2080,0,1120,80); addSpikes(1920,0,160);
    addPlat(760,200,160,24,true); addCoinRow(770,250,3,50);
    addPlat(1120,300,150,24,true); addCoin(1180,350);
    addPlat(1500,180,140,24,false,false,120,0.9f,0);
    addCoinRow(360,150,3,50); addCoin(2200,150); addCoinRow(2400,140,4,45); addCoin(900,120); addCoin(1300,120); addCoin(2700,120);
    addBreakable(640,80,42,84,4); addBreakable(2500,80,42,84,5);   // shoot/slash for hidden gold
    addEnemy(EN_STALKER,980,160,-1); addEnemy(EN_STALKER,1560,140,1); addEnemy(EN_STALKER,2400,180,-1); addEnemy(EN_STALKER,3000,160,1);
    addCheck(1080,80); addCheck(2160,80);
    // --- extended section ---
    addSpikes(3200,0,160); addPlat(3360,0,1240,80);
    addPlat(3560,180,150,24,true); addPlat(3820,260,150,24,true); addCoinRow(3580,228,3,45); addCoinRow(3840,308,3,45);
    addEnemy(EN_STALKER,3500,150,-1); addEnemy(EN_STALKER,4150,150,1);
    addBreakable(4050,80,42,84,6); addCoinRow(3380,150,3,45);
    addCheck(3420,80);
    goalRect={4500,80,60,110}; worldWidth=4640; P.spawnX=120;P.spawnY=120;
}
static void buildL2(){ // Sunset: dash + movers + saw + archer
    worldWidth=4400; addPlat(0,0,800,80); addSpikes(800,0,300); addPlat(1100,0,760,80);  // 300px pit (<=L2 ceiling)
    addPlat(1300,220,140,24,false,false,120,1.0f,0); addPlat(1900,260,140,24,false,false,90,1.3f,1);
    addPlat(1860,0,2540,80); addSpikes(2300,0,200);
    addSaw(2400,150,34,90,2.0f,0); addSaw(3100,200,34,120,1.6f,1);
    addCoinRow(300,150,4,45); addCoinRow(1300,280,3,45); addCoinRow(2000,150,4,45); addCoinRow(3300,140,5,42); addCoinRow(900,200,3,40);
    addEnemy(EN_STALKER,600,120,1); addEnemy(EN_ARCHER,1500,0,-1); addEnemy(EN_STALKER,2000,150,1); addEnemy(EN_ARCHER,2800,0,-1); addEnemy(EN_STALKER,3400,180,-1); addEnemy(EN_STALKER,3800,150,1);
    addCheck(700,80); addCheck(1900,80); addCheck(3000,80);
    // --- extended section ---
    addSpikes(4400,0,200); addPlat(4600,0,1280,80);
    addSaw(4950,175,36,110,1.8f,0);
    addPlat(4800,230,150,24,false,false,130,1.1f,0); addCoinRow(4760,290,3,45);
    addEnemy(EN_ARCHER,4700,0,-1); addEnemy(EN_STALKER,5150,170,1); addEnemy(EN_DASHER,5450,0,-1);
    addBreakable(5050,80,42,84,6); addCoinRow(5250,150,4,42);
    addCheck(4660,80);
    goalRect={5800,80,60,110}; worldWidth=5940; P.spawnX=120;P.spawnY=120;
}
static void buildL3(){ // Moonlit: wall-jump, crusher(saw), crumble, dasher/mine/brute
    worldWidth=5200; addPlat(0,0,900,80); addSpikes(900,0,220); addPlat(1120,0,640,80);
    // spike pit crossed via reachable stepping platforms (all within single/double-jump reach)
    addSpikes(1760,0,340);
    addPlat(1820,160,150,24); addPlat(2010,250,150,24);   // step up & across
    addCoinRow(1840,210,3,40);
    addPlat(2100,0,900,80); addSpikes(2300,0,160);
    addPlat(3050,80,120,20,true,true); addPlat(3250,140,120,20,true,true); addPlat(3450,200,120,20,true,true);
    addPlat(3650,0,1550,80);
    addSaw(2600,160,38,100,1.8f,0); addSaw(4200,150,38,80,2.0f,1);
    addCoinRow(300,150,4,45); addCoinRow(2500,150,3,45); addCoinRow(3050,140,3,200); addCoinRow(4000,150,5,42); addCoinRow(1300,200,3,40);
    addBreakable(2750,80,42,84,6); addBreakable(4500,80,42,84,5);
    addEnemy(EN_DASHER,700,0,1); addEnemy(EN_MINE,1300,0,1); addEnemy(EN_MINE,1400,0,-1); addEnemy(EN_BRUTE,2400,0,-1); addEnemy(EN_DASHER,3800,0,-1); addEnemy(EN_MINE,4100,0,1); addEnemy(EN_STALKER,4600,150,-1);
    addCheck(800,80); addCheck(2150,80); addCheck(3700,80); addCheck(4500,80);
    // --- extended section ---
    addSpikes(5200,0,180); addPlat(5380,0,1380,80);
    addPlat(5560,150,120,24,true); addPlat(5780,150,120,24,true,true); addCoinRow(5580,200,3,40);
    addSaw(6050,175,38,90,2.0f,1);
    addEnemy(EN_MINE,5520,0,1); addEnemy(EN_BRUTE,5950,0,-1); addEnemy(EN_DASHER,6350,0,-1);
    addBreakable(6150,80,42,84,7); addCoinRow(6250,150,4,42);
    addCheck(5440,80);
    goalRect={6680,80,60,110}; worldWidth=6820; P.spawnX=120;P.spawnY=120;
}
static void buildL4(){ // Twilight: firejet, mixes
    worldWidth=6000; addPlat(0,0,1000,80); addSpikes(1000,0,200); addPlat(1200,0,1200,80);
    addFirejet(1500,80,40,150,0.0f); addFirejet(1900,80,40,150,1.4f);
    addPlat(2400,0,200,80); addSpikes(2600,0,280); addPlat(2880,0,1200,80);
    addSaw(3000,150,38,120,1.8f,0); addFirejet(3350,80,40,180,0.7f);
    // SHRINK tunnel: ceiling 32px above floor -> must press S to fit through
    addPlat(3640,112,300,220); addCoin(3700,96); addCoin(3790,96); addCoin(3880,96);
    addPlat(4080,0,400,80); addSpikes(4480,0,320); addPlat(4800,0,1200,80);
    addPlat(4200,240,140,24,false,false,140,1.2f,0);
    addCoinRow(300,150,5,42); addCoinRow(2900,150,5,42); addCoinRow(4900,150,6,40); addCoinRow(1300,200,4,40); addCoinRow(3600,220,3,45);
    addEnemy(EN_ARCHER,800,0,1); addEnemy(EN_BRUTE,1400,0,-1); addEnemy(EN_DASHER,2900,0,1); addEnemy(EN_MINE,3100,0,-1); addEnemy(EN_MINE,3200,0,1); addEnemy(EN_ARCHER,3700,0,-1); addEnemy(EN_BRUTE,5000,0,-1); addEnemy(EN_DASHER,5400,0,-1); addEnemy(EN_STALKER,5700,150,-1);
    addCheck(900,80); addCheck(2300,80); addCheck(2980,80); addCheck(4000,80); addCheck(4900,80);
    // --- extended section ---
    addSpikes(6000,0,200); addPlat(6200,0,1460,80);
    addFirejet(6420,80,40,170,0.3f); addFirejet(6820,80,40,170,1.5f);
    addPlat(6600,250,140,24,false,false,130,1.2f,0); addCoinRow(6560,310,3,45);
    addEnemy(EN_ARCHER,6320,0,1); addEnemy(EN_DASHER,6960,0,-1); addEnemy(EN_BRUTE,7350,0,-1);
    addBreakable(7150,80,42,84,8); addCoinRow(6480,150,4,42); addCoinRow(7050,150,4,42);
    addCheck(6260,80);
    goalRect={7580,80,60,110}; worldWidth=7720; P.spawnX=120;P.spawnY=120;
}
static void buildL5(){ // Volcanic: approach gauntlet -> boss arena
    worldWidth=2760;
    // --- approach gauntlet ---
    addPlat(0,0,860,80); addSpikes(860,0,160); addPlat(1020,0,420,80);
    addFirejet(480,80,40,150,0.5f);
    addEnemy(EN_MINE,300,0,1); addEnemy(EN_DASHER,680,0,-1); addEnemy(EN_BRUTE,1180,0,-1);
    addBreakable(980,80,42,84,8); addCoinRow(200,150,4,40); addCoinRow(1090,150,3,40);
    addCheck(1300,80);
    // --- boss arena (1440..2760) ---
    addPlat(1440,0,1320,80);
    addPlat(1560,260,200,24,true); addPlat(2460,260,200,24,true); addPlat(2000,360,240,24,true);
    addLava(1440,-40,1320,40);
    addCoinRow(1640,150,4,40); addCoinRow(2320,150,4,40); addCoinRow(2000,420,4,40);
    addEnemy(EN_MINE,1700,0,1); addEnemy(EN_DASHER,2250,0,-1);
    addEnemy(EN_BOSS,2300,0,-1);
    goalRect={0,0,0,0}; P.spawnX=120;P.spawnY=120;
}
static void loadLevel(int idx){
    if(idx<0||idx>4) idx=0;                       // guard OOB (e.g. CLI 'play 9')
    curLevel=idx; clearLevel();
    int themeByLevel[5]={TH_BAMBOO,TH_SUNSET,TH_MOONLIT,TH_TWILIGHT,TH_VOLCANIC}; curTheme=themeByLevel[idx];
    if(idx==0)buildL1(); else if(idx==1)buildL2(); else if(idx==2)buildL3(); else if(idx==3)buildL4(); else buildL5();
    coinsGot=0; kills=0; killsTotal=0; for(auto&e:enemies)killsTotal++; levelTime=0; energy=0; streak=0; streakT=0;
    playerSpawn(); camX=clampf(P.x-WINW*0.42f,0,worldWidth-WINW); camY=0;
}
static void resetRun(){ score=0; lives=3; loadLevel(0); }

// ===========================================================================
//  UPDATE + STATE MACHINE
// ===========================================================================
static void updatePlatforms(){ for(auto&p:plats){ float ox=p.box.x,oy=p.box.y;
    if(p.speed!=0){ p.box.x=(p.axis==0?p.ax+sinf(gTime*p.speed)*p.range:p.ax); p.box.y=(p.axis==1?p.ay+sinf(gTime*p.speed)*p.range:p.ay); }
    p.dx=p.box.x-ox; p.dy=p.box.y-oy;
    if(p.crumble){ if(p.cstate==1){p.ct-=DT; if(p.ct<=0){p.cstate=2;p.ct=CRUMBLE_RESPAWN;}} else if(p.cstate==2){p.ct-=DT; if(p.ct<=0)p.cstate=0;} }
 } }
static void updateParticles(){ for(size_t i=0;i<parts.size();){ Particle&p=parts[i]; p.life-=DT; if(p.life<=0){parts[i]=parts.back();parts.pop_back();continue;} p.vy-=600*DT;p.x+=p.vx*DT;p.y+=p.vy*DT;i++; }
    for(size_t i=0;i<floaters.size();){ Floater&f=floaters[i]; f.life-=DT; f.y+=f.vy*DT; if(f.life<=0){floaters[i]=floaters.back();floaters.pop_back();}else i++; } }

static void update(){
    gTime+=DT;
    // timescale ease back
    timeScale=lerpf(timeScale,1.f,0.06f);
    if(hitstop>0){ hitstop-=DT; glutPostRedisplay(); return; }
    if(flashWhite>0)flashWhite-=DT*2;
    if(shake>0.001f)shake*=0.85f; else shake=0;

    if(gameState==ST_MENU){ updateAmbient(THEMES[0]); curTheme=0; if(key[13]&&!pEnter){resetRun();gameState=ST_INTRO;stateT=0;} pEnter=key[13]; glutPostRedisplay(); return; }
    if(gameState==ST_INTRO){ stateT+=DT; updateAmbient(THEMES[curTheme]); if((key[13]&&!pEnter)||stateT>2.2f){gameState=ST_PLAY;stateT=0;} pEnter=key[13]; glutPostRedisplay(); return; }
    if(gameState==ST_CLEAR||gameState==ST_OVER||gameState==ST_COMPLETE){ stateT+=DT; updateAmbient(THEMES[curTheme]); updateParticles();
        if(gameState==ST_CLEAR){
            if(stateT<CLIPS[AN_FLIP].dur){ if(P.anim!=AN_FLIP)setAnim(AN_FLIP); P.animT=clampf(P.animT+DT,0,CLIPS[AN_FLIP].dur); }
            else { if(P.anim!=AN_VICTORY)setAnim(AN_VICTORY); P.animT+=DT; P.animT=fmodf(P.animT,CLIPS[AN_VICTORY].dur); }
            if(stateT<1.6f && fmodf(stateT,0.05f)<DT) spawnP(P.x+P.w*0.5f+frand(-30,30),P.y+frand(0,50),frand(-20,20),frand(40,120),frand(0.6f,1.2f),frand(2,4),1,.82f,.30f,1,true);
            if(key[13]&&!pEnter&&stateT>0.8f){ if(curLevel<4){loadLevel(curLevel+1);gameState=ST_INTRO;stateT=0;} else {gameState=ST_COMPLETE;stateT=0;} } }
        else if(gameState==ST_OVER){ if(key[13]&&!pEnter&&stateT>0.6f){gameState=ST_MENU;stateT=0;} }
        else { if(P.anim!=AN_VICTORY)setAnim(AN_VICTORY); P.animT+=DT;P.animT=fmodf(P.animT,CLIPS[AN_VICTORY].dur);
            if(fmodf(stateT,0.04f)<DT && stateT<3.0f){ float cx2=camX+frand(0,WINW); spawnP(cx2,camY+WINH+10,frand(-30,30),-frand(60,140),frand(1.5f,3.0f),frand(3,5), (frand(0,1)<0.5f?1.f:1.f), (frand(0,1)<0.5f?0.3f:0.85f), (frand(0,1)<0.5f?0.2f:0.3f),1,true); }
            if(key[13]&&!pEnter&&stateT>0.8f){gameState=ST_MENU;stateT=0;} }
        pEnter=key[13]; glutPostRedisplay(); return; }

    // ST_PLAY
    static bool pR=false; if(key['r']&&!pR){ loadLevel(curLevel); } pR=key['r'];
    // slow-motion: step the sim fewer frames when timeScale<1 (death/kill cinematics)
    static float simAcc=0; simAcc+=clampf(timeScale,0.05f,1.f); if(simAcc<1.f){ glutPostRedisplay(); return; } simAcc-=1.f;

    levelTime+=DT;
    buildColliders();
    updatePlatforms();
    buildColliders();
    updatePlayer();
    updateEnemies();
    updateProjectiles();
    updateHazards();
    updatePickups();
    updateParticles();
    updateAmbient(THEMES[curTheme]);
    // camera
    float tx=P.x+P.w*0.5f-WINW*0.42f; tx=clampf(tx,0,(worldWidth>WINW?worldWidth-WINW:0));
    float ty=P.y+P.h*0.5f-WINH*0.55f; ty=clampf(ty,0,400);
    camX=lerpf(camX,tx,0.12f); camY=lerpf(camY,ty,0.12f);
    glutPostRedisplay();
}

// ===========================================================================
//  HUD + OVERLAYS
// ===========================================================================
static void drawHUD(){
    setProj(0,0); char b[128];
    // HP bar
    float hp=clampf(P.hp/PLAYER_MAXHP,0,1);
    glColor4f(0,0,0,0.5f); fillRect(20,WINH-44,260,22);
    glColor3f(0.25f,0.05f,0.05f); fillRect(24,WINH-40,252,14);
    glColor3f(0.9f,0.2f,0.2f); fillRect(24,WINH-40,252*hp,14);
    glColor3f(1,1,1); text(28,WINH-37,GLUT_BITMAP_8_BY_13,"HP");
    // chakra / energy bar under HP -> ultimate
    float ef=energy/ENERGY_MAX; bool full=energy>=ENERGY_MAX;
    glColor4f(0,0,0,0.5f); fillRect(20,WINH-60,260,12);
    glColor3f(0.12f,0.14f,0.28f); fillRect(24,WINH-58,252,8);
    if(full)glColor3f(0.55f+0.35f*sinf(gTime*9),0.9f,1.0f); else glColor3f(0.35f,0.65f,1.0f);
    fillRect(24,WINH-58,252*ef,8);
    glColor3f(.8f,.9f,1); text(28,WINH-57,GLUT_BITMAP_8_BY_13, full?"[U] ULTIMATE!":"CHAKRA");
    // coins/score/kills
    glColor3f(1,.85f,.4f); snprintf(b,128,"GOLD %d/%d",coinsGot,coinsTotal); text(300,WINH-38,GLUT_BITMAP_9_BY_15,b);
    glColor3f(.8f,.9f,1); snprintf(b,128,"SCORE %d",score); text(470,WINH-38,GLUT_BITMAP_9_BY_15,b);
    // lives as small ninja-head pips
    glColor3f(1,.6f,.6f); text(650,WINH-38,GLUT_BITMAP_9_BY_15,"LIVES"); for(int i=0;i<lives;i++){ glColor3f(.9f,.3f,.3f); disc(720+i*20,WINH-33,7,12); }
    if(streak>=2){ glColor3f(1,.85f,.35f); snprintf(b,128,"COMBO x%d",1+streak/3); text(470,WINH-58,GLUT_BITMAP_8_BY_13,b); }
    glColor3f(.7f,.95f,1); snprintf(b,128,"LEVEL %d/5  %s",curLevel+1,THEMES[curTheme].name); text(20,WINH-78,GLUT_BITMAP_8_BY_13,b);
    // ability pips: shuriken + dash readiness
    const float*acc=THEMES[curTheme].accent;
    glColor3f(P.fireCD<=0?acc[0]:0.35f, P.fireCD<=0?acc[1]:0.35f, P.fireCD<=0?acc[2]:0.4f); disc(840,WINH-33,7,10); text(852,WINH-38,GLUT_BITMAP_8_BY_13,"SHURIKEN");
    glColor3f((P.dashCD<=0&&P.dashLeft>0)?0.5f:0.3f,(P.dashCD<=0&&P.dashLeft>0)?0.9f:0.3f,1.f); disc(960,WINH-33,7,10); text(972,WINH-38,GLUT_BITMAP_8_BY_13,"DASH");
    glColor4f(1,1,1,0.6f); text(20,14,GLUT_BITMAP_8_BY_13,"A/D move  W jump(x2)  L dash  J shuriken  K slash  U ultimate  S shrink  Down+W drop  R restart");
}
static void dimScreen(float a){ setProj(0,0); glColor4f(0,0,0,a); fillRect(0,0,WINW,WINH); }
static void drawStars(float cx,float y,int n){ for(int i=0;i<3;i++){ float x=cx-60+i*60; if(i<n)glColor3f(1,.85f,.3f);else glColor4f(1,1,1,0.2f);
    glBegin(GL_TRIANGLE_FAN);glVertex2f(x,y); for(int k=0;k<=10;k++){float a=M_PI*k/5; float rr=(k%2==0)?16:7; glVertex2f(x+cosf(a+1.57f)*rr,y+sinf(a+1.57f)*rr);} glEnd(); } }

static void display(){
    glClear(GL_COLOR_BUFFER_BIT);
    float scx=camX,scy=camY; if(shake>0.01f){camX+=frand(-1,1)*shake*12;camY+=frand(-1,1)*shake*12;}
    drawBackground();
    if(gameState!=ST_MENU){ drawWorld(); drawBreakables(); drawHazards(); drawCoins(); drawChecks(); drawGoal(); drawEnemies(); drawProjectiles(); drawPlayer(); }
    // particles
    setProj(camX,camY); for(auto&p:parts){ float a=p.a*clampf(p.life/p.max,0,1); if(p.additive)glowDisc(p.x,p.y,p.size*2.5f,p.r,p.g,p.b,a*0.7f); else {glColor4f(p.r,p.g,p.b,a);fillRect(p.x-p.size*0.5f,p.y-p.size*0.5f,p.size,p.size);} }
    drawFloaters();
    drawForeground(); drawAmbient();
    camX=scx;camY=scy;

    if(flashWhite>0){ setProj(0,0); glColor4f(1,1,1,clampf(flashWhite,0,1)*0.6f); fillRect(0,0,WINW,WINH); }

    // menu / overlays
    if(gameState==ST_MENU){ setProj(0,0); dimScreen(0.25f);
        glColor3f(.9f,.97f,1); textC(WINW*0.5f,WINH*0.62f,GLUT_BITMAP_TIMES_ROMAN_24,"S H A D O W   N I N J A");
        glColor4f(1,1,1,0.7f+0.3f*sinf(gTime*3)); textC(WINW*0.5f,WINH*0.40f,GLUT_BITMAP_9_BY_15,"Press [ENTER] to begin");
        glColor4f(.8f,.85f,1,0.6f); textC(WINW*0.5f,WINH*0.32f,GLUT_BITMAP_8_BY_13,"A/D move   W jump (x2)   L dash   J shuriken   K slash"); }
    else { drawHUD();
        if(gameState==ST_INTRO){ setProj(0,0); dimScreen(0.45f); char b[64]; snprintf(b,64,"LEVEL %d",curLevel+1);
            glColor3f(.9f,.97f,1); textC(WINW*0.5f,WINH*0.56f,GLUT_BITMAP_TIMES_ROMAN_24,b);
            glColor3f(.7f,.9f,1); textC(WINW*0.5f,WINH*0.48f,GLUT_BITMAP_9_BY_15,THEMES[curTheme].name);
            glColor4f(1,1,1,0.6f); textC(WINW*0.5f,WINH*0.40f,GLUT_BITMAP_8_BY_13,"Press [ENTER]"); }
        else if(gameState==ST_CLEAR){ setProj(0,0); dimScreen(0.55f); char b[80];
            float slide=smooth(clampf(stateT/0.4f,0,1)); float ph=250,pw=470,px=WINW*0.5f-pw*0.5f;
            float py=lerpf(-300.f, WINH*0.30f, slide);
            glColor4f(0,0,0,0.78f); fillRect(px,py,pw,ph);
            glColor3f(1,.82f,.35f); glBegin(GL_LINE_LOOP);glVertex2f(px,py);glVertex2f(px+pw,py);glVertex2f(px+pw,py+ph);glVertex2f(px,py+ph);glEnd();
            glColor3f(.75f,1,.85f); textC(WINW*0.5f,py+ph-42,GLUT_BITMAP_TIMES_ROMAN_24,"LEVEL CLEARED!");
            snprintf(b,80,"TIME   %02d:%02d",(int)levelTime/60,(int)levelTime%60); glColor3f(.85f,.9f,1); textC(WINW*0.5f,py+ph-86,GLUT_BITMAP_9_BY_15,b);
            int gshow=(int)clampf(floorf((stateT-0.4f)/0.6f*coinsGot+0.001f),0,(float)coinsGot);
            snprintf(b,80,"GOLD  %d / %d       KILLS  %d / %d",gshow,coinsTotal,kills,killsTotal); glColor3f(1,.9f,.55f); textC(WINW*0.5f,py+ph-116,GLUT_BITMAP_9_BY_15,b);
            int st=1; if(coinsTotal>0&&coinsGot>=coinsTotal*0.8f)st=2; if(st==2&&lives==3)st=3;
            for(int i=0;i<3;i++){ float appear=1.0f+i*0.25f; bool lit=(i<st)&&stateT>appear;
                float pop=clampf((stateT-appear)/0.12f,0,1); float scl=lit?(1.0f+0.28f*sinf(pop*(float)M_PI)):1.f; float sx=WINW*0.5f-70+i*70,sy=py+62;
                if(lit)glColor3f(1,.85f,.3f); else glColor4f(1,1,1,0.18f);
                glPushMatrix();glTranslatef(sx,sy,0);glScalef(scl,scl,1); glBegin(GL_TRIANGLE_FAN);glVertex2f(0,0);for(int k=0;k<=10;k++){float a=(float)M_PI*k/5;float rr=(k%2==0)?16:7;glVertex2f(cosf(a+1.57f)*rr,sinf(a+1.57f)*rr);}glEnd(); glPopMatrix(); }
            glColor4f(.7f,.85f,1,0.55f+0.45f*sinf(gTime*6)); textC(WINW*0.5f,py+20,GLUT_BITMAP_9_BY_15,"Press [ENTER]  ->  Next"); }
        else if(gameState==ST_OVER){ setProj(0,0); dimScreen(0.65f);
            glColor3f(1,.4f,.4f); textC(WINW*0.5f,WINH*0.58f,GLUT_BITMAP_TIMES_ROMAN_24,"YOU FELL...");
            glColor4f(1,1,1,0.7f); textC(WINW*0.5f,WINH*0.46f,GLUT_BITMAP_9_BY_15,"Press [ENTER] to return to title"); }
        else if(gameState==ST_COMPLETE){ setProj(0,0);
            float vig=clampf(stateT/1.2f,0,1); dimScreen(0.38f+0.30f*vig);
            float drop=smooth(clampf(stateT/0.55f,0,1)); float ty=lerpf(WINH+70.f,WINH*0.64f,drop);
            glColor3f(1,.85f,.4f); textC(WINW*0.5f,ty,GLUT_BITMAP_TIMES_ROMAN_24,"V  I  C  T  O  R  Y");
            if(stateT>0.7f){ char b[80];
                glColor3f(.95f,.85f,.7f); textC(WINW*0.5f,WINH*0.54f,GLUT_BITMAP_9_BY_15,"The Crimson Shogun has fallen.");
                snprintf(b,80,"FINAL SCORE  %d      LEVELS  5/5",score); glColor3f(.9f,.95f,1); textC(WINW*0.5f,WINH*0.46f,GLUT_BITMAP_9_BY_15,b);
                glColor4f(1,1,1,0.6f+0.4f*sinf(gTime*4)); textC(WINW*0.5f,WINH*0.37f,GLUT_BITMAP_8_BY_13,"Press [ENTER] for title"); } }
    }
    glutSwapBuffers();
}
static void reshape(int w,int h){ glViewport(0,0,w,h); }
static void timer(int){ update(); glutTimerFunc(16,timer,0); }
static void kd(unsigned char k,int,int){ if(k<256)key[tolower(k)]=true; }
static void ku(unsigned char k,int,int){ if(k<256)key[tolower(k)]=false; }
static void sd(int k,int,int){ if(k<512)spec[k]=true; }
static void su(int k,int,int){ if(k<512)spec[k]=false; }
static void mouse(int button,int state,int x,int y){ if(button==GLUT_LEFT_BUTTON&&state==GLUT_DOWN&&gameState==ST_PLAY){ float wx=camX+x; if(wx<P.x+P.w*0.5f)P.facing=-1; else P.facing=1; throwShuriken(); } }

int main(int argc,char**argv){
    glutInit(&argc,argv); glutInitDisplayMode(GLUT_DOUBLE|GLUT_RGBA|GLUT_MULTISAMPLE);
    glutInitWindowSize(WINW,WINH); glutCreateWindow("Shadow Ninja");
    glEnable(GL_BLEND); glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_MULTISAMPLE); glEnable(GL_LINE_SMOOTH); glHint(GL_LINE_SMOOTH_HINT,GL_NICEST); glLineWidth(2.0f);
    glDisable(GL_TEXTURE_2D); glDisable(GL_DEPTH_TEST);
    P.spawnX=120; P.spawnY=120; playerSpawn();
    loadLevel(0); gameState=ST_MENU;
    glutDisplayFunc(display); glutReshapeFunc(reshape); glutKeyboardFunc(kd); glutKeyboardUpFunc(ku); glutSpecialFunc(sd); glutSpecialUpFunc(su); glutMouseFunc(mouse);
    glutIgnoreKeyRepeat(1); glutTimerFunc(16,timer,0);
#ifdef GLUT_ACTION_ON_WINDOW_CLOSE
    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE,GLUT_ACTION_GLUTMAINLOOP_RETURNS);
#endif
    glutMainLoop(); return 0;
}
