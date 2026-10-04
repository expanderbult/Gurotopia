#include "pch.hpp"
#include "store.hpp"
#include "onVariant/SetBux.hpp"
#include "database/shouhin.hpp"
#include "buy.hpp"

static int growtokens_of(const ::peer &peer)
{
    auto growtoken = std::ranges::find(peer.slots, 1486, &::slot::id);
    return (growtoken != peer.slots.end()) ? growtoken->count : 0;
}

/* @return the random part of a pack (packs without fixed items in store.txt) */
static std::vector<std::pair<short, short>> random_items(const std::string &btn)
{
    std::vector<std::pair<short, short>> pack{};
    std::vector<short> ids{};
    int amount = 0;

    if (btn == "basic_splice") // @note source: https://growtopia.fandom.com/wiki/Basic_Splicing_Kit
    {
        pack.emplace_back(11, 10);
        ids = {3567, 2793, 57, 13, 17, 21, 101, 381, 1139}; // @note instead of iterating seeds with rarity 2 each time
        amount = 10;
    }
    else if (btn == "rare_seed") // @note source: https://growtopia.fandom.com/wiki/Rare_Seed_Pack
    {
        for (const ::item &item : items)
            if (item.type == type::SEED && item.rarity >= 13 && item.rarity <= 60) ids.emplace_back(item.id);
        amount = 5;
    }
    else if (btn == "clothes_pack") // @note source: https://growtopia.fandom.com/wiki/Clothes_Pack
    {
        for (const ::item &item : items)
            if (item.type == type::CLOTHING && item.rarity <= 10) ids.emplace_back(item.id);
        amount = 3;
    }
    else if (btn == "rare_clothes_pack") // @note source: https://growtopia.fandom.com/wiki/Rare_Clothes_Pack
    {
        for (const ::item &item : items)
            if (item.type == type::CLOTHING && item.rarity >= 11 && item.rarity <= 60) ids.emplace_back(item.id);
        amount = 3;
    }

    for (int i = 0; i < amount && !ids.empty(); ++i)
        pack.emplace_back(ids[rand() % ids.size()], 1);
    return pack;
}

void action::buy(ENetEvent& event, const std::string& header, const std::string_view selection = "")
{
    ::hPipe hPipe{ header };
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    const int No = (pPeer->slot_size - 16) / 10 + 1; // @note number of upgrades | credits: https://growtopia.fandom.com/wiki/Backpack_Upgrade
    const int backpack_cost = 100 * No * No - 200 * No + 200;

    const std::string item = hPipe["item"];
    if (item == "main")
    {
        action::store(event, "");
        return;
    }

    /* a tab of the store */
    static const std::array<std::string_view, 6> tabs{ "", "locks", "itempack", "bigitems", "weather", "token" };
    const auto found = std::ranges::find(tabs, item);
    if (found != tabs.end() && found != tabs.begin())
    {
        const int tab = static_cast<int>(found - tabs.begin());
        static const std::array<std::string_view, 5> titles{ "", "Locks And Stuff!", "Item Packs!", "Awesome Items!", "Weather Machines!" };

        std::string StoreRequest = (tab == 5) ?
            std::format("set_description_text|`2Spend your Growtokens!`` (You have `5{}``) You earn Growtokens from Crazy Jim and Sales-Man. Select the item you'd like more info on, or BACK to go back.\n", growtokens_of(*pPeer)) :
            std::format("set_description_text|`2{}``  Select the item you'd like more info on, or BACK to go back.\n", titles[tab]);

        StoreRequest.append("enable_tabs|1\nadd_tab_button|main_menu|Home|interface/large/btn_shop.rttex||0|0|0|0||||-1|-1|||0|0|CustomParams:|\n");
        StoreRequest.append(std::format(
            "add_tab_button|locks_menu|Locks And Stuff|interface/large/btn_shop.rttex||{}|1|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|itempack_menu|Item Packs|interface/large/btn_shop.rttex||{}|3|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|bigitems_menu|Awesome Items|interface/large/btn_shop.rttex||{}|4|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|weather_menu|Weather Machines|interface/large/btn_shop.rttex|Tired of the same sunny sky?  We offer alternatives within...|{}|5|0|0||||-1|-1|||0|0|CustomParams:|\n"
            "add_tab_button|token_menu|Growtoken Items|interface/large/btn_shop.rttex||{}|2|0|0||||-1|-1|||0|0|CustomParams:|\n",
            int(tab == 1), int(tab == 2), int(tab == 3), int(tab == 4), int(tab == 5)
        ));
        for (const auto &[_tab, shouhin] : shouhin_tachi)
        {
            if (_tab != tab) continue;

            int cost = shouhin.cost;
            if (shouhin.btn == "upgrade_backpack")
            {
                if (No > 38) continue; // @note hide upgrade_backpack in store if maxed out
                cost = backpack_cost;
            }
            StoreRequest.append(std::format(
                "add_button|{}|{}|{}|{}|{}|{}|{}|0|||-1|-1||-1|-1||1||||||0|0|CustomParams:|\n",
                shouhin.btn, shouhin.name, shouhin.rttx, shouhin.description, shouhin.tex1, shouhin.tex2, cost
            ));
        }
        if (!selection.empty()) StoreRequest.append(std::format("select_item|{}\n", selection));

        send_varlist(event.peer, { "OnStoreRequest", StoreRequest });
        return;
    }

    /* buying an item */
    for (const auto &[tab, shouhin] : shouhin_tachi)
    {
        if (shouhin.btn != item) continue;

        const bool growtokens = (tab == 5);
        const std::string currency = growtokens ? "`2Growtokens``" : "Gems";
        const int cost = (shouhin.btn == "upgrade_backpack") ? backpack_cost : std::abs(shouhin.cost); // @note growtoken items are negative in store.txt
        const int have = growtokens ? growtokens_of(*pPeer) : pPeer->gems;

        if (have < cost)
        {
            send_varlist(event.peer, { "OnStorePurchaseResult",
                std::format("You can't afford `0{}``!  You're `${}`` {} short.", shouhin.name, cost - have, currency) });
            return;
        }

        /* pay first: the backpack can change while the items are given */
        if (growtokens) modify_item_inventory(event, ::slot(1486, static_cast<short>(-cost)));
        else pPeer->gems -= cost;

        std::vector<std::pair<short, short>> contents = shouhin.items; // @note a copy, so store.txt contents never change
        for (const auto &extra : random_items(shouhin.btn)) contents.push_back(extra);

        std::string received{};
        for (const auto &[id, amount] : contents)
        {
            if (id == 9412) // @note 9412 is the id for increase backpack sprite, but peer wont actually be given that item.
            {
                pPeer->slot_size += 10;
                send_inventory_state(event); // @note update the new slots
            }
            else modify_item_inventory(event, { id, amount });
            received.append(std::format("{}, ", id_to_item(id).raw_name)); // @todo add green text to rare items, or something cool.
        }

        send_varlist(event.peer, { "OnStorePurchaseResult", std::format(
            "You've purchased `0{}`` for `${}`` {}.\nYou have `${}`` {} left.\n\n`5Received: ```0{}``",
            shouhin.name, cost, currency, have - cost, currency, received) });
        if (!growtokens) on::SetBux(event);
        return;
    }
}
