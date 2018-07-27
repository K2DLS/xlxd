//
//  cusers.cpp
//  xlxd
//
//  Created by Jean-Luc Deltombe (LX3JL) on 13/11/2015.
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
#include "cusers.h"
#include "creflector.h"

////////////////////////////////////////////////////////////////////////////////////////
// constructor

CUsers::CUsers()
{
}

////////////////////////////////////////////////////////////////////////////////////////
// users management

void CUsers::AddUser(const CUser &user)
{
	// add
	m_Users.push_front(user);

	// if list size too big, remove oldest
	while (m_Users.size() > LASTHEARD_USERS_MAX_SIZE)
		m_Users.pop_back();

	// notify
	g_Reflector.OnUsersChanged();
}

////////////////////////////////////////////////////////////////////////////////////////
// operation

void CUsers::Hearing(const CCallsign &my, const CCallsign &rpt1, const CCallsign &rpt2)
{
	Hearing(my, rpt1, rpt2, g_Reflector.GetCallsign());
}

void CUsers::Hearing(const CCallsign &my, const CCallsign &rpt1, const CCallsign &rpt2, const CCallsign &xlx)
{
	CUser heard(my, rpt1, rpt2, xlx);

	for (auto it=m_Users.begin(); it!=m_Users.end(); it++) {
		if (*it == heard) {
			(*it).HeardNow();
			m_Users.sort();
			return;
		}
	}

	// if not found, add user to list (it will go on the front as the most recent)
	AddUser(heard);
}

