#pragma once

#include <cstdint>
#include <memory>
#include "logger.h"
#include "ls_config.h"

#ifdef UPDATE_FIRMWARE
#undef UPDATE_FIRMWARE
#endif
#ifdef FILE_ERROR
#undef FILE_ERROR
#endif
#ifdef FILE_NOT_FOUND
#undef FILE_NOT_FOUND
#endif

#define START_BYTE_CAT  0x41
#define START_BYTE_DOG  0x24

#define DEBUG_LS        0

#if DEBUG_LS
typedef struct ppp {
	uint8_t a = 1;
	uint16_t b = 1234;
	uint32_t c = 123456;
	uint16_t d = 4321;
}pp;

static void debug_loop()
{
	pp p{ 1, 123, 123456 };
	uint8_t data[256];
	uint8_t r;
	uint16_t len;
	uint16_t crc;
	for (;;)
	{
		static int ccc = 0;
		ccc++;
		if (ccc == 488)
			LOG(LOG_DEBUG, "time to crc error");
		data[0] = 65;
		data[1] = 36;
		data[2] = (uint8_t)FrameType::FILE_ERROR;
		r = rand() % 20;
		len = sizeof(p) * r;
		memcpy(&data[3], &len, 2);
		for (int i = 0; i < r; i++)
			memcpy(&data[5] + (i * sizeof(p)), &p, sizeof(p));

		//for (int i = 0; i < len + 3; i++)
		//      std::cout << std::hex << (int)data[2 + i] << " ";
		//std::cout << std::endl;

		crc = crcFast(&data[2], len + 3);
		//if ((len == 36 && crc == 32841) || (len == 0 && crc == 3569) || (len == 180 && crc == 3010) ||
		//      (len == 24 && crc == 13890))
		//      LOG(LOG_DEBUG, "time to get crc error");
		memcpy(&data[len + 5], &crc, 2);

		size_t a = lstream->Size();
		if (a >= len + 7)
		{
			memcpy(lstream->Buffer(), data, len + 7);
			lstream->Check(len + 7);
		}
		else
		{
			memcpy(lstream->Buffer(), data, a);
			lstream->Check(a);
			memcpy(lstream->Buffer(), data + a, len + 7 - a);
			lstream->Check(len + 7 - a);
		}
	}
}
#endif

enum class Status:uint8_t
{
	OK = 0, 
	CRC_ERROR		= 1,
	STORAGE_ERROR   = 2,
	SAME_VERSION,
	NO_NEED_UPLOAD,
	FILE_NOT_FOUND,
	FILE_READ_ERROR,
	FILE_SIZE,

	INVALID = 255
};

enum class CheckType:uint8_t {
	FILE, UPDATE, DOWNLOAD
};

enum class FileType :uint8_t {
	FIRMWARE, BOOTLOADER, EVENTS
};

enum class FrameType :uint8_t
{
    PING = 0x00,
    GET_MEASUREMENT		= 1,
    GET_SETTINGS,
    UPLOAD,
    UPDATE,
    MEASUREMENT,
    STATUS				= 6,
    SETTING,
    GET_FAULT_LIST,
    FAULT_LIST,
    GET_FAULT_REGISTER,
    FAULT_REGISTER,
    GET_SCATTER,
    SCATTER,
	CHECK				= 14,
	GET_SIGNAL,
	SIGNAL,
	RESET,
	SAVE_FILE,
	DOWNLOAD			= 19,
	FILE,

    PONG = 0x1F// we using 3 high-bits of type field to show protocol version so we can support 32 types
};

class LSBuff
{
    friend class LightStream;
public:
    LSBuff(size_t size)
		:
		position(0)
    {
    	ptr = new uint8_t[size];
    }
	uint8_t *End()
	{
		return (ptr+position);
	}
	uint8_t *Buffer()
	{
		return ptr;
	}
	size_t Size()
	{
		return position;
	}
private:
	uint8_t *ptr;
	size_t position;
};

class SFrame
{
    friend class LightStream;
public:
	uint8_t* buffer;
	size_t length;
    FrameType type;
    Status status;
private:
    uint8_t step, msg_type;
    uint16_t msg_len, first_piece_len, nread;
};

class IFrame
{
public:
	virtual ~IFrame(){};
    virtual void OnFrame(SFrame&) = 0;
};

class LightStream
{
public:
	LightStream(std::shared_ptr<IFrame>);
	uint8_t* Buffer();
	size_t Size();
	void Check(size_t);
	void InitFrame();
	void FrameHeader(FrameType, size_t);
	void PushDataToFrame(uint8_t*,size_t);
	LSBuff* Frame();

private:
	void Reset();
    std::shared_ptr<IFrame> upper;
	SFrame frame;
	uint8_t* rbuffer;
    int rposition;
    LSBuff wbuff;
};

