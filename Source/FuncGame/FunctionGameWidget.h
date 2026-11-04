#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "FunctionGameWidget.generated.h"

class AFunctionGameMode;
class UButton;
class UCanvasPanel;
class UEditableTextBox;
class UTextBlock;
class UVerticalBox;

UCLASS()
class FUNCGAME_API UFunctionGameWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeOnInitialized() override;
    void SetGame(AFunctionGameMode* InGame);
    void Refresh();

private:
    UPROPERTY()
    TObjectPtr<AFunctionGameMode> Game;
    UPROPERTY()
    TObjectPtr<UTextBlock> HudText;
    UPROPERTY()
    TObjectPtr<UTextBlock> StatusText;
    UPROPERTY()
    TObjectPtr<UTextBlock> ResultText;
    UPROPERTY()
    TObjectPtr<UEditableTextBox> ExpressionBox;
    UPROPERTY()
    TObjectPtr<UEditableTextBox> StartBox;
    UPROPERTY()
    TObjectPtr<UButton> FireButton;
    UPROPERTY()
    TObjectPtr<UButton> NextButton;
    UPROPERTY()
    TObjectPtr<UCanvasPanel> MenuPanel;
    UPROPERTY()
    TObjectPtr<UCanvasPanel> HelpPanel;
    UPROPERTY()
    TObjectPtr<UCanvasPanel> ResultPanel;
    UPROPERTY()
    TObjectPtr<UCanvasPanel> CombatPanel;

    void BuildInterface();
    void Revalidate();
    UFUNCTION() void OnExpressionChanged(const FText& Value);
    UFUNCTION() void OnStartChanged(const FText& Value);
    UFUNCTION() void OnFire();
    UFUNCTION() void OnEasy();
    UFUNCTION() void OnHard();
    UFUNCTION() void OnHelp();
    UFUNCTION() void OnCloseHelp();
    UFUNCTION() void OnRetry();
    UFUNCTION() void OnNext();
    UFUNCTION() void OnMenu();
};
