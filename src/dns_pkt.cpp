/**
 *  Copyright (C) 2005 Andrea Leofreddi <andrea.leofreddi@libero.it>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* Hydranode's resolver hack for ANDNA */

#include <iostream>
#include <string>
#include <sstream>
#include <iterator>
#include <map>

#include <boost/format.hpp>
#include <boost/iterator.hpp>
#include <boost/iterator/reverse_iterator.hpp>
#include <boost/tuple/tuple.hpp>
#include <boost/type_traits.hpp>
#include <boost/scoped_array.hpp>
#include <boost/shared_array.hpp>
#include <boost/tokenizer.hpp>

#include <netinet/in.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#include <dns_rendian.h>
#include <dns_utils.h>

int abort_(const char *msg) {
	std::cerr << msg << std::endl;

	exit(1);
}

#define TRACE_RESOLVER 0
#ifndef DEBUG
# define logTrace(x, y)
#else
void logTrace(int dummy, std::string s) {
        std::cout << s << std::endl;
}

void logTrace(int dummy, boost::format f) {
        logTrace(0, f.str());
}
#endif

// Do not mangle this
extern "C" {
void resolver_process(
        const char *question, unsigned question_length,
        char *answer, unsigned *answer_length,
        int (*callback)(const char *name, uint32_t *ip)
);
};

/**
 * Query types
 */
enum DNSType {
	TYPE_A          = 1,  // hostname->ip resolution
	TYPE_NS         = 2,  // domain->nameserver resolution
	TYPE_CNAME      = 5,  // hostname->hostname resolution (alias)
	TYPE_SOA        = 6,  // start of authority (not implemented)
	TYPE_PTR        = 12, // ip->hostname resolution
	TYPE_MX         = 15, // domain->mail server resolution
	TYPE_TXT        = 16  // comment text
};

/**
 * DNS Class
 */
enum DNSClass {
	CLASS_IN = 1, // the Internet
	CLASS_CS = 2, // the CSNET
	CLASS_CH = 3, // the CHAOS
	CLASS_HS = 4  // Hesiod
};

// Forward declaration
class DNSPacket;

/** DNS Packet Question */
struct Question {
	//! Hostname
	std::string m_name;

	//! Question type and class
	uint16_t m_type, m_class;

	/**
	 * Write to stream
	 */
	template<typename T>
	void write(T &, DNSPacket &);

	/**
	 * Read from stream
	 */
	template<typename T>
	void read(T &, DNSPacket &);

	/**
	 * Default constructor
	 */
	Question(
		std::string name = std::string(),
		DNSType type = TYPE_A,
		DNSClass class_ = CLASS_IN
	) : m_name(name), m_type(type), m_class(class_) { }
};

/** DNS Packet Answer */
struct Answer {
	std::string m_name;

	uint16_t m_type;
	uint16_t m_class;
	uint32_t m_ttl;
	uint32_t m_addr;

	/**
	 * Write to stream
	 */
	template<typename T>
	void write(T &, DNSPacket &);

	/**
	 * Read from stream
	 */
	template<typename T>
	void read(T &, DNSPacket &);

	Answer() : m_type(TYPE_A), m_class(CLASS_IN), m_ttl(0), m_addr(0) { }
};

/** DNS packet */
struct DNSPacket {
	enum RCode {
		_NO_ERROR       = 0,
		FORMAT_ERROR    = 1,
		SERVER_FAILURE  = 2,
		NAME_ERROR      = 3,
		NOT_IMPLEMENTED = 4,
		REFUSED         = 5
	};

	//! Packet id
	uint16_t m_id;

	//! Packet flags
	uint16_t m_flags;

	//! Packet questions
	std::vector<Question> questions;

	//! Packet answers
	std::vector<Answer> answers;

	//! Hostnames map (for DNSName pointers)
	std::map<uint16_t, std::string> labels;

	/// Stream offset at start of packet
	std::istream::pos_type initOffset;

	/**
	 * Read a DNSName from a stream
	 */
	template<typename T>
	std::string getDNSName(T &s);

	/**
	 * Write a DNSName to a stream
	 */
	template<typename T>
	void putDNSName(std::string name, T &s);

	/**
	 * Write the whole packet to a stream
	 */
	template<typename T>
	void write(T &s);

	/**
	 * Read the whole packet from a stream
	 */
	template<typename T>
	void read(T &s);

	/**
	 * Constructor
	 */
	DNSPacket(uint32_t id = 0) : m_id(id)
	{ }
};

/**
 * Read a DNSName from a stream
 */
template<typename T>
std::string DNSPacket::getDNSName(T &s) {
	uint8_t ol = 0;
	std::string r;
	std::vector<uint16_t> labelOffset;

	std::istream::pos_type pos = s.tellg();

	while((ol = Utils::getVal<uint8_t>(s))) {
		if(ol & 0xc0) {
			uint16_t ptr = (
				uint16_t(ol & ~0xc0) << 8
				| Utils::getVal<uint8_t>(s)
			);

			std::map<uint16_t, std::string>::iterator itor;
			itor = labels.find(ptr);
			CHECK_THROW_MSG(
				itor != labels.end(),
				"Unable to parse malformed "
				"DNSPacket (invalid pointer)"
			);

			r += itor->second;

			break;
		} else {
			r += Utils::getVal<std::string>(s, ol);
			r += ".";

			labelOffset.push_back(s.tellg() - pos);
		}
	}

	// Throw the last (null data) label
	if(labelOffset.size())
		labelOffset.pop_back();

	labels[uint16_t(pos - initOffset)] = r;
	for(
		std::vector<uint16_t>::iterator itor = labelOffset.begin();
		itor != labelOffset.end();
		++itor
	) {
		labels[
			uint16_t(pos + std::istream::pos_type(*itor)-initOffset)
		] = std::string(r.begin() + *itor, r.end());
	}

	CHECK_THROW(r.size());

	// Remove final '.'
	return r.substr(0, r.size() - 1);
}

/**
 * Write a DNSName to a stream
 */
template<typename T>
void DNSPacket::putDNSName(std::string t, T &s) {
	t.insert(0, 1, ' ');

	std::string::size_type l = t.length(), i(0), j;

	for(;;) {
		j = i;

		if((i = t.find('.', i)) == std::string::npos) {
			t[j] = uint8_t(l - j - 1);

			break;
		} else {
			t[j] = uint8_t(i - j - 1);
		}
	}

	s << t;
	Utils::putVal<uint8_t>(s, 0);
}

/**
 * Write the whole packet to a stream
 */
template<typename T>
void DNSPacket::write(T &s) {
	Utils::putVal<uint16_t>(s, m_id);             //!< id
	Utils::putVal<uint16_t>(s, m_flags);          //!< flags
	Utils::putVal<uint16_t>(s, questions.size()); //!< m_qdcount
	Utils::putVal<uint16_t>(s, answers.size());   //!< m_ancount
	Utils::putVal<uint16_t>(s, 0);                //!< m_nscount
	Utils::putVal<uint16_t>(s, 0);                //!< m_arcount

	// Send questions
	for(
		std::vector<Question>::iterator i = questions.begin();
		i != questions.end(); ++i
	) {
		i->write(s, *this);
	}

	// Send answers
	for(
		std::vector<Answer>::iterator i = answers.begin();
		i != answers.end(); ++i
	) {
		i->write(s, *this);
	}
}

/**
 * Read the whole packet from a stream
 */
template<typename T>
void DNSPacket::read(T &s) {
	uint16_t m_flags, m_qdcount, m_ancount, m_nscount, m_arcount;

	// Save initial offset of this packet
	initOffset = s.tellg();

	m_id = Utils::getVal<uint16_t>(s);
	m_flags = Utils::getVal<uint16_t>(s);
	m_qdcount = Utils::getVal<uint16_t>(s);
	m_ancount = Utils::getVal<uint16_t>(s);
	m_nscount = Utils::getVal<uint16_t>(s);
	m_arcount = Utils::getVal<uint16_t>(s);

	uint8_t rcode = (m_flags & 0xf);

	logTrace(
		TRACE_RESOLVER, boost::format(
			"DNSPacket::read: got packet (id %i, flags %i, qdcount "
			"%i, ancount %i, nscount %i, arcount %i, rcode %i)"
		) % m_id % m_flags % m_qdcount % m_ancount
		% m_nscount % m_arcount % int(rcode)
	);

	bool aliasSearch(false);

	switch(rcode) {
		case REFUSED:
			logTrace(
				TRACE_RESOLVER, "DNSPacket::read: query refused"
			);

			return;

		case _NO_ERROR:
		case NAME_ERROR:
			logTrace(TRACE_RESOLVER,
				"DNSPacket::read: No error "
				"or name error (aliases)"
			);

			aliasSearch = true;
			break;

		case FORMAT_ERROR:
			logTrace(
				TRACE_RESOLVER, "DNSPacket::read: format error"
			);
			break;

		default:
			logTrace(TRACE_RESOLVER,
				"DNSPacket::read: not implemented answer type"
			);
			return;
	}

	// Receive questions
	while(m_qdcount--) {
		questions.push_back(Question());

		questions.back().read(s, *this);
	}

	// Receive answers
	while(m_ancount--) {
		answers.push_back(Answer());

		answers.back().read(s, *this);
	}
}

template<typename T>
void Question::write(T &stream, DNSPacket &packet) {
	packet.putDNSName(m_name, stream);

	Utils::putVal<uint16_t>(stream, m_type);
	Utils::putVal<uint16_t>(stream, m_class);
}

template<typename T>
void Question::read(T &stream, DNSPacket &packet) {
	m_name = packet.getDNSName(stream);

	m_type = Utils::getVal<uint16_t>(stream);
	m_class = Utils::getVal<uint16_t>(stream);
}

template<typename T>
void Answer::read(T &stream, DNSPacket &packet) {
	std::string m_name = packet.getDNSName(stream);
	m_type = Utils::getVal<uint16_t>(stream);
	m_class = Utils::getVal<uint16_t>(stream);
	m_ttl = Utils::getVal<uint32_t>(stream);
	uint16_t rdlength = Utils::getVal<uint16_t>(stream);

	switch(m_type) {
		case TYPE_A: 
			{
				uint32_t addr32 = Utils::getVal<uint32_t>(stream);

				// Force swap of this on LITTLE ENDIAN?
				addr32 = SWAP32_ON_LE(addr32);
			}
			break;

		default:
			logTrace(TRACE_RESOLVER,
				boost::format(
					"Answer::read: answer is of unknown "
					"type %i, skipping %i bytes"
				) % m_type % rdlength
			);
			Utils::getVal<std::string>(stream, rdlength);
	}
}

template<typename T>
void Answer::write(T &stream, DNSPacket &packet) {
	packet.putDNSName(m_name, stream);

	Utils::putVal<uint16_t>(stream, m_type);
	Utils::putVal<uint16_t>(stream, m_class);
	Utils::putVal<uint32_t>(stream, m_ttl);

	switch(m_type) {
		case TYPE_A: 
			{
				Utils::putVal<uint16_t>(stream, 4); // rdlength
				Utils::putVal<uint32_t>(stream, SWAP32_ON_LE(m_addr));
			}
			break;

		default:
			CHECK(0);
	}
}

void resolver_process(
        const char *question, unsigned question_length,
        char *answer, unsigned *answer_length,
        int (*callback)(const char *name, uint32_t *ip)
) {
	DNSPacket questionPacket, answerPacket;

	// (1) Read question
	Utils::EndianStream<std::stringstream, BIG_ENDIAN> ss(std::string(question, question_length));
	questionPacket.read(ss);

	// (2) Build answer
	answerPacket.m_id = questionPacket.m_id;
	//answerPacket.m_flags = 0x80 | (0xf << 11);
	answerPacket.m_flags = 0x8000;
	//questionPacket.m_flags

	std::vector<Question>::iterator iter;
	for(iter = questionPacket.questions.begin(); iter != questionPacket.questions.end(); ++iter) {
		if(iter->m_type == TYPE_A) {
			answerPacket.questions.push_back(*iter);
			
			in_addr_t addr;
			if(callback(iter->m_name.c_str(), &addr)) {
				Answer answerData;
				answerData.m_name = iter->m_name;
				answerData.m_ttl = 3600; // 1 hour 

				answerData.m_addr = addr;

				answerPacket.answers.push_back(answerData);
			}
		}
	}

	// (3) Serialize answer
	ss.str(std::string());
	answerPacket.write(ss);

	std::string out(ss.str());
	memcpy(answer, out.data(), out.size() < *answer_length ? out.size() : *answer_length);

	*answer_length = out.size();
}

