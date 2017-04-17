/*
*  Copyright (C) 2014-2017 Savoir-faire Linux Inc.
*  Author: Hana Ben Arab <hana.ben-arab@polymtl.ca>
*  Author: Abderrahmane Laribi <abderrahmane.laribi@polymtl.ca>
*  Author: Sonia Farrah <sonia.farrah@polymtl.ca>
*  Author: Ahmed Belhaouane <ahmed.belhaouane@polymtl.ca>
*  This program is free software; you can redistribute it and/or modify
*  it under the terms of the GNU General Public License as published by
*  the Free Software Foundation; either version 3 of the License, or
*  (at your option) any later version.
*
*  This program is distributed in the hope that it will be useful,
*  but WITHOUT ANY WARRANTY; without even the implied warranty of
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*  GNU General Public License for more details.
*
*  You should have received a copy of the GNU General Public License
*  along with this program; if not, write to the Free Software
*  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
*/

#pragma once

#include <opendht/dhtrunner.h>
#include <opendht/default_types.h>

#include <vector>
#include <map>
#include <chrono>
#include <list>
#include <future>

/**
* @file contactsmanager.h
* @brief Contacts Manager is used by Ring Account to handle contacts operations
*/


namespace ring {

	class Contact;
	class ContactsManager {

	private:
		std::string accountID_;
		std::string idPath_{};

	public:
		ContactsManager(const std::string& accountID);



		std::map<dht::InfoHash, Contact> contacts_;
		void loadContacts();
		void saveContacts() const;
		bool updateContact(const dht::InfoHash&, const Contact&, Contact& returnedContact);

		/**
		* Add contact to the account contact list.
		* Set confirmed if we know the contact also added us.
		*/
		Contact addContact(const std::string& uri, bool confirmed = false);
		Contact removeContact(const std::string& uri, bool banned = true);
		std::vector<std::map<std::string, std::string>> getContacts() const;

	};
}
