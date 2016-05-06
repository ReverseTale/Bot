#pragma once

#include "common.hpp"
#include "module.hpp"

#include <map>
#include <chrono>
#include <mutex>


class PlayerModule;

class AttackModule : public Module
{
	using high_resolution_clock = std::chrono::high_resolution_clock;

public:
	AttackModule(PlayerModule* player, Utils::Game::Session* session) :
		Module(session),
		_player(player),
		_attackInterval(1500),
		_isAttacking(false)
	{}

	void update() override;
	void onSend(NString packet) override;
	void onReceive(NString packet) override;

	inline bool isAttacking() { return _isAttacking; }

private:
	void target(uint32_t id);
	void attack(uint32_t id);

private:
	PlayerModule* _player;

	high_resolution_clock::time_point _lastAttack;
	std::chrono::milliseconds _attackInterval;
	uint32_t _target;
	bool _isAttacking;

	std::mutex _entitiesMutex;
	std::map<uint32_t /*id*/, EntityPosition* /*pos*/> _entities;
};
