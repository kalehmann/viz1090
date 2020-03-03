// view1090, a Mode S messages viewer for dump1090 devices.
//
// Copyright (C) 2014 by Malcolm Robb <Support@ATTAvionics.com>
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//  *  Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//
//  *  Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#include "view1090.h"
#include "structs.h"

int go = 1;

//
// ============================= Utility functions ==========================
//
void sigintHandler(int dummy) {
    NOTUSED(dummy);
    signal(SIGINT, SIG_DFL);  // reset signal handler - bit extra safety
    Modes.exit = 1;           // Signal to threads that we are done
}

//
// =============================== Initialization ===========================
//
void view1090InitConfig(void) {
    // Default everything to zero/NULL
    memset(&Modes,    0, sizeof(Modes));
    memset(&View1090, 0, sizeof(View1090));

    // Now initialise things that should not be 0/NULL to their defaults
    Modes.check_crc               = 1;
    strcpy(View1090.net_input_beast_ipaddr,VIEW1090_NET_OUTPUT_IP_ADDRESS); 
    Modes.net_input_beast_port    = MODES_NET_OUTPUT_BEAST_PORT;
    Modes.fUserLat                = MODES_USER_LATITUDE_DFLT;
    Modes.fUserLon                = MODES_USER_LONGITUDE_DFLT;

    Modes.interactive             = 0;
    Modes.quiet                   = 1;

    // Map options
    appData.maxDist                 = 25.0;
    appData.centerLon               = Modes.fUserLon;
    appData.centerLat               = Modes.fUserLat;

    // Display options
    appData.screen_uiscale          = 1;
    appData.screen_width            = 0;
    appData.screen_height           = 0;    
    appData.screen_depth            = 32;
    appData.fullscreen              = 0;

    // Initialize status
    Status.msgRate                = 0;
    Status.avgSig                 = 0;
    Status.numPlanes              = 0;
    Status.numVisiblePlanes     = 0;
    Status.maxDist                = 0;
}
//
//=========================================================================
//
void view1090Init(void) {

    // pthread_mutex_init(&Modes.pDF_mutex,NULL);
    // pthread_mutex_init(&Modes.data_mutex,NULL);
    // pthread_cond_init(&Modes.data_cond,NULL);

#ifdef _WIN32
    if ( (!Modes.wsaData.wVersion) 
      && (!Modes.wsaData.wHighVersion) ) {
      // Try to start the windows socket support
      if (WSAStartup(MAKEWORD(2,1),&Modes.wsaData) != 0) 
        {
        fprintf(stderr, "WSAStartup returned Error\n");
        }
      }
#endif

    // Allocate the various buffers used by Modes
    if ( NULL == (Modes.icao_cache = (uint32_t *) malloc(sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2)))
    {
        fprintf(stderr, "Out of memory allocating data buffer.\n");
        exit(1);
    }

    // Clear the buffers that have just been allocated, just in-case
    memset(Modes.icao_cache, 0,   sizeof(uint32_t) * MODES_ICAO_CACHE_LEN * 2);

    // Validate the users Lat/Lon home location inputs
    if ( (Modes.fUserLat >   90.0)  // Latitude must be -90 to +90
      || (Modes.fUserLat <  -90.0)  // and 
      || (Modes.fUserLon >  360.0)  // Longitude must be -180 to +360
      || (Modes.fUserLon < -180.0) ) {
        Modes.fUserLat = Modes.fUserLon = 0.0;
    } else if (Modes.fUserLon > 180.0) { // If Longitude is +180 to +360, make it -180 to 0
        Modes.fUserLon -= 360.0;
    }
    // If both Lat and Lon are 0.0 then the users location is either invalid/not-set, or (s)he's in the 
    // Atlantic ocean off the west coast of Africa. This is unlikely to be correct. 
    // Set the user LatLon valid flag only if either Lat or Lon are non zero. Note the Greenwich meridian 
    // is at 0.0 Lon,so we must check for either fLat or fLon being non zero not both. 
    // Testing the flag at runtime will be much quicker than ((fLon != 0.0) || (fLat != 0.0))
    Modes.bUserFlags &= ~MODES_USER_LATLON_VALID;
    if ((Modes.fUserLat != 0.0) || (Modes.fUserLon != 0.0)) {
        Modes.bUserFlags |= MODES_USER_LATLON_VALID;
    }

    // Prepare error correction tables
    modesInitErrorInfo();
}

// Set up data connection
int setupConnection(struct client *c) {
    int fd;

    // Try to connect to the selected ip address and port. We only support *ONE* input connection which we initiate.here.
    if ((fd = anetTcpConnect(Modes.aneterr, View1090.net_input_beast_ipaddr, Modes.net_input_beast_port)) != ANET_ERR) {
		anetNonBlock(Modes.aneterr, fd);
		//
		// Setup a service callback client structure for a beast binary input (from dump1090)
		// This is a bit dodgy under Windows. The fd parameter is a handle to the internet
		// socket on which we are receiving data. Under Linux, these seem to start at 0 and 
		// count upwards. However, Windows uses "HANDLES" and these don't nececeriy start at 0.
		// dump1090 limits fd to values less than 1024, and then uses the fd parameter to 
		// index into an array of clients. This is ok-ish if handles are allocated up from 0.
		// However, there is no gaurantee that Windows will behave like this, and if Windows 
		// allocates a handle greater than 1024, then dump1090 won't like it. On my test machine, 
		// the first Windows handle is usually in the 0x54 (84 decimal) region.

		c->next    = NULL;
		c->buflen  = 0;
		c->fd      = 
		c->service =
		Modes.bis  = fd;
		Modes.clients = c;
    }
    return fd;
}
//
// ================================ Main ====================================
//
void showHelp(void) {
    printf(
"-----------------------------------------------------------------------------\n"
"|                        view1090 dump1090 Viewer        Ver : "MODES_DUMP1090_VERSION " |\n"
"-----------------------------------------------------------------------------\n"
  "--server <IPv4/hosname>          TCP Beast output listen IPv4 (default: 127.0.0.1)\n"
  "--port <port>                    TCP Beast output listen port (default: 30005)\n"
  "--lat <latitude>                 Reference/receiver latitide for surface posn (opt)\n"
  "--lon <longitude>                Reference/receiver longitude for surface posn (opt)\n"
  "--metric                         Use metric units (meters, km/h, ...)\n"
  "--help                           Show this help\n"
  "--uiscale <factor>               UI global scaling (default: 1)\n"  
  "--screensize <width> <height>    Set frame buffer resolution (default: screen resolution)\n"
  "--fullscreen                     Start fullscreen\n"
    );
}

#ifdef _WIN32
void showCopyright(void) {
    uint64_t llTime = time(NULL) + 1;

    printf(
"-----------------------------------------------------------------------------\n"
"|                        view1090 ModeS Viewer           Ver : " MODES_DUMP1090_VERSION " |\n"
"-----------------------------------------------------------------------------\n"
"\n"
" Copyright (C) 2012 by Salvatore Sanfilippo <antirez@gmail.com>\n"
" Copyright (C) 2014 by Malcolm Robb <support@attavionics.com>\n"
" Copyright (C) 2020 by Nathan Matsuda <info@nathanmatsuda.com>\n"
"\n"
" All rights reserved.\n"
"\n"
" THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
" ""AS IS"" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
" LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
" A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
" HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
" SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT\n"
" LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,\n"
" DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY\n"
" THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT\n"
" (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE\n"
" OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n"
"\n"
" For further details refer to <https://github.com/MalcolmRobb/dump1090>\n" 
"\n"
    );

  // delay for 1 second to give the user a chance to read the copyright
  while (llTime >= time(NULL)) {}
}
#endif
//
//=========================================================================
//

int main(int argc, char **argv) {
    int j, fd;
    struct client *c;
    char pk_buf[8];

    // Set sane defaults

    view1090InitConfig();
    signal(SIGINT, sigintHandler); // Define Ctrl/C handler (exit program)

    // Parse the command line options
    for (j = 1; j < argc; j++) {
        int more = ((j + 1) < argc); // There are more arguments

        if        (!strcmp(argv[j],"--net-bo-port") && more) {
            Modes.net_input_beast_port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--port") && more) {
            Modes.net_input_beast_port = atoi(argv[++j]);
        } else if (!strcmp(argv[j],"--net-bo-ipaddr") && more) {
            strcpy(View1090.net_input_beast_ipaddr, argv[++j]);
        } else if (!strcmp(argv[j],"--server") && more) {
            strcpy(View1090.net_input_beast_ipaddr, argv[++j]);            
        } else if (!strcmp(argv[j],"--lat") && more) {
            Modes.fUserLat = atof(argv[++j]);
            appData.centerLat = Modes.fUserLat;
        } else if (!strcmp(argv[j],"--lon") && more) {
            Modes.fUserLon = atof(argv[++j]);
            appData.centerLon = Modes.fUserLon;
        } else if (!strcmp(argv[j],"--metric")) {
            Modes.metric = 1;
        } else if (!strcmp(argv[j],"--fullscreen")) {
            appData.fullscreen = 1;         
        } else if (!strcmp(argv[j],"--uiscale") && more) {
            appData.screen_uiscale = atoi(argv[++j]);   
         } else if (!strcmp(argv[j],"--screensize") && more) {
            appData.screen_width = atoi(argv[++j]);        
            appData.screen_height = atoi(argv[++j]);        
        } else if (!strcmp(argv[j],"--help")) {
            showHelp();
            exit(0);
        } else {
            fprintf(stderr, "Unknown or not enough arguments for option '%s'.\n\n", argv[j]);
            showHelp();
            exit(1);
        }
    }

    // Initialization
    view1090Init();

    // Try to connect to the selected ip address and port. We only support *ONE* input connection which we initiate.here.
    c = (struct client *) malloc(sizeof(*c));
    while(1) {
        if ((fd = setupConnection(c)) == ANET_ERR) {
            fprintf(stderr, "Waiting on %s:%d\n", View1090.net_input_beast_ipaddr, Modes.net_input_beast_port);     
            sleep(1);      
        } else {
            break;
        }
    }

    int go;
    
    init("sdl1090");
    
    atexit(cleanup);
        
    go = 1;
          
    while (go == 1)
    {
        getInput();
    
        interactiveRemoveStaleAircrafts();
    
        draw();

        if ((fd == ANET_ERR) || (recv(c->fd, pk_buf, sizeof(pk_buf), MSG_PEEK | MSG_DONTWAIT) == 0)) {
            free(c);
            usleep(1000000);
            c = (struct client *) malloc(sizeof(*c));
            fd = setupConnection(c);
            continue;
        }
        modesReadFromClient(c,"",decodeBinMessage);

        //usleep(10000);
    }
    
    // The user has stopped us, so close any socket we opened
    if (fd != ANET_ERR) 
      {close(fd);}

    return (0);
}
//
//=========================================================================
//
