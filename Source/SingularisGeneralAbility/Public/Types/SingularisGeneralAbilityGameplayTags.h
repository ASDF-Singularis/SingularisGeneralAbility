#pragma once

#include <CoreMinimal.h>
#include <NativeGameplayTags.h>

/**
 * Singularis General Ability 插件使用的原生 GameplayTag 声明。
 *
 * 标签层级：
 *   Singularis.General
 *   Singularis.General.Ability
 *   Singularis.General.Ability.Trigger
 *   Singularis.General.Ability.Trigger.Default
 *   Singularis.General.Ability.Identity
 *   Singularis.General.Ability.Identity.Default
 *   Singularis.General.Ability.State
 *   Singularis.General.Ability.State.Default
 */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Trigger);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Trigger_Default);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Identity);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_Identity_Default);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_State);
UE_DECLARE_GAMEPLAY_TAG_EXTERN(SingularisGeneral_Ability_State_Default);
