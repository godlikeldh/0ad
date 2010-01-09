/* Copyright (C) 2010 Wildfire Games.
 * This file is part of 0 A.D.
 *
 * 0 A.D. is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * 0 A.D. is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with 0 A.D.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "precompiled.h"

#include "ComponentManager.h"
#include "IComponent.h"
#include "ParamNode.h"

#include "simulation2/serialization/DebugSerializer.h"
#include "simulation2/serialization/HashSerializer.h"
#include "simulation2/serialization/StdSerializer.h"
#include "simulation2/serialization/StdDeserializer.h"

#include "simulation2/components/ICmpTemplateManager.h"

#include "ps/CLogger.h"

bool CComponentManager::DumpDebugState(std::ostream& stream)
{
	CDebugSerializer serializer(m_ScriptInterface, stream);

	// We want the output to be grouped by entity ID, so invert the CComponentManager data structures
	std::map<entity_id_t, std::map<ComponentTypeId, IComponent*> > components;
	std::map<ComponentTypeId, std::string> names;

	std::map<ComponentTypeId, std::map<entity_id_t, IComponent*> >::const_iterator ctit = m_ComponentsByTypeId.begin();
	for (; ctit != m_ComponentsByTypeId.end(); ++ctit)
	{
		std::map<entity_id_t, IComponent*>::const_iterator eit = ctit->second.begin();
		for (; eit != ctit->second.end(); ++eit)
		{
			components[eit->first][ctit->first] = eit->second;
		}
	}

	std::map<entity_id_t, std::map<ComponentTypeId, IComponent*> >::const_iterator cit = components.begin();
	for (; cit != components.end(); ++cit)
	{
		std::stringstream n;
		n << "- id: " << cit->first;
		serializer.TextLine(n.str());

		std::map<ComponentTypeId, IComponent*>::const_iterator ctit = cit->second.begin();
		for (; ctit != cit->second.end(); ++ctit)
		{
			std::stringstream n;
			n << "  " << LookupComponentTypeName(ctit->first) << ":";
			serializer.TextLine(n.str());
			serializer.Indent(4);
			ctit->second->Serialize(serializer);
			serializer.Dedent(4);
		}
		serializer.TextLine("");
	}

	// TODO: catch exceptions
	return true;
}

bool CComponentManager::ComputeStateHash(std::string& outHash)
{
	CHashSerializer serializer(m_ScriptInterface);

	std::map<ComponentTypeId, std::map<entity_id_t, IComponent*> >::const_iterator cit = m_ComponentsByTypeId.begin();
	for (; cit != m_ComponentsByTypeId.end(); ++cit)
	{
		// Skip component types with no components
		if (cit->second.empty())
			continue;

		serializer.NumberI32_Unbounded("component type id", cit->first);

		std::map<entity_id_t, IComponent*>::const_iterator eit = cit->second.begin();
		for (; eit != cit->second.end(); ++eit)
		{
			serializer.NumberU32_Unbounded("entity id", eit->first);
			eit->second->Serialize(serializer);
		}
	}

	outHash = std::string((const char*)serializer.ComputeHash(), serializer.GetHashLength());

	// TODO: catch exceptions
	return true;
}

/*
 * Simulation state serialization format:
 *
 * TODO: Global version number.
 * Number of (non-empty) component types.
 * For each component type:
 *   Component type name.
 *   TODO: Component type version number.
 *   Number of entities.
 *   For each entity:
 *     Entity id.
 *     Component state.
 *
 * Rationale:
 * Saved games should be valid across patches, which might change component
 * type IDs. Thus the names are serialized, not the IDs.
 * Version numbers are used so saved games from future versions can be rejected,
 * and those from older versions can be fixed up to work with the latest version.
 * (These aren't really needed for networked games (where everyone will have the same
 * version), but it doesn't seem worth having a separate codepath for that.)
 */

bool CComponentManager::SerializeState(std::ostream& stream)
{
	CStdSerializer serializer(m_ScriptInterface, stream);

	uint32_t numComponentTypes = 0;

	std::map<ComponentTypeId, std::map<entity_id_t, IComponent*> >::const_iterator cit;

	for (cit = m_ComponentsByTypeId.begin(); cit != m_ComponentsByTypeId.end(); ++cit)
	{
		if (cit->second.empty())
			continue;

		numComponentTypes++;
	}

	serializer.NumberU32_Unbounded("num component types", numComponentTypes);

	for (cit = m_ComponentsByTypeId.begin(); cit != m_ComponentsByTypeId.end(); ++cit)
	{
		if (cit->second.empty())
			continue;

		std::map<ComponentTypeId, ComponentType>::const_iterator ctit = m_ComponentTypesById.find(cit->first);
		if (ctit == m_ComponentTypesById.end())
		{
			debug_warn(L"Invalid ctit"); // this should never happen
			return false;
		}

		serializer.StringASCII("name", ctit->second.name, 0, 255);

		uint32_t numComponents = cit->second.size();
		serializer.NumberU32_Unbounded("num components", numComponents);

		std::map<entity_id_t, IComponent*>::const_iterator eit = cit->second.begin();
		for (; eit != cit->second.end(); ++eit)
		{
			serializer.NumberU32_Unbounded("entity id", eit->first);
			eit->second->Serialize(serializer);
		}
	}

	// TODO: catch exceptions
	return true;
}

bool CComponentManager::DeserializeState(std::istream& stream)
{
	CStdDeserializer deserializer(m_ScriptInterface, stream);

	DestroyAllComponents();

	uint32_t numComponentTypes;
	deserializer.NumberU32_Unbounded(numComponentTypes);

	ICmpTemplateManager* templateManager = NULL;
	CParamNode noParam;

	for (size_t i = 0; i < numComponentTypes; ++i)
	{
		std::string ctname;
		deserializer.StringASCII(ctname, 0, 255);

		ComponentTypeId ctid = LookupCID(ctname);
		if (ctid == CID__Invalid)
		{
			LOGERROR(L"Deserialization saw unrecognised component type '%hs'", ctname.c_str());
			return false;
		}

		uint32_t numComponents;
		deserializer.NumberU32_Unbounded(numComponents);

		for (size_t j = 0; j < numComponents; ++j)
		{
			entity_id_t ent;
			deserializer.NumberU32_Unbounded(ent);
			IComponent* component = ConstructComponent(ent, ctid);
			if (!component)
				return false;

			const CParamNode* cmpTemplate = NULL;
			if (templateManager && ent != SYSTEM_ENTITY)
			{
				const CParamNode* entTemplate = templateManager->LoadLatestTemplate(ent);
				if (entTemplate)
					cmpTemplate = entTemplate->GetChild(ctname.c_str());
			}
			const CParamNode& paramNode = (cmpTemplate ? *cmpTemplate : noParam);

			component->Deserialize(m_SimContext, paramNode, deserializer);

			// If this was the template manager, remember it so we can use it when
			// deserializing any further non-system entities
			if (ent == SYSTEM_ENTITY && ctid == CID_TemplateManager)
				templateManager = static_cast<ICmpTemplateManager*> (component);
		}
	}

	if (stream.peek() != EOF)
	{
		LOGERROR(L"Deserialization didn't reach EOF");
		return false;
	}

	// TODO: catch exceptions
	return true;
}
