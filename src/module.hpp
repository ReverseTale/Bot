#pragma once

#include <Tools/utils.h>

namespace Net
{
	class Packet;
}

class Module
{
public:
	Module(Utils::Game::Session* session) :
		_session(session),
		_isEnabled(false)
	{}

	inline void enable() { _isEnabled = true; }
	inline void disable() { _isEnabled = false; }
	inline void toggle() { _isEnabled = !_isEnabled; }
	inline bool isEnabled() { return _isEnabled; }

	virtual void update() {}
	virtual void onSend(NString packet) = 0;
	virtual void onReceive(NString packet) = 0;

protected:
	Utils::Game::Session* _session;
	bool _isEnabled;
};
