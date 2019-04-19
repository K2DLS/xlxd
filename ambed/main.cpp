//
//  main.cpp
//  ambed
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/04/2017.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//
// ----------------------------------------------------------------------------
//    This file is part of ambed.
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

//#include <sys/stat.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#include "main.h"
#include "cambeserver.h"

////////////////////////////////////////////////////////////////////////////////////////
// global objects


////////////////////////////////////////////////////////////////////////////////////////
// function declaration

// signal catching function
static void sigCatch(int signum)
{
	// do NOT do any serious work here
	if ((signum == SIGTERM) || (signum == SIGINT))
		std::cout << "Signal caught, shutting down reflector..." << std::endl;
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

    // Check for valid IP argument
    char IP_ADDRESS[16];

    if ( argc == 2 ) {
        strcpy(IP_ADDRESS, argv[1]);
    }
    else if (argc == 1) {
        strcpy(IP_ADDRESS, "127.0.0.1");
    }

    // check for valid number of args
    if ( argc > 2 ) {
        std::cerr << "Usage: ambed IP.ADDR.TO.BIND" << std::endl;
        std::cerr << "IP Address Parameter is optional.  If ommited, ambed binds to all interfaces." << std::endl;
        return EXIT_FAILURE;
    }

    g_AmbeServer.SetListenIp(CIp(IP_ADDRESS));
    std::cout << "Setting IP address to " << IP_ADDRESS << std::endl;

    // initialize ambeserver
    g_AmbeServer.SetListenIp(CIp(IP_ADDRESS));

    // and let it run
    std::cout << "Starting AMBEd " << VERSION_MAJOR << "." << VERSION_MINOR << "." << VERSION_REVISION << std::endl << std::endl;

    if ( !g_AmbeServer.Start() )
    {
        std::cerr << "Error starting AMBEd" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "AMBEd started and listening on " << g_AmbeServer.GetListenIp() << std::endl;

    pause(); // wait for a signal

    // and wait for end
    g_AmbeServer.Stop();
    std::cout << "AMBEd stopped" << std::endl;

    // done
    return EXIT_SUCCESS;
}
