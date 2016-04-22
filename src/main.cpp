#include <iostream>
#include <thread>
#include <mutex>
#include <map>

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


bool threadCreated = false;
bool running;
std::thread inputThread;
std::thread botThread;
std::vector<std::string> filterSend;
std::vector<std::string> filterRecv;
std::vector<std::string> excludeRecv;
bool showAsHex = false;

SOCKET sendSock;
SOCKET recvSock;
DWORD baseAddress = 0x686260;
uint32_t sessionID;
uint32_t ingameID;
Utils::Game::Session session;

std::mutex _sendMutex;
std::mutex _entitiesMutex;
std::mutex _dropsMutex;

bool isLooting = false;
bool isAttacking = false;
uint32_t attackTarget = 0;
uint32_t lootTarget = 0;

using high_resolution_clock = std::chrono::high_resolution_clock;

high_resolution_clock::time_point lastAttack = high_resolution_clock::now();
std::chrono::milliseconds attackInterval(1500);


struct EntityPosition
{
	uint16_t x;
	uint16_t y;

	EntityPosition() :
		EntityPosition(0, 0)
	{}

	EntityPosition(uint16_t x, uint16_t y) :
		x(x), y(y)
	{}
};

struct Entity : public EntityPosition
{
	uint32_t type;
	uint32_t amount;

	Entity(uint32_t type, uint32_t amount, uint16_t x, uint16_t y) :
		type(type), amount(amount),
		EntityPosition(x, y)
	{}
};

std::map<uint32_t /*id*/, EntityPosition* /*pos*/> entities;
std::map<uint32_t /*id*/, Entity*  /*item*/> drops;
EntityPosition selfPosition;

void processInput();
void botLoop();

inline bool isLogin()
{
	DWORD pointer1 = *(DWORD*)(baseAddress);
	DWORD pointer2 = *(DWORD*)(pointer1);
	return *(BYTE*)(pointer2 + 0x31) == 0x00;
}

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

Detour<int, int, const char*, int, int>* sendDetour = NULL;
Detour<int, int, char*, int, int>* recvDetour = NULL;
int WINAPI nuestro_send(SOCKET s, const char *buf, int len, int flags)
{
	__asm PUSHAD;
	__asm PUSHFD;

	sendSock = s;

	std::lock_guard<std::mutex> lock(_sendMutex);

	if (!threadCreated)
	{
		threadCreated = true;

		std::cout << "Initializing thread" << std::endl;
		running = true;
		inputThread = std::thread(processInput);
		botThread = std::thread(botLoop);
		
		std::cout << "Done, joining" << std::endl;
	}

	bool login = isLogin();
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
					printf("\nSend (%d, %d):\n", packets.size(), packet.tokens().length());
					if (!showAsHex)
					{
						std::cout << ">> ";

						for (int i = 0; i < packet.tokens().length(); ++i)
						{
							std::cout << packet.tokens()[i] << ' ';
						}

						std::cout << std::endl;
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

				std::string opcode = packet.tokens().str(1);
				if (opcode == "walk")
				{
					selfPosition.x = packet.tokens().from_int<uint16_t>(2);
					selfPosition.y = packet.tokens().from_int<uint16_t>(3);
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

void target(uint32_t id)
{
	std::lock_guard<std::mutex> lock(_sendMutex);

	auto packet = gFactory->make(PacketType::CLIENT_GAME, &session, NString("ncif 3 ") << id);
	packet->commit();
	packet->finish();

	(*sendDetour)(sendSock, packet->data().get(), packet->data().length(), 0);
}

void attack(uint32_t id)
{
	std::lock_guard<std::mutex> lock(_sendMutex);

	auto packet = gFactory->make(PacketType::CLIENT_GAME, &session, NString("u_s 0 3 ") << id);
	packet->commit();
	packet->finish();

	(*sendDetour)(sendSock, packet->data().get(), packet->data().length(), 0);
}

void loot(uint32_t id)
{
	std::lock_guard<std::mutex> lock(_sendMutex);

	auto packet = gFactory->make(PacketType::CLIENT_GAME, &session, NString("get 1 ") << ingameID << ' ' << id);
	packet->commit();
	packet->finish();

	(*sendDetour)(sendSock, packet->data().get(), packet->data().length(), 0);
}

void attack(std::string id)
{
	std::lock_guard<std::mutex> lock(_sendMutex);

	auto packet = gFactory->make(PacketType::CLIENT_GAME, &session, NString("u_s 0 3 ") << id);
	packet->commit();
	packet->finish();

	(*sendDetour)(sendSock, packet->data().get(), packet->data().length(), 0);
}

int WINAPI nuestro_recv(SOCKET s, char *buf, int len, int flags)
{
	int ret = (*recvDetour)(s, buf, len, flags);

	__asm PUSHAD;
	__asm PUSHFD;

	recvSock = s;

	bool login = isLogin();
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

				printf("\nRecv (%d):\n", packet.tokens().length());
				if (!showAsHex)
				{
					std::cout << "<< ";

					for (int i = 0; i < packet.tokens().length(); ++i)
					{
						std::cout << packet.tokens()[i] << ' ';
					}

					std::cout << std::endl;
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

				ingameID = packet.tokens().from_int<uint32_t>(i);
			}
			else if (opcode == "c_map")
			{
				std::lock_guard<std::mutex> lock(_entitiesMutex);

				for (auto&& it : entities)
				{
					delete it.second;
				}

				entities.clear();
			}
			else if (opcode == "in")
			{
				if (packet.tokens().from_int<int>(1) == 3)
				{
					std::lock_guard<std::mutex> lock(_entitiesMutex);

					uint32_t id = packet.tokens().from_int<uint32_t>(3);
					entities.emplace(id, new EntityPosition{ packet.tokens().from_int<uint16_t>(4), packet.tokens().from_int<uint16_t>(5) });
				}
			}
			else if (opcode == "mv")
			{
				if (packet.tokens().from_int<int>(1) == 3)
				{
					std::lock_guard<std::mutex> lock(_entitiesMutex);

					uint32_t id = packet.tokens().from_int<uint32_t>(2);
					auto it = entities.find(id);
					if (it != entities.end())
					{
						it->second->x = packet.tokens().from_int<uint16_t>(3);
						it->second->y = packet.tokens().from_int<uint16_t>(4);
					}
				}
			}
			else if (opcode == "at")
			{
				uint32_t id = packet.tokens().from_int<uint32_t>(1);
				if (id == ingameID)
				{
					selfPosition.x = packet.tokens().from_int<uint16_t>(3);
					selfPosition.y = packet.tokens().from_int<uint16_t>(4);
				}
			}
			else if (opcode == "st")
			{
				if (packet.tokens().from_int<int>(1) == 3)
				{
					uint32_t id = packet.tokens().from_int<uint32_t>(2);
					int hp = packet.tokens().from_int<int>(7);

					if (id == attackTarget)
					{
						if (hp <= 0)
						{
							isAttacking = false;
						}
					}

					if (hp <= 0)
					{
						std::lock_guard<std::mutex> lock(_entitiesMutex);

						entities.erase(id);
					}
				}
			}
			else if (opcode == "drop")
			{
				uint32_t owner = packet.tokens().from_int<uint32_t>(7);
				if (owner == ingameID)
				{
					uint32_t type = packet.tokens().from_int<uint32_t>(1);
					uint32_t id = packet.tokens().from_int<uint32_t>(2);
					uint16_t x = packet.tokens().from_int<uint16_t>(3);
					uint16_t y = packet.tokens().from_int<uint16_t>(4);
					uint32_t amount = packet.tokens().from_int<uint32_t>(5);

					std::lock_guard<std::mutex> lock(_dropsMutex);
					drops.emplace(id, new Entity { type, amount, x, y });
				}
			}
			else if (opcode == "get")
			{
				uint8_t confirm = packet.tokens().from_int<uint32_t>(1);
				uint32_t owner = packet.tokens().from_int<uint32_t>(2);
				uint32_t id = packet.tokens().from_int<uint16_t>(3);

				if (confirm == 1 && owner == ingameID && id == lootTarget)
				{
					isLooting = false;

					std::lock_guard<std::mutex> lock(_dropsMutex);
					drops.erase(id);
				}
			}
		}
	}

	if (login && packets.size() > 0 && packets[0].tokens().length() >= 2)
	{
		printf("Getting ID\n");
		sessionID = packets[0].tokens().from_int<uint32_t>(1);
		printf("SAVING SESSION ID %d\n", sessionID);
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
		std::cout << "Enter Filter: ";
		std::cin >> input;

		if (input.length() >= 2)
		{
			if (input.compare("toggle_hex") == 0)
			{
				showAsHex = !showAsHex;
				continue;
			}

			std::vector<std::string>* filterVec;
			bool recv = input[0] == '<';
			bool send = input[0] == '>';
			bool inject = input[0] == '=';
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
			else if (inject)
			{
				attack(input.substr(1));
				continue;
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
}

void botLoop()
{
	while (running)
	{
		if (!isLogin())
		{
			if (!isAttacking && !isLooting)
			{
				std::lock_guard<std::mutex> dropLock(_dropsMutex);
				if (!drops.empty())
				{
					isLooting = true;
					lootTarget = drops.begin()->first;
					loot(lootTarget);
					continue;
				}

				std::lock_guard<std::mutex> entitiesLock(_entitiesMutex);

				int dist = std::numeric_limits<int>::max();
				for (auto&& it : entities)
				{
					int currentDist = ((int)selfPosition.x - (int)it.second->x) * ((int)selfPosition.x - (int)it.second->x) +
						((int)selfPosition.y - (int)it.second->y) * ((int)selfPosition.y - (int)it.second->y);

					if (currentDist < dist)
					{
						dist = currentDist;
						attackTarget = it.first;
					}
				}

				if (dist < 15)
				{
					isAttacking = true;
				}
			}

			if (isAttacking)
			{
				if (std::chrono::duration_cast<std::chrono::milliseconds>(high_resolution_clock::now() - lastAttack) > attackInterval)
				{
					target(attackTarget);
					attack(attackTarget);
					lastAttack = high_resolution_clock::now();
				}
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

		// Call our function
		Hooks();
	}
	else if (reason == DLL_PROCESS_DETACH)
	{
		running = false;
		inputThread.join();
		botThread.join();
	}

	return true;
}
