#include <iostream>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>

#include "player.hpp"
#include "attack.hpp"
#include "loot.hpp"
#include "sender.hpp"

#include <xhacking/xHacking.h>
#include <xhacking/Utilities/Utilities.h>
#include <xhacking/Loader/Loader.h>
#include <xhacking/Detour/Detour.h>

#include <Tools/utils.h>
#include <Game/packet.h>
#include <Cryptography/login.h>
#include <Cryptography/game.h>


#ifdef max
	#undef max
	#undef min
#endif

using namespace xHacking;
using namespace Net;

xHacking::Detour<int, int, const char*, int, int>* sendDetour = NULL;
xHacking::Detour<int, int, char*, int, int>* recvDetour = NULL;

bool threadCreated = false;
bool running;
std::ofstream logger;
std::thread inputThread;
std::thread botThread;
std::vector<std::string> filterSend;
std::vector<std::string> filterRecv;
std::vector<std::string> excludeRecv;
bool showAsHex = false;

SOCKET sendSock;
SOCKET recvSock;
uint32_t sessionID;
Utils::Game::Session session;


void processInput();
void botLoop();

std::list<Module*> _modules;
PlayerModule* _player = nullptr;
AttackModule* _attack = nullptr;
LootModule* _loot = nullptr;


struct special_compare : public std::unary_function<std::string, bool>
{
	explicit special_compare(const std::string &baseline) : baseline(baseline) {}
	bool operator() (const std::string &arg)
	{
		if (arg.size() > baseline.size())
		{
			return false;
		}

		for (size_t pos = 0; pos < arg.size(); ++pos)
		{
			if (arg[pos] == '*')
			{
				return true;
			}

			if (baseline[pos] != arg[pos])
			{
				return false;
			}
		}

		return true;
	}

	std::string baseline;
};

int WINAPI nuestro_send(SOCKET s, const char *buf, int len, int flags)
{
	__asm PUSHAD;
	__asm PUSHFD;

	Sender::get()->setSendSocket(s);
	sendSock = s;

	std::lock_guard<std::mutex> lock(Sender::get()->acquire());

	if (!threadCreated)
	{
		threadCreated = true;

		std::cout << "Initializing thread" << std::endl;
		running = true;
		inputThread = std::thread(processInput);
		botThread = std::thread(botLoop);
		
		std::cout << "Done, joining" << std::endl;
	}

	bool login = _player->isAtLogin();
	Packet* packet = nullptr;
	if (login)
	{
		packet = gFactory->make(PacketType::SERVER_LOGIN, &session, NString(buf, len));
		session.reset();
	}
	else
	{
		packet = gFactory->make(PacketType::SERVER_GAME, &session, NString(buf, len));
	}

	int ret = 0;

	auto packets = packet->decrypt();

	if (!login)
	{
		if (session.id() != -1)
		{
			for (auto packet : packets)
			{
				if (packet.tokens().length() < 2)
				{
					continue;
				}

				std::string pattern = packet.tokens().str(1);
				if (std::find_if(filterSend.begin(), filterSend.end(), special_compare(pattern)) != filterSend.end())
				{
					if (!showAsHex)
					{
						logger << ">> ";

						for (int i = 0; i < packet.tokens().length(); ++i)
						{
							logger << packet.tokens()[i] << ' ';
						}

						logger << std::endl;
					}
					else
					{
						for (int i = 0; i < packet.length(); ++i)
						{
							printf("%.2X ", (uint8_t)packet[i]);
						}
						printf("\n");
					}
				}

				for (Module* module : _modules)
				{
					module->onReceive(packet);
				}
				
				NString newPacket;
				for (int i = 1; i < packet.tokens().length(); ++i)
				{
					if (i != 1)
					{
						newPacket << ' ';
					}

					newPacket << packet.tokens()[i];
				}

				auto reencryptedPacket = gFactory->make(PacketType::CLIENT_GAME, &session, newPacket);
				reencryptedPacket->commit();
				reencryptedPacket->finish();

				ret += (*sendDetour)(s, reencryptedPacket->data().get(), reencryptedPacket->data().length(), flags);
			}
		}
	}

	if (ret == 0)
	{
		ret = (*sendDetour)(s, buf, len, flags);
	}
	
	// Set session after decrypting
	if (!login && session.id() == -1)
	{
		session.setID(sessionID);
		session.setAlive(packets[0].tokens().from_int<uint32_t>(0));
	}

	gFactory->recycle(packet);

	__asm POPFD;
	__asm POPAD;

	return ret;
}

int WINAPI nuestro_recv(SOCKET s, char *buf, int len, int flags)
{
	int ret = (*recvDetour)(s, buf, len, flags);

	__asm PUSHAD;
	__asm PUSHFD;

	recvSock = s;

	bool login = _player->isAtLogin();
	Packet* packet = nullptr;
	if (login)
	{
		packet = gFactory->make(PacketType::CLIENT_LOGIN, &session, NString(buf, ret));
	}
	else
	{
		packet = gFactory->make(PacketType::CLIENT_GAME, &session, NString(buf, ret));
	}

	auto packets = packet->decrypt();

	for (NString& packet : packets)
	{
		if (!packet.empty())
		{
			if (packet.tokens().length() < 1)
			{
				continue;
			}

			if (std::find_if(filterRecv.begin(), filterRecv.end(), special_compare(packet.tokens().str(0))) != filterRecv.end())
			{
				if (std::find_if(excludeRecv.begin(), excludeRecv.end(), special_compare(packet.tokens().str(0))) != excludeRecv.end())
				{
					continue;
				}

				if (!showAsHex)
				{
					logger << "<< ";

					for (int i = 0; i < packet.tokens().length(); ++i)
					{
						logger << packet.tokens()[i] << ' ';
					}

					logger << std::endl;
				}
				else
				{
					for (int i = 0; i < packet.length(); ++i)
					{
						printf("%.2X ", (uint8_t)packet[i]);
					}
					printf("\n");
				}
			}

			for (Module* module : _modules)
			{
				module->onReceive(packet);
			}
		}
	}

	if (login && packets.size() > 0 && packets[0].tokens().length() >= 2)
	{
		sessionID = packets[0].tokens().from_int<uint32_t>(1);
	}

	gFactory->recycle(packet);

	__asm POPFD;
	__asm POPAD;

	return ret;
}

void Hooks()
{
	sendDetour = new Detour<int, int, const char*, int, int>();
	sendDetour->Wait("WS2_32.dll", "send", (BYTE*)nuestro_send);

	recvDetour = new Detour<int, int, char*, int, int>();
	recvDetour->Wait("wsock32.dll", "recv", (BYTE*)nuestro_recv);
}

void processInput()
{
	std::cout << "Welcome to ReverseTale-Bot, you can filter packets by issuing:" << std::endl;
	std::cout << "\t<packetOpcode : Adds a recv for `packetOpcode`" << std::endl;
	std::cout << "\t<-packetOpcode : Removes a recv for `packetOpcode`" << std::endl;
	std::cout << "\t>packetOpcode : Adds a send for `packetOpcode`" << std::endl;
	std::cout << "\t>-packetOpcode : Removes a send for `packetOpcode`" << std::endl;

	while (running)
	{
		std::string input;
		std::cout << "1. Filter options" << std::endl;
		std::cout << "2. Enable/Disable Auto-Attack (" << std::boolalpha << _attack->isEnabled() << ")" << std::endl;
		std::cout << "3. Enable/Disable Auto-Loot (" << std::boolalpha << _loot->isEnabled() << ")" << std::endl;
		std::cout << "Choose an option 1-3: ";
		std::cin >> input;

		if (input[0] == '1')
		{
			std::cout << "Enter filter option: ";
			std::cin >> input;

			if (input.length() >= 2)
			{
				if (input.compare("toggle_hex") == 0)
				{
					showAsHex = !showAsHex;
					continue;
				}

				if (input.compare("savelog") == 0)
				{
					logger.close();
					logger.open("nostale.log", std::ios::app);
					continue;
				}

				std::vector<std::string>* filterVec;
				bool recv = input[0] == '<';
				bool send = input[0] == '>';
				bool rem = input[1] == '-';
				bool excl = input[1] == '/';

				if (excl)
				{
					rem = input[2] == '-';
				}

				std::cout << "Recv: " << recv << " Rem: " << rem << " -- " << input << std::endl;

				if (recv)
				{
					filterVec = &filterRecv;
				}
				else if (send)
				{
					filterVec = &filterSend;
				}

				if (excl)
				{
					if (!rem)
					{
						input = input.substr(1);
					}

					filterVec = &excludeRecv;

					printf("Excluding %s\n", input.c_str());
				}

				if (rem)
				{
					std::string filter = input.substr(2);
					filterVec->erase(std::remove(filterVec->begin(), filterVec->end(), filter), filterVec->end());
				}
				else
				{
					std::string filter = input.substr(1);
					filterVec->push_back(filter);
				}
			}
		}
		else if (input[0] == '2')
		{
			_attack->toggle();
		}
		else if (input[0] == '3')
		{
			_loot->toggle();
		}
	}
}

void botLoop()
{
	while (running)
	{
		for (Module* module : _modules)
		{
			if (module->isEnabled())
			{
				module->update();
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(500));
	}
}

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, DWORD reserved)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		// Use the xHacking::CreateConsole function
		CreateConsole();
		
		// Initialize modules
		_player = new PlayerModule(&session);
		_attack = new AttackModule(_player, &session);
		_loot = new LootModule(_player, &session);
		_modules = { _player, _loot, _attack }; // Loot must be first

		// Call our function
		Hooks();

		// Open logging file
		logger.open("nostale.log", std::ios::trunc);
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		logger.close();
		running = false;
		inputThread.join();
		botThread.join();
	}

	return true;
}
