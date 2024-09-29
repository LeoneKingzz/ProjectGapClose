#include "hook.h"
#include "ClibUtil/editorID.hpp"

namespace hooks
{

	OnMeleeHitHook& OnMeleeHitHook::GetSingleton() noexcept
	{
		static OnMeleeHitHook instance;
		return instance;
	}


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

	void remove_item(RE::TESObjectREFR* a_ref, RE::TESBoundObject* a_item, std::uint32_t a_count, bool a_silent, RE::TESObjectREFR* a_otherContainer)
	{
		using func_t = decltype(&remove_item);
		static REL::Relocation<func_t> func{ RELOCATION_ID(56261, 56647) };
		return func(a_ref, a_item, a_count, a_silent, a_otherContainer);
	}

	

	bool OnMeleeHitHook::getrace_VLserana(RE::Actor* a_actor)
	{
		bool result = false;
		const auto race = a_actor->GetRace();
		const auto raceEDID = race->formEditorID;
		if (raceEDID == "DLC1VampireBeastRace") {
			if (a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key")) {
				result = true;
			}
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
		a_actor->UpdateCombat();
	}

	

	void OnMeleeHitHook::Evaluate_AI(RE::Actor* actor)
	{
		auto isLevitating = false;
		auto bVLS_IsLanding = false;
		if ((actor->GetGraphVariableBool("isLevitating", isLevitating) && isLevitating) && (actor->GetGraphVariableBool("bVLS_IsLanding", bVLS_IsLanding) && !bVLS_IsLanding) && ((actor->AsActorValueOwner()->GetActorValue(RE::ActorValue::kMagicka) <= 30.0f) || (actor->HasSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"))))) {
			if (!IsCasting(actor)){
				actor->InterruptCast(false);
				actor->NotifyAnimationGraph("InterruptCast");
				actor->NotifyAnimationGraph("attackStop");
				actor->NotifyAnimationGraph("attackStopInstant");
				actor->NotifyAnimationGraph("staggerStop");
				actor->AsActorState()->actorState1.knockState = RE::KNOCK_STATE_ENUM::kNormal;
				actor->NotifyAnimationGraph("GetUpEnd");
				actor->NotifyAnimationGraph("InitiateEnd");
				actor->NotifyAnimationGraph("LandStart");
				const auto Evaluate = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_Spell_EvaluteAI_Trigger");
				const auto caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
				caster->CastSpellImmediate(Evaluate, true, actor, 1, false, 0.0, actor);
			}
		}
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

	void OnMeleeHitHook::ResetAttack(STATIC_ARGS, RE::Actor* a_actor)
	{
		auto isLevitating = false;
		if (a_actor->GetGraphVariableBool("isLevitating", isLevitating) && isLevitating){
			if(!a_actor->HasSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"))){
				a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"));
			}
		}else{
			if (a_actor->HasSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"))) {
				a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"));
			}
		}
	}

	void OnMeleeHitHook::LevitateToggle(STATIC_ARGS, RE::Actor* a_actor)
	{
		if (OnMeleeHitHook::getrace_VLserana(a_actor)) {
			auto isLevitating = false;
			auto bVLS_IsLanding = false;
			if ((a_actor->GetGraphVariableBool("isLevitating", isLevitating) && isLevitating) && (a_actor->GetGraphVariableBool("bVLS_IsLanding", bVLS_IsLanding) && !bVLS_IsLanding)) {
				const auto Reset = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_Spell_RestAI_Trigger");
				const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
				caster->CastSpellImmediate(Reset, true, a_actor, 1, false, 0.0, a_actor);
				UpdateCombatTarget(a_actor);
				if (IsMoving(a_actor)) {
					a_actor->NotifyAnimationGraph("LevitationToggleMoving");

				} else {
					a_actor->NotifyAnimationGraph("LevitationToggle");
				}
			}
		}
	}

	void OnMeleeHitHook::BatForm(STATIC_ARGS, RE::Actor* a_actor, bool forward)
	{
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		const auto power_forward = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaDLC1VampireBats2");
		const auto power_skirmish = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaDLC1VampireBats");
		if (forward){
			caster->CastSpellImmediate(power_forward, true, a_actor, 1, false, 0.0, a_actor);
		}else{
			caster->CastSpellImmediate(power_skirmish, true, a_actor, 1, false, 0.0, a_actor);
		}
	}

	void OnMeleeHitHook::Night_Powers(STATIC_ARGS, RE::Actor* a_actor, bool mistform, bool sreflexes, bool tremor)
	{
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		const auto power_mist = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSerana_MistForm");
		const auto power_sreflexes = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_VampireLord_Spell_Power_SupernaturalReflexes");
		const auto power_echo = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_VampireLord_Spell_Power_EchoScream");
		const auto power_tremor = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_VampireLord_Spell_Power_Tremor");
		if (mistform) {
			caster->CastSpellImmediate(power_mist, true, a_actor, 1, false, 0.0, a_actor);
		}else if(sreflexes) {
			caster->CastSpellImmediate(power_sreflexes, true, a_actor, 1, false, 0.0, a_actor);
		
		}else if(tremor){
			caster->CastSpellImmediate(power_tremor, true, a_actor, 1, false, 0.0, a_actor);

		}else{
			caster->CastSpellImmediate(power_echo, true, a_actor, 1, false, 0.0, a_actor);
		}
	}

	void OnMeleeHitHook::Mortal_Powers(STATIC_ARGS, RE::Actor* a_actor, bool transform, bool shadow, bool scream)
	{
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		const auto power_transform = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaAb");
		const auto power_shadow = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_VampiresShadow_Power");
		const auto power_scream = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_VampiresScream_Power");
		const auto power_seduction = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_VampiresSeduction_Power");
		if (transform) {
			caster->CastSpellImmediate(power_transform, true, a_actor, 1, false, 0.0, a_actor);
		} else if (shadow) {
			caster->CastSpellImmediate(power_shadow, true, a_actor, 1, false, 0.0, a_actor);
		} else if (scream) {
			caster->CastSpellImmediate(power_scream, true, a_actor, 1, false, 0.0, a_actor);
		}else{
			auto CT = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
			if (CT){
				caster->CastSpellImmediate(power_seduction, true, CT, 1, false, 0.0, a_actor);
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

			//auto getcombatstate = event->newState.get();

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
					case "VLSeranaValericaLevitateAb"_h:
						break;
					case "VLSeranaValericaDescendAb"_h:
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
		vm->RegisterFunction("ResetAttack", "VLS_NativeFunctions", ResetAttack);
		vm->RegisterFunction("BatForm", "VLS_NativeFunctions", BatForm);
		vm->RegisterFunction("Night_Powers", "VLS_NativeFunctions", Night_Powers);
		vm->RegisterFunction("Mortal_Powers", "VLS_NativeFunctions", Mortal_Powers);
		vm->RegisterFunction("LevitateToggle", "VLS_NativeFunctions", LevitateToggle);
		return true;
	}

}



// a_this->SetGraphVariableBool("bWFT_IsGliding", true);
// actor->NotifyAnimationGraph("JumpStandingStart");
// actor->NotifyAnimationGraph("JumpDirectionalStart");
// a_actor->SetGraphVariableBool("bMLh_Ready", true);
// a_actor->SetGraphVariableBool("bMRh_Ready", true);
// a_actor->SetGraphVariableBool("bMagicDraw", true);
// a_actor->NotifyAnimationGraph("weaponDraw");
// a_actor->SetGraphVariableBool("WeapEquip", true);
// a_actor->NotifyAnimationGraph("tailCombatIdle");
// a_actor->AsActorState()->actorState2.forceSneak = 0;
// a_actor->NotifyAnimationGraph("tailSneakLocomotion");

// auto moving = GetSingleton().IsMoving(a_actor);
// if (moving) {
// 	a_actor->NotifyAnimationGraph("LevitationToggleMoving");
// } else {
// 	a_actor->NotifyAnimationGraph("LevitationToggle");
// }

// if (a_actor->GetActorRuntimeData().combatController) {
// 	RE::TESCombatStyle* style = a_actor->GetActorRuntimeData().combatController->combatStyle;
// 	if (style) {
// 		style->generalData.magicScoreMult = 0.0f;
// 		style->generalData.meleeScoreMult = 10.0f;
// 	}
// }
// auto it = OnMeleeHitHook::GetSingleton().GetAttackSpell(a_actor);
// auto it2 = OnMeleeHitHook::GetSingleton().GetAttackSpell(a_actor, true);
// if (it.first) {
// 	OnMeleeHitHook::GetSingleton().Unequip_DescendMode(a_actor, it.second);
// }
// if (it2.first) {
// 	OnMeleeHitHook::GetSingleton().Unequip_DescendMode(a_actor, it2.second);
// }

// OnMeleeHitHook::GetSingleton().Re_EquipAll_LevitateMode(a_actor);




// void OnMeleeHitHook::update(RE::Actor* a_actor, float a_delta)
// {
// 	if (!_bUpdate) {
// 		return;
// 	}
// 	if (!(a_actor->GetActorRuntimeData().currentProcess && a_actor->GetActorRuntimeData().currentProcess->InHighProcess() && a_actor->Is3DLoaded())){
// 		return;
// 	}
// 	if (!(a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))) {
// 		return;
// 	}
// 	bool bIsDodging = false;
// 	if ((a_actor)->GetGraphVariableBool("bIsDodging", bIsDodging) && !bIsDodging) {
// 		return;
// 	}
// 	uniqueLocker lock(mtx_parryTimer);
// 	auto it = _parryTimer.begin();
// 	if (!it->first) {
// 		it = _parryTimer.erase(it);
// 		return;
// 	}
// 	if (it->second > 0.6f) {
// 		it = _parryTimer.erase(it);
// 		_bUpdate = false;
// 		VLS_CompleteTransformation(a_actor);
// 		return;
// 	}else{
// 		it->second += a_delta;
// 	}
// 	//*static float* g_deltaTime = (float*)RELOCATION_ID(523660, 410199).address();*/          // 2F6B948
// }

// void OnMeleeHitHook::startTiming(RE::Actor* a_actor, float a_time)
// {
// 	uniqueLocker lock(mtx_parryTimer);
// 	auto it = _parryTimer.find(a_actor);
// 	if (it != _parryTimer.end()) {
// 		it->second = 0;
// 	} else {
// 		_parryTimer.insert({ a_actor, a_time });
// 	}

// 	_bUpdate = true;
// }

// void EldenParry::update()
// {
// 	if (!_bUpdate) {
// 		return;
// 	}
// 	uniqueLocker lock(mtx_parryTimer);
// 	auto it = _parryTimer.begin();
// 	if (it == _parryTimer.end()) {
// 		_bUpdate = false;
// 		return;
// 	}
// 	while (it != _parryTimer.end()) {
// 		if (!it->first) {
// 			it = _parryTimer.erase(it);
// 			continue;
// 		}
// 		if (it->second > EldenSettings::fParryWindow_End) {
// 			it = _parryTimer.erase(it);
// 			continue;
// 		}
// 		//*static float* g_deltaTime = (float*)RELOCATION_ID(523660, 410199).address();*/          // 2F6B948
// 		it->second += g_deltaTime;
// 		it++;
// 	}
// }

// void OnMeleeHitHook::finishTiming(RE::Actor* a_actor)
// {
// 	uniqueLocker lock(mtx_parryTimer);
// 	_parryTimer.erase(a_actor);
// }

// if (OnMeleeHitHook::getrace_VLserana(actor)) {
// 	if (!actor->IsAttacking() && !OnMeleeHitHook::IsCasting(actor)) {
// 		actor->NotifyAnimationGraph("InterruptCast");
// 		actor->InterruptCast(false);
// 		const auto reset = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSerana_LevitateAIReset_Spell");
// 		const auto caster = actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
// 		caster->CastSpellImmediate(reset, true, actor, 1, false, 0.0, actor);
// 	}
// }

// case "InitiateStart"_h:
// case "InitiateStartLeft"_h:
// case "InitiateStartRight"_h:
// 	if (OnMeleeHitHook::getrace_VLserana(actor)) {
// 		if (!actor->IsAttacking()) {
// 			OnMeleeHitHook::LevitateToggle(nullptr, 0, nullptr, actor);
// 		}
// 	}

// 	break;

// auto it = OnMeleeHitHook::GetSingleton().GetAttackSpell(actor);
// auto it2 = OnMeleeHitHook::GetSingleton().GetAttackSpell(actor, true);
// if (it.first) {
// 	OnMeleeHitHook::GetSingleton().Unequip_DescendMode(actor, it.second);
// }
// if (it2.first) {
// 	OnMeleeHitHook::GetSingleton().Unequip_DescendMode(actor, it2.second);
// }

// int iState = 0;
// hit_causer->GetGraphVariableInt("iState", iState);
// switch (iState) {
// case 1:
// 	/* code */
// 	break;

// case 5:
// 	/* code */
// 	break;

// default:
// 	break;
// }
// raceFormID == "WerewolfBeastRace" || raceFormID == "DLC2WerebearBeastRace"

// actor->NotifyAnimationGraph("MRh_Equipped_Event");
// actor->NotifyAnimationGraph("MLh_Equipped_Event");
// actor->NotifyAnimationGraph("PitchStop");
// actor->NotifyAnimationGraph("PitchStop");

// auto bVLS_WantstoAttack = false;
// if (actor->GetGraphVariableBool("bVLS_WantstoAttack", bVLS_WantstoAttack) && bVLS_WantstoAttack) {
// 	actor->SetGraphVariableBool("bVLS_WantstoAttack", false);
// }

//OnMeleeHitHook::LevitateToggle(nullptr, 0, nullptr, actor);
// logger::info("Name {} ThreatLVL {}"sv, Teammate->GetName(), it->threatValue);