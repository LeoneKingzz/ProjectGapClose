#include "hook.h"
#include "ClibUtil/editorID.hpp"

namespace hooks
{
	bool bShouldBleedOut_Serana = false;
	bool bShouldBleedOut_Valerica = false;
	bool bElderScrollEquipped = false;

	OnMeleeHitHook& OnMeleeHitHook::GetSingleton() noexcept
	{
		static OnMeleeHitHook instance;
		return instance;
	}

	void OnMeleeHitHook::InstallHook()
	{
		// Taken from: https://github.com/doodlum/MaxsuWeaponSwingParry-ng/blob/main/src/WeaponParry_Hooks.h
		// (with permission from doodlum and Maxsu)
		SKSE::AllocTrampoline(1 << 4);

		REL::Relocation<std::uintptr_t> OnMeleeHitBase{ REL::RelocationID(37650, 38603) };
		auto&                           trampoline = SKSE::GetTrampoline();
		_OnMeleeHit = trampoline.write_call<5>(OnMeleeHitBase.address() + REL::Relocate(0x38B, 0x45A), OnMeleeHit);
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

	void OnMeleeHitHook::UnequipAll(RE::Actor *a_actor){

		uniqueLocker lock(mtx_Inventory);
		auto itt = _Inventory.find(a_actor);
		if (itt == _Inventory.end()) {
			std::vector<RE::TESBoundObject *> Hen; 
			_Inventory.insert({ a_actor, Hen });
		}

		for (auto it = _Inventory.begin(); it != _Inventory.end(); ++it){
			if (it->first == a_actor) {
				auto inv = a_actor->GetInventory();
				for (auto& [item, data] : inv) {
					const auto& [count, entry] = data;
					if (count > 0 && entry->IsWorn()) {
						RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, item);
						it->second.push_back(item);
					}
				}
				break;
			}
			continue;
		}
	}

	void OnMeleeHitHook::Unequip_DescendMode(RE::Actor* a_actor, RE::SpellItem* a_spell)
	{
		uniqueLocker lock(mtx_SpellList);
		auto itt = _SpellList.find(a_actor);
		if (itt == _SpellList.end()) {
			std::vector<RE::SpellItem*> Hen;
			_SpellList.insert({ a_actor, Hen });
		}

		for (auto it = _SpellList.begin(); it != _SpellList.end(); ++it) {
			if (it->first == a_actor) {
				if (std::find(it->second.begin(), it->second.end(), a_spell) != it->second.end()){
					RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, a_spell);
					//a_actor->RemoveSpell(a_spell);
				}else{
					RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, a_spell);
					//a_actor->RemoveSpell(a_spell);
					it->second.push_back(a_spell);
				}
				break;
			}
			continue;
		}
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

	bool OnMeleeHitHook::Can_Transform(RE::Actor* a_actor)
	{
		auto tolerant_teammates = true;
		auto adequate_threat = false;
		float MyTeam_total_threat = 0.0f;
		float EnemyTeam_total_threat = 0.0f;
		auto combatGroup = a_actor->GetCombatGroup();
		if (combatGroup) {
			for (auto it = combatGroup->members.begin(); it != combatGroup->members.end(); ++it) {
				if (it->memberHandle && it->memberHandle.get().get()) {
					auto Teammate = it->memberHandle.get().get();
					if (Teammate->IsGuard() || Teammate->IsInFaction(RE::TESForm::LookupByEditorID<RE::TESFaction>("DLC1DawnguardExteriorGuardFaction")) 
					|| Teammate->IsInFaction(RE::TESForm::LookupByEditorID<RE::TESFaction>("DLC1DawnguardFaction")) 
					|| Teammate->IsInFaction(RE::TESForm::LookupByEditorID<RE::TESFaction>("VigilantOfStendarrFaction"))) {
						tolerant_teammates = false;
						break;
					}
					
				}
				continue;
			}
			for (auto it = combatGroup->members.begin(); it != combatGroup->members.end(); ++it){
				if (it->memberHandle && it->memberHandle.get().get()){
					//auto Teammate = it->memberHandle.get().get();
					MyTeam_total_threat += it->threatValue;
				}
				continue;
			}
		}
		auto CTarget = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
		if (CTarget){
			auto EnemyGroup = CTarget->GetCombatGroup();
			if (EnemyGroup) {
				for (auto it = EnemyGroup->members.begin(); it != EnemyGroup->members.end(); ++it) {
					if (it->memberHandle && it->memberHandle.get().get()) {
						//auto Teammate = it->memberHandle.get().get();
						EnemyTeam_total_threat += it->threatValue;
					}
					continue;
				}
			}
		}

		if (MyTeam_total_threat > 0 && EnemyTeam_total_threat > 0) {
			logger::info("Name {} TeamThreatLVL {}"sv, CTarget->GetName(), (MyTeam_total_threat / EnemyTeam_total_threat));
			if ((MyTeam_total_threat / EnemyTeam_total_threat) <= 0.625f) {
				adequate_threat = true;
			}
		}

		return tolerant_teammates && adequate_threat;
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

	void OnMeleeHitHook::Store_CStyleSettings(RE::Actor* a_actor)
	{
		uniqueLocker lock(mtx_CStyledefault);
		auto itt = _CStyledefault.find(a_actor);
		if (itt == _CStyledefault.end()) {
			std::vector<RE::TESCombatStyle*> Hen;
			_CStyledefault.insert({ a_actor, Hen });
		}

		for (auto it = _CStyledefault.begin(); it != _CStyledefault.end(); ++it) {
			if (it->first == a_actor) {
				if (a_actor->GetActorRuntimeData().combatController) {
					RE::TESCombatStyle* style = a_actor->GetActorRuntimeData().combatController->combatStyle;
					if (style) {
						it->second.push_back(style);
						a_actor->GetActorRuntimeData().combatController->combatStyle = nullptr;
					}
				}
				break;
			}
			continue;
		}
	}

	void OnMeleeHitHook::Restore_CStyleSettings(RE::Actor* a_actor)
	{
		uniqueLocker lock(mtx_CStyledefault);
		auto itt = _CStyledefault.find(a_actor);
		if (itt == _CStyledefault.end()) {
			return;
		}

		for (auto it = _CStyledefault.begin(); it != _CStyledefault.end(); ++it) {
			if (it->first == a_actor) {
				if (a_actor->GetActorRuntimeData().combatController) {
					a_actor->GetActorRuntimeData().combatController->combatStyle = it->second.front();
				}
				_CStyledefault.erase(it);
				break;
			}
			continue;
		}
	}

	void OnMeleeHitHook::Re_EquipAll_LevitateMode(RE::Actor* a_actor)
	{
		uniqueLocker lock(mtx_SpellList);
		auto itt = _SpellList.find(a_actor);
		if (itt == _SpellList.end()) {
			return;
		}

		for (auto it = _SpellList.begin(); it != _SpellList.end(); ++it) {
			if (it->first == a_actor) {
				for (auto spell : it->second) {
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, spell);
				}
				_SpellList.erase(it);
				break;
			}
			continue;
		}
	}

	void OnMeleeHitHook::Re_EquipAll(RE::Actor *a_actor){

		uniqueLocker lock(mtx_Inventory);
		for (auto it = _Inventory.begin(); it != _Inventory.end(); ++it) {
			if (it->first == a_actor){
				for (auto item : it->second) {
					RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, item);
				}
				_Inventory.erase(it);
				break;
			}
			continue;
		}
	}


	void OnMeleeHitHook::EquipfromInvent(RE::Actor* a_actor, RE::FormID a_formID)
	{
		auto inv = a_actor->GetInventory();
		for (auto& [item, data] : inv) {
			const auto& [count, entry] = data;
			if (count > 0 && entry->object->formID == a_formID) {
				RE::ActorEquipManager::GetSingleton()->EquipObject(a_actor, entry->object);
				break;
			}
			continue;
		}
	}

	void OnMeleeHitHook::VLDrain(RE::Actor* a_actor, bool remove){

		if(remove){
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaConjureGargoyle"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaDrain_Raze"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaDrain_BloodStorm"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSerana_Gutwrench"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSerana_VampiricGrip"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_VampireLord_Spell_Hand_Maelstrom"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaUnarmedDamage01"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaUnarmedDamage02"));
			a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaUnarmedDamage03"));

		}else{
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaConjureGargoyle"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaDrain_Raze"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaDrain_BloodStorm"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSerana_Gutwrench"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSerana_VampiricGrip"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_VampireLord_Spell_Hand_Maelstrom"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaUnarmedDamage01"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaUnarmedDamage02"));
			a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLSeranaUnarmedDamage03"));
		}
	}

	std::pair<bool, RE::SpellItem*> OnMeleeHitHook::GetAttackSpell(RE::Actor* actor, bool lefthand)
	{
		bool result = false;
		RE::SpellItem* a_spell = nullptr;
		auto  limbospell = actor->GetActorRuntimeData().currentProcess;

		if (limbospell) {
			if (lefthand) {
				auto eSpell = limbospell->GetEquippedLeftHand();
				if (eSpell && eSpell->Is(RE::FormType::Spell) && !eSpell->As<RE::SpellItem>()->HasKeyword(RE::TESForm::LookupByEditorID<RE::BGSKeyword>("VLS_AbSpells_Key"))) {
					result = true;
					a_spell = eSpell->As<RE::SpellItem>();
				}
			} else {
				auto eSpell = limbospell->GetEquippedRightHand();
				if (eSpell && eSpell->Is(RE::FormType::Spell) && !eSpell->As<RE::SpellItem>()->HasKeyword(RE::TESForm::LookupByEditorID<RE::BGSKeyword>("VLS_AbSpells_Key"))) {
					result = true;
					a_spell = eSpell->As<RE::SpellItem>();
				}
			}
		}
		return { result, a_spell };
	}

	bool OnMeleeHitHook::VLS_SendVampireLordTransformation(STATIC_ARGS, RE::Actor* a_actor)
	{
		const auto race = a_actor->GetRace();
		const auto raceEDID = race->formEditorID;
		if ((raceEDID == "DLC1VampireBeastRace") || !Can_Transform(a_actor)) {
			return false;
		}
		a_actor->SetGraphVariableBool("bIsDodging", true);
		logger::info("Began Transformation");
		//GetSingleton().Store_CStyleSettings(a_actor);
		auto data = RE::TESDataHandler::GetSingleton();
		util::playSound(a_actor, (data->LookupForm<RE::BGSSoundDescriptorForm>(0x10F564, "Skyrim.esm")));
		//a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_MortalForm_ability"));
		const auto FXchange = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaChangeFX");
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		caster->CastSpellImmediate(FXchange, true, a_actor, 1, false, 0.0, a_actor);
		InterruptAttack(a_actor);
		a_actor->NotifyAnimationGraph("IdleVampireTransformation");
		util::playSound(a_actor, (data->LookupForm<RE::BGSSoundDescriptorForm>(0x5050, "Dawnguard.esm")));
		GetSingleton().UnequipAll(a_actor);
		Set_iFrames(a_actor);
		return true;
	}

	void OnMeleeHitHook::VLS_CompleteTransformation(RE::Actor* a_actor){
		logger::info("completing Transformation");
		const auto FXchange = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaChangeFX");
		dispelEffect(FXchange, a_actor);
		auto data = RE::TESDataHandler::GetSingleton();
		const auto FXExpl = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaTransformToVLExplosionSPELL");
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		logger::info("Vampire lord form succesful");
		a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_VampireLadyForm_ability"));
		caster->CastSpellImmediate(FXExpl, true, a_actor, 1, false, 0.0, a_actor);
		util::playSound(a_actor, (data->LookupForm<RE::BGSSoundDescriptorForm>(0x5052, "Dawnguard.esm")));
		auto vamp_armour = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("VLSeranaDLC1ClothesVampireLordRoyalArmor");
		VLDrain(a_actor);
		dispelEffect(FXExpl, a_actor);
		a_actor->AddWornItem(vamp_armour, 1, false, 0, 0);
		EquipfromInvent(a_actor, vamp_armour->formID);
		a_actor->SetGraphVariableBool("bIsDodging", false);
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

	// float OnMeleeHitHook::GetRelativeHeight(STATIC_ARGS, RE::Actor* a_actor)
	// {
	// 	auto CT = a_actor->GetActorRuntimeData().currentCombatTarget.get().get();
	// 	float VH = a_actor->GetPositionZ();
	// 	float TH = CT->GetPositionZ();
	// 	float MH = 0.0;
	// 	if (VH > TH){

	// 	}else if (VH < TH){

	// 	}
	// }

	bool OnMeleeHitHook::VLS_RevertVampireLordform(STATIC_ARGS, RE::Actor* a_actor)
	{
		const auto race = a_actor->GetRace();
		const auto raceEDID = race->formEditorID;
		if (!(raceEDID == "DLC1VampireBeastRace")) {
			return false;
		}
		a_actor->SetGraphVariableBool("bIsDodging", true);
		logger::info("Reverting Form");
		auto data = RE::TESDataHandler::GetSingleton();
		util::playSound(a_actor, (data->LookupForm<RE::BGSSoundDescriptorForm>(0x10F564, "Skyrim.esm")));
		const auto FXchange = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaTransformToNormal");
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		caster->CastSpellImmediate(FXchange, true, a_actor, 1, false, 0.0, a_actor);
		a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_VampireLadyForm_ability"));
		a_actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"));
		auto vamp_armour = RE::TESForm::LookupByEditorID<RE::TESObjectARMO>("VLSeranaDLC1ClothesVampireLordRoyalArmor");
		a_actor->RemoveItem(vamp_armour, 1, RE::ITEM_REMOVE_REASON::kRemove, nullptr, nullptr);
		VLDrain(a_actor, true);
		return true;
	}

	void OnMeleeHitHook::VLS_CompleteReversion(RE::Actor* a_actor)
	{
		const auto FXchange = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaTransformToNormal");
		const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
		const auto FXchange2 = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaTransformToNormal2");
		const auto Gargoyle = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaConjureGargoyle");
		dispelEffect(FXchange, a_actor);
		caster->CastSpellImmediate(FXchange2, true, a_actor, 1, false, 0.0, a_actor);
		dispelEffect(Gargoyle, a_actor);
		//a_actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_MortalForm_ability"));
		GetSingleton().Re_EquipAll(a_actor);
		//GetSingleton().Restore_CStyleSettings(a_actor);
		a_actor->SetGraphVariableBool("bIsDodging", false);
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

			if (!(a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))) {
				return RE::BSEventNotifyControl::kContinue;
			}
			const auto race = a_actor->GetRace();
			const auto raceEDID = race->formEditorID;
			if (!(raceEDID == "DLC1VampireBeastRace")) {
				//Not vamp form//
				OnMeleeHitHook::Reset_iFrames(a_actor);
				OnMeleeHitHook::VLS_CompleteReversion(a_actor);

			}else {//vamp form//
				OnMeleeHitHook::Reset_iFrames(a_actor);
				OnMeleeHitHook::VLS_CompleteTransformation(a_actor);
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESEquipEvent* event, RE::BSTEventSource<RE::TESEquipEvent>*){
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!(a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))) {
				return RE::BSEventNotifyControl::kContinue;
			}

			const auto race = a_actor->GetRace();
			const auto raceEDID = race->formEditorID;
			if (raceEDID == "DLC1VampireBeastRace") {
				auto ElderScroll = RE::TESForm::LookupByEditorID<RE::TESAmmo>("DLC1ElderScrollBack");
				auto item = event->originalRefr;
				if (item && item == ElderScroll->formID && event->equipped) {
					RE::ActorEquipManager::GetSingleton()->UnequipObject(a_actor, ElderScroll);
				}
			}
			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const SKSE::ModCallbackEvent* event, RE::BSTEventSource<SKSE::ModCallbackEvent>*)
		{
			auto Ename = event->eventName;

			if (Ename != "UND_DodgeEvent") {
				return RE::BSEventNotifyControl::kContinue;
			}

			logger::info("recieved dodge event"sv);

			auto a_actor = event->sender->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto magicTarget = a_actor->AsMagicTarget();
			const auto magicEffect1 = RE::TESForm::LookupByEditorID("VLS_TransformCooldown_VampAb")->As<RE::EffectSetting>();
			const auto magicEffect2 = RE::TESForm::LookupByEditorID("VLS_VampireLord_Effect_Power_MistForm_Dummy")->As<RE::EffectSetting>();
			const auto magicEffect3 = RE::TESForm::LookupByEditorID("VLSeranaDLC1BatsEffect")->As<RE::EffectSetting>();
			const auto magicEffect4 = RE::TESForm::LookupByEditorID("VLSeranaDLC1BatsEffects2")->As<RE::EffectSetting>();

			if (magicTarget->HasMagicEffect(magicEffect1) || magicTarget->HasMagicEffect(magicEffect2) || magicTarget->HasMagicEffect(magicEffect3) || magicTarget->HasMagicEffect(magicEffect4)) {
				return RE::BSEventNotifyControl::kContinue;
			}

			//a_actor->NotifyAnimationGraph("InterruptCast");
			//a_actor->InterruptCast(false);

			OnMeleeHitHook::BatForm(nullptr, 0, nullptr, a_actor);
			
			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESCombatEvent* event, RE::BSTEventSource<RE::TESCombatEvent>*){
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!(a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto getcombatstate = event->newState.get();

			const auto race = a_actor->GetRace();
			const auto raceEDID = race->formEditorID;
			if (raceEDID == "DLC1VampireBeastRace") {
				if (getcombatstate != RE::ACTOR_COMBAT_STATE::kCombat) {
					
					auto getcombatstate2 = event->newState.get();
					if (getcombatstate2 == RE::ACTOR_COMBAT_STATE::kNone) {
						const auto Revert = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaValericaRevertFormSpell");
						const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
						caster->CastSpellImmediate(Revert, true, a_actor, 1, false, 0.0, a_actor);
					}
				}
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESActorLocationChangeEvent* event, RE::BSTEventSource<RE::TESActorLocationChangeEvent>*)
		{
			auto a_actor = event->actor->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!(a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))) {
				return RE::BSEventNotifyControl::kContinue;
			}

			auto Playerhandle = RE::PlayerCharacter::GetSingleton();

			if (Playerhandle->IsSneaking() || event->newLoc && event->newLoc->HasKeyword(RE::TESForm::LookupByEditorID<RE::BGSKeyword>("LocTypeHabitation"))){
				const auto Revert = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLSeranaValericaRevertFormSpell");
				const auto caster = a_actor->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
				caster->CastSpellImmediate(Revert, true, a_actor, 1, false, 0.0, a_actor);
			}

			return RE::BSEventNotifyControl::kContinue;
		}

		RE::BSEventNotifyControl ProcessEvent(const RE::TESSpellCastEvent* event, RE::BSTEventSource<RE::TESSpellCastEvent>*)
		{
			auto a_actor = event->object->As<RE::Actor>();

			if (!a_actor) {
				return RE::BSEventNotifyControl::kContinue;
			}

			if (!(a_actor->HasKeywordString("VLS_Serana_Key") || a_actor->HasKeywordString("VLS_Valerica_Key"))) {
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
		case "BatSprintOff"_h:
			if (OnMeleeHitHook::getrace_VLserana(actor)) {
				OnMeleeHitHook::GetSingleton().Reset_iFrames(actor);
			}
			break;

		case "BatSprintOn"_h:
		    if (OnMeleeHitHook::getrace_VLserana(actor)){
				OnMeleeHitHook::GetSingleton().Set_iFrames(actor);
			}
			break;

		// case "BeginCastLeft"_h:
		// case "BeginCastRight"_h:
		// 	if (OnMeleeHitHook::getrace_VLserana(actor)) {
		// 		auto isLevitating = false;
		// 		auto bVLS_IsLiftingOff = false;
		// 		if ((actor->GetGraphVariableBool("isLevitating", isLevitating) && !isLevitating) && (actor->GetGraphVariableBool("bVLS_IsLiftingOff", bVLS_IsLiftingOff) && !bVLS_IsLiftingOff)) {
		// 			OnMeleeHitHook::LevitateToggle(nullptr, 0, nullptr, actor);
		// 		}
		// 	}
		// 	break;

		case "LevitateStart"_h:
			if (OnMeleeHitHook::getrace_VLserana(actor)) {
				actor->SetGraphVariableBool("bVLS_IsLiftingOff", false);
				actor->SetGraphVariableBool("bNoStagger", false);
				auto it = OnMeleeHitHook::GetSingleton().GetAttackSpell(actor);
				auto it2 = OnMeleeHitHook::GetSingleton().GetAttackSpell(actor, true);
				if (!(it.first && it2.first)) {
					//OnMeleeHitHook::LevitateToggle(nullptr, 0, nullptr, actor);
					actor->NotifyAnimationGraph("InitiateEnd");
					actor->NotifyAnimationGraph("LandStart");
				}
			}
			break;

		case "LandStart"_h:
			if (OnMeleeHitHook::getrace_VLserana(actor)) {
				actor->SetGraphVariableBool("bVLS_IsLanding", true);
				actor->SetGraphVariableBool("bNoStagger", true);
				if (!actor->HasSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"))) {
					actor->AddSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"));
				}
			}
			break;

		case "GroundStart"_h:
			if (OnMeleeHitHook::getrace_VLserana(actor)) {
				actor->SetGraphVariableBool("bVLS_IsLanding", false);
				actor->SetGraphVariableBool("bNoStagger", false);
				auto it = OnMeleeHitHook::GetSingleton().GetAttackSpell(actor);
				auto it2 = OnMeleeHitHook::GetSingleton().GetAttackSpell(actor, true);
				if (it.first) {
					OnMeleeHitHook::GetSingleton().Unequip_DescendMode(actor, it.second);
				}
				if (it2.first) {
					OnMeleeHitHook::GetSingleton().Unequip_DescendMode(actor, it2.second);
				}
				actor->NotifyAnimationGraph("weaponDraw");
			}
			break;

		case "LiftoffStart"_h:
			if (OnMeleeHitHook::getrace_VLserana(actor)) {
				actor->SetGraphVariableBool("bVLS_IsLiftingOff", true);
				actor->SetGraphVariableBool("bNoStagger", true);
				if (actor->HasSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"))) {
					actor->RemoveSpell(RE::TESForm::LookupByEditorID<RE::SpellItem>("VLS_InhibitMagicks_ability"));
				}
			}
			break;

		case "staggerStart"_h:
		case "MRh_Equipped_Event"_h:
		case "MLh_Equipped_Event"_h:
		case "PitchStop"_h:
		case "InitiateStart"_h:
		case "InitiateStartLeft"_h:
		case "InitiateStartRight"_h:
			if (OnMeleeHitHook::getrace_VLserana(actor)) {
				OnMeleeHitHook::Evaluate_AI(actor);
			}	
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
		vm->RegisterFunction("VLS_SendVampireLordTransformation", "VLS_NativeFunctions", VLS_SendVampireLordTransformation);
		vm->RegisterFunction("VLS_RevertVampireLordform", "VLS_NativeFunctions", VLS_RevertVampireLordform);
		vm->RegisterFunction("ResetAttack", "VLS_NativeFunctions", ResetAttack);
		vm->RegisterFunction("BatForm", "VLS_NativeFunctions", BatForm);
		vm->RegisterFunction("Night_Powers", "VLS_NativeFunctions", Night_Powers);
		vm->RegisterFunction("Mortal_Powers", "VLS_NativeFunctions", Mortal_Powers);
		vm->RegisterFunction("LevitateToggle", "VLS_NativeFunctions", LevitateToggle);
		//vm->RegisterFunction("GetRelativeHeight", "VLS_NativeFunctions", GetRelativeHeight);
		return true;
	}

	void OnMeleeHitHook::OnMeleeHit(RE::Actor* hit_causer, RE::Actor* hit_target, std::int64_t a_int1, bool a_bool, void* a_unkptr){
		if (OnMeleeHitHook::getrace_VLserana(hit_causer)) {
			auto magicTarget = hit_causer->AsMagicTarget();
			const auto magicEffect = RE::TESForm::LookupByEditorID<RE::EffectSetting>("VLS_VampireLord_Effect_Power_SupernaturalReflexes_Cloak");
			if (magicTarget->HasMagicEffect(magicEffect) && OnMeleeHitHook::isPowerAttacking(hit_causer)){
				const auto knockdown = RE::TESForm::LookupByEditorID<RE::MagicItem>("VLS_SRflex_Knockdown_TriggerEffect");
				const auto caster = hit_causer->GetMagicCaster(RE::MagicSystem::CastingSource::kInstant);
				caster->CastSpellImmediate(knockdown, true, hit_target, 1, false, 0.0, hit_causer);
			}
		}
		if (OnMeleeHitHook::getrace_VLserana(hit_target)){
			OnMeleeHitHook::Evaluate_AI(hit_target);
		}
		// Call the normal game's code
		_OnMeleeHit(hit_causer, hit_target, a_int1, a_bool, a_unkptr);
	}
}

namespace FallLongDistance
{
	struct CalcDoDamage
	{
		static float thunk(RE::Actor* a_this, float a_fallDistance, float a_defaultMult)
		{
			const auto fallDamage = func(a_this, a_fallDistance, a_defaultMult);
			if (fallDamage > 0.0f) {
				if (a_this->HasKeywordString("VLS_Serana_Key") || a_this->HasKeywordString("VLS_Valerica_Key")) {
					const auto race = a_this->GetRace();
					const auto raceEDID = race->formEditorID;
					if (raceEDID == "DLC1VampireBeastRace"){
						auto isLevitating = false;
						if (a_this->GetGraphVariableBool("isLevitating", isLevitating) && isLevitating) {
							// a_this->NotifyAnimationGraph("sprintStart");
							// a_this->AsActorState()->actorState1.sprinting = 1;
						}
					}
				}
			}
			return fallDamage;
		}
		static inline REL::Relocation<decltype(thunk)> func;
	};

	void Install()
	{
		REL::Relocation<std::uintptr_t> take_ragdoll_damage{ RELOCATION_ID(36346, 37336), 0x35 };
		stl::write_thunk_call<CalcDoDamage>(take_ragdoll_damage.address());

		REL::Relocation<std::uintptr_t> process_movefinish_event{ RELOCATION_ID(36973, 37998), REL::Relocate(0xAE, 0xAB) };
		stl::write_thunk_call<CalcDoDamage>(process_movefinish_event.address());

		logger::info("Hooked Fall Damage"sv);
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