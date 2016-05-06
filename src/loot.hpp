#pragma once

#include "common.hpp"
#include "module.hpp"

#include <map>
#include <chrono>
#include <mutex>


class PlayerModule;

class LootModule : public Module
{
	using high_resolution_clock = std::chrono::high_resolution_clock;

public:
	LootModule(PlayerModule* player, Utils::Game::Session* session) :
		Module(session),
		_player(player),
		_isLooting(false)
	{}

	void update() override;
	void onSend(NString packet) override;
	void onReceive(NString packet) override;

	inline bool isLooting() { return _isLooting; }

private:
	void loot(uint32_t id);

private:
	PlayerModule* _player;

	uint32_t _target;
	bool _isLooting;

	std::mutex _dropsMutex;
	std::map<uint32_t /*id*/, Entity*  /*item*/> _drops;
};
