#include "WoW/Source/Duskhaven/Network/DuskhavenNetwork.h"
#include "Engine/Source/Base/CDataStore.h"
#include "Engine/Source/FrameScript/FrameScript.h"
#include "WoW/Source/Duskhaven/Network/CustomPacketBuffer.h"
#include "WoW/Source/WowSvcs/WowSvcsClient/ClientServices.h"
#include <map>
#include <vector>
#include <string>
#include <sstream>

namespace Duskhaven
{
    namespace Network
    {
        class ClientMessageWrite : public CustomPacketWrite
        {
        public:
            ClientMessageWrite() : CustomPacketWrite(0, MAX_FRAGMENT_SIZE, 0) {}
            ClientMessageWrite(opcode_t opcode, totalSize_t size) : CustomPacketWrite(opcode, MAX_FRAGMENT_SIZE, size) {}

            void Send()
            {
                ClientConnection* conn = ClientServices::Connection();
                if (!conn || conn->m_netState != NS_CONNECTED)
                {
                    Destroy();
                    Clear();
                    return;
                }

                std::vector<CustomPacketChunk>& chunks = buildMessages();
                for (auto& chunk : chunks)
                {
                    CDataStore p;
                    p.Put((uint32_t)CLIENT_TO_SERVER_OPCODE);
                    p.PutData(chunk.Data(), chunk.FullSize());
                    p.Finalize();
                    conn->Send(&p);
                    chunk.Destroy();
                }
                Clear();
            }
        };

        template <typename T>
        class MessageRegistry
        {
            uint32_t m_cur = 0;

        public:
            std::map<uint32_t, T> m_map;
            uint32_t freeId()
            {
                while (m_map.find(m_cur) != m_map.end())
                {
                    ++m_cur;
                }
                return m_cur;
            }

            uint32_t add(T& value, T** ptr)
            {
                *ptr = &(m_map[freeId()] = value);
                return m_cur++;
            }

            T* get(uint32_t id)
            {
                auto itr = m_map.find(id);
                if (itr == m_map.end())
                {
                    return nullptr;
                }
                return &itr->second;
            }
        };

        static MessageRegistry<ClientMessageWrite> s_writes;
        static CustomPacketRead* s_curRead = nullptr;
        static std::map<opcode_t, std::vector<std::function<void(CustomPacketRead*)>>> s_cppListeners;

        class ClientMessageBuffer : public CustomPacketBuffer
        {
        public:
            ClientMessageBuffer() : CustomPacketBuffer(MIN_FRAGMENT_SIZE, BUFFER_QUOTA, MAX_FRAGMENT_SIZE) {}

            void OnPacket(CustomPacketRead* value) override
            {
                s_curRead = value;
                char lua[64];
                snprintf(lua, sizeof(lua), "__FireCustomPacket(%u)", (unsigned int)value->Opcode());
                FrameScript_Execute(lua, "DuskhavenNetwork", nullptr);
                s_curRead = nullptr;

                auto itr = s_cppListeners.find(value->Opcode());
                if (itr != s_cppListeners.end())
                {
                    for (auto const& cb : itr->second)
                    {
                        cb(value);
                    }
                }
            }

            void OnError(CustomPacketResult error) override
            {
                // Packet reading error
            }
        };

        static ClientMessageBuffer s_clientBuffer;

        int OnCustomPacket(opcode_t opcode, std::function<void(CustomPacketRead*)> callback)
        {
            auto itr = s_cppListeners.find(opcode);
            if (itr != s_cppListeners.end())
            {
                itr->second.push_back(callback);
                return 1;
            }
            else
            {
                s_cppListeners[opcode] = {callback};
                return 2;
            }
        }

        void SendCustomPacket(opcode_t opcode, totalSize_t size, std::function<void(CustomPacketWrite&)> writer)
        {
            ClientMessageWrite packet(opcode, size);
            writer(packet);
            packet.Send();
        }

        int32_t HandleServerToClientPacket(void* param, uint32_t msgId, uint32_t time, CDataStore* msg)
        {
            if (!msg || msg->m_size <= msg->m_read)
                return 1;

            uint32_t payloadSize = msg->m_size - msg->m_read;
            char* payloadData    = reinterpret_cast<char*>(msg->m_data + msg->m_read);
            s_clientBuffer.ReceivePacket((chunkSize_t)payloadSize, payloadData);
            return 1;
        }

        void Initialize()
        {
            ClientServices::SetMessageHandler((NETMESSAGE)SERVER_TO_CLIENT_OPCODE,
                                              (MESSAGE_HANDLER)HandleServerToClientPacket, nullptr);
        }

        template <typename T>
        static int WriteNum(lua_State* L)
        {
            uint32_t id = (uint32_t)lua_tonumber(L, 2);
            T val       = (T)lua_tonumber(L, 3);

            auto itr = s_writes.m_map.find(id);
            if (itr == s_writes.m_map.end())
            {
                return 0;
            }
            itr->second.Write(val);
            return 0;
        }

        template <typename T>
        static int ReadNum(lua_State* L)
        {
            T def = (T)lua_tonumber(L, 2);
            lua_pushnumber(L, s_curRead == nullptr ? def : s_curRead->Read<T>(def));
            return 1;
        }

        enum class LuaNetworkOpcode
        {
            WRITE_SIZE   = 0,
            READ_SIZE    = 1,
            WRITE_UINT8  = 2,
            WRITE_INT8   = 3,
            WRITE_UINT16 = 4,
            WRITE_INT16  = 5,
            WRITE_UINT32 = 6,
            WRITE_INT32  = 7,
            WRITE_UINT64 = 8,
            WRITE_INT64  = 9,
            WRITE_FLOAT  = 10,
            WRITE_DOUBLE = 11,
            WRITE_STRING = 12,

            READ_UINT8          = 13,
            READ_INT8           = 14,
            READ_UINT16         = 15,
            READ_INT16          = 16,
            READ_UINT32         = 17,
            READ_INT32          = 18,
            READ_UINT64         = 19,
            READ_INT64          = 20,
            READ_FLOAT          = 21,
            READ_DOUBLE         = 22,
            READ_STRING         = 23,
            MAKE_CUSTOM_PACKET  = 24,
            SEND_CUSTOM_PACKET  = 25,
            RESET_CUSTOM_PACKET = 26,
        };

        static int Script_CLIENT_NETWORK(lua_State* L)
        {
            LuaNetworkOpcode opcode = (LuaNetworkOpcode)(int)lua_tonumber(L, 1);
            switch (opcode)
            {
                case LuaNetworkOpcode::WRITE_SIZE:
                {
                    auto itr = s_writes.m_map.find((uint32_t)lua_tonumber(L, 2));
                    lua_pushnumber(L, itr == s_writes.m_map.end() ? 0 : itr->second.Size());
                    return 1;
                }
                case LuaNetworkOpcode::READ_SIZE:
                {
                    lua_pushnumber(L, s_curRead ? s_curRead->Size() : 0);
                    return 1;
                }
                case LuaNetworkOpcode::WRITE_UINT8:
                {
                    return WriteNum<uint8_t>(L);
                }
                case LuaNetworkOpcode::WRITE_INT8:
                {
                    return WriteNum<int8_t>(L);
                }
                case LuaNetworkOpcode::WRITE_UINT16:
                {
                    return WriteNum<uint16_t>(L);
                }
                case LuaNetworkOpcode::WRITE_INT16:
                {
                    return WriteNum<int16_t>(L);
                }
                case LuaNetworkOpcode::WRITE_UINT32:
                {
                    return WriteNum<uint32_t>(L);
                }
                case LuaNetworkOpcode::WRITE_INT32:
                {
                    return WriteNum<int32_t>(L);
                }
                case LuaNetworkOpcode::WRITE_UINT64:
                {
                    uint32_t id = (uint32_t)lua_tonumber(L, 2);
                    auto itr    = s_writes.m_map.find(id);
                    if (itr == s_writes.m_map.end())
                    {
                        return 0;
                    }
                    if (lua_isstring(L, 3))
                    {
                        uint64_t v = 0;
                        std::stringstream ss(lua_tostring(L, 3));
                        ss >> v;
                        itr->second.Write(v);
                    }
                    else
                    {
                        itr->second.Write(uint64_t(lua_tonumber(L, 3)));
                    }
                    return 0;
                }
                case LuaNetworkOpcode::WRITE_INT64:
                {
                    uint32_t id = (uint32_t)lua_tonumber(L, 2);
                    auto itr    = s_writes.m_map.find(id);
                    if (itr == s_writes.m_map.end())
                    {
                        return 0;
                    }
                    if (lua_isstring(L, 3))
                    {
                        int64_t v = 0;
                        std::stringstream ss(lua_tostring(L, 3));
                        ss >> v;
                        itr->second.Write(v);
                    }
                    else
                    {
                        itr->second.Write(int64_t(lua_tonumber(L, 3)));
                    }
                    return 0;
                }
                case LuaNetworkOpcode::WRITE_FLOAT:
                {
                    return WriteNum<float>(L);
                }
                case LuaNetworkOpcode::WRITE_DOUBLE:
                {
                    return WriteNum<double>(L);
                }
                case LuaNetworkOpcode::WRITE_STRING:
                {
                    uint32_t id = (uint32_t)lua_tonumber(L, 2);
                    auto itr    = s_writes.m_map.find(id);
                    if (itr == s_writes.m_map.end())
                    {
                        return 0;
                    }
                    itr->second.WriteString(lua_tostring(L, 3));
                    return 0;
                }
                case LuaNetworkOpcode::READ_UINT8:
                {
                    return ReadNum<uint8_t>(L);
                }
                case LuaNetworkOpcode::READ_INT8:
                {
                    return ReadNum<int8_t>(L);
                }
                case LuaNetworkOpcode::READ_UINT16:
                {
                    return ReadNum<uint16_t>(L);
                }
                case LuaNetworkOpcode::READ_INT16:
                {
                    return ReadNum<int16_t>(L);
                }
                case LuaNetworkOpcode::READ_UINT32:
                {
                    return ReadNum<uint32_t>(L);
                }
                case LuaNetworkOpcode::READ_INT32:
                {
                    return ReadNum<int32_t>(L);
                }
                case LuaNetworkOpcode::READ_UINT64:
                {
                    uint64_t def = 0;
                    if (lua_isstring(L, 2))
                    {
                        std::stringstream ss(lua_tostring(L, 2));
                        ss >> def;
                    }
                    else
                    {
                        def = (uint64_t)lua_tonumber(L, 2);
                    }
                    uint64_t val = s_curRead == nullptr ? def : s_curRead->Read<uint64_t>(def);
                    lua_pushstring(L, std::to_string(val).c_str());
                    return 1;
                }
                case LuaNetworkOpcode::READ_INT64:
                {
                    int64_t def = 0;
                    if (lua_isstring(L, 2))
                    {
                        std::stringstream ss(lua_tostring(L, 2));
                        ss >> def;
                    }
                    else
                    {
                        def = (int64_t)lua_tonumber(L, 2);
                    }
                    int64_t val = s_curRead == nullptr ? def : s_curRead->Read<int64_t>(def);
                    lua_pushstring(L, std::to_string(val).c_str());
                    return 1;
                }
                case LuaNetworkOpcode::READ_FLOAT:
                {
                    return ReadNum<float>(L);
                }
                case LuaNetworkOpcode::READ_DOUBLE:
                {
                    return ReadNum<double>(L);
                }
                case LuaNetworkOpcode::READ_STRING:
                {
                    std::string def = lua_isstring(L, 2) ? lua_tostring(L, 2) : "";
                    lua_pushstring(L, (s_curRead == nullptr ? def : s_curRead->ReadString(def)).c_str());
                    return 1;
                }
                case LuaNetworkOpcode::MAKE_CUSTOM_PACKET:
                {
                    ClientMessageWrite write((opcode_t)lua_tonumber(L, 2), (totalSize_t)lua_tonumber(L, 3));
                    ClientMessageWrite* ptr = nullptr;
                    uint32_t id             = s_writes.add(write, &ptr);
                    lua_pushnumber(L, id);
                    return 1;
                }
                case LuaNetworkOpcode::SEND_CUSTOM_PACKET:
                {
                    uint32_t id = (uint32_t)lua_tonumber(L, 2);
                    ClientMessageWrite* write = s_writes.get(id);
                    if (write == nullptr)
                    {
                        return 0;
                    }
                    write->Send();
                    s_writes.m_map.erase(id);
                    return 0;
                }
                case LuaNetworkOpcode::RESET_CUSTOM_PACKET:
                {
                    if (s_curRead == nullptr)
                        return 0;
                    s_curRead->Reset();
                    return 0;
                }
                default:
                {
                    return 0;
                }
            }
        }

        void RegisterLuaFunctions()
        {
            FrameScript_RegisterFunction("_CLIENT_NETWORK", Script_CLIENT_NETWORK);
        }
    } // namespace Network
} // namespace Duskhaven
