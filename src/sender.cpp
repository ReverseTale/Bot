#include "sender.hpp"

Sender* Sender::_instance = nullptr;

using namespace Net;

void Sender::send(Utils::Game::Session* session, NString data)
{
	std::lock_guard<std::mutex> lock(_sendMutex);

	auto packet = gFactory->make(PacketType::CLIENT_GAME, session, data);
	packet->commit();
	packet->finish();

	(*sendDetour)(_sendSocket, packet->data().get(), packet->data().length(), 0);
}
