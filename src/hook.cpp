#include "hook.h"
#include "ClibUtil/editorID.hpp"

namespace hooks
{
	void OnMeleeHitHook::Set_iFrames(RE::Actor* actor)
	{
		actor->SetGraphVariableBool("bIframeActive", true);
		actor->SetGraphVariableBool("bInIframe", true);
	}

	void OnMeleeHitHook::Reset_iFrames(RE::Actor* actor)
	{
		actor->SetGraphVariableBool("bIframeActive", false);
		actor->SetGraphVariableBool("bInIframe", false);
	}

	void OnMeleeHitHook::dispelEffect(RE::MagicItem* spellForm, RE::Actor* a_target)
	{
		const auto targetActor = a_target->AsMagicTarget();
		if (targetActor->HasMagicEffect(spellForm->effects[0]->baseEffect)) {
			auto activeEffects = targetActor->GetActiveEffectList();
			for (const auto& effect : *activeEffects) {
				if (effect->spell == spellForm) {
					effect->Dispel(true);
				}
			}
		}
	}

	bool OnMeleeHitHook::isHumanoid(RE::Actor* a_actor)
	{
		auto bodyPartData = a_actor->GetRace() ? a_actor->GetRace()->bodyPartData : nullptr;
		return bodyPartData && bodyPartData->GetFormID() == 0x1d;
	}

	bool OnMeleeHitHook::is_valid_actor(RE::Actor* a_actor)
	{
		bool result = true;
		if (a_actor->IsPlayerRef() || a_actor->IsDead() || !isHumanoid(a_actor) 
		|| !(a_actor->HasKeywordString("ActorTypeNPC") || a_actor->HasKeywordString("DLC2ActorTypeMiraak")) 
		|| a_actor->HasKeywordString("PCG_ExcludeSprintAttacks")){
			result = false;
		}
		return result;
	}

	bool OnMeleeHitHook::isPowerAttacking(RE::Actor* a_actor)
	{
		auto currentProcess = a_actor->GetActorRuntimeData().currentProcess;
		if (currentProcess) {
			auto highProcess = currentProcess->high;
			if (highProcess) {
				auto attackData = highProcess->attackData;
				if (attackData) {
					auto flags = attackData->data.flags;
					return flags.any(RE::AttackData::AttackFlag::kPowerAttack);
				}
			}
		}
		return false;
	}

	void OnMeleeHitHook::UpdateCombatTarget(RE::Actor* a_actor){
		auto CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		if (!CTarget) {
			auto combatGroup = a_actor->GetCombatGroup();
			if (combatGroup) {
				for (auto it = combatGroup->targets.begin(); it != combatGroup->targets.end(); ++it) {
					if (it->targetHandle && it->targetHandle.get().get()) {
						a_actor->GetActorRuntimeData().currentCombatTarget = it->targetHandle.get().get();
						break;
					}
					continue;
				}
			}
		}
		//a_actor->UpdateCombat();
	}


	bool OnMeleeHitHook::IsCasting(RE::Actor* a_actor)
	{
		bool result = false;

		auto IsCastingRight = false;
		auto IsCastingLeft = false;
		auto IsCastingDual = false;

		if ((a_actor->GetGraphVariableBool("IsCastingRight", IsCastingRight) && IsCastingRight) 
		|| (a_actor->GetGraphVariableBool("IsCastingLeft", IsCastingLeft) && IsCastingLeft) 
		|| (a_actor->GetGraphVariableBool("IsCastingDual", IsCastingDual) && IsCastingDual)) {
			result = true;
		}
		
		return result;
	}

	void OnMeleeHitHook::InterruptAttack(RE::Actor* a_actor){
		a_actor->NotifyAnimationGraph("attackStop");
		a_actor->NotifyAnimationGraph("recoilStop");
		a_actor->NotifyAnimationGraph("bashStop");
		a_actor->NotifyAnimationGraph("blockStop");
		a_actor->NotifyAnimationGraph("staggerStop");
	}

	std::vector<RE::TESForm*> OnMeleeHitHook::GetEquippedForm(RE::Actor* actor)
	{
		std::vector<RE::TESForm*> Hen;

		// if (actor->GetEquippedObject(true)) {
		// 	Hen.push_back(actor->GetEquippedObject(true));
		// }
		if (actor->GetEquippedObject(false)) {
			Hen.push_back(actor->GetEquippedObject(false));
		}

		return Hen;
	}

	bool OnMeleeHitHook::GetEquippedType_IsMelee(RE::Actor* actor)
	{
		bool result = false;
		auto form_list = GetEquippedForm(actor);

		if (!form_list.empty()) {
			for (auto form : form_list) {
				if (form) {
					switch (*form->formType) {
					case RE::FormType::Weapon:
						if (const auto equippedWeapon = form->As<RE::TESObjectWEAP>()) {
							switch (equippedWeapon->GetWeaponType()) {
							case RE::WEAPON_TYPE::kHandToHandMelee:
							case RE::WEAPON_TYPE::kOneHandSword:
							case RE::WEAPON_TYPE::kOneHandDagger:
							case RE::WEAPON_TYPE::kOneHandAxe:
							case RE::WEAPON_TYPE::kOneHandMace:
							case RE::WEAPON_TYPE::kTwoHandSword:
							case RE::WEAPON_TYPE::kTwoHandAxe:
								result = true;
								break;
							default:
								break;
							}
						}
						break;
					case RE::FormType::Armor:
						// if (auto equippedShield = form->As<RE::TESObjectARMO>()) {
						// 	result = true;
						// }
						break;
					default:
						break;
					}
					if (result) {
						break;
					}
				}
				continue;
			}
		}
		return result;
	}

	bool OnMeleeHitHook::is_melee(RE::Actor* actor)
	{
		return GetEquippedType_IsMelee(actor);
	}

	float OnMeleeHitHook::deduce_wait_time(STATIC_ARGS, RE::Actor* a_actor)
	{
		auto wait_time = 0.1f;

		auto CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		if (!CTarget) {
			UpdateCombatTarget(a_actor);
			CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		}
		auto distance = a_actor->GetPosition().GetDistance(CTarget->GetPosition());

		if (distance > 130.0f){
			int k;
			for (k = 130; k <= distance; k += 10) {
				wait_time += 0.1f;
			}
		}

		return wait_time;
	}

	float OnMeleeHitHook::AV_Mod(RE::Actor* a_actor, int actor_value, float input, float mod)
	{

		int k;
		for (k = 0; k <= actor_value; k += 1) {
			input += mod;
		}

		return input;
	}

	void OnMeleeHitHook::begin_sprint(STATIC_ARGS, RE::Actor* a_actor)
	{

		auto magicTarget = a_actor->AsMagicTarget();
		const auto magicEffect1 = RE::TESForm::LookupByEditorID("PCG_SprintAttackExecute_effect")->As<RE::EffectSetting>();

		if (magicTarget->HasMagicEffect(magicEffect1)) {
			return;
		}

		auto isShouting = false;
		if (((a_actor->GetGraphVariableBool("isShouting ", isShouting) && isShouting)) || IsCasting(a_actor)) {
			return;
		}

		auto CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		if (!CTarget) {
			UpdateCombatTarget(a_actor);
			CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		}

		if(a_actor->GetPosition().GetDistance(CTarget->GetPosition()) < 130.0f){
			return;
		}

		bool hasLOS = false;
		if (a_actor->HasLineOfSight(CTarget, hasLOS) && !hasLOS) {
			return;
		}

		InterruptAttack(a_actor);

		auto bAnimationDriven = false;
		if ((a_actor->GetGraphVariableBool("bAnimationDriven", bAnimationDriven) && !bAnimationDriven)) {
			a_actor->SetGraphVariableBool("bPGC_altered_drivenState", true);
			a_actor->SetGraphVariableBool("bAnimationDriven", true);
		}

		if(!a_actor->AsActorState()->IsSprinting()){
			a_actor->NotifyAnimationGraph("SprintStart");
			a_actor->AsActorState()->actorState1.sprinting = 1;
		}

		const auto wait = RE::TESForm::LookupByEditorID<RE::MagicItem>("PCG_SprintAttack_Execute_Spell");
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		caster->CastSpellImmediate(wait, true, a_actor, 1, false, 0.0, a_actor);

	}

	void OnMeleeHitHook::execute_sprint_attack(STATIC_ARGS, RE::Actor* a_actor)
	{
		if (a_actor->AsActorState()->IsSprinting()){
			a_actor->NotifyAnimationGraph("MCO_SprintPowerAttackInitiate");
			a_actor->NotifyAnimationGraph("attackStartSprint");
			a_actor->NotifyAnimationGraph("attackStart");
			auto bPGC_altered_drivenState = false;
			if ((a_actor->GetGraphVariableBool("bPGC_altered_drivenState", bPGC_altered_drivenState) && bPGC_altered_drivenState)) {
				a_actor->SetGraphVariableBool("bAnimationDriven", false);
				a_actor->SetGraphVariableBool("bPGC_altered_drivenState", false);
			}
			a_actor->AsActorState()->actorState1.sprinting = 0;
		}else{
			auto bPGC_altered_drivenState = false;
			if ((a_actor->GetGraphVariableBool("bPGC_altered_drivenState", bPGC_altered_drivenState) && bPGC_altered_drivenState)) {
				a_actor->SetGraphVariableBool("bAnimationDriven", false);
				a_actor->SetGraphVariableBool("bPGC_altered_drivenState", false);
			}
		}
	}


	class OurEventSink :
		public RE::BSTEventSink<RE::TESSwitchRaceCompleteEvent>,
		public RE::BSTEventSink<RE::TESEquipEvent>,
		public RE::BSTEventSink<RE::TESCombatEvent>,
		public RE::BSTEventSink<RE::TESActorLocationChangeEvent>,
		public RE::BSTEventSink<RE::TESSpellCastEvent>,
		public RE::BSTEventSink<SKSE::ModCallbackEvent>
	{
		OurEventSink() = default;
		OurEventSink(const OurEventSink&) = delete;
		OurEventSink(OurEventSink&&) = delete;
		OurEventSink& operator=(const OurEventSink&) = delete;
		OurEventSink& operator=(OurEventSink&&) = delete;

	public:
		static OurEventSink* GetSingleton()
		{
			static OurEventSink singleton;
			return &singleton;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSwitchRaceCompleteEvent* event, RE::BSTEventSource<RE::TESSwitchRaceCompleteEvent>*)
		{
			auto a_actor = event->subject->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*){
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
		{
			
			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event, RE::BSTEventSource<RE::TESCombatEvent>*){
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			switch (event->newState.get()) {
			case RE::ACTOR_COMBAT_STATE::kCombat:
				a_actor->SetGraphVariableBool("bPGC_IsInCombat", true);
				break;

			case RE::ACTOR_COMBAT_STATE::kSearching:
			case RE::ACTOR_COMBAT_STATE::kNone:
				a_actor->SetGraphVariableBool("bPGC_IsInCombat", false);
				break;

			default:
				break;
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*)
		{

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*)
		{
			auto a_actor = event->object->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto eSpell = RE::TESForm::LookupByID(event->spell);

			if (eSpell && eSpell->Is(RE::FormType::Spell)) {
				auto rSpell = eSpell->As<RE::SpellItem>();
				if (rSpell->GetSpellType() == RE::MagicSystem::SpellType::kSpell) {
					std::string Lsht = (clib_util::editorID::get_editorID(rSpell));
					switch (hash(Lsht.c_str(), Lsht.size())) {
					case "PCG_SprintAttack_Execute_Spell"_h:
						break;
					default:
						break;
					}
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}
	};

	RE::BSEventNotifyControl animEventHandler::HookedProcessEvent(RE::BSAnimationGraphEvent& a_event, RE::BSTEventSource<RE::BSAnimationGraphEvent>* src)
	{
		FnProcessEvent fn = fnHash.at(*(uint64_t*)this);

		if (!a_event.holder) {
			return fn ? (this->*fn)(a_event, src) : RE::BSEventNotifyControl::kContinue;
		}

		RE::Actor* actor = const_cast<RE::TESObjectREFR*>(a_event.holder)->As<RE::Actor>();
		switch (hash(a_event.tag.c_str(), a_event.tag.size())) {
		case "tailSprint"_h:
		// case "FootLeft"_h:
		// case "FootSprintLeft"_h:
		// case "FootRight"_h:
		// case "FootSprintRight"_h:
			 if (actor->IsInCombat() && OnMeleeHitHook::is_melee(actor) && !actor->IsAttacking()) {
			 	
				
			 }
			break;

		default:
			break;
		}

		return fn ? (this->*fn)(a_event, src) : RE::BSEventNotifyControl::kContinue;
	}

	std::unordered_map<uint64_t, animEventHandler::FnProcessEvent> animEventHandler::fnHash;

	void OnMeleeHitHook::install(){

		auto eventSink = OurEventSink::GetSingleton();

		// ScriptSource
		auto* eventSourceHolder = RE::ScriptEventSourceHolder::GetSingleton();
		eventSourceHolder->AddEventSink<RE::TESSwitchRaceCompleteEvent>(eventSink);
		eventSourceHolder->AddEventSink<RE::TESEquipEvent>(eventSink);
		eventSourceHolder->AddEventSink<RE::TESCombatEvent>(eventSink);
		eventSourceHolder->AddEventSink<RE::TESActorLocationChangeEvent>(eventSink);
		eventSourceHolder->AddEventSink<RE::TESSpellCastEvent>(eventSink);
		SKSE::GetModCallbackEventSource()->AddEventSink(eventSink);
	}

	bool OnMeleeHitHook::BindPapyrusFunctions(VM* vm)
	{
		vm->RegisterFunction("deduce_wait_time", "PGC_NativeFunctions", deduce_wait_time);
		vm->RegisterFunction("execute_sprint_attack", "PGC_NativeFunctions", execute_sprint_attack);
		vm->RegisterFunction("begin_sprint", "PGC_NativeFunctions", begin_sprint);
		return true;
	}

	float OnMeleeHitHook::get_group_threatRatio(RE::Actor* protagonist, RE::Actor* combat_target)
	{
		float result = 0.0f;
		float MyTeam_total_threat = 0.0f;
		float EnemyTeam_total_threat = 0.0f;
		auto combatGroup = protagonist->GetCombatGroup();
		if (combatGroup) {
			
			for (auto it = combatGroup->members.begin(); it != combatGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get()) {
					MyTeam_total_threat += it->threatValue;
				}
				continue;
			}
		}
		auto EnemyGroup = combat_target->GetCombatGroup();
		if (EnemyGroup) {
			for (auto it = EnemyGroup->members.begin(); it != EnemyGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get()) {
					EnemyTeam_total_threat += it->threatValue;
				}
				continue;
			}
		}

		if (MyTeam_total_threat > 0 && EnemyTeam_total_threat > 0) {
			result = MyTeam_total_threat / EnemyTeam_total_threat;
		}

		return result;
	}

	float OnMeleeHitHook::get_personal_survivalRatio(RE::Actor* protagonist, RE::Actor* combat_target)
	{
		float result = 0.0f;
		float MyTeam_total_threat = 0.0f;
		float EnemyTeam_total_threat = 0.0f;
		float personal_threat = 0.0f;
		auto  combatGroup = protagonist->GetCombatGroup();
		if (combatGroup) {
			for (auto it = combatGroup->members.begin(); it != combatGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get()) {
					MyTeam_total_threat += it->threatValue;
					if (it->memberHandle.get().get() == protagonist){
						personal_threat += it->threatValue;
					}
				}
				continue;
			}
		}
		auto EnemyGroup = combat_target->GetCombatGroup();
		if (EnemyGroup) {
			for (auto it = EnemyGroup->members.begin(); it != EnemyGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get()) {
					EnemyTeam_total_threat += it->threatValue;
				}
				continue;
			}
		}

		if (MyTeam_total_threat > 0 && EnemyTeam_total_threat > 0 && personal_threat > 0) {

			auto personal_survival = personal_threat/EnemyTeam_total_threat;
			auto Enemy_groupSurvival = EnemyTeam_total_threat/MyTeam_total_threat;

			result = personal_survival/Enemy_groupSurvival;
		}

		return result;
	}

	float OnMeleeHitHook::get_personal_threatRatio(RE::Actor* protagonist, RE::Actor* combat_target)
	{
		float result = 0.0f;
		float personal_threat = 0.0f;
		float CTarget_threat = 0.0f;

		auto  combatGroup = protagonist->GetCombatGroup();
		if (combatGroup) {
			for (auto it = combatGroup->members.begin(); it != combatGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get() && it->memberHandle.get().get() == protagonist) {
					personal_threat += it->threatValue;
					break;
				}
				continue;
			}
		}
		auto EnemyGroup = combat_target->GetCombatGroup();
		if (EnemyGroup) {
			for (auto it = EnemyGroup->members.begin(); it != EnemyGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get() && it->memberHandle.get().get() == combat_target) {
					CTarget_threat += it->threatValue;
					break;
				}
				continue;
			}
		}

		if (personal_threat > 0 && CTarget_threat > 0) {
			result = personal_threat / CTarget_threat;
		}

		return result;
	}

	void OnMeleeHitHook::Update(RE::Actor* a_actor, [[maybe_unused]] float a_delta)
	{
		if (a_actor->GetActorRuntimeData().currentProcess && a_actor->GetActorRuntimeData().currentProcess->InHighProcess() && a_actor->Is3DLoaded()){
			auto bPGC_IsInCombat = false;
			if ((a_actor->GetGraphVariableBool("bPGC_IsInCombat", bPGC_IsInCombat) && bPGC_IsInCombat) && is_melee(a_actor)) {
				auto confidence = static_cast<int>(a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kConfidence));
				auto aggression = static_cast<int>(a_actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kAggression));

				auto CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
				if (!CTarget) {
					return;
				}

				auto personal_threat = get_personal_threatRatio(a_actor, CTarget);
				auto group_threat = get_group_threatRatio(a_actor, CTarget);
				auto personal_survival = get_personal_survivalRatio(a_actor, CTarget);

				if (personal_threat <= 0.625f){
					a_actor->SetGraphVariableBool("CPR_EnableCircling", true);
				}else{
					a_actor->SetGraphVariableBool("CPR_EnableCircling", false);
				}

				if (group_threat > 0.625f){
					a_actor->SetGraphVariableBool("CPR_EnableAdvanceRadius", true);
				}else{
					a_actor->SetGraphVariableBool("CPR_EnableAdvanceRadius", false);
				}


				if (personal_survival <= 0.625f){
					a_actor->SetGraphVariableBool("CPR_EnableBackoff", true);
					a_actor->SetGraphVariableBool("CPR_EnableFallback", true);
				}else{
					a_actor->SetGraphVariableBool("CPR_EnableBackoff", false);
					a_actor->SetGraphVariableBool("CPR_EnableFallback", false);
				}

				auto CPR_EnableCircling = false;
				auto CPR_EnableAdvanceRadius = false;
				auto CPR_EnableBackoff = false;
				auto CPR_EnableFallback = false;

				if ((a_actor->GetGraphVariableBool("CPR_EnableCircling", CPR_EnableCircling) && CPR_EnableCircling)) {
					a_actor->SetGraphVariableFloat("CPR_CirclingDistMax", 45.0f);
					a_actor->SetGraphVariableFloat("CPR_CirclingAngleMin", 30.0f);
					a_actor->SetGraphVariableFloat("CPR_CirclingAngleMax", 500.0f);
				}

				if ((a_actor->GetGraphVariableBool("CPR_EnableAdvanceRadius", CPR_EnableAdvanceRadius) && CPR_EnableAdvanceRadius)) {
					a_actor->SetGraphVariableFloat("CPR_InnerRadiusMax", AV_Mod(a_actor, confidence, 256.0f, -10.0f) + AV_Mod(a_actor, aggression, 0.0f, -5.0f));
					a_actor->SetGraphVariableFloat("CPR_OuterRadiusMin", AV_Mod(a_actor, confidence, 256.0f, -10.0f) + AV_Mod(a_actor, aggression, 0.0f, -5.0f));
					a_actor->SetGraphVariableFloat("CPR_OuterRadiusMid", AV_Mod(a_actor, confidence, 512.0f, -50.0f) + AV_Mod(a_actor, aggression, 0.0f, -5.0f));
					a_actor->SetGraphVariableFloat("CPR_OuterRadiusMax", AV_Mod(a_actor, confidence, 1024.0f, -100.0f) + AV_Mod(a_actor, aggression, 0.0f, -10.0f));
				}

				if ((a_actor->GetGraphVariableBool("CPR_EnableBackoff", CPR_EnableBackoff) && CPR_EnableBackoff)) {
					a_actor->SetGraphVariableFloat("CPR_BackoffMinDistMult", 0.75f);
					a_actor->SetGraphVariableFloat("CPR_BackoffChance", 0.25f);
					
				}

				if ((a_actor->GetGraphVariableBool("CPR_EnableFallback", CPR_EnableFallback) && CPR_EnableFallback)) {
					a_actor->SetGraphVariableFloat("CPR_FallbackDistMin", 96.0f);
					a_actor->SetGraphVariableFloat("CPR_FallbackDistMax", 256.0f);
					a_actor->SetGraphVariableFloat("CPR_FallbackWaitTimeMin", 0.75f);
					a_actor->SetGraphVariableFloat("CPR_FallbackWaitTimeMax", 1.5f);
					
				}
			}
		}
	}
}

// if (IsMoving(a_actor)) {
// 	a_actor->NotifyAnimationGraph("LevitationToggleMoving");
// }

// auto isLevitating = false;
// if ((a_actor->GetGraphVariableBool("isLevitating", isLevitating) && isLevitating)) {
// }
