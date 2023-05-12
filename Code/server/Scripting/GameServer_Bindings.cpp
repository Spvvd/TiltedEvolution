
#include "GameServer.h"
#include <Messages/NotifyChatMessageBroadcast.h>
#include <regex>

namespace Script
{
void CreateGameServerBindings(sol::state_view aState)
{
    auto type = aState.new_usertype<GameServer>("GameServer", sol::meta_function::construct, sol::no_constructor);
    type["get"] = []() { return GameServer::Get(); };
    type["Kill"] = &GameServer::Kill;
    type["Kick"] = &GameServer::Kick;
    type["GetTick"] = &GameServer::GetTick;

    type["SetTime"] = [](GameServer& aSelf, const int64_t hour, const int64_t minutes) {
        auto out = spdlog::get("ConOut");

        auto m_pWorld = &GameServer::Get()->GetWorld();
        auto timescale = m_pWorld->GetCalendarService().GetTimeScale();

        bool time_set_successfully = m_pWorld->GetCalendarService().SetTime(hour, minutes, timescale);

        if (time_set_successfully)
        {
            out->info("Time set to {}:{}", hour, minutes);
        }
        else
        {
            out->error("Hour must be between 0-23 and minute must be between 0-59");
        }
    };

    type["SendChatMessage"] = [](GameServer& aSelf, ConnectionId_t aConnectionId, const std::string& acMessage) {
        NotifyChatMessageBroadcast notifyMessage{};

        std::regex escapeHtml{"<[^>]+>\\s+(?=<)|<[^>]+>"};
        notifyMessage.MessageType = ChatMessageType::kLocalChat;
        notifyMessage.PlayerName = "[Server]";
        notifyMessage.ChatMessage = std::regex_replace(acMessage, escapeHtml, "");
        GameServer::Get()->Send(aConnectionId, notifyMessage);
    };
    // type["SendPacket"]
}
} // namespace Script
