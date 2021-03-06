//
//  creflector.cpp
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

#include "version.h"
#include "main.h"
#include <string.h>
#include "creflector.h"
#include "cgatekeeper.h"
#ifdef IS_XLX
#include "xlx/cdmriddirfile.h"
#include "xlx/cdmriddirhttp.h"
#include "xlx/ctranscoder.h"
#endif

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CReflector::CReflector()
{
	m_bStopThreads = false;
	m_XmlReportThread = NULL;
	m_JsonReportThread = NULL;
	for ( int i=0; i<NB_OF_MODULES; i++ ) {
		m_RouterThreads[i] = NULL;
	}
}

CReflector::CReflector(const CCallsign &callsign)
{
	m_bStopThreads = false;
	m_XmlReportThread = NULL;
	m_JsonReportThread = NULL;
	for ( int i=0; i<NB_OF_MODULES; i++ ) {
		m_RouterThreads[i] = NULL;
	}
	m_Callsign = callsign;
}

////////////////////////////////////////////////////////////////////////////////////////
// destructor

CReflector::~CReflector()
{
	m_bStopThreads = true;
	if ( m_XmlReportThread != NULL ) {
		m_XmlReportThread->join();
		delete m_XmlReportThread;
	}
	if ( m_JsonReportThread != NULL ) {
		m_JsonReportThread->join();
		delete m_JsonReportThread;
	}
	for ( int i=0; i<NB_OF_MODULES; i++ ) {
		if ( m_RouterThreads[i] != NULL ) {
			m_RouterThreads[i]->join();
			delete m_RouterThreads[i];
		}
	}
}


////////////////////////////////////////////////////////////////////////////////////////
// operation

bool CReflector::Start(void)
{
	// reset stop flag
	m_bStopThreads = false;

	// init gate keeper
	bool ok = g_GateKeeper.Init();
#ifdef IS_XLX
	// init dmrid directory
	g_DmridDir.Init();

	// init the transcoder
	g_Transcoder.Init();
#endif
	// create protocols
	ok &= m_Protocols.Init();

	// if ok, start threads
	if ( ok ) {
		// start one thread per reflector module
		for ( int i = 0; i < NB_OF_MODULES; i++ ) {
			m_RouterThreads[i] = new std::thread(CReflector::RouterThread, this, &(m_Streams[i]));
		}

		// start the reporting threads
		m_XmlReportThread = new std::thread(CReflector::XmlReportThread, this);
#ifdef JSON_MONITOR
		m_JsonReportThread = new std::thread(CReflector::JsonReportThread, this);
#endif
	} else {
		m_Protocols.Close();
	}

	// done
	return ok;
}

void CReflector::Stop(void)
{
	// stop & delete all threads
	m_bStopThreads = true;
	std::cout << "Stopping all CReflector Threads..." << std::endl;

	// stop & delete report threads
	if ( m_XmlReportThread != NULL ) {
		m_XmlReportThread->join();
		delete m_XmlReportThread;
		m_XmlReportThread = NULL;
		std::cout << "Stopped XmlReportThread." << std::endl;
	}
	if ( m_JsonReportThread != NULL ) {
		m_JsonReportThread->join();
		delete m_JsonReportThread;
		m_JsonReportThread = NULL;
		std::cout << "Stopped JsonReportThread." << std::endl;
	}

	// stop & delete all router thread
	for ( int i = 0; i < NB_OF_MODULES; i++ ) {
		if ( m_RouterThreads[i] != NULL ) {
			m_RouterThreads[i]->join();
			delete m_RouterThreads[i];
			m_RouterThreads[i] = NULL;
			std::cout << "Stopped Module " << (char)(i + 'A') << " thread." << std::endl;
		}
	}

	// close all Peers
	m_Peers.RemoveAllPeers();

	// close all clients
	m_Clients.RemoveAllClients();

	// close protocols
	m_Protocols.Close();

#ifdef IS_XLX
	// close transcoder
	g_Transcoder.Close();
	std::cout << "Closed Transcoder." << std::endl;
#endif

	// close gatekeeper
	g_GateKeeper.Close();
	std::cout << "Closed Gatekeeper." << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// stream opening & closing

bool CReflector::IsStreaming(char /*module*/)
{
	return false;
}

CPacketStream *CReflector::OpenStream(CDvHeaderPacket *DvHeader, CClient *client)
{
	CPacketStream *retStream = NULL;

	// clients MUST have been locked by the caller
	// so we can freely access it within the fuction

	// check sid is not NULL
	if ( DvHeader->GetStreamId() != 0 ) {
		// check if client is valid candidate
		if ( m_Clients.IsClient(client) && !client->IsAMaster() ) {
			// check if no stream with same streamid already open
			// to prevent loops
			if ( !IsStreamOpen(DvHeader) ) {
				// get the module's queue
				char module = DvHeader->GetRpt2Module();
				CPacketStream *stream = GetStream(module);
				if ( stream != NULL ) {
					// lock it
					stream->Lock();
					// is it available ?
					if ( stream->Open(*DvHeader, client) ) {
						// stream open, mark client as master
						// so that it can't be deleted
						client->SetMasterOfModule(module);

						// update last heard time
						client->Heard();
						retStream = stream;

						// and push header packet
						stream->Push(DvHeader);

						// report
						std::cout << "Opening stream on module " << module << " for client " << client->GetCallsign() << " with sid " << DvHeader->GetStreamId() << " by user " << DvHeader->GetMyCallsign() << std::endl;

						// notify
						g_Reflector.OnStreamOpen(stream->GetUserCallsign());

					}
					// unlock now
					stream->Unlock();
				}
			} else {
				// report
				std::cerr << "Detected stream loop on module " << DvHeader->GetRpt2Module() << " for client " << client->GetCallsign() << " with sid " << DvHeader->GetStreamId() << std::endl;
			}
		}
	}

	// done
	return retStream;
}

void CReflector::CloseStream(CPacketStream *stream)
{
	//
	if ( stream != NULL ) {
		// wait queue is empty
		// this waits forever
		bool bEmpty = false;
		do {
			stream->Lock();
			// do not use stream->IsEmpty() has this "may" never succeed
			// and anyway, the DvLastFramPacket short-circuit the transcoder
			// loop queues
			bEmpty = stream->empty();
			stream->Unlock();
			if ( !bEmpty ) {
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
			}
			if (m_bStopThreads) {
				stream->Close();
				return;
			}
		} while (!bEmpty);

		// lock clients
		GetClients();

		// lock stream
		stream->Lock();

		// get and check the master
		CClient *client = stream->GetOwnerClient();
		if ( client != NULL ) {
			// client no longer a master
			client->NotAMaster();

			// notify
			g_Reflector.OnStreamClose(stream->GetUserCallsign());

			std::cout << "Closing stream of module " << GetStreamModule(stream) << std::endl;
		}

		// release clients
		ReleaseClients();

		// unlock before closing
		// to avoid double lock in assiociated
		// codecstream close/thread-join
		stream->Unlock();

		// and stop the queue
		stream->Close();


	}
}

////////////////////////////////////////////////////////////////////////////////////////
// router threads

void CReflector::RouterThread(CReflector *This, CPacketStream *streamIn)
{
	// get our module
	uint8 uiModuleId = This->GetStreamModule(streamIn);

	// get on input queue
	CPacket *packet;

	while ( !This->m_bStopThreads ) {
		// any packet in our input queue ?
		streamIn->Lock();
		if ( !streamIn->empty() ) {
			// get the packet
			packet = streamIn->front();
			streamIn->pop();
		} else {
			packet = NULL;
		}
		streamIn->Unlock();

		// route it
		if ( packet != NULL ) {
			// set origin
			packet->SetModuleId(uiModuleId);

			// iterate on all protocols
			for ( int i=0; i<This->m_Protocols.Size(); i++ ) {
				// duplicate packet
				CPacket *packetClone = packet->Duplicate();

				// get protocol
				CProtocol *protocol = This->m_Protocols.GetProtocol(i);

				// if packet is header, update RPT2 according to protocol
				if ( packetClone->IsDvHeader() ) {
					// get our callsign
					CCallsign csRPT = protocol->GetReflectorCallsign();
					csRPT.SetModule(This->GetStreamModule(streamIn));
					((CDvHeaderPacket *)packetClone)->SetRpt2Callsign(csRPT);
				}

				// and push it
				CPacketQueue *queue = protocol->GetQueue();
				queue->push(packetClone);
				protocol->ReleaseQueue();
			}
			// done
			delete packet;
			packet = NULL;
		} else {
			// wait a bit
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// report threads

void CReflector::XmlReportThread(CReflector *This)
{
	while ( !This->m_bStopThreads ) {
		// report to xml file
		std::ofstream xmlFile;
		xmlFile.open(XML_PATH, std::ios::out | std::ios::trunc);
		if ( xmlFile.is_open() ) {
			// write xml file
			This->WriteXmlFile(xmlFile);

			// and close file
			xmlFile.close();
		} else {
			std::cerr << "Failed to open " << XML_PATH  << std::endl;
		}

		// and wait a bit
		std::this_thread::sleep_for(std::chrono::seconds(XML_UPDATE_PERIOD));
	}
}

void CReflector::JsonReportThread(CReflector *This)
{
	CUdpSocket Socket;
	CBuffer    Buffer;
	CIp        Ip;
	bool       bOn;

	// init variable
	bOn = false;

	// create listening socket
	if ( Socket.Open(JSON_PORT) ) {
		// and loop
		while ( !This->m_bStopThreads ) {
			// any command ?
			if ( Socket.Receive(&Buffer, &Ip, 50) != -1 ) {
				// check verb
				if ( 0 == Buffer.Compare((uint8 *)"hello", 5) ) {
					std::cout << "Monitor socket connected with " << Ip << std::endl;

					// connected
					bOn = true;

					// announce ourselves
					This->SendJsonReflectorObject(Socket, Ip);

					// dump tables
					This->SendJsonNodesObject(Socket, Ip);
					This->SendJsonStationsObject(Socket, Ip);
				} else if ( Buffer.Compare((uint8 *)"bye", 3) == 0 ) {
					std::cout << "Monitor socket disconnected" << std::endl;

					// diconnected
					bOn = false;
				}
			}

			// any notifications ?
			CNotification notification;
			This->m_Notifications.Lock();
			if ( !This->m_Notifications.empty() ) {
				// get the packet
				notification = This->m_Notifications.front();
				This->m_Notifications.pop();
			}
			This->m_Notifications.Unlock();

			// handle it
			if ( bOn ) {
				switch ( notification.GetId() ) {
				case NOTIFICATION_CLIENTS:
				case NOTIFICATION_PEERS:
					//std::cout << "Monitor updating nodes table" << std::endl;
					This->SendJsonNodesObject(Socket, Ip);
					break;
				case NOTIFICATION_USERS:
					//std::cout << "Monitor updating stations table" << std::endl;
					This->SendJsonStationsObject(Socket, Ip);
					break;
				case NOTIFICATION_STREAM_OPEN:
					//std::cout << "Monitor notify station " << notification.GetCallsign() << "going ON air" << std::endl;
					This->SendJsonStationsObject(Socket, Ip);
					This->SendJsonOnairObject(Socket, Ip, notification.GetCallsign());
					break;
				case NOTIFICATION_STREAM_CLOSE:
					//std::cout << "Monitor notify station " << notification.GetCallsign() << "going OFF air" << std::endl;
					This->SendJsonOffairObject(Socket, Ip, notification.GetCallsign());
					break;
				case NOTIFICATION_NONE:
				default:
					// nothing to do, just sleep a bit
					std::this_thread::sleep_for(std::chrono::milliseconds(250));
					break;
				}
			}
		}
	} else {
		std::cerr << "Error creating monitor socket" << std::endl;
	}
}

////////////////////////////////////////////////////////////////////////////////////////
// notifications

void CReflector::OnPeersChanged(void)
{
	CNotification notification(NOTIFICATION_PEERS);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnClientsChanged(void)
{
	CNotification notification(NOTIFICATION_CLIENTS);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnUsersChanged(void)
{
	CNotification notification(NOTIFICATION_USERS);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnStreamOpen(const CCallsign &callsign)
{
	CNotification notification(NOTIFICATION_STREAM_OPEN, callsign);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

void CReflector::OnStreamClose(const CCallsign &callsign)
{
	CNotification notification(NOTIFICATION_STREAM_CLOSE, callsign);

	m_Notifications.Lock();
	m_Notifications.push(notification);
	m_Notifications.Unlock();
}

////////////////////////////////////////////////////////////////////////////////////////
// modules & queues

int CReflector::GetModuleIndex(char module) const
{
	int i = (int)module - (int)'A';
	if ( (i < 0) || (i >= NB_OF_MODULES) ) {
		i = -1;
	}
	return i;
}

CPacketStream *CReflector::GetStream(char module)
{
	int i = GetModuleIndex(module);
	if ( i >= 0 ) {
		return &(m_Streams[i]);
	}
	return NULL;
}

bool CReflector::IsStreamOpen(const CDvHeaderPacket *DvHeader)
{
	for ( unsigned int i=0; i<m_Streams.size(); i++  ) {
		if ( (m_Streams[i].GetStreamId()==DvHeader->GetStreamId()) && m_Streams[i].IsOpen() )
			return true;
	}
	return false;
}

char CReflector::GetStreamModule(CPacketStream *stream)
{
	for ( unsigned int i=0; i<m_Streams.size(); i++ ) {
		if ( &(m_Streams[i]) == stream ) {
			return GetModuleLetter(i);
		}
	}
	return ' ';
}

////////////////////////////////////////////////////////////////////////////////////////
// xml helpers

void CReflector::WriteXmlFile(std::ofstream &xmlFile)
{
	// write header
	xmlFile << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" << std::endl;

	// software version
	char sz[64];
	::sprintf(sz, "%d.%d.%d", VERSION_MAJOR, VERSION_MINOR, VERSION_REVISION);
	xmlFile << "<Version>" << sz << "</Version>" << std::endl;

	// linked peers
	xmlFile << "<" << m_Callsign << "linked peers>" << std::endl;
	// lock
	CPeers *peers = GetPeers();
	// iterate on peers
	auto pit = peers->InitPeerIterator();
	while (NULL != peers->GetPeer(pit)) {
		peers->GetPeer(pit++)->WriteXml(xmlFile);
	}
	// unlock
	ReleasePeers();
	xmlFile << "</" << m_Callsign << "linked peers>" << std::endl;

	// linked nodes
	xmlFile << "<" << m_Callsign << "linked nodes>" << std::endl;
	// lock
	CClients *clients = GetClients();
	// iterate on clients
	auto cit = clients->InitClientIterator();
	CClient *client;
	while ( NULL != (client = clients->GetClient(cit)) ) {
		if ( client->IsNode() ) {
			client->WriteXml(xmlFile);
		}
		cit++;
	}
	// unlock
	ReleaseClients();
	xmlFile << "</" << m_Callsign << "linked nodes>" << std::endl;

	// last heard users
	xmlFile << "<" << m_Callsign << "heard users>" << std::endl;
	// lock
	CUsers *users = GetUsers();
	// iterate on users
	auto uit = users->InitUserIterator();
	CUser *user;
	while ( NULL != (user = users->GetUser(uit)) ) {
		user->WriteXml(xmlFile);
		uit++;
	}
	// unlock
	ReleaseUsers();
	xmlFile << "</" << m_Callsign << "heard users>" << std::endl;
}

////////////////////////////////////////////////////////////////////////////////////////
// json helpers

void CReflector::SendJsonReflectorObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[1024];
	char cs[CALLSIGN_LEN+1];
	char mod[8] = "\"A\"";

	// get reflector callsign
	m_Callsign.GetCallsign((uint8 *)cs);
	cs[CALLSIGN_LEN] = 0;

	// build string
	::sprintf(Buffer, "{\"reflector\":\"%s\",\"modules\":[", cs);
	for ( int i = 0; i < NB_OF_MODULES; i++ ) {
		::strcat(Buffer, mod);
		mod[1]++;
		if ( i < NB_OF_MODULES-1 ) {
			::strcat(Buffer, ",");
		}
	}
	::strcat(Buffer, "]}");

	// and send
	Socket.Send(Buffer, Ip);
}

#define JSON_NBMAX_NODES	250

void CReflector::SendJsonNodesObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[12+(JSON_NBMAX_NODES*94)];

	// nodes object table
	::sprintf(Buffer, "{\"nodes\":[");
	// lock
	CClients *clients = GetClients();
	// iterate on clients
	auto it = clients->InitClientIterator();
	CClient *client = clients->GetClient(it);
	int i = 0;
	while ( (NULL != client) && (i < JSON_NBMAX_NODES) ) {
		client->GetJsonObject(Buffer);
		client = clients->GetClient(++it);
		if ( (NULL != client) && (++i < JSON_NBMAX_NODES) ) {
			::strcat(Buffer, ",");
		}
	}
	// unlock
	ReleaseClients();
	::strcat(Buffer, "]}");

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonStationsObject(CUdpSocket &Socket, CIp &Ip)
{
	char Buffer[15+(LASTHEARD_USERS_MAX_SIZE*94)];

	// stations object table
	::sprintf(Buffer, "{\"stations\":[");

	// lock
	CUsers *users = GetUsers();
	// iterate on users
	auto it = users->InitUserIterator();
	CUser *user = users->GetUser(it);
	while ( NULL != user ) {
		user->GetJsonObject(Buffer);
		user = users->GetUser(++it);
		if ( NULL != user ) {
			::strcat(Buffer, ",");
		}
	}
	// unlock
	ReleaseUsers();

	::strcat(Buffer, "]}");

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonOnairObject(CUdpSocket &Socket, CIp &Ip, const CCallsign &Callsign)
{
	char Buffer[128];
	char sz[CALLSIGN_LEN+1];

	// onair object
	Callsign.GetCallsignString(sz);
	::sprintf(Buffer, "{\"onair\":\"%s\"}", sz);

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}

void CReflector::SendJsonOffairObject(CUdpSocket &Socket, CIp &Ip, const CCallsign &Callsign)
{
	char Buffer[128];
	char sz[CALLSIGN_LEN+1];

	// offair object
	Callsign.GetCallsignString(sz);
	::sprintf(Buffer, "{\"offair\":\"%s\"}", sz);

	// and send
	//std::cout << Buffer << std::endl;
	Socket.Send(Buffer, Ip);
}
