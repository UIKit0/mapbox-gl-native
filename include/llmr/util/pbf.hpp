#ifndef LLMR_UTIL_PBF
#define LLMR_UTIL_PBF

/*
 * Some parts are from upb - a minimalist implementation of protocol buffers.
 *
 * Copyright (c) 2008-2011 Google Inc.  See LICENSE for details.
 * Author: Josh Haberman <jhaberman@gmail.com>
 */

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <string>
// #include <cassert>

#define PBF_USE_SETJMP

#ifdef PBF_USE_SETJMP
#include <csetjmp>
#endif

namespace llmr {

struct pbf {
	inline pbf(const unsigned char *data, uint32_t length);

	inline bool next();
	template <typename T = uint32_t> inline T varint();
	template <typename T = uint32_t> inline T svarint();
	inline std::string string();
	inline float float32();
	inline double float64();
	// inline int64_t int64();
	inline bool boolean();

	inline void skip();
	inline void skipValue(uint32_t val);
	inline void skipBytes(uint32_t bytes);

	const uint8_t *data;
	const uint8_t *end;
	uint32_t value;
	uint32_t tag;

	enum error_code {
		UNTERMINATED_VARINT = 1,
		UNKNOWN_TYPE = 2,
		END_OF_BUFFER = 3
	};

	#ifdef PBF_USE_SETJMP
	std::jmp_buf jump_buffer;
	std::string msg;
	#endif
};

pbf::pbf(const unsigned char *data, uint32_t length)
	: data(data),
	  end(data + length)
{}

// pbf::pbf(const std::string& buffer)
// 	: data((const unsigned char *)buffer.data()),
// 	  end((const unsigned char *)buffer.data() + buffer.size())
// {}

bool pbf::next()
{
	if (data < end) {
		value = (uint32_t)varint();
		tag = value >> 3;
		return true;
	}
	return false;
}


template <typename T>
T pbf::varint()
{
	uint8_t byte = 0x80;
	T result = 0;
	int bitpos;
	for (bitpos = 0; bitpos < 70 && (byte & 0x80); bitpos += 7) {
		if (data >= end) {
			#ifdef PBF_USE_SETJMP
			msg = "unterminated varint, unexpected end of buffer";
			std::longjmp(jump_buffer, UNTERMINATED_VARINT);
			#endif
			exit(1);
		}
		result |= ((T)(byte = *data) & 0x7F) << bitpos;

		data++;
	}
	if (bitpos == 70 && (byte & 0x80)) {
		#ifdef PBF_USE_SETJMP
		msg = "unterminated varint (too long)";
		std::longjmp(jump_buffer, UNTERMINATED_VARINT);
		#endif
		exit(1);
	}

	return result;
}

template <typename T>
T pbf::svarint()
{
	T n = varint<T>();
	return (n >> 1) ^ -(T)(n & 1);
}

std::string pbf::string()
{
	uint32_t bytes = (uint32_t)varint();
	const char *string = (const char *)data;
	skipBytes(bytes);
	return std::string(string, bytes);
}

float pbf::float32()
{
	skipBytes(4);
	float result;
	memcpy(&result, data - 4, 4);
	return result;
}

double pbf::float64()
{
	skipBytes(8);
	double result;
	memcpy(&result, data - 8, 8);
	return result;
}

// int64_t pbf::int64()
// {
// 	return (int64_t)varint();
// }

bool pbf::boolean()
{
	skipBytes(1);
	return *(bool *)(data - 1);
}

// template <typename T, typename... Args> T *pbf::message(const Args&... args)
// {
// 	uint32_t bytes = (uint32_t)varint();
//     T *result = new T(data, bytes, args...);
//     skipBytes(bytes);
//     return result;
// }

void pbf::skip()
{
	skipValue(value);
}

void pbf::skipValue(uint32_t val)
{
	switch (val & 0x7) {
		case 0: // varint
			varint();
			break;
		case 1: // 64 bit
			skipBytes(8);
			break;
		case 2: // string/message
			skipBytes((uint32_t)varint());
			break;
		case 5: // 32 bit
			skipBytes(4);
			break;
		default:
			char text[32];
			snprintf(text, 32, "cannot skip unknown type %d", val & 0x7);
			#ifdef PBF_USE_SETJMP
			msg = text;
			std::longjmp(jump_buffer, UNKNOWN_TYPE);
			#endif
			exit(1);
	}
}

void pbf::skipBytes(uint32_t bytes)
{
	data += bytes;
	if (data > end) {
		#ifdef PBF_USE_SETJMP
		msg = "unexpected end of buffer";
		std::longjmp(jump_buffer, END_OF_BUFFER);
		#endif
		exit(1);
	}
}

} // end namespace llmr

#endif
