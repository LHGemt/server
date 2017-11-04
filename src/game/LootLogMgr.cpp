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

INSTANTIATE_SINGLETON_1(LootLogManager);

/*
	When an item drops from a creature, each item gets an entry in the LootLog table.
	The entry will contain the entry of the item, the creature which dropped the item, a foreign key
	pointing to a set of playerGuids in the LootCandidates table and nullable playerGuid, lootedBy.
	LootedBy will be null when the row is first inserted into the table. As soon as the item is given 
	to a player lootedBy will be updated to that playerGuid.

	Table:	loot_log
	type:   shared primary key		                    creature-entry		    nullable player GUID	unix-timestamp
	cols:	key						itemEntry			creatureEntry			looterGuid				timestamp

    Table:  loot_creature_death
    type:

    Table:	loot_items
    type:   loot_creature_death.key creature-entry		    nullable player GUID	unix-timestamp
    cols:	key						itemEntry			creatureEntry			looterGuid				timestamp

	Table:	loot_log_players
    type:   shared primary key		GUID of player
	cols:	key						playerGuid



    loot_creature_death   key         creatureEntry   timestamp
    loot_items            key         itemEntry       looterGuid       
    loot_candidates       key         playerGuid
*/

void LootLogManager::LogGroupKill(Creature * pCreature, Group * pGroup)
{
	if (!pCreature || !pGroup)
		return;
	Loot* loot = &pCreature->loot;
	if (!loot || loot->empty())
		return;

	// Only loot-log boss loot
	if (!(pCreature->GetCreatureInfo()->type_flags & CREATURE_TYPEFLAGS_BOSS)
		&& !pCreature->IsWorldBoss())
		return;
	

	// Finding the players in range for the loot
	std::map<Player*, std::vector<LootItem>> lootLog;

    /*
	auto it = pGroup->GetMemberSlots().begin();
	for (it; it != pGroup->GetMemberSlots().end(); ++it)
	{
		if (Player* pl = pCreature->GetMap()->GetPlayer(it->guid))
		{
			if (pl->IsAtGroupRewardDistance(pCreature))
			{
				// log this shit
                for (auto lIt = loot->items.begin(); lIt != loot->items.end(); ++lIt) 
                {
                    if (lIt->AllowedForPlayer(pl, pCreature))
                    {
                        lootLog[pl].push_back(*lIt);
                        sLog.outBasic("Player: %s, Item: %d", pl->GetName(), lIt->itemid);
                    }
                }
			}
		}
	}

    std::map<uint32, uint64> log_row_ids;
    */
	
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
