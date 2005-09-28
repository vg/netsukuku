/*
 *  Copyright (C) 2004-2005 Alo Sarv <madcat_@users.sourceforge.net>
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

#ifndef __RESOLVER_UTILS_H__
#define __RESOLVER_UTILS_H__

#define CHECK_THROW_MSG(x, m) (x) || abort_(m)
#define CHECK_THROW(x) (x) || abort_("failed assertion")
#define CHECK(x) (x) || abort_("failed assertion")

int abort_(const char *);

/**
 * Various utility functions
 */
namespace Utils {
	/**
	 * A type to carry stream endianess
	 *
	 * @param Endianess	Utils::LITTLE_ENDIAN or Utils::BIG_ENDIAN
	 */
	template<bool Endianess>
	class EndianType { };

	/**
	 * An object who's carrying endianess information.
	 *
	 * @param T		Stream type
	 * @param Endianess	Utils::LITTLE_ENDIAN or Utils::BIG_ENDIAN
	 */
	template<typename T, bool Endianess>
	class EndianStream : public T, public EndianType<Endianess> {
	public:
		EndianStream() {}
		template<typename Arg1>
		EndianStream(Arg1 a1) : T(a1) {}
		template<typename Arg1, typename Arg2>
		EndianStream(Arg1 a1, Arg2 a2) : T(a1, a2) {}
		template<typename Arg1, typename Arg2, typename Arg3>
		EndianStream(Arg1 a1, Arg2 a2, Arg3 a3) : T(a1, a2, a3) {}
	};

	/**
	 * Endianess test. GetEndianInfo<T>::value returns the endianess (if no
	 * endianess is available, Utils::LITTLE_ENDIAN is assumed)
	 */
	template<typename T>
	struct GetEndianInfo {
		enum { value =
			::boost::is_base_and_derived<
				EndianType<BIG_ENDIAN>, T
			>::value
		};
	};

	/**
	 * Swaps endianess if swap is true for various type of data T via the
	 * static function swap.
	 */
	template<typename T, bool Swap>
	struct SwapData;

	//! @name Specializations for swapping various datas
	//@{
	// If host endianess is equal to stream's one, then do not swap
	template<typename T>
	struct SwapData<T, false> {
		static T swap(T t) { return t; };
	};

	// uint8_t never get swapped
	template<>
	struct SwapData<uint8_t, true> {
		static uint8_t swap(uint8_t t) { return t; };
	};
	// uint16_t
	template<>
	struct SwapData<uint16_t, true> {
		static uint16_t swap(uint16_t t) { return SWAP16_ALWAYS(t); }
	};
	// uint32_t
	template<>
	struct SwapData<uint32_t, true> {
		static uint32_t swap(uint32_t t) { return SWAP32_ALWAYS(t); }
	};
	// uint64_t
	template<>
	struct SwapData<uint64_t, true> {
		static uint64_t swap(uint64_t t) { return SWAP64_ALWAYS(t); }
	};
	// float
	template<>
	struct SwapData<float, true> {
		static float swap(float t) {
			return (float)SWAP32_ALWAYS((uint32_t)t);
		}
	};
	// unimplemented
	template<typename T>
	struct SwapData<T, true> {
		static T swap(T t) {
			BOOST_STATIC_ASSERT(sizeof(T::__type_not_supported__));
		}
	};

	//@}

	/**
	 * This function takes care of swapping data if streams need it.
	 */
	template<typename T, typename Stream>
	inline T swapHostToStream(T t) {
		// since values are boolean, !! avoids this warning on gcc4.0.0:
		// comparison between `enum Utils::GetEndianInfo<...>
		// ::<anonymous>` and `enum Utils::<anonymous>`

		return SwapData<
			T, !!GetEndianInfo<Stream>::value != !!HOST_ENDIAN
		>::swap(t);
	}

	/**
	 * Exception class, thrown when read methods detect attempt to read
	 * past end of stream. We use this exception instead of any of
	 * pre-defined standard exceptions to make explicit differenciating
	 * between stream I/O errors and other generic errors thrown by STL.
	 */
	class ReadError : public std::runtime_error {
	public:
		ReadError(const std::string &msg) : std::runtime_error(msg) {}
	};

	/**
	 * Generic getVal functor
	 */
	template<typename T = std::string>
	struct getVal {
		//! Conversion to T
		operator T() const { return m_value; }

		//! Explicit access to read value
		T value() const { return m_value; }

		//! Generic constructor
		template<typename Stream>
		getVal(Stream &s) {
			s.read(reinterpret_cast<char *>(&m_value), sizeof(T));

			if(!s.good()) {
				throw ReadError("unexpected end of stream");
			}
			m_value = swapHostToStream<T, Stream>(m_value);
		}
	private:
		T m_value;
	};

	/**
	 * std::string specialization of getVal
	 */
	template<>
	struct getVal<std::string> {
		//! Conversion to T
		operator std::string() const { return m_value; }

		//! Explicit access to read value
		std::string value() const { return m_value; }

		//! @name std::string getVal constructors
		//@{
		template<typename Stream>
		getVal(Stream &i) {
			uint16_t len = getVal<uint16_t>(i);
			boost::scoped_array<char> buf(new char[len]);
			i.read(buf.get(), len);

			if (!i.good()) {
				throw ReadError("unexpected end of stream");
			}

			m_value = std::string(buf.get(), len);
		}
		getVal(std::istream &i, uint32_t len) {
			boost::scoped_array<char> tmp(new char[len]);
			i.read(tmp.get(), len);

			if (!i.good()) {
                        	throw ReadError("unexpected end of stream");
			}

			m_value = std::string(tmp.get(), len);
		}
		//@}
	private:
		std::string m_value;
	};

	/**
	 * Generic putVal functor
	 */
	template<typename T = std::string>
	struct putVal {
		//! Generic constructor
		template<typename Stream>
		putVal(Stream &s, T t) {
			T tmp = swapHostToStream<T, Stream>(t);

			s.write(reinterpret_cast<char *>(&tmp), sizeof(T));
		}
	};

	/**
	 * std::string specialization of putVal
	 */
	template<>
	struct putVal<std::string> {
		//! @name std::string putVal constructors
		//@{
		template<typename Stream>
		putVal(Stream &o, const std::string &str) {
			putVal<uint16_t>(o, str.size());
			o.write(str.data(), str.size());
		}

		template<typename Stream>
		putVal(Stream &o, const std::string &str, uint32_t len) {
			o.write(str.data(), len);
		}

		template<typename Stream>
		putVal(Stream &o, const char *const str, uint32_t len) {
			CHECK_THROW(str);
			o.write(str, len);
		}

		template<typename Stream>
		putVal(Stream &o, const uint8_t *const str, uint32_t len) {
			CHECK_THROW(str);
			o.write(reinterpret_cast<const char*>(str), len);
		}

		template<typename Stream>
		putVal(
			Stream &o,
			const boost::shared_array<char> &str,
			uint32_t len
		) {
			CHECK_THROW(str);
			o.write(str.get(), len);
		}
		//@}
	};
} //! namespace Utils

#endif
