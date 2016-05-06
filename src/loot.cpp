#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>

#include "loot.hpp"
#include "player.hpp"
#include "sender.hpp"

#include <Tools/utils.h>
#include <Game/packet.h>
#include <Cryptography/login.h>
#include <Cryptography/game.h>


#ifdef max
#undef max
#undef min
#endif


void LootModule::onSend(NString packet)
{
}

void LootModule::onReceive(NString packet)
{
	std::string opcode = packet.tokens().str(0);

	if (opcode == "drop")
	{
		uint32_t owner = packet.tokens().from_int<uint32_t>(7);
		if (owner == _player->getIngameID())
		{
			uint32_t type = packet.tokens().from_int<uint32_t>(1);
			uint32_t id = packet.tokens().from_int<uint32_t>(2);
			uint16_t x = packet.tokens().from_int<uint16_t>(3);
			uint16_t y = packet.tokens().from_int<uint16_t>(4);
			uint32_t amount = packet.tokens().from_int<uint32_t>(5);

			std::lock_guard<std::mutex> lock(_dropsMutex);
			_drops.emplace(id, new Entity{ type, amount, x, y });
		}
	}
	else if (opcode == "get")
	{
		uint8_t confirm = packet.tokens().from_int<uint32_t>(1);
		uint32_t owner = packet.tokens().from_int<uint32_t>(2);
		uint32_t id = packet.tokens().from_int<uint32_t>(3);
		
		if (confirm == 1 && owner == _player->getIngameID() && id == _target)
		{
			_isLooting = false;

			std::lock_guard<std::mutex> lock(_dropsMutex);
			_drops.erase(id);
		}

		if (owner == _player->getIngameID())
		{
			_player->setBusy(false);
		}
	}
}

void LootModule::loot(uint32_t id)
{
	auto packet = NString("get 1 ") << _player->getIngameID() << ' ' << id;
	Sender::get()->send(_session, packet);
}

void LootModule::update()
{
	if (!_player->isAtLogin())
	{
		if (!_player->isBusy())
		{
			std::lock_guard<std::mutex> dropLock(_dropsMutex);
			if (!_drops.empty())
			{
				_isLooting = true;
				_player->setBusy(true);
				_target = _drops.begin()->first;
				loot(_target);
			}
		}
	}
}

