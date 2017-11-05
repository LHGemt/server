/*
 * Copyright (C) 2017 Light's Hope <https://github.com/lightshope>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "AuraRemovalMgr.h"
#include "Policies/Singleton.h"
#include "Database/DatabaseEnv.h"
#include "Policies/SingletonImp.h"
#include "Player.h"
#include "SpellEntry.h"
#include "ProgressBar.h"
#include "LootLogMgr.h"
#include "Chat.h"
#include "Language.h"

#include <ctime>

INSTANTIATE_SINGLETON_1(LootLogManager);

/*
	When an item drops from a creature, each item gets an entry in the LootLog table.
	The entry will contain the entry of the item, the creature which dropped the item, a foreign key
	pointing to a set of playerGuids in the LootCandidates table and nullable playerGuid, lootedBy.
	LootedBy will be null when the row is first inserted into the table. As soon as the item is given 
	to a player lootedBy will be updated to that playerGuid.

    When a boss is killed we get hook into the Unit::Kill function and store the following information:
    loot_creature_death: creature-entry, a timestamp and a key 
    loot_items:          maps each item dropped by the creature to the loot_creature_death table through the same key
    loot_candidates:     maps each eligible player to the same creature through the same key.


    loot_creature_death   key         creatureEntry   timestamp    instanceID
    loot_items            key         itemEntry       looterGuid       
    loot_candidates       key         playerGuid

    //.lootlogg ID # <Name>
    //Confirms if player is saved to that ID
    //Output: <Name> Is/Isnot currently saved to ID

    .lootlogg <name> ITEMID
    Returns if player has been awarded that item ID - Could combine with .hasitem ?
    Output: <Name> was awarded ID for raid ID # - Has/hasnot got item.

    .lootlog items #
    Returns all items dropped in the loot id and who it was awarded
    Output: Listing players who recieved loot in ID #
    <Name> awarded Item #(edited)
    Could try that as a starter
*/

void LootLogManager::LogGroupKill(Creature * pCreature, Group * pGroup)
{
	if (!pCreature || !pGroup)
		return;
	Loot* loot = &pCreature->loot;
	if (!loot || loot->empty())
		return;

	// Only loot-log boss loot
	if (!(pCreature->GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_BOSS) && !pCreature->IsWorldBoss())
		return;
	

	// Finding the players in range for the loot
	std::map<Player*, std::vector<LootItem>> lootLog;

    //std::lock_guard<std::mutex> guard(_mutex);
    uint64 currentKey = highestKey++;

    CharacterDatabase.PExecute(
        "INSERT INTO `loot_creature_death` (`key`, `creatureEntry`, `timestamp`) VALUES (%u, %u, %lld)", 
        currentKey, pCreature->GetEntry(), (long long)time(NULL));

    for (auto it = loot->items.begin(); it != loot->items.end(); it++)
    {
        CharacterDatabase.PExecute(
            "INSERT INTO `loot_items` (`key`, `itemEntry`) VALUES (%u, %u)",
            currentKey, it->itemid);
    }

    auto it = pGroup->GetMemberSlots().begin();
    for (it; it != pGroup->GetMemberSlots().end(); ++it)
    {
        if (Player* pl = pCreature->GetMap()->GetPlayer(it->guid))
        {
            if (pl->IsAtGroupRewardDistance(pCreature))
            {
                CharacterDatabase.PExecute(
                    "INSERT INTO `loot_candidates` (`key`, `playerGuid`) VALUES (%u, %u)",
                    currentKey, pl->GetGUIDLow());
            }
        }
    }
}

void LootLogManager::LogLootReceived(Creature * pCreature, Player * pPlayer, Item* item)
{
    if (!pCreature || !pPlayer || !item)
        return;

    // Only loot-log boss loot
    if (!(pCreature->GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_BOSS) && !pCreature->IsWorldBoss())
        return;

}

void LootLogManager::Load()
{
    std::lock_guard<std::mutex> guard(_mutex);
    QueryResult* res = CharacterDatabase.PQuery("SELECT MAX(`key`) from loot_creature_death");
    if (res) {
        Field* f = res->Fetch();
        highestKey = f[0].GetUInt64();
    }
    else {
        sLog.outError("Failed retreiving LAST_INSERT_ID for an item in loot_log");
    }

    sLog.outString("> Finished loading Loot Log variables");
}

/*
.lootlogg <name> ITEMID
Returns if player has been awarded that item ID - Could combine with .hasitem ?
Output: <Name> was awarded ID for raid ID # - Has/hasnot got item.
*/
bool ChatHandler::HandleCanReceiveItem(char* args)
{
    if (!*args)
        return false;

    char* nameStr = ExtractOptNotLastArg(&args);
    ObjectGuid target_guid;
    std::string player_name;
    Player* plTarget;
    if (!ExtractPlayerTarget(&nameStr, &plTarget, &target_guid, &player_name))
    {
        SendSysMessage(LANG_PLAYER_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }
    
    uint32 itemId = 0;
    if (!ExtractUInt32(&args, itemId))
    {
        SendSysMessage(LANG_ITEM_NOT_FOUND);
        SetSentErrorMessage(true);
        return false;
    }
    
    ItemPrototype const* pItem = ObjectMgr::GetItemPrototype(itemId);
    if (!pItem)
    {
        PSendSysMessage("Unknown item %u", itemId);
        SetSentErrorMessage(true);
        return false;
    }

    uint32 itemCount = 0;
    if (plTarget)
    {
        itemCount = plTarget->GetItemCount(itemId, true);
    }
    else
    {
        std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
            "SELECT SUM(count) AS item_count FROM item_instance ii WHERE itemEntry = %u and owner_guid = %u",
            itemId, target_guid.GetCounter()
        ));

        if (result)
        {
            auto fields = result->Fetch();
            itemCount = fields[0].GetUInt32();
        }
    }

    PSendSysMessage("%s's amount of %s (id %u) is: %u", player_name.c_str(), GetItemLink(pItem).c_str(), itemId, itemCount);

    std::unique_ptr<QueryResult> result(CharacterDatabase.PQuery(
        "SELECT DISTINCT loot_candidates.key, loot_items.itemEntry, loot_items.looterGuid, loot_creature_death.timestamp, loot_creature_death.creatureEntry "
        "FROM loot_candidates "
        "JOIN loot_items ON loot_candidates.key = loot_items.key "
        "JOIN loot_creature_death ON loot_candidates.key = loot_creature_death.key "
        "WHERE loot_candidates.playerGuid = %u AND loot_items.itemEntry = %u",
        target_guid.GetCounter(), itemId));
    
    if (!result)
    {
        SendSysMessage("Player had no recorded loot-log entries");
        SetSentErrorMessage(true);
        return false;
    }
    else 
    {
        BarGoLink bar((int)result->GetRowCount());
        do
        {
            bar.step();

            Field* fields = result->Fetch();

            uint32 key           = fields[0].GetUInt32();
            uint32 itemEntry     = fields[1].GetUInt32();
            uint32 looterGuid    = fields[2].GetUInt32();
            time_t timestamp     = (time_t)fields[3].GetUInt64();
            uint32 creatureEntry = fields[4].GetUInt32();
            
            std::string ts = TimeToTimestampStr(timestamp);

            PSendSysMessage("Player %s loot-log entries:", player_name.c_str());
            PSendSysMessage("Item: %u, Creature: %u, Kill-Date: %s", itemEntry, creatureEntry, ts.c_str());

        } while (result->NextRow());
    }
    

    return true;
}
