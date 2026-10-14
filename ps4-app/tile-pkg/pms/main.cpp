#include <stdio.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include <stdlib.h>
#include <math.h>
#include <orbis/Bgft.h>
#include "graphics.h"
#include "theme_packages.h"
#include "assassins_creed_image.h"

#define W 1920
#define H 1080

std::stringstream debugLogStream;
static int frameID = 0;

typedef OrbisBgftTaskId BgftTaskId;
typedef OrbisBgftInitParams BgftInitParams;
typedef OrbisBgftDownloadParam BgftDownloadParam;
typedef OrbisBgftTaskProgress BgftTaskProgress;

static BgftTaskId lastTask = -1;
static int lastPackageIndex = -1;
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
        static void *bgftHeap = NULL;
        bgftHeap = malloc(1024 * 1024);
        if (!bgftHeap) {
            snprintf(installStatus, sizeof(installStatus), "SIN MEMORIA PARA INSTALADOR");
            return -3;
        }
        memset(bgftHeap, 0, 1024 * 1024);
        BgftInitParams init = { bgftHeap, 1024 * 1024 };
        int rc = sceBgftServiceIntInit(&init);
        if (rc != 0 && (uint32_t)rc != 0x80990001u) {
            snprintf(installStatus, sizeof(installStatus), "BGFT NO INICIO: %08X", (unsigned)rc);
            free(bgftHeap);
            bgftHeap = NULL;
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
        int progressRc = sceBgftServiceIntDownloadGetProgress(lastTask, &progress);
        if (progressRc == 0 && progress.lengthTotal > 0 &&
            progress.transferredTotal < progress.lengthTotal) {
            snprintf(installStatus, sizeof(installStatus), "ESPERA A QUE TERMINE EL PAQUETE ANTERIOR");
            return -4;
        }
        /* Only reach here when the previous transfer reports all bytes or BGFT no longer reports it.
           The owner should press X for package 2 only after the PS4 confirms package 1 installed. */
        (void)sceBgftServiceIntDownloadStopTask(lastTask);
        (void)sceBgftServiceIntDownloadUnregisterTask(lastTask);
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
    /* Leave option at zero: OpenOrbis defines this as a struct, not an integer flag. */
    p.packageType = "PS4GD";
    p.packageSubType = "";
    p.packageSize = pkg.size;
    BgftTaskId task = -1;
    int rc = sceBgftServiceIntDebugDownloadRegisterPkg(&p, &task);
    if (rc != 0) {
        snprintf(installStatus, sizeof(installStatus), "PS4 RECHAZO EL PAQUETE");
        return rc;
    }
    rc = sceBgftServiceIntDownloadStartTask(task);
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
    if (!autoInstallSecond || lastTask < 0 || lastPackageIndex != 0 || !bgftReady) return;
    BgftTaskProgress progress;
    memset(&progress, 0, sizeof(progress));
    int rc = sceBgftServiceIntDownloadGetProgress(lastTask, &progress);
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
        case '@': G(14,17,23,21,23,16,14);
        case '_': G(0,0,0,0,0,0,31);
        case ' ': return blank;
        default: return blank;
    }
    #undef G
}

static void text(Scene2D *s, const char *str, int x, int y, int scale, Color color) {
    for (size_t i=0; str[i]; ++i) {
        char ch = str[i];
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 'a' + 'A');
        const uint8_t *rows = glyph(ch);
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
/* Clean flat geometry: one surface per component, no stacked rectangles,
   fake depth, borders or glossy strips. */
static void roundPanel(Scene2D *s, int x, int y, int w, int h, int r, Color c) {
    (void)r;
    if (w > 0 && h > 0) panel(s, x, y, w, h, c);
}
static void liftedPanel(Scene2D *s, int x, int y, int w, int h, Color c, bool active) {
    if (w <= 0 || h <= 0) return;
    panel(s, x, y, w, h, c);
    if (active) {
        panel(s, x, y, 5, h, BLUE);
    }
}
static void render(Scene2D *s, int selected, int page) {
    const int t = frameID;
    (void)t;
    (void)selected;
    s->FrameBufferClear();
    s->FrameBufferFill(BG);

    // Quiet, uncluttered backdrop with a single brand accent.
    panel(s, 0, 0, W, 4, BLUE);

    // Header / brand.
    roundPanel(s, 32, 27, 92, 92, 20, {0,4,12});
    roundPanel(s, 36, 23, 92, 92, 20, {9,28,48});
    roundPanel(s, 36, 23, 92, 4, 2, BLUE);
    roundPanel(s, 36, 109, 92, 3, 2, BLUE_DARK);
    text(s, "PS4", 55, 52, 4, WHITE);
    text(s, "MARKPS4", 151, 31, 4, WHITE);
    text(s, "THEMES SHOP", 153, 77, 3, BLUE);
    text(s, "TU CATALOGO DE TEMAS PARA PS4", 153, 111, 2, MUTED);

    liftedPanel(s, 1300, 32, 270, 55, {12,24,39}, false);
    roundPanel(s, 1318, 51, 9, 9, 5, {42,230,115});
    text(s, "TIENDA LISTA", 1341, 50, 2, WHITE);
    liftedPanel(s, 1590, 32, 292, 55, {12,24,39}, false);
    text(s, "MARKPS4 / PS4", 1612, 50, 2, BLUE);
    roundPanel(s, 32, 144, 1850, 2, 1, PANEL2);

    // Main navigation strip. Each section is reachable with D-pad and L1/R1.
    const char *nav[5] = {"INICIO", "EXPLORAR TEMAS", "MIS TEMAS", "CONFIGURACION", "ACERCA DE"};
    int nx[5] = {32, 252, 598, 826, 1130};
    int nw[5] = {202, 326, 210, 284, 250};
    int activePage = page;
    for (int i=0; i<5; ++i) {
        bool active = (i == activePage);
        liftedPanel(s, nx[i], 163, nw[i], 58, active ? Color{0,75,126} : PANEL, active);
        text(s, nav[i], nx[i]+17, 184, 2, active ? WHITE : MUTED);
    }
    text(s, "X SELECCIONAR", 1514, 184, 2, MUTED);
    roundPanel(s, 32, 239, 1850, 2, 1, PANEL2);

    if (page == 0) {
        text(s, "BIENVENIDO A MARKPS4 THEMES", 48, 271, 4, WHITE);
        text(s, "DESCARGA  /  INSTALA  /  PERSONALIZA", 51, 319, 2, BLUE);
        // Large featured banner.
        liftedPanel(s, 32, 355, 1112, 360, {12,25,42}, true);
        roundPanel(s, 54, 379, 1068, 3, 1, BLUE);
        text(s, "TU PS4, TU ESTILO", 74, 414, 4, WHITE);
        text(s, "EXPLORA UNA COLECCION DE TEMAS", 77, 469, 3, BLUE);
        text(s, "VISTA PREVIA, TAMANO Y DESCRIPCION", 78, 516, 2, WHITE);
        text(s, "ELIGE TU TEMA FAVORITO Y PREPARA LA INSTALACION", 78, 554, 2, MUTED);
        liftedPanel(s, 78, 602, 330, 66, BLUE, true);
        text(s, "EXPLORAR TEMAS", 111, 624, 2, BLACK);
        // Quick-action cards.
        liftedPanel(s, 1170, 355, 340, 168, {14,29,47}, false);
        text(s, "CATALOGO", 1194, 390, 3, BLUE);
        text(s, "TEMAS DISPONIBLES", 1194, 431, 2, WHITE);
        text(s, "ACTUALIZA TU COLECCION", 1194, 468, 1, MUTED);
        liftedPanel(s, 1532, 355, 350, 168, {14,29,47}, false);
        text(s, "INSTALACION", 1556, 390, 3, BLUE);
        text(s, "MEDIANTE BGFT / RPI", 1556, 431, 2, WHITE);
        text(s, "SEGUN COMPATIBILIDAD", 1556, 468, 1, MUTED);
        liftedPanel(s, 1170, 545, 712, 170, {14,29,47}, false);
        text(s, "ESTADO DE LA TIENDA", 1194, 580, 3, WHITE);
        text(s, installStatus, 1194, 629, 2, BLUE);
        text(s, "USA L1 / R1 PARA CAMBIAR DE SECCION", 48, 760, 2, MUTED);
    } else if (page == 1) {
        text(s, "EXPLORAR TEMAS", 48, 271, 4, WHITE);
        text(s, "SELECCIONA UN TEMA PARA VER SUS DETALLES", 51, 319, 2, MUTED);
        const int xs[3] = {32, 654, 1276};
        const char *names[3] = {"ASSASSINS CREED", "NUEVOS TEMAS", "PROXIMAMENTE"};
        const char *subs[3] = {"TEMA COMPLETO / 2 PKG", "CATALOGO EN EXPANSION", "MAS TEMAS MUY PRONTO"};
        for (int i=0; i<3; ++i) {
            bool active = (i == 0);
            int y = 354;
            liftedPanel(s, xs[i], y, 574, 356, active ? Color{16,33,52} : PANEL, active);
            roundPanel(s, xs[i]+20, y+20, 534, 4, 1, active ? BLUE : PANEL2);
            if (active) {
                s->DrawRGBImage(xs[i]+25, y+35, 524, 166, kAssassinsCreedRGB, 240, 165);
            } else {
                roundPanel(s, xs[i]+25, y+35, 524, 166, 12, {10,23,39});
                text(s, "MARKPS4 THEMES", xs[i]+147, y+100, 3, BLUE);
            }
            text(s, names[i], xs[i]+28, y+225, 3, WHITE);
            text(s, subs[i], xs[i]+28, y+269, 2, MUTED);
            if (active) {
                liftedPanel(s, xs[i]+28, y+296, 240, 42, BLUE, true);
                text(s, "X VER DETALLES", xs[i]+48, y+309, 2, BLACK);
            }
        }
        liftedPanel(s, 32, 740, 1818, 74, {14,27,43}, false);
        text(s, "ESTADO:", 55, 765, 2, MUTED);
        text(s, installStatus, 190, 765, 2, BLUE);
        text(s, "X INSTALAR TEMA", 48, 850, 2, WHITE);
        text(s, "O VOLVER", 330, 850, 2, MUTED);
    } else if (page == 2) {
        text(s, "MIS TEMAS", 48, 271, 4, WHITE);
        text(s, "GESTIONA LOS TEMAS QUE HAS ENVIADO A INSTALAR", 51, 319, 2, MUTED);
        liftedPanel(s, 32, 354, 1818, 210, {14,28,45}, true);
        text(s, "DESCARGAS E INSTALACIONES", 66, 394, 3, BLUE);
        text(s, installStatus, 66, 451, 3, WHITE);
        text(s, "CONSULTA LAS NOTIFICACIONES DEL SISTEMA PS4 PARA VERIFICAR EL RESULTADO", 66, 504, 2, MUTED);
        liftedPanel(s, 32, 590, 1818, 120, {14,28,45}, false);
        text(s, "LOS TEMAS INSTALADOS SE ADMINISTRAN DESDE AJUSTES DE PS4", 66, 632, 2, WHITE);
    } else if (page == 3) {
        text(s, "CONFIGURACION", 48, 271, 4, WHITE);
        text(s, "PREFERENCIAS DE MARKPS4 THEMES", 51, 319, 2, MUTED);
        const char *settings[3] = {"INTERFAZ GAMING AZUL Y NEGRA", "CATALOGO DE TEMAS EN TARJETAS", "INSTALACION MEDIANTE SERVICIOS PS4"};
        for (int i=0; i<3; ++i) {
            int y = 366 + i*124;
            liftedPanel(s, 32, y, 1818, 92, {14,28,45}, i == 0);
            roundPanel(s, 58, y+28, 7, 35, 3, i == 0 ? BLUE : PANEL2);
            text(s, settings[i], 86, y+38, 2, WHITE);
            text(s, i == 0 ? "ACTIVO" : "INTEGRADO", 1640, y+38, 2, MUTED);
        }
    } else {
        // About page: short store description and explicit creator credit.
        text(s, "ACERCA DE MARKPS4 THEMES", 48, 271, 4, WHITE);
        text(s, "TU ESPACIO PARA DESCUBRIR TEMAS DE PLAYSTATION 4", 51, 319, 2, BLUE);
        liftedPanel(s, 32, 356, 1132, 340, {14,28,45}, true);
        text(s, "SOBRE LA THEMES SHOP", 64, 394, 3, BLUE);
        text(s, "MARKPS4 THEMES ES UNA TIENDA HOMEbrew", 64, 448, 2, WHITE);
        text(s, "PARA EXPLORAR TEMAS PERSONALIZADOS DE PS4.", 64, 485, 2, WHITE);
        text(s, "CONSULTA INFORMACION DE CADA TEMA Y, CUANDO", 64, 522, 2, MUTED);
        text(s, "SEA COMPATIBLE, ENVIALO A INSTALAR EN TU CONSOLA.", 64, 559, 2, MUTED);
        text(s, "PROYECTO INDEPENDIENTE PARA LA COMUNIDAD PS4.", 64, 616, 2, BLUE);
        liftedPanel(s, 1192, 356, 658, 340, {14,28,45}, true);
        // Simple GitHub-style mark rendered natively without external assets.
        roundPanel(s, 1230, 398, 72, 72, 36, {3,8,15});
        roundPanel(s, 1240, 408, 52, 52, 26, BLUE);
        text(s, "GH", 1250, 429, 2, BLACK);
        text(s, "CREADA POR", 1322, 411, 2, MUTED);
        text(s, "@MARQUITO1611", 1322, 451, 3, WHITE);
        roundPanel(s, 1230, 500, 570, 2, 1, PANEL2);
        text(s, "GITHUB", 1230, 528, 2, BLUE);
        text(s, "MARQUITO1611 / MARKPS4-THEMES", 1230, 568, 2, WHITE);
        text(s, "GRACIAS POR APOYAR EL PROYECTO", 1230, 625, 2, MUTED);
        liftedPanel(s, 32, 730, 1818, 74, {14,27,43}, false);
        text(s, "MARKPS4 THEMES  /  DESCUBRE. PERSONALIZA. DISFRUTA.", 56, 756, 2, BLUE);
    }

    // Consistent footer hints.
    roundPanel(s, 32, 1010, 1850, 2, 1, PANEL2);
    text(s, "L1 / R1  SECCIONES", 48, 1031, 1, MUTED);
    text(s, "X  SELECCIONAR", 330, 1031, 1, MUTED);
    text(s, "O  VOLVER", 600, 1031, 1, MUTED);
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
                if (pressed & ORBIS_PAD_BUTTON_L1) page = (page + 4) % 5;
                if (pressed & ORBIS_PAD_BUTTON_R1) page = (page + 1) % 5;
                if (pressed & ORBIS_PAD_BUTTON_CROSS) {
                    if (page == 1) startThemeInstall(userID);
                    else if (page == 0) page = 1;
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

