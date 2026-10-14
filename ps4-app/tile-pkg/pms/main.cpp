#include <stdio.h>
#include <sstream>
#include <stdint.h>
#include <string.h>
#include <orbis/Pad.h>
#include <orbis/UserService.h>
#include "graphics.h"

#define W 1920
#define H 1080

std::stringstream debugLogStream;
static int frameID = 0;

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
    text(s,"TEMAS PARA PS4",96,132,3,MUTED);

    panel(s,70,215,1780,2,PANEL2);
    const char *tabs[]={"CATALOGO","INSTALADOS","AJUSTES"};
    for (int i=0;i<3;i++) {
        int x=90+i*285;
        panel(s,x,185,245,66,(page==i)?YELLOW:PANEL);
        text(s,tabs[i],x+20,207,3,(page==i)?BLACK:WHITE);
    }

    if (page==0) {
        text(s,"CATALOGO DE TEMAS",95,300,5,WHITE);
        text(s,"ELIGE UN DISENO PARA TU PS4",98,355,3,MUTED);
        const char *names[]={"DARK CARBON","NEON GOLD","MIDNIGHT BLUE","CLASSIC BLACK"};
        const char *sub[]={"INTERFAZ OSCURA","ACENTO AMARILLO","ESTILO NOCTURNO","MINIMALISTA"};
        Color accents[]={{95,105,125},{255,210,45},{70,145,255},{180,185,195}};
        for (int i=0;i<4;i++) {
            int x=95+i*445;
            panel(s,x,440,405,385,(selected==i)?YELLOW:PANEL2);
            panel(s,x+12,452,381,210,BG);
            panel(s,x+32,472,341,15,accents[i]);
            panel(s,x+32,507,165,108,PANEL);
            panel(s,x+211,507,162,48,PANEL);
            panel(s,x+211,569,162,46,PANEL);
            panel(s,x+32,632,341,3,accents[i]);
            text(s,names[i],x+24,690,3,(selected==i)?YELLOW:WHITE);
            text(s,sub[i],x+24,738,2,MUTED);
            if (selected==i) {
                panel(s,x+24,778,180,30,YELLOW);
                text(s,"SELECCIONADO",x+34,785,2,BLACK);
            }
        }
        text(s,"CRUCETA: MOVER   L1/R1: SECCIONES   X: SELECCIONAR",98,900,2,MUTED);
        text(s,"VISTA DE DISENO: LOS TEMAS SE MOSTRARAN AL CONECTAR EL CATALOGO REAL",98,950,2,MUTED);
    } else if (page==1) {
        text(s,"TEMAS INSTALADOS",95,310,5,WHITE);
        text(s,"AQUI APARECERAN LOS TEMAS DETECTADOS EN LA CONSOLA",98,390,3,MUTED);
        panel(s,95,470,1730,220,PANEL);
        text(s,"SIN DATOS DE TEMAS INSTALADOS",135,545,4,WHITE);
    } else {
        text(s,"AJUSTES",95,310,5,WHITE);
        panel(s,95,400,1730,110,PANEL);
        text(s,"MARKPS4 THEMES - INTERFAZ NATIVA",130,440,3,WHITE);
        panel(s,95,535,1730,110,PANEL);
        text(s,"NAVEGACION CON MANDO DUALSHOCK 4",130,575,3,WHITE);
        panel(s,95,670,1730,110,PANEL);
        text(s,"SIN NAVEGADOR NI BINLOADER PARA DIBUJAR ESTA PANTALLA",130,710,3,WHITE);
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
                if (pressed & ORBIS_PAD_BUTTON_LEFT) selected = (selected + 3) % 4;
                if (pressed & ORBIS_PAD_BUTTON_RIGHT) selected = (selected + 1) % 4;
                if (pressed & ORBIS_PAD_BUTTON_UP) selected = (selected + 3) % 4;
                if (pressed & ORBIS_PAD_BUTTON_DOWN) selected = (selected + 1) % 4;
                if (pressed & ORBIS_PAD_BUTTON_L1) page = (page + 2) % 3;
                if (pressed & ORBIS_PAD_BUTTON_R1) page = (page + 1) % 3;
                if (pressed & ORBIS_PAD_BUTTON_CROSS) {
                    /* Selection is visual only until a real theme-package catalog is connected. */
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

