#include "pch.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "action/quit_to_exit.hpp" // @note peer leave world
#include "action/join_request.hpp" // @note peer enter (blast) world

#include "create_blast.hpp"

void create_blast(ENetEvent& event, const ::hPipe &hPipe)
{
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const int id = atoi(hPipe["id"].c_str());
    std::string world_name = hPipe["name"];

    for (char &c : world_name) c = std::toupper(c); // @note start -> START

    if (std::ranges::find(pPeer->slots, id, &::slot::id) == pPeer->slots.end()) return; // @note the blast has to be in the backpack
    if (world_name.empty() || world_name.size() > 24 || !alnum(world_name))
    {
        on::ConsoleMessage(event.peer, "Sorry, spaces and special characters are not allowed in world names.  Try again.");
        return;
    }
    
    switch (id)
    {
        case 1402: // @note Thermonuclear Blast
        {
            if (std::ranges::find(worlds, world_name, &::world::name) != worlds.end() || ::world::exists(world_name)) // @note also worlds that aren't loaded right now
            {
                on::ConsoleMessage(event.peer, std::format("`4{}`` already exists. Try another name.", world_name));
                return;
            }
            ::world world{world_name}; // @note we don't need emplace into worlds since no one is in the world yet!

            modify_item_inventory(event, ::slot(static_cast<short>(id), -1)); // @note a blast is used up
            action::quit_to_exit(event, "", true);
            blast::thermonuclear(world);
            break; // @note save the world and continue in action::join_request. seems tedious so i might change later.
        }
        default: return;
    }
    action::join_request(event, "", world_name);
}
