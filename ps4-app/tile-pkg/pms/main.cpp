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

static const Color BG       = { 12, 14, 20 };
static const Color PANEL    = { 25, 29, 39 };
static const Color PANEL2   = { 34, 39, 51 };
static const Color YELLOW   = { 255, 210, 45 };
static const Color WHITE    = { 245, 246, 250 };
static const Color MUTED    = { 153, 162, 179 };
static const Color BLACK    = { 12, 14, 20 };

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

static void render(Scene2D *s, int selected, int page) {
    s->FrameBufferClear();
    s->FrameBufferFill(BG);
    panel(s,0,0,W,12,YELLOW);
    text(s,"MARKPS4 THEMES",92,66,7,WHITE);
    text(s,"TIENDA NATIVA PARA PS4",96,132,3,MUTED);
    panel(s,70,215,1780,2,PANEL2);
    const char *tabs[]={"CATALOGO","INSTALADOS","AJUSTES"};
    for (int i=0;i<3;i++) {
        int x=90+i*285;
        panel(s,x,185,245,66,(page==i)?YELLOW:PANEL);
        text(s,tabs[i],x+20,207,3,(page==i)?BLACK:WHITE);
    }
    if (page==0) {
        text(s,"ASSASSIN'S CREED",95,300,6,WHITE);
        text(s,"PAQUETES OFICIALES DE TU REPOSITORIO MARKPS4",98,365,3,MUTED);
        panel(s,95,430,1730,170,PANEL2);
        panel(s,115,450,12,130,YELLOW);
        text(s,"INSTALA LOS DOS PAQUETES EN ORDEN",155,465,4,WHITE);
        text(s,"ESPERA EL AVISO DE INSTALACION COMPLETA ANTES DEL PAQUETE 2",155,490,2,MUTED);
        text(s,"SELECCIONA UN PAQUETE Y PULSA X PARA ENVIARLO A DESCARGAS",155,525,2,MUTED);
        for (int i=0;i<2;i++) {
            int y=640+i*130;
            panel(s,95,y,1730,105,(selectedPackage==i)?YELLOW:PANEL);
            Color tc=(selectedPackage==i)?BLACK:WHITE;
            text(s,i==0?"PAQUETE 1":"PAQUETE 2",130,y+24,4,tc);
            text(s,i==0?"1-EP0001-CUSA09303...V0100.PKG":"2-EP0001-CUSA09303...PKG",500,y+28,2,tc);
            text(s,"PULSA X PARA INSTALAR",1390,y+35,2,tc);
        }
        text(s,"ESTADO:",98,930,2,MUTED);
        text(s,installStatus,270,930,2,YELLOW);
        text(s,"D-PAD: ELEGIR PAQUETE   X: INICIAR   L1/R1: SECCIONES",98,985,2,MUTED);
    } else if (page==1) {
        text(s,"TEMAS INSTALADOS",95,310,5,WHITE);
        text(s,"EL SISTEMA PS4 GESTIONA LAS INSTALACIONES Y SUS NOTIFICACIONES",98,390,3,MUTED);
        panel(s,95,470,1730,220,PANEL);
        text(s,"REVISA NOTIFICACIONES Y DESCARGAS DEL PS4",135,545,4,WHITE);
    } else {
        text(s,"AJUSTES",95,310,5,WHITE);
        panel(s,95,400,1730,110,PANEL);
        text(s,"MARKPS4 THEMES - INTERFAZ NATIVA",130,440,3,WHITE);
        panel(s,95,535,1730,110,PANEL);
        text(s,"SIN NAVEGADOR PARA MOSTRAR LA INTERFAZ",130,575,3,WHITE);
        panel(s,95,670,1730,110,PANEL);
        text(s,"LA INSTALACION USA BGFT SI EL FIRMWARE DA ACCESO",130,710,3,WHITE);
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
                if (pressed & ORBIS_PAD_BUTTON_LEFT) selectedPackage = (selectedPackage + 1) % 2;
                if (pressed & ORBIS_PAD_BUTTON_RIGHT) selectedPackage = (selectedPackage + 1) % 2;
                if (pressed & ORBIS_PAD_BUTTON_UP) selectedPackage = (selectedPackage + 1) % 2;
                if (pressed & ORBIS_PAD_BUTTON_DOWN) selectedPackage = (selectedPackage + 1) % 2;
                if (pressed & ORBIS_PAD_BUTTON_L1) page = (page + 2) % 3;
                if (pressed & ORBIS_PAD_BUTTON_R1) page = (page + 1) % 3;
                if (pressed & ORBIS_PAD_BUTTON_CROSS) {
                    startPackageInstall(userID, selectedPackage);
                }
                if (pressed & ORBIS_PAD_BUTTON_CIRCLE) page = 0;
                previous = now;
            }
        }
        render(scene, selected, page);
        sceKernelUsleep(16000);
    }
    return 0;
}

