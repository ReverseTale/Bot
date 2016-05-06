#pragma once

#include "common.hpp"

#include <mutex>

#include <Game/packet.h>

class Sender
{
private:
	Sender() {}

public:
	static Sender* get()
	{
		if (!_instance)
		{
			_instance = new Sender();
		}

		return _instance;
	}

	inline void setSendSocket(int sendSocket) { _sendSocket = sendSocket; }
	void send(Utils::Game::Session* session, NString data);

	inline std::mutex& acquire() { return _sendMutex; }

private:
	static Sender* _instance;

	int _sendSocket;
	std::mutex _sendMutex;
};
