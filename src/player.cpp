#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>

#include "player.hpp"
#include "sender.hpp"

#include <Tools/utils.h>
#include <Game/packet.h>
#include <Cryptography/login.h>
#include <Cryptography/game.h>


void PlayerModule::onSend(NString packet)
{
	std::string opcode = packet.tokens().str(1);
	if (opcode == "walk")
	{
		_position.x = packet.tokens().from_int<uint16_t>(2);
		_position.y = packet.tokens().from_int<uint16_t>(3);
	}
}

void PlayerModule::onReceive(NString packet)
{
	std::string opcode = packet.tokens().str(0);

	if (opcode == "c_info")
	{
		int i = 1;
		int foundSlashes = 0;
		for (; i < packet.tokens().length() && foundSlashes < 2; ++i)
		{
			if (packet.tokens().str(i) == "-")
			{
				++foundSlashes;
			}
		}

		_ingameID = packet.tokens().from_int<uint32_t>(i);
	}
	else if (opcode == "at")
	{
		uint32_t id = packet.tokens().from_int<uint32_t>(1);
		if (id == _ingameID)
		{
			_position.x = packet.tokens().from_int<uint16_t>(3);
			_position.y = packet.tokens().from_int<uint16_t>(4);
		}
	}
}

const DWORD baseAddress = 0x686260;
bool PlayerModule::isAtLogin()
{
	DWORD pointer1 = *(DWORD*)(baseAddress);
	DWORD pointer2 = *(DWORD*)(pointer1);
	return *(BYTE*)(pointer2 + 0x31) == 0x00;
}