
#include "dump1090.h"
#include "structs.h"
#include "parula.h"
#include "magma.h"
#include "monokai.h"
#include "SDL/SDL_gfxPrimitives.h"

void CROSSVP(double *v, double *u, double *w) 
{                                                                       
    v[0] = u[1]*w[2] - u[2]*(w)[1];                             
    v[1] = u[2]*w[0] - u[0]*(w)[2];                             
    v[2] = u[0]*w[1] - u[1]*(w)[0];                             
}

SDL_Color setColor(uint8_t r, uint8_t g, uint8_t b) {
    SDL_Color out;
    out.r = r;
    out.g = g;
    out.b = b;
    return out;
}

SDL_Color signalToColor(int signal) {
    SDL_Color planeColor;

    if(signal > 127) {
        signal = 127;
    }

    if(signal < 0) {
        planeColor = setColor(96, 96, 96);      
    } else {
        if(MAGMA) {
            planeColor = setColor(magma[signal][0], magma[signal][1], magma[signal][2]);                    
        } else {
            planeColor = setColor(parula[signal][0], parula[signal][1], parula[signal][2]);                 
        }
    }

    return planeColor;
}

int screenDist(double d) {

    double scale_factor = (Modes.screen_width > Modes.screen_height) ? Modes.screen_width : Modes.screen_height;

    if(Modes.mapLogDist) {
        return round(0.95 * scale_factor * 0.5 * log(1.0+fabs(d)) / log(1.0+LOGMAXDIST));    
    } else {
        return round(0.95 * scale_factor * 0.5 * fabs(d) / MAXDIST);    
    }
}

void screenCoords(int *outX, int *outY, double dx, double dy) {
    *outX = (Modes.screen_width>>1) + ((dx>0) ? 1 : -1) * screenDist(dx);    
    *outY = (Modes.screen_height * 0.4) + ((dy>0) ? 1 : -1) * screenDist(dy);        
}

int outOfBounds(int x, int y) {
    if(x < 0 || x >= Modes.screen_width || y < 0 || y >= Modes.screen_height ) {
        return 1;
    } else {
        return 0;
    }
}

void drawPlaneHeading(double dx, double dy, double heading, int signal, char *flight)
{
    int x, y;
    screenCoords(&x, &y, dx, dy);

    if(outOfBounds(x,y)) {
        return;
    }

    SDL_Color planeColor = signalToColor(signal);

    double body = 8.0;
    double wing = 6.0;
    double tail = 3.0;

    double vec[3];
    vec[0] = sin(heading * M_PI / 180);
    vec[1] = cos(heading * M_PI / 180);
    vec[2] = 0;

    double up[] = {0,0,1};

    double out[3];

    CROSSVP(out,vec,up);

    int x1, x2, y1, y2;

    //body
    x1 = x + round(-body*vec[0]);
    y1 = y + round(-body*vec[1]);
    x2 = x + round(body*vec[0]);
    y2 = y + round(body*vec[1]);

    if(AA) {
        aalineRGBA(game.screen,x1,y1,x2,y2,planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
        aatrigonRGBA(game.screen, x + round(-wing*.35*out[0]), y + round(-wing*.35*out[1]), x + round(wing*.35*out[0]), y + round(wing*.35*out[1]), x1, y1,planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);        
        aacircleRGBA(game.screen, x2,y2,1,planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
    } else {
        thickLineRGBA(game.screen,x,y,x2,y2,2,planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
        filledTrigonRGBA(game.screen, x + round(-wing*.35*out[0]), y + round(-wing*.35*out[1]), x + round(wing*.35*out[0]), y + round(wing*.35*out[1]), x1, y1,planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);        
        filledCircleRGBA(game.screen, x2,y2,1,planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
    }

    //wing
    x1 = x + round(-wing*out[0]);
    y1 = y + round(-wing*out[1]);
    x2 = x + round(wing*out[0]);
    y2 = y + round(wing*out[1]);

    if(AA) {
        aatrigonRGBA(game.screen, x1, y1, x2, y2, x+round(body*.35*vec[0]), y+round(body*.35*vec[1]),planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
    } else {
        filledTrigonRGBA(game.screen, x1, y1, x2, y2, x+round(body*.35*vec[0]), y+round(body*.35*vec[1]),planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
    }

    //tail
    x1 = x + round(-body*.75*vec[0]) + round(-tail*out[0]);
    y1 = y + round(-body*.75*vec[1]) + round(-tail*out[1]);
    x2 = x + round(-body*.75*vec[0]) + round(tail*out[0]);
    y2 = y + round(-body*.75*vec[1]) + round(tail*out[1]);

    if(AA) {
        aatrigonRGBA (game.screen, x1, y1, x2, y2, x+round(-body*.5*vec[0]), y+round(-body*.5*vec[1]),planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
    } else {
        filledTrigonRGBA (game.screen, x1, y1, x2, y2, x+round(-body*.5*vec[0]), y+round(-body*.5*vec[1]),planeColor.r,planeColor.g,planeColor.b,SDL_ALPHA_OPAQUE);
    }

    drawString(flight, x + 10, y + 10, game.font, planeColor);
}

void drawPlane(double dx, double dy, int signal)
{

    int x, y;
    screenCoords(&x, &y, dx, dy);

    if(outOfBounds(x,y)) {
        return;
    }

    SDL_Color planeColor = signalToColor(signal);

    int length = 3.0;

    rectangleRGBA (game.screen, x - length, y - length, x+length, y + length, planeColor.r, planeColor.g, planeColor.b, SDL_ALPHA_OPAQUE);   
}

void drawTrail(double *oldDx, double *oldDy, time_t * oldSeen, int idx) {

    
    int currentIdx, prevIdx;

    int currentX, currentY, prevX, prevY;

    time_t now = time(NULL);

    for(int i=0; i < (MODES_INTERACTIVE_TRAIL_LENGTH - 1); i++) {
        currentIdx = (idx - i) % MODES_INTERACTIVE_TRAIL_LENGTH;
        currentIdx = currentIdx < 0 ? currentIdx + MODES_INTERACTIVE_TRAIL_LENGTH : currentIdx;     
        prevIdx = (idx - (i + 1)) % MODES_INTERACTIVE_TRAIL_LENGTH;
        prevIdx = prevIdx < 0 ? prevIdx + MODES_INTERACTIVE_TRAIL_LENGTH : prevIdx;         

        if(oldDx[currentIdx] == 0 || oldDy[currentIdx] == 0) {
            continue;
        }

        if(oldDx[prevIdx] == 0 || oldDy[prevIdx] == 0) {
            continue;
        }

        screenCoords(&currentX, &currentY, oldDx[currentIdx], oldDy[currentIdx]);

        screenCoords(&prevX, &prevY, oldDx[prevIdx], oldDy[prevIdx]);

        if(outOfBounds(currentX,currentY)) {
            return;
        }

        if(outOfBounds(prevX,prevY)) {
            return;
        }

        double age = pow(1.0 - (double)(now - oldSeen[currentIdx]) / MODES_INTERACTIVE_TRAIL_TTL, 2.2);

        if(age < 0) {
            age = 0;
        }

        uint8_t colorVal = (uint8_t)floor(255.0 * age);
  
        if(AA) {
            aalineRGBA(game.screen, prevX, prevY, currentX, currentY,colorVal, colorVal, colorVal, SDL_ALPHA_OPAQUE);       
        } else {
            //thickLineRGBA(game.screen, prevX, prevY, currentX, currentY, 2, colorVal, colorVal, colorVal, SDL_ALPHA_OPAQUE);                  
            thickLineRGBA(game.screen, prevX, prevY, currentX, currentY, 2, colorVal, colorVal, colorVal, SDL_ALPHA_OPAQUE);                    
        }   
    }
}


void drawGrid()
{
    int p1km = screenDist(1.0);
    int p10km = screenDist(10.0);
    int p100km = screenDist(100.0);

    hlineRGBA (game.screen, 0, Modes.screen_width, Modes.screen_height*0.4, 127, 127, 127, SDL_ALPHA_OPAQUE);
    vlineRGBA (game.screen, Modes.screen_width>>1, 0, Modes.screen_height, 127, 127, 127, SDL_ALPHA_OPAQUE);

    if(AA) {
        aacircleRGBA (game.screen, Modes.screen_width>>1, Modes.screen_height>>1, p1km, 249,38,114, SDL_ALPHA_OPAQUE);  
        aacircleRGBA (game.screen, Modes.screen_width>>1, Modes.screen_height>>1, p10km, 187,29,86, SDL_ALPHA_OPAQUE);
        aacircleRGBA (game.screen, Modes.screen_width>>1, Modes.screen_height>>1, p100km, 125,19,57, SDL_ALPHA_OPAQUE);     
    } else {
        circleRGBA (game.screen, Modes.screen_width>>1, Modes.screen_height*0.4, p1km, 249,38,114, SDL_ALPHA_OPAQUE);    
        circleRGBA (game.screen, Modes.screen_width>>1, Modes.screen_height*0.4, p10km, 187,29,86, SDL_ALPHA_OPAQUE);
        circleRGBA (game.screen, Modes.screen_width>>1, Modes.screen_height*0.4, p100km, 125,19,57, SDL_ALPHA_OPAQUE);       
    }

    drawString("1km", (Modes.screen_width>>1) + p1km + 5, (Modes.screen_height*0.4) + 5, game.font, setColor(249,38,114));   
    drawString("10km", (Modes.screen_width>>1) + p10km + 5, (Modes.screen_height*0.4) + 5, game.font, setColor(187,29,86));  
    drawString("100km", (Modes.screen_width>>1) + p100km + 5, (Modes.screen_height*0.4) + 5, game.font, setColor(125,19,57));            
}

void drawGeography() {
    int x1, y1, x2, y2;
    for(int i=1; i<mapPoints_count/2; i++) {

        if(!mapPoints_relative[i * 2] || !mapPoints_relative[(i - 1) * 2 + 1] || !mapPoints_relative[i * 2] || !mapPoints_relative[i * 2 + 1]) {
            continue;
        }

        screenCoords(&x1, &y1, mapPoints_relative[(i - 1) * 2], -mapPoints_relative[(i - 1) * 2 + 1]);      
        screenCoords(&x2, &y2, mapPoints_relative[i * 2], -mapPoints_relative[i * 2 + 1]);

        if(outOfBounds(x1,y1) && outOfBounds(x2,y2)) {
            continue;
        }

        if(AA) {
            aalineRGBA(game.screen, x1, y1, x2, y2,purple.r,purple.g,purple.b, SDL_ALPHA_OPAQUE);
        } else {
            lineRGBA(game.screen, x1, y1, x2, y2,purple.r,purple.g,purple.b, SDL_ALPHA_OPAQUE);
        }
    }
}

void drawMap(void) {
    struct aircraft *a = Modes.aircrafts;
    time_t now = time(NULL);

    drawGeography();

    drawGrid();     

    while(a) {
        if ((now - a->seen) < Modes.interactive_display_ttl) {
            if (a->bFlags & MODES_ACFLAGS_LATLON_VALID) {

                unsigned char * pSig       = a->signalLevel;
                unsigned int signalAverage = (pSig[0] + pSig[1] + pSig[2] + pSig[3] + 
                                              pSig[4] + pSig[5] + pSig[6] + pSig[7] + 3) >> 3; 

                drawTrail(a->oldDx, a->oldDy, a->oldSeen, a->oldIdx);

                int colorIdx;
                if((int)(now - a->seen) > MODES_INTERACTIVE_DISPLAY_ACTIVE) {
                    colorIdx = -1;
                } else {
                    colorIdx = signalAverage;
                }

                if(MODES_ACFLAGS_HEADING_VALID) {
                    drawPlaneHeading(a->dx, a->dy,a->track, colorIdx, a->flight);
                } else {
                    drawPlane(a->dx, a->dy, colorIdx);
                }                
            }
        }
        a = a->next;
    }
}