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

#include "lib/self_test.h"

#include "simulation2/system/ComponentManager.h"

#include "simulation2/components/ICmpTemplateManager.h"
#include "simulation2/components/ICmpTest.h"
#include "simulation2/MessageTypes.h"
#include "simulation2/system/ParamNode.h"
#include "simulation2/system/SimContext.h"

#include "ps/Filesystem.h"
#include "ps/CLogger.h"
#include "ps/XML/Xeromyces.h"

class TestCmpTemplateManager : public CxxTest::TestSuite
{
public:
	void setUp()
	{
		g_VFS = CreateVfs(20 * MiB);
		TS_ASSERT_OK(g_VFS->Mount(L"", DataDir()/L"mods/_test.sim"));
		CXeromyces::Startup();
	}

	void tearDown()
	{
		CXeromyces::Terminate();
		g_VFS.reset();
	}

	void test_LoadTemplate()
	{
		CSimContext context;
		CComponentManager man(context);
		man.LoadComponentTypes();

		entity_id_t ent1 = 1, ent2 = 2;
		CParamNode noParam;

		TS_ASSERT(man.AddComponent(ent1, CID_TemplateManager, noParam));

		ICmpTemplateManager* tempMan = static_cast<ICmpTemplateManager*> (man.QueryInterface(ent1, IID_TemplateManager));
		TS_ASSERT(tempMan != NULL);

		const CParamNode* basic = tempMan->LoadTemplate(ent2, L"basic", -1);
		TS_ASSERT(basic != NULL);
		TS_ASSERT_WSTR_EQUALS(basic->ToXML(), L"<x>12345</x>");

		const CParamNode* inherit2 = tempMan->LoadTemplate(ent2, L"inherit2", -1);
		TS_ASSERT(inherit2 != NULL);
		TS_ASSERT_WSTR_EQUALS(inherit2->ToXML(), L"<x a=\"a2\" b=\"b1\" c=\"c1\"><d>d2</d><e>e1</e><f>f1</f><g>g2</g></x>");

		const CParamNode* inherit1 = tempMan->LoadTemplate(ent2, L"inherit1", -1);
		TS_ASSERT(inherit1 != NULL);
		TS_ASSERT_WSTR_EQUALS(inherit1->ToXML(), L"<x a=\"a1\" b=\"b1\" c=\"c1\"><d>d1</d><e>e1</e><f>f1</f></x>");

		const CParamNode* actor = tempMan->LoadTemplate(ent2, L"actor|example", -1);
		TS_ASSERT(actor != NULL);
		TS_ASSERT_WSTR_EQUALS(actor->ToXML(), L"<MotionBallScripted></MotionBallScripted><Position><Altitude>0</Altitude><Anchor>upright</Anchor><Floating>false</Floating></Position><VisualActor><Actor>example</Actor></VisualActor>");
	}

	void test_LoadTemplate_errors()
	{
		CSimContext context;
		CComponentManager man(context);
		man.LoadComponentTypes();

		entity_id_t ent1 = 1, ent2 = 2;
		CParamNode noParam;

		TS_ASSERT(man.AddComponent(ent1, CID_TemplateManager, noParam));

		ICmpTemplateManager* tempMan = static_cast<ICmpTemplateManager*> (man.QueryInterface(ent1, IID_TemplateManager));
		TS_ASSERT(tempMan != NULL);

		TestLogger logger;

		TS_ASSERT(tempMan->LoadTemplate(ent2, L"nonexistent", -1) == NULL);

		const CParamNode* inherit_loop = tempMan->LoadTemplate(ent2, L"inherit-loop", -1);
		TS_ASSERT(inherit_loop == NULL);

		const CParamNode* inherit_broken = tempMan->LoadTemplate(ent2, L"inherit-broken", -1);
		TS_ASSERT(inherit_broken == NULL);
	}

	void test_LoadTemplate_multiple()
	{
		CSimContext context;
		CComponentManager man(context);
		man.LoadComponentTypes();

		entity_id_t ent1 = 1, ent2 = 2;
		CParamNode noParam;

		TS_ASSERT(man.AddComponent(ent1, CID_TemplateManager, noParam));

		ICmpTemplateManager* tempMan = static_cast<ICmpTemplateManager*> (man.QueryInterface(ent1, IID_TemplateManager));
		TS_ASSERT(tempMan != NULL);

		const CParamNode* basicA = tempMan->LoadTemplate(ent2, L"basic", -1);
		TS_ASSERT(basicA != NULL);

		const CParamNode* basicB = tempMan->LoadTemplate(ent2, L"basic", -1);
		TS_ASSERT(basicA == basicB);

		const CParamNode* inherit2A = tempMan->LoadTemplate(ent2, L"inherit2", -1);
		TS_ASSERT(inherit2A != NULL);

		const CParamNode* inherit2B = tempMan->LoadTemplate(ent2, L"inherit2", -1);
		TS_ASSERT(inherit2A == inherit2B);

		TestLogger logger;

		TS_ASSERT(tempMan->LoadTemplate(ent2, L"nonexistent", -1) == NULL);
		TS_ASSERT(tempMan->LoadTemplate(ent2, L"nonexistent", -1) == NULL);

		TS_ASSERT(tempMan->LoadTemplate(ent2, L"inherit-loop", -1) == NULL);
		TS_ASSERT(tempMan->LoadTemplate(ent2, L"inherit-loop", -1) == NULL);

		TS_ASSERT(tempMan->LoadTemplate(ent2, L"inherit-broken", -1) == NULL);
		TS_ASSERT(tempMan->LoadTemplate(ent2, L"inherit-broken", -1) == NULL);
	}
};
