#pragma once

#include <xhacking/xHacking.h>
#include <xhacking/Utilities/Utilities.h>
#include <xhacking/Loader/Loader.h>
#include <xhacking/Detour/Detour.h>

extern xHacking::Detour<int, int, const char*, int, int>* sendDetour;
extern xHacking::Detour<int, int, char*, int, int>* recvDetour;

struct EntityPosition
{
	uint16_t x;
	uint16_t y;

	EntityPosition() :
		EntityPosition(0, 0)
	{}

	EntityPosition(uint16_t x, uint16_t y) :
		x(x), y(y)
	{}
};

struct Entity : public EntityPosition
{
	uint32_t type;
	uint32_t amount;

	Entity(uint32_t type, uint32_t amount, uint16_t x, uint16_t y) :
		type(type), amount(amount),
		EntityPosition(x, y)
	{}
};
