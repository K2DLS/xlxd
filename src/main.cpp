//
//  main.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//  Copyright © 2018 by Thomas A. Early, N7TAE
//
// ----------------------------------------------------------------------------
//    This file is part of xlxd.
//
//    xlxd is free software: you can redistribute it and/or modify
//    it under the terms of the GNU General Public License as published by
//    the Free Software Foundation, either version 3 of the License, or
//    (at your option) any later version.
//
//    xlxd is distributed in the hope that it will be useful,
//    but WITHOUT ANY WARRANTY; without even the implied warranty of
//    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//    GNU General Public License for more details.
//
//    You should have received a copy of the GNU General Public License
//    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "version.h"
#include "main.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// global objects

CReflector  g_Reflector;

////////////////////////////////////////////////////////////////////////////////////////
// function declaration

// signal catching function
static void sigCatch(int signum)
{
	/* do NOT do any serious work here */
	std::string sigstr;
	switch (signum) {
		case SIGTERM: sigstr = "SIGTERM"; break;
		case SIGHUP:  sigstr = "SIGHUP";  break;
		case SIGINT:  sigstr = "SIGINT";  break;
		case SIGQUIT: sigstr = "SIGQUIT"; break;
		default:      sigstr = "unknown"; break;
	}
	std::cout << sigstr << " signal caught, shutting down reflector..." << std::endl;
	return;
}

int main(int argc, const char *argv[])
{
	struct sigaction act;

	act.sa_handler = sigCatch;
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;

	if (sigaction(SIGTERM, &act, 0) != 0) {
		std::cerr << "sigaction-TERM failed" << std::endl;
		return EXIT_FAILURE;
	}

	if (sigaction(SIGINT, &act, 0) != 0) {
		std::cerr << "sigaction-INT failed" << std::endl;
		return EXIT_FAILURE;
	}

	if (sigaction(SIGHUP, &act, 0) != 0) {
		std::cerr << "sigaction-HUP failed" << std::endl;
		return EXIT_FAILURE;
	}

	if (sigaction(SIGQUIT, &act, 0) != 0) {
		std::cerr << "sigaction-QUIT failed" << std::endl;
		return EXIT_FAILURE;
	}

        // Check for valid arguments
	char REFLECTOR_CALLSIGN[8];
        char MY_IP_ADDRESS[16];
	strcpy(REFLECTOR_CALLSIGN, "CHNGME");
	strcpy(MY_IP_ADDRESS, "0.0.0.0");

#ifdef IS_XLX
	char TRANSCODER_IP_ADDRESS[16];
	strcpy(TRANSCODER_IP_ADDRESS, "127.0.0.1");

	if ( argc == 4 ) {
	    strcpy(REFLECTOR_CALLSIGN, argv[1]);
            strcpy(MY_IP_ADDRESS, argv[2]);
	    strcpy(TRANSCODER_IP_ADDRESS, argv[3]); 
	}
	else if ( argc == 3 ) {
	    strcpy(REFLECTOR_CALLSIGN, argv[1]);
            strcpy(MY_IP_ADDRESS, argv[2]);
	}
	else if ( argc == 2 ) {
            strcpy(REFLECTOR_CALLSIGN, argv[1]);
	}
        else if ( argc > 4) {
            std::cerr << "Usage: xlxd <reflname> <bind IP addr> <transcoder IP addr>\n" << std::endl;
            return EXIT_FAILURE;
        }

	printf("\nReflector Name: %s\n", REFLECTOR_CALLSIGN);
	printf("Bind IP Address: %s\n", MY_IP_ADDRESS);
	printf("Transcoder IP Address: %s\n", TRANSCODER_IP_ADDRESS);
#else
        if ( argc == 3 ) {
            strcpy(REFLECTOR_CALLSIGN, argv[1]);
            strcpy(MY_IP_ADDRESS, argv[2]);
	}
        else if ( argc == 2 ) {
            strcpy(REFLECTOR_CALLSIGN, argv[1]);
        }
        else if ( argc > 3) {
            std::cerr << "Usage: xrfd <reflname> <bind IP addr>\n" << std::endl;
            return EXIT_FAILURE;
        }

	printf("\nReflector Name: %s\n", REFLECTOR_CALLSIGN);
	printf("Bind IP Address: %s\n", MY_IP_ADDRESS);
#endif

	// delete the old pidfile. this will reset the web uptimer.
	if (::remove(PIDFILE_PATH))
		perror("Error deleting old pid file");	// it's okay if it doesn't yet exist

	// splash
#ifdef IS_XLX
	std::cout << "Starting xlxd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;
#else
	std::cout << "Starting xrfd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;
#endif

	// initialize reflector
	g_Reflector.SetCallsign(REFLECTOR_CALLSIGN);
	g_Reflector.SetListenIp(CIp(MY_IP_ADDRESS));
#ifdef IS_XLX
	g_Reflector.SetTranscoderIp(CIp(TRANSCODER_IP_ADDRESS));
#endif

	// and let it run
	if ( !g_Reflector.Start() ) {
		std::cerr << "Error starting reflector" << std::endl;
		return EXIT_FAILURE;
	}
	std::cout << "Reflector " << g_Reflector.GetCallsign() << "started and listening on " << g_Reflector.GetListenIp() << std::endl;

	// write new pid file
	std::ofstream ofs(PIDFILE_PATH, std::ofstream::out);
	ofs << ::getpid() << std::endl;
	ofs.close();

	pause(); // wait on a signal

	g_Reflector.Stop();	// clean-up!

	return EXIT_SUCCESS;
}
