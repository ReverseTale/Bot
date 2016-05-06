#pragma once

#include "module.hpp"
#include "common.hpp"

class PlayerModule : public Module
{
public:
	PlayerModule(Utils::Game::Session* session) :
		Module(session),
		_ingameID(0),
		_isBusy(false)
	{
		_isEnabled = true;
	}

	void onSend(NString packet) override;
	void onReceive(NString packet) override;

	bool isAtLogin();
	inline uint32_t getIngameID() { return _ingameID; }
	inline EntityPosition& getPosition() { return _position; }

	inline void setBusy(bool busy) { _isBusy = busy; }
	inline bool isBusy() { return _isBusy; }

private:
	uint32_t _ingameID;
	EntityPosition _position;
	bool _isBusy;
};
