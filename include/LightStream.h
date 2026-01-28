#pragma once

#include <cstdint>
#include <memory>
//#include "logger.h"
#include "ls_config.h"

#ifndef FRAME_TYPE
enum class FrameType :uint8_t
{
	PING = 0x00
};
#endif
#ifndef FRAME_STATUS
enum class Status :uint8_t
{
	OK = 0x00,
	CRC_ERROR = 0x01,
	INVALID = 0xFF
};
#endif

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

