#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>

#include "attack.hpp"
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


void AttackModule::onSend(NString packet)
{
}

void AttackModule::onReceive(NString packet)
{
	std::string opcode = packet.tokens().str(0);

	if (opcode == "c_map")
	{
		std::lock_guard<std::mutex> lock(_entitiesMutex);

		for (auto&& it : _entities)
		{
			delete it.second;
		}

		_entities.clear();
	}
	else if (opcode == "in")
	{
		if (packet.tokens().from_int<int>(1) == 3)
		{
			std::lock_guard<std::mutex> lock(_entitiesMutex);

			uint32_t id = packet.tokens().from_int<uint32_t>(3);
			_entities.emplace(id, new EntityPosition{ packet.tokens().from_int<uint16_t>(4), packet.tokens().from_int<uint16_t>(5) });
		}
	}
	else if (opcode == "mv")
	{
		if (packet.tokens().from_int<int>(1) == 3)
		{
			std::lock_guard<std::mutex> lock(_entitiesMutex);

			uint32_t id = packet.tokens().from_int<uint32_t>(2);
			auto it = _entities.find(id);
			if (it != _entities.end())
			{
				it->second->x = packet.tokens().from_int<uint16_t>(3);
				it->second->y = packet.tokens().from_int<uint16_t>(4);
			}
		}
	}
	else if (opcode == "st")
	{
		if (packet.tokens().from_int<int>(1) == 3)
		{
			uint32_t id = packet.tokens().from_int<uint32_t>(2);
			int hp = packet.tokens().from_int<int>(7);

			if (id == _target)
			{
				if (hp <= 0)
				{
					_isAttacking = false;
					_player->setBusy(false);
				}
			}

			if (hp <= 0)
			{
				std::lock_guard<std::mutex> lock(_entitiesMutex);

				_entities.erase(id);
			}
		}
	}
}

void AttackModule::target(uint32_t id)
{
	auto packet = NString("ncif 3 ") << id;
	Sender::get()->send(_session, packet);
}

void AttackModule::attack(uint32_t id)
{
	auto packet = NString("u_s 0 3 ") << id;
	Sender::get()->send(_session, packet);
}

void AttackModule::update()
{
	if (!_player->isAtLogin())
	{
		if (!_player->isBusy())
		{
			std::lock_guard<std::mutex> entitiesLock(_entitiesMutex);

			int dist = std::numeric_limits<int>::max();
			for (auto&& it : _entities)
			{
				EntityPosition& position = _player->getPosition();
				int currentDist = ((int)position.x - (int)it.second->x) * ((int)position.x - (int)it.second->x) +
					((int)position.y - (int)it.second->y) * ((int)position.y - (int)it.second->y);

				if (currentDist < dist)
				{
					dist = currentDist;
					_target = it.first;
				}
			}

			if (dist < 15)
			{
				_isAttacking = true;
				_player->setBusy(true);
			}
		}
		
		if (_isAttacking)
		{
			if (std::chrono::duration_cast<std::chrono::milliseconds>(high_resolution_clock::now() - _lastAttack) > _attackInterval)
			{
				std::lock_guard<std::mutex> entitiesLock(_entitiesMutex);
				EntityPosition& position = _player->getPosition();
				int currentDist = ((int)position.x - (int)_entities[_target]->x) * ((int)position.x - (int)_entities[_target]->x) +
					((int)position.y - (int)_entities[_target]->y) * ((int)position.y - (int)_entities[_target]->y);
				
				// Check distance
				if (currentDist < 15)
				{
					target(_target);
					attack(_target);
					_lastAttack = high_resolution_clock::now();
				}
				else
				{
					_isAttacking = false;
					_player->setBusy(false);
				}
			}
		}
	}
}

