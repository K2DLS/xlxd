//
//  cclients.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 31/10/2015.
//  Copyright © 2015 Jean-Luc Deltombe (LX3JL). All rights reserved.
//  Copyright © 2018 Thomas A. Early, N7TAE
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

#include "main.h"
#include "creflector.h"
#include "cclients.h"


////////////////////////////////////////////////////////////////////////////////////////
// constructor


CClients::CClients()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// destructors

CClients::~CClients()
{
	RemoveAllClients();
}

void CClients::RemoveAllClients()
{
	Lock();
	while (! m_Clients.empty()) {
		auto it = m_Clients.begin();
		RemoveClient(*it);
	}
	Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// manage Clients

void CClients::AddClient(CClient *client)
{
	// first check if client already exists
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if (*client == *(*it)) {
			// delete new one
			delete client;
			//std::cout << "Adding existing client " << client->GetCallsign() << " at " << client->GetIp() << std::endl;
			return;
		}
	}

	// if not, append to the vector
	m_Clients.push_back(client);
	std::cout << "New client " << client->GetCallsign() << " at " << client->GetIp() << " added with protocol " << client->GetProtocolName();
	if ( client->GetReflectorModule() != ' ' ) {
		std::cout << " on module " << client->GetReflectorModule();
	}
	std::cout << std::endl;
	// notify
	g_Reflector.OnClientsChanged();
}

void CClients::RemoveClient(CClient *client)
{
	// look for the client
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		// compare objetc pointers
		if ( (*it) ==  client ) {
			// found it !
			if ( !(*it)->IsAMaster() ) {
				// remove it
				std::cout << "Client " << (*it)->GetCallsign() << " at " << (*it)->GetIp() << " removed with protocol " << client->GetProtocolName();
				if ( client->GetReflectorModule() != ' ' ) {
					std::cout << " on module " << client->GetReflectorModule();
				}
				std::cout << std::endl;
				delete *it;
				m_Clients.erase(it);
				// notify
				g_Reflector.OnClientsChanged();
				return;
			}
		}
	}
}

bool CClients::IsClient(CClient *client) const
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if (*it == client)
			return true;
	}
	return false;
}

////////////////////////////////////////////////////////////////////////////////////////
// find Clients

CClient *CClients::FindClient(const CIp &Ip)
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if ( (*it)->GetIp() == Ip ) {
			return *it;
		}
	}

	// done
	return NULL;
}

CClient *CClients::FindClient(const CIp &Ip, int Protocol)
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if ( ((*it)->GetIp() == Ip)  && ((*it)->GetProtocol() == Protocol)) {
			return *it;
		}
	}

	// done
	return NULL;
}

CClient *CClients::FindClient(const CIp &Ip, int Protocol, char ReflectorModule)
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if ( ((*it)->GetIp() == Ip) &&
				((*it)->GetReflectorModule() == ReflectorModule) &&
				((*it)->GetProtocol() == Protocol) ) {
			return *it;
		}
	}

	// done
	return NULL;
}

CClient *CClients::FindClient(const CCallsign &Callsign, const CIp &Ip, int Protocol)
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if ( (*it)->GetCallsign().HasSameCallsign(Callsign) &&
				((*it)->GetIp() == Ip)  &&
				((*it)->GetProtocol() == Protocol) ) {
			return *it;
		}
	}

	// done
	return NULL;
}

CClient *CClients::FindClient(const CCallsign &Callsign, char module, const CIp &Ip, int Protocol)
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if ( (*it)->GetCallsign().HasSameCallsign(Callsign) &&
				((*it)->GetModule() == module) &&
				((*it)->GetIp() == Ip)  &&
				((*it)->GetProtocol() == Protocol) ) {
			return *it;
		}
	}

	// done
	return NULL;
}

CClient *CClients::FindClient(const CCallsign &Callsign, int Protocol)
{
	for ( auto it=m_Clients.begin(); it!=m_Clients.end(); it++ ) {
		if ( ((*it)->GetProtocol() == Protocol) &&
				(*it)->GetCallsign().HasSameCallsign(Callsign) ) {
			return *it;
		}
	}

	// done
	return NULL;
}

////////////////////////////////////////////////////////////////////////////////////////
// iterate on clients

CClient *CClients::FindNextClient(int Protocol, std::list<CClient *>::iterator &it)
{
	while ( it != m_Clients.end() ) {
		if ( (*it)->GetProtocol() == Protocol ) {
			return *it++;
		}
		it++;
	}
	return NULL;
}

CClient *CClients::FindNextClient(const CIp &Ip, int Protocol, std::list<CClient *>::iterator &it)
{
	while ( it != m_Clients.end() ) {
		if ( ((*it)->GetProtocol() == Protocol) && ((*it)->GetIp() == Ip) ) {
			return *it++;
		}
		it++;
	}
	return NULL;
}

CClient *CClients::FindNextClient(const CCallsign &Callsign, const CIp &Ip, int Protocol, std::list<CClient *>::iterator &it)
{
	while ( it != m_Clients.end() ) {
		if ( ((*it)->GetProtocol() == Protocol) && ((*it)->GetIp() == Ip) && (*it)->GetCallsign().HasSameCallsign(Callsign) ) {
			return *it++;
		}
		it++;
	}
	return NULL;
}
