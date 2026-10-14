#include <stdio.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <stdlib.h>
#include <dlfcn.h>
#include "graphics.h"
#include "theme_packages.h"
#include "assassins_creed_image.h"

#define W 1920
#define H 1080

std::stringstream debugLogStream;
static int frameID = 0;

typedef int32_t BgftTaskId;
typedef struct { void *heap; size_t heapSize; } BgftInitParams;
typedef struct {
    int32_t userId, entitlementType;
    const char *id, *contentUrl, *contentExUrl, *contentName, *iconPath, *skuId;
    int32_t option;
    const char *playgoScenarioId, *releaseDate, *packageType, *packageSubType;
    uint32_t packageSize;
} BgftDownloadParam;
typedef int32_t (*BgftInitFn)(BgftInitParams *);
typedef int32_t (*BgftRegisterFn)(BgftDownloadParam *, BgftTaskId *);
typedef int32_t (*BgftStartFn)(BgftTaskId);
typedef int32_t (*BgftStopFn)(BgftTaskId);
typedef int32_t (*BgftUnregisterFn)(BgftTaskId);
typedef struct {
    uint32_t bits; int32_t errorResult; uint32_t length; uint32_t transferred;
    uint32_t lengthTotal; uint32_t transferredTotal; uint32_t numIndex; uint32_t numTotal;
    uint32_t restSec; uint32_t restSecTotal; int32_t preparingPercent; int32_t localCopyPercent;
} BgftTaskProgress;
typedef int32_t (*BgftProgressFn)(BgftTaskId, BgftTaskProgress *);
static BgftInitFn bgftInit = NULL;
static BgftRegisterFn bgftRegister = NULL;
static BgftStartFn bgftStart = NULL;
static BgftStopFn bgftStop = NULL;
static BgftUnregisterFn bgftUnregister = NULL;
static BgftProgressFn bgftProgress = NULL;
static BgftTaskId lastTask = -1;
static int lastPackageIndex = -1;
static void *bgftLib = NULL;
static void *bgftHeap = NULL;
static int bgftReady = 0;
static char installStatus[96] = "LISTO PARA INSTALAR";
static int selectedPackage = 0;
static bool autoInstallSecond = false;

struct ThemePackage { const char *label; const char *url; const char *contentId; uint32_t size; };
static ThemePackage packages[2] = {
    {"PAQUETE 1", MARKPS4_PKG1_URL, MARKPS4_PKG1_CONTENT_ID, MARKPS4_PKG1_SIZE},
    {"PAQUETE 2", MARKPS4_PKG2_URL, MARKPS4_PKG2_CONTENT_ID, MARKPS4_PKG2_SIZE}
};

static int startPackageInstall(int userId, int index) {
    ThemePackage &pkg = packages[index];
    if (!pkg.contentId[0] || pkg.size == 0) {
        snprintf(installStatus, sizeof(installStatus), "METADATOS PKG NO DISPONIBLES");
        return -1;
    }
    if (!bgftReady) {
        typedef int (*LoadModuleFn)(const char *, size_t, const void *, uint32_t, void *, int *);
        LoadModuleFn loadModule = (LoadModuleFn)dlsym(RTLD_DEFAULT, "sceKernelLoadStartModule");
        if (loadModule) {
            int moduleResult = 0;
            (void)loadModule("/system/common/lib/libSceBgft.sprx", 0, NULL, 0, NULL, &moduleResult);
        }
        const char *libs[] = {"/system/common/lib/libSceBgft.sprx", NULL};
        for (int i=0; libs[i]; ++i) {
            bgftLib = dlopen(libs[i], RTLD_LAZY | RTLD_GLOBAL);
            if (bgftLib) break;
        }
        if (bgftLib) {
            bgftInit = (BgftInitFn)dlsym(bgftLib, "sceBgftServiceIntInit");
            bgftRegister = (BgftRegisterFn)dlsym(bgftLib, "sceBgftServiceIntDebugDownloadRegisterPkg");
            bgftStart = (BgftStartFn)dlsym(bgftLib, "sceBgftServiceIntDownloadStartTask");
            bgftStop = (BgftStopFn)dlsym(bgftLib, "sceBgftServiceIntDownloadStopTask");
            bgftUnregister = (BgftUnregisterFn)dlsym(bgftLib, "sceBgftServiceIntDownloadUnregisterTask");
            bgftProgress = (BgftProgressFn)dlsym(bgftLib, "sceBgftServiceIntDownloadGetProgress");
        }
        if (!bgftInit || !bgftRegister || !bgftStart || !bgftProgress || !bgftStop || !bgftUnregister) {
            snprintf(installStatus, sizeof(installStatus), "BGFT NO ACCESIBLE DESDE ESTA APP");
            return -2;
        }
        bgftHeap = malloc(1024 * 1024);
        if (!bgftHeap) {
            snprintf(installStatus, sizeof(installStatus), "SIN MEMORIA PARA INSTALADOR");
            return -3;
        }
        memset(bgftHeap, 0, 1024 * 1024);
        BgftInitParams init = { bgftHeap, 1024 * 1024 };
        int rc = bgftInit(&init);
        /* BGFT can already be initialized by the system or another trusted component. */
        if (rc != 0 && (uint32_t)rc != 0x80990001u) {
            snprintf(installStatus, sizeof(installStatus), "BGFT NO INICIO");
            return rc;
        }
        bgftReady = 1;
    }

    /* The two Assassin's Creed packages share one content ID. Never replace a task while it is
       still transferring; after the user confirms the first install finished, release its BGFT
       ticket so the second package can be registered for the same title. */
    if (lastTask >= 0 && lastPackageIndex != index) {
        BgftTaskProgress progress;
        memset(&progress, 0, sizeof(progress));
        int progressRc = bgftProgress(lastTask, &progress);
        if (progressRc == 0 && progress.lengthTotal > 0 &&
            progress.transferredTotal < progress.lengthTotal) {
            snprintf(installStatus, sizeof(installStatus), "ESPERA A QUE TERMINE EL PAQUETE ANTERIOR");
            return -4;
        }
        /* Only reach here when the previous transfer reports all bytes or BGFT no longer reports it.
           The owner should press X for package 2 only after the PS4 confirms package 1 installed. */
        (void)bgftStop(lastTask);
        (void)bgftUnregister(lastTask);
        lastTask = -1;
        lastPackageIndex = -1;
    }

    BgftDownloadParam p;
    memset(&p, 0, sizeof(p));
    p.userId = userId;
    p.entitlementType = 5;
    p.id = pkg.contentId;
    p.contentUrl = pkg.url;
    p.contentName = index == 0 ? "MARKPS4 THEMES AC PAQUETE 1" : "MARKPS4 THEMES AC PAQUETE 2";
    p.iconPath = "";
    p.playgoScenarioId = "0";
    p.option = 0x10000; /* Disable CDN query parameters for a non-Store PKG. */
    p.packageType = "PS4GD";
    p.packageSubType = "";
    p.packageSize = pkg.size;
    BgftTaskId task = -1;
    int rc = bgftRegister(&p, &task);
    if (rc != 0) {
        snprintf(installStatus, sizeof(installStatus), "PS4 RECHAZO EL PAQUETE");
        return rc;
    }
    rc = bgftStart(task);
    if (rc != 0) {
        snprintf(installStatus, sizeof(installStatus), "NO SE PUDO INICIAR DESCARGA");
        return rc;
    }
    lastTask = task;
    lastPackageIndex = index;
    snprintf(installStatus, sizeof(installStatus), "ENVIADO A DESCARGAS DEL PS4");
    return 0;
}


/* One catalog button represents the complete Assassin's Creed theme (both linked PKGs). */
static int startThemeInstall(int userId) {
    if (autoInstallSecond) {
        snprintf(installStatus, sizeof(installStatus), "INSTALACION DEL TEMA EN CURSO");
        return -1;
    }
    int rc = startPackageInstall(userId, 0);
    if (rc == 0) {
        autoInstallSecond = true;
        snprintf(installStatus, sizeof(installStatus), "PARTE 1 EN DESCARGAS; PARTE 2 AUTOMATICA");
    }
    return rc;
}

static void advanceQueuedInstall(int userId) {
    if (!autoInstallSecond || lastTask < 0 || lastPackageIndex != 0 || !bgftProgress) return;
    BgftTaskProgress progress;
    memset(&progress, 0, sizeof(progress));
    int rc = bgftProgress(lastTask, &progress);
    if (rc == 0 && progress.lengthTotal > 0 &&
        progress.transferredTotal >= progress.lengthTotal) {
        autoInstallSecond = false;
        int nextRc = startPackageInstall(userId, 1);
        if (nextRc == 0) {
            snprintf(installStatus, sizeof(installStatus), "PARTE 2 ENVIADA; TEMA COMPLETO");
        } else {
            snprintf(installStatus, sizeof(installStatus), "PARTE 1 LISTA; ERROR AL ENVIAR PARTE 2");
        }
    }
}

static const Color BG       = { 8, 11, 18 };       // #080b12
static const Color PANEL    = { 16, 23, 35 };      // #101723
static const Color PANEL2   = { 23, 34, 52 };     // #172234
static const Color BLUE     = { 0, 186, 255 };    // #00baff
static const Color BLUE_DARK= { 8, 124, 255 };    // #087cff
static const Color WHITE    = { 244, 248, 255 };  // #f4f8ff
static const Color MUTED    = { 141, 155, 176 };  // #8d9bb0
static const Color BLACK    = { 0, 17, 27 };

static const uint8_t *glyph(char c) {
    static const uint8_t blank[7] = {0,0,0,0,0,0,0};
    #define G(a,b,c,d,e,f,g) { static const uint8_t x[7]={a,b,c,d,e,f,g}; return x; }
    switch (c) {
        case 'A': G(14,17,17,31,17,17,17);
        case 'B': G(30,17,17,30,17,17,30);
        case 'C': G(14,17,16,16,16,17,14);
        case 'D': G(30,17,17,17,17,17,30);
        case 'E': G(31,16,16,30,16,16,31);
        case 'F': G(31,16,16,30,16,16,16);
        case 'G': G(14,17,16,23,17,17,15);
        case 'H': G(17,17,17,31,17,17,17);
        case 'I': G(14,4,4,4,4,4,14);
        case 'J': G(7,2,2,2,18,18,12);
        case 'K': G(17,18,20,24,20,18,17);
        case 'L': G(16,16,16,16,16,16,31);
        case 'M': G(17,27,21,21,17,17,17);
        case 'N': G(17,25,25,21,19,19,17);
        case 'O': G(14,17,17,17,17,17,14);
        case 'P': G(30,17,17,30,16,16,16);
        case 'Q': G(14,17,17,17,21,18,13);
        case 'R': G(30,17,17,30,20,18,17);
        case 'S': G(15,16,16,14,1,1,30);
        case 'T': G(31,4,4,4,4,4,4);
        case 'U': G(17,17,17,17,17,17,14);
        case 'V': G(17,17,17,17,17,10,4);
        case 'W': G(17,17,17,21,21,21,10);
        case 'X': G(17,17,10,4,10,17,17);
        case 'Y': G(17,17,10,4,4,4,4);
        case 'Z': G(31,1,2,4,8,16,31);
        case '0': G(14,17,19,21,25,17,14);
        case '1': G(4,12,4,4,4,4,14);
        case '2': G(14,17,1,2,4,8,31);
        case '3': G(30,1,1,14,1,1,30);
        case '4': G(2,6,10,18,31,2,2);
        case '5': G(31,16,16,30,1,1,30);
        case '6': G(14,16,16,30,17,17,14);
        case '7': G(31,1,2,4,8,8,8);
        case '8': G(14,17,17,14,17,17,14);
        case '9': G(14,17,17,15,1,1,14);
        case ':': G(0,4,4,0,4,4,0);
        case '.': G(0,0,0,0,0,12,12);
        case '-': G(0,0,0,31,0,0,0);
        case '/': G(1,2,2,4,8,8,16);
        case '+': G(0,4,4,31,4,4,0);
        case '!': G(4,4,4,4,4,0,4);
        case ' ': return blank;
        default: return blank;
    }
    #undef G
}

static void text(Scene2D *s, const char *str, int x, int y, int scale, Color color) {
    for (size_t i=0; str[i]; ++i) {
        const uint8_t *rows = glyph(str[i]);
        for (int ry=0; ry<7; ++ry) for (int rx=0; rx<5; ++rx) {
            if (rows[ry] & (1u << (4-rx)))
                s->DrawRectangle(x + (int)i*scale*6 + rx*scale, y + ry*scale, scale, scale, color);
        }
    }
}

static void panel(Scene2D *s, int x, int y, int w, int h, Color c) {
    s->DrawRectangle(x,y,w,h,c);
}
static Color shade(Color c, int d) {
    Color o;
    o.r = (uint8_t)(c.r > d ? c.r-d : 0);
    o.g = (uint8_t)(c.g > d ? c.g-d : 0);
    o.b = (uint8_t)(c.b > d ? c.b-d : 0);
    return o;
}
/* Rounded-look panels are rasterized from horizontal slices so no external graphics
   dependency is required. Soft layered shadows provide a restrained 3D lift. */
static void roundPanel(Scene2D *s, int x, int y, int w, int h, int r, Color c) {
    if (w < 2 || h < 2) return;
    if (r < 1) { panel(s,x,y,w,h,c); return; }
    if (r > h/2) r = h/2;
    if (r > w/2) r = w/2;
    for (int yy=0; yy<h; ++yy) {
        int inset=0;
        if (yy<r) {
            int dy=r-yy;
            inset=r-(int)sqrt((double)(r*r-dy*dy));
        } else if (yy>=h-r) {
            int dy=yy-(h-r-1);
            inset=r-(int)sqrt((double)(r*r-dy*dy));
        }
        if (w-2*inset>0) panel(s,x+inset,y+yy,w-2*inset,1,c);
    }
}
static void liftedPanel(Scene2D *s, int x, int y, int w, int h, Color c, bool active) {
    roundPanel(s,x+2,y+9,w,h,18,{2,4,9});
    roundPanel(s,x,y+4,w,h,18,{7,12,21});
    roundPanel(s,x,y,w,h,18,c);
    roundPanel(s,x+2,y+2,w-4,2,8,active?BLUE:PANEL2);
    if (active) {
        roundPanel(s,x+5,y+7,4,h-14,2,BLUE);
        roundPanel(s,x+w-9,y+12,3,h-24,1,{0,91,151});
    }
}
static void render(Scene2D *s, int selected, int page) {
    const int t=frameID;
    const int pulse=(t/3)%28;
    (void)selected;
    s->FrameBufferClear();
    s->FrameBufferFill(BG);

    // Subtle animated ambient bands, kept inexpensive for the PS4.
    for (int i=0;i<7;i++) {
        int yy=(i*177 + (t/5)%177)%1080;
        panel(s,315,yy,1605,1,{10,18,30});
    }
    panel(s,0,0,315,H,{11,17,28});
    panel(s,0,0,5,H,BLUE);
    panel(s,314,0,2,H,{26,50,72});

    // Brand block with a dimensional emblem.
    roundPanel(s,27,32,82,82,18,{0,4,12});
    roundPanel(s,31,28,82,82,18,{11,29,47});
    roundPanel(s,31,28,82,4,2,BLUE);
    roundPanel(s,31,106,82,3,2,BLUE_DARK);
    text(s,"M",52,52,5,WHITE);
    text(s,"MarkPS4",125,42,3,WHITE);
    text(s,"THEMES STORE",126,76,2,BLUE);
    roundPanel(s,30,130,250,2,1,PANEL2);

    // Navigation cards.
    liftedPanel(s,24,158,266,72,PANEL2,page==0);
    text(s,"01",42,184,2,page==0?BLUE:MUTED);
    text(s,"TEMAS",82,183,3,page==0?WHITE:MUTED);
    liftedPanel(s,24,249,266,72,PANEL,page==1);
    text(s,"02",42,275,2,page==1?BLUE:MUTED);
    text(s,"INSTALADOS",82,274,2,page==1?WHITE:MUTED);
    liftedPanel(s,24,340,266,72,PANEL,page==2);
    text(s,"03",42,366,2,page==2?BLUE:MUTED);
    text(s,"AJUSTES",82,365,3,page==2?WHITE:MUTED);

    roundPanel(s,30,435,250,2,1,PANEL2);
    text(s,"MARKPS4 THEMES",37,462,2,BLUE);
    text(s,"EXPERIENCIA NATIVA",37,490,2,MUTED);
    roundPanel(s,30,984,248,44,12,{17,27,42});
    text(s,"L1 / R1  SECCIONES",47,1000,2,MUTED);

    // Header with accent and status chip.
    text(s,page==0?"CATALOGO DE TEMAS":(page==1?"TEMAS INSTALADOS":"AJUSTES"),360,49,4,WHITE);
    text(s,page==0?"DESCUBRE TU PROXIMO TEMA":"GESTION Y PERSONALIZACION",362,91,2,MUTED);
    liftedPanel(s,1518,43,326,52,{14,25,39},false);
    roundPanel(s,1532,57,7,24,3,BLUE);
    text(s,"MARKPS4  /  PS4",1554,62,2,WHITE);
    roundPanel(s,360,125,1484,2,1,PANEL2);

    if (page==0) {
        const int cardW=350, cardH=470, cardY=153;
        const int xs[4]={360,733,1106,1479};
        for (int i=0;i<4;i++) {
            int x=xs[i];
            bool active=(i==0);
            int bob=active ? (pulse<14 ? pulse/7 : (27-pulse)/7) : 0;
            int y=cardY-bob;
            liftedPanel(s,x,y,cardW,cardH,active?Color{17,31,48}:PANEL,active);
            if (active) {
                // Framed artwork with depth and a moving highlight.
                roundPanel(s,x+17,y+17,cardW-34,184,13,{2,6,13});
                roundPanel(s,x+21,y+21,cardW-42,176,10,{8,17,28});
                s->DrawRGBImage(x+23,y+23,cardW-46,172,kAssassinsCreedRGB,240,165);
                int shine=(t*5)%(cardW-70);
                panel(s,x+25+shine,y+24,2,169,{38,74,100});
                text(s,"ASSASSIN'S",x+24,y+225,3,WHITE);
                text(s,"CREED",x+24,y+261,3,WHITE);
                text(s,"TEMA COMPLETO",x+24,y+300,2,BLUE);
                roundPanel(s,x+18,y+333,cardW-36,65,15,{0,48,78});
                roundPanel(s,x+18,y+328,cardW-36,65,15,BLUE);
                roundPanel(s,x+25,y+333,cardW-50,2,1,{103,226,255});
                text(s,"INSTALAR",x+106,y+352,3,BLACK);
                text(s,"2 PKG INCLUIDOS",x+24,y+420,2,MUTED);
                roundPanel(s,x+cardW-112,y+426,88,24,8,{7,35,51});
                text(s,"DISPONIBLE",x+cardW-105,y+434,1,BLUE);
            } else {
                roundPanel(s,x+17,y+17,cardW-34,184,13,{10,19,31});
                roundPanel(s,x+28,y+28,cardW-56,162,10,{15,28,43});
                text(s,"MARKPS4",x+99,y+81,3,BLUE);
                text(s,"THEMES",x+105,y+117,2,WHITE);
                text(s,"PROXIMAMENTE",x+24,y+231,2,MUTED);
                text(s,"NUEVOS TEMAS",x+24,y+268,2,WHITE);
                roundPanel(s,x+18,y+333,cardW-36,65,15,{20,31,47});
                text(s,"EN PREPARACION",x+58,y+358,2,MUTED);
                text(s,"CATALOGO EN EXPANSION",x+24,y+420,1,MUTED);
            }
        }
        liftedPanel(s,360,649,1484,72,{16,27,42},false);
        roundPanel(s,378,668,8,32,3,BLUE);
        text(s,"ESTADO",402,666,2,MUTED);
        text(s,installStatus,530,666,2,BLUE);
        text(s,"X  INSTALAR TEMA",362,758,2,WHITE);
        text(s,"L1 / R1  CAMBIAR SECCION",625,758,2,MUTED);
    } else if (page==1) {
        liftedPanel(s,360,166,1484,245,{16,28,44},true);
        text(s,"TU COLECCION",400,209,3,BLUE);
        text(s,"TEMAS INSTALADOS",400,261,4,WHITE);
        text(s,"EL SISTEMA PS4 GESTIONA LAS INSTALACIONES Y NOTIFICACIONES",402,316,2,MUTED);
        roundPanel(s,400,352,620,2,1,PANEL2);
        text(s,"REVISA DESCARGAS Y NOTIFICACIONES DEL PS4",402,370,2,BLUE);
    } else {
        text(s,"EXPERIENCIA MARKPS4",362,165,2,BLUE);
        const char *settings[]={"INTERFAZ NATIVA PARA PS4","CATALOGO DE TEMAS EN TARJETAS","INSTALACION MEDIANTE EL SISTEMA PS4"};
        for (int i=0;i<3;i++) {
            int y=211+i*122;
            liftedPanel(s,360,y,1484,91,{16,28,44},i==0);
            roundPanel(s,386,y+27,7,36,3,i==0?BLUE:PANEL2);
            text(s,settings[i],418,y+36,2,WHITE);
            text(s,i==0?"ACTIVO":"INTEGRADO",1548,y+36,1,MUTED);
        }
    }

    s->SubmitFlip(frameID);
    s->FrameWait(frameID);
    s->FrameBufferSwap();
    ++frameID;
}

int main() {
    setvbuf(stdout, NULL, _IONBF, 0);
    Scene2D *scene = new Scene2D(W, H, 4);
    if (!scene->Init(0xC000000, 2)) {
        printf("[MarkPS4 Themes] No se pudo iniciar el renderizador nativo.\n");
        for (;;) sceKernelUsleep(1000000);
    }

    scene->SetActiveFrameBuffer(0);

    int userID = -1;
    OrbisUserServiceInitializeParams params;
    memset(&params, 0, sizeof(params));
    params.priority = ORBIS_KERNEL_PRIO_FIFO_LOWEST;
    (void)sceUserServiceInitialize(&params);
    (void)sceUserServiceGetInitialUser(&userID);
    (void)scePadInit();
    int pad = (userID >= 0) ? scePadOpen(userID, 0, 0, NULL) : -1;
    uint32_t previous = 0;
    int selected = 0;
    int page = 0;

    for (;;) {
        if (pad >= 0) {
            OrbisPadData data;
            memset(&data, 0, sizeof(data));
            if (scePadReadState(pad, &data) == 0) {
                uint32_t now = data.buttons;
                uint32_t pressed = now & ~previous;
                if (pressed & ORBIS_PAD_BUTTON_L1) page = (page + 2) % 3;
                if (pressed & ORBIS_PAD_BUTTON_R1) page = (page + 1) % 3;
                if (pressed & ORBIS_PAD_BUTTON_CROSS) {
                    startThemeInstall(userID);
                }
                if (pressed & ORBIS_PAD_BUTTON_CIRCLE) page = 0;
                previous = now;
            }
        }
        advanceQueuedInstall(userID);
        render(scene, selected, page);
        sceKernelUsleep(16000);
    }
    return 0;
}

