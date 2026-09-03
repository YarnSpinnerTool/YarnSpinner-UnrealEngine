// Stubs for parsing Unreal Engine headers with libclang without the engine
// source on the include path. Force-included via -include.

#ifndef GEN_DOCS_UNREAL_STUBS_H
#define GEN_DOCS_UNREAL_STUBS_H

// Module export macros: treat as empty so `class FOO_API NAME` parses as
// `class NAME`.
#define YARNSPINNER_API
#define YARNSPINNEREDITOR_API

// UE reflection markup: discovered separately by re-tokenising the source
// before each declaration. Define them to nothing here so clang accepts them.
#define UCLASS(...)
#define USTRUCT(...)
#define UENUM(...)
#define UINTERFACE(...)
#define UDELEGATE(...)
#define UFUNCTION(...)
#define UPROPERTY(...)
#define UPARAM(...)
#define UMETA(...)

#define GENERATED_BODY()
#define GENERATED_USTRUCT_BODY()
#define GENERATED_UCLASS_BODY()
#define GENERATED_IINTERFACE_BODY()
#define GENERATED_UINTERFACE_BODY()

// Dynamic / static delegate declarations create types that get used as
// UPROPERTY field types. Stub them as forward-declared structs so the field
// declarations parse.
#define DECLARE_DELEGATE(NAME) struct NAME;
#define DECLARE_DELEGATE_OneParam(NAME, ...) struct NAME;
#define DECLARE_DELEGATE_TwoParams(NAME, ...) struct NAME;
#define DECLARE_DELEGATE_ThreeParams(NAME, ...) struct NAME;
#define DECLARE_DELEGATE_RetVal(NAME, ...) struct NAME;
#define DECLARE_DELEGATE_RetVal_OneParam(NAME, ...) struct NAME;
#define DECLARE_DELEGATE_RetVal_TwoParams(NAME, ...) struct NAME;
#define DECLARE_DELEGATE_RetVal_ThreeParams(NAME, ...) struct NAME;

#define DECLARE_MULTICAST_DELEGATE(NAME) struct NAME;
#define DECLARE_MULTICAST_DELEGATE_OneParam(NAME, ...) struct NAME;
#define DECLARE_MULTICAST_DELEGATE_TwoParams(NAME, ...) struct NAME;
#define DECLARE_MULTICAST_DELEGATE_ThreeParams(NAME, ...) struct NAME;

#define DECLARE_DYNAMIC_DELEGATE(NAME) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_OneParam(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_TwoParams(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_ThreeParams(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_RetVal(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_RetVal_OneParam(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_RetVal_TwoParams(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_DELEGATE_RetVal_ThreeParams(NAME, ...) struct NAME;

#define DECLARE_DYNAMIC_MULTICAST_DELEGATE(NAME) struct NAME;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(NAME, ...) struct NAME;
#define DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(NAME, ...) struct NAME;

// Build-config gates we want to elaborate as if the feature is on.
#define WITH_EDITOR 1
#define WITH_EDITORONLY_DATA 1
#define YARNSPINNER_WITH_ENHANCED_INPUT 1

// Engine version macros sometimes used in conditional compilation.
#define ENGINE_MAJOR_VERSION 5
#define ENGINE_MINOR_VERSION 4

// Forward declarations of common engine base classes so `class Foo : public
// UActorComponent` produces a CXX_BASE_SPECIFIER cursor in the AST. Without
// these, clang silently drops unresolved base specifiers.
class UObject {};
class UInterface : public UObject {};
class UActorComponent : public UObject {};
class USceneComponent : public UActorComponent {};
class UPrimitiveComponent : public USceneComponent {};
class UCapsuleComponent : public UPrimitiveComponent {};
class USphereComponent : public UPrimitiveComponent {};
class USkeletalMeshComponent : public UPrimitiveComponent {};
class UStaticMeshComponent : public UPrimitiveComponent {};
class UWidgetComponent : public UPrimitiveComponent {};
class UCameraComponent : public USceneComponent {};
class UTextRenderComponent : public UPrimitiveComponent {};
class AActor : public UObject {};
class APawn : public AActor {};
class APlayerController : public AActor {};
class APlayerCameraManager : public AActor {};
class AGameModeBase : public AActor {};
class AGameStateBase : public AActor {};
class UBlueprintFunctionLibrary : public UObject {};
class UDataAsset : public UObject {};
class UFactory : public UObject {};
class UAssetTypeActions_Base {};
class UWidget : public UObject {};
class UUserWidget : public UWidget {};
class UPanelWidget : public UWidget {};
class UTextBlock : public UWidget {};
class UButton : public UWidget {};
class UInputComponent : public UActorComponent {};
class UInputAction : public UObject {};
class UInputMappingContext : public UObject {};
struct FInputActionValue {};
class UWorld : public UObject {};
struct FFrame {};
struct FHitResult {};
struct FTimerHandle {};

#endif // GEN_DOCS_UNREAL_STUBS_H
