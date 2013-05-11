/* Copyright (C) 2006 - 2013 ScriptDev2 <http://www.scriptdev2.com/>
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

/* ScriptData
SDName: instance_end_of_time
SD%Complete: 0
SDComment: Placeholder
SDCategory: End of Time
EndScriptData */

#include "precompiled.h"
#include "end_of_time.h"

instance_end_of_time::instance_end_of_time(Map* pMap) : ScriptedInstance(pMap)
{
    Initialize();
};

void instance_end_of_time::Initialize()
{
    memset(&m_auiEncounter, 0, sizeof(m_auiEncounter));

    uint32 mask = 0;
    if (urand(0, 1))
        mask |= 1 << (TYPE_BAINE - 1);
    else
        mask |= 1 << (TYPE_JAINA - 1);
    if (urand(0, 1))
        mask |= 1 << (TYPE_SYLVANAS - 1);
    else
        mask |= 1 << (TYPE_TYRANDE - 1);

    SetData(TYPE_ENCOUNTER_MASK, mask);
}

void instance_end_of_time::OnCreatureCreate(Creature* pCreature)
{
    switch (pCreature->GetEntry())
    {
        default:
            return;
    }
}

void instance_end_of_time::OnObjectCreate(GameObject* pGo)
{
    switch (pGo->GetEntry())
    {
        case GO_MUROZOND_CACHE:
        case GO_HOURGLASS_OF_TIME:
            break;
        default:
            return;
    }

    m_mGoEntryGuidStore[pGo->GetEntry()] = pGo->GetObjectGuid();
}

void instance_end_of_time::SetData(uint32 uiType, uint32 uiData)
{
    switch (uiType)
    {
        case TYPE_ENCOUNTER_MASK:
        case TYPE_BAINE:
        case TYPE_JAINA:
        case TYPE_SYLVANAS:
        case TYPE_TYRANDE:
            m_auiEncounter[uiType] = uiData;
            break;
        case TYPE_MUROZOND:
            m_auiEncounter[uiType] = uiData;

            if (GameObject* go = GetSingleGameObjectFromStorage(GO_HOURGLASS_OF_TIME))
                if (uiData == IN_PROGRESS)
                {
                    go->RemoveFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                    hourglassUseCount = 0;
                    DoCastSpellOnPlayers(SPELL_SANDS_OF_THE_HOURGLASS);
                }
                else
                {
                    go->SetFlag(GAMEOBJECT_FLAGS, GO_FLAG_NO_INTERACT);
                    DoRemoveAurasDueToSpellOnPlayers(SPELL_SANDS_OF_THE_HOURGLASS);
                }

            if (uiData == DONE)
                DoRespawnGameObject(GO_MUROZOND_CACHE, HOUR);
            break;
        default:
            return;
    }

    if (uiData == DONE || uiType == TYPE_ENCOUNTER_MASK)
    {
        OUT_SAVE_INST_DATA;

        std::ostringstream saveStream;
        saveStream << m_auiEncounter[0] << " " << m_auiEncounter[1] << " " << m_auiEncounter[2] << " " <<
            m_auiEncounter[3] << " " << m_auiEncounter[4] << " " << m_auiEncounter[5];

        m_strInstData = saveStream.str();

        SaveToDB();
        OUT_SAVE_INST_DATA_COMPLETE;
    }
}

uint32 instance_end_of_time::GetData(uint32 uiType)
{
    if (uiType < MAX_ENCOUNTER)
        return m_auiEncounter[uiType];

    return 0;
}

void instance_end_of_time::Load(const char* chrIn)
{
    if (!chrIn)
    {
        OUT_LOAD_INST_DATA_FAIL;
        return;
    }

    OUT_LOAD_INST_DATA(chrIn);

    std::istringstream loadStream(chrIn);
    loadStream >> m_auiEncounter[0] >> m_auiEncounter[1] >> m_auiEncounter[2] >>
        m_auiEncounter[3] >> m_auiEncounter[4] >> m_auiEncounter[5];

    for (uint8 i = 1; i < MAX_ENCOUNTER; ++i)
    {
        if (m_auiEncounter[i] == IN_PROGRESS)
            m_auiEncounter[i] = NOT_STARTED;
    }

    OUT_LOAD_INST_DATA_COMPLETE;
}

struct AuraInfo
{
    uint32 spellId;
    uint32 duration;
    uint32 bp[MAX_EFFECT_INDEX];
    ObjectGuid casterGuid;
};

struct SaveStruct
{
    float posX;
    float posY;
    float posZ;
    float posO;

    std::list<AuraInfo> auraInfo;
};

void instance_end_of_time::OnHourglassUse(Player* who)
{
    if (GetData(TYPE_MUROZOND) != IN_PROGRESS)
        return;

    if (hourglassUseCount >= 5)
        return;

    ++hourglassUseCount;

    DoCastSpellOnPlayers(SPELL_SANDS_OF_THE_HOURGLASS, &hourglassUseCount);

    if (hourglassUseCount == 1)
    {
        savedData.clear();

        Map::PlayerList const& players = instance->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
            if (Player* player = itr->getSource())
            {
                if (player->isGameMaster())
                    continue;

                player->RemoveArenaSpellCooldowns();

                savedData[player->GetGUIDLow()].posX = player->GetPositionX();
                savedData[player->GetGUIDLow()].posY = player->GetPositionY();
                savedData[player->GetGUIDLow()].posZ = player->GetPositionZ();
                savedData[player->GetGUIDLow()].posO = player->GetOrientation();

                for(Unit::SpellAuraHolderMap::iterator iter = player->GetSpellAuraHolderMap().begin(); iter != player->GetSpellAuraHolderMap().end(); ++itr)
                {
                    if (!iter->second->GetSpellProto()->HasAttribute(SPELL_ATTR_EX4_UNK21) &&
                                                                        // don't remove stances, shadowform, pally/hunter auras
                        !iter->second->IsPassive() &&                   // don't remove passive auras
                        (!iter->second->GetSpellProto()->HasAttribute(SPELL_ATTR_UNAFFECTED_BY_INVULNERABILITY) ||
                        !iter->second->GetSpellProto()->HasAttribute(SPELL_ATTR_UNK8)) &&
                                                                        // not unaffected by invulnerability auras or not having that unknown flag (that seemed the most probable)
                        iter->second->IsPositive() &&
                        iter->second->GetAuraDuration() > 0 &&
                        iter->second->GetId() != 101591)
                    {
                        AuraInfo info;
                        info.spellId = iter->second->GetId();
                        info.duration = iter->second->GetAuraDuration();
                        info.casterGuid = iter->second->GetCasterGuid();

                        for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                            if (Aura const* aura = iter->second->GetAuraByEffectIndex(SpellEffectIndex(i)))
                                info.bp[i] = aura->GetModifier()->m_amount;
                            else
                                info.bp[i] = 0;
                    }
                }
            }
        return;
    }

    Map::PlayerList const& players = instance->GetPlayers();
    for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        if (Player* player = itr->getSource())
        {
            if (player->isGameMaster())
                continue;

            player->RemoveArenaSpellCooldowns();

            std::map<uint32, SaveStruct>::iterator itr2 = savedData.find(player->GetGUIDLow());
            if (itr2 == savedData.end())
                continue;

            for (std::list<AuraInfo>::iterator itr3 = itr2->second.auraInfo.begin(); itr3 != itr2->second.auraInfo.end(); ++itr3)
            {
                Unit* caster = instance->GetUnit(itr3->casterGuid);
                if (!caster)
                    continue;

                player->RemoveAurasByCasterSpell(itr3->spellId, itr3->casterGuid);
                if (SpellAuraHolder* holder = player->_AddAura(itr3->spellId, itr3->duration, caster))
                    for (int i = 0; i < MAX_EFFECT_INDEX; ++i)
                        if (Aura* aura = holder->GetAuraByEffectIndex(SpellEffectIndex(i)))
                            if (aura->GetModifier()->m_amount != itr3->bp[i])
                            {
                                aura->ApplyModifier(false, true);
                                aura->ChangeAmount(itr3->bp[i]);
                                aura->ApplyModifier(true, true);
                            }
            }

            player->NearTeleportTo(itr2->second.posX, itr2->second.posY, itr2->second.posZ, itr2->second.posO);
        }
}

InstanceData* GetInstanceData_instance_end_of_time(Map* pMap)
{
    return new instance_end_of_time(pMap);
}

void AddSC_instance_end_of_time()
{
    Script* pNewScript;

    pNewScript = new Script;
    pNewScript->Name = "instance_end_of_time";
    pNewScript->GetInstanceData = &GetInstanceData_instance_end_of_time;
    pNewScript->RegisterSelf();
}
