#include "FunctionGameWidget.h"
#include "FunctionGameMode.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/EditableTextBox.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"

namespace
{
    UTextBlock* MakeText(UWidgetTree* Tree,const FString& Value,int32 Size=20)
    {
        UTextBlock* Text=Tree->ConstructWidget<UTextBlock>();
        Text->SetText(FText::FromString(Value));
        FSlateFontInfo Font=Text->GetFont();
        Font.Size=Size;
        Text->SetFont(Font);
        Text->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f,0.96f,1.0f)));
        return Text;
    }

    UButton* MakeButton(UWidgetTree* Tree,const FString& Label)
    {
        UButton* Button=Tree->ConstructWidget<UButton>();
        Button->SetBackgroundColor(FLinearColor(0.025f,0.18f,0.24f,1.0f));
        UTextBlock* Text=MakeText(Tree,Label,20);
        Text->SetJustification(ETextJustify::Center);
        Button->AddChild(Text);
        return Button;
    }

    void StyleInput(UEditableTextBox* Box)
    {
        FEditableTextBoxStyle Style=Box->GetWidgetStyle();
        Style.TextStyle.Font.Size=20;
        const FSlateColor Ink(FLinearColor(0.025f,0.04f,0.06f));
        Style.TextStyle.SetColorAndOpacity(Ink);
        Style.SetForegroundColor(Ink);
        Style.SetFocusedForegroundColor(Ink);
        Box->SetWidgetStyle(Style);
    }

    void Fill(UCanvasPanel* Parent,UWidget* Child)
    {
        UCanvasPanelSlot* Slot=Parent->AddChildToCanvas(Child);
        Slot->SetAnchors(FAnchors(0,0,1,1));
        Slot->SetOffsets(FMargin(0));
    }

    void Center(UCanvasPanel* Parent,UWidget* Child,const FVector2D& Size)
    {
        UCanvasPanelSlot* Slot=Parent->AddChildToCanvas(Child);
        Slot->SetAnchors(FAnchors(0.5f,0.5f));
        Slot->SetAlignment(FVector2D(0.5,0.5));
        Slot->SetPosition(FVector2D::ZeroVector);
        Slot->SetSize(Size);
    }

    UBorder* Backdrop(UWidgetTree* Tree,UCanvasPanel* Panel)
    {
        UBorder* Border=Tree->ConstructWidget<UBorder>();
        Border->SetBrushColor(FLinearColor(0.015f,0.025f,0.045f,0.94f));
        Fill(Panel,Border);
        return Border;
    }

    void AddButton(UVerticalBox* Box,UButton* Button)
    {
        UVerticalBoxSlot* Slot=Box->AddChildToVerticalBox(Button);
        Slot->SetPadding(FMargin(0,6));
    }
}

void UFunctionGameWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    BuildInterface();
}

void UFunctionGameWidget::SetGame(AFunctionGameMode* InGame)
{
    Game=InGame;
    Revalidate();
    Refresh();
}

void UFunctionGameWidget::BuildInterface()
{
    UCanvasPanel* Root=WidgetTree->ConstructWidget<UCanvasPanel>();
    WidgetTree->RootWidget=Root;

    CombatPanel=WidgetTree->ConstructWidget<UCanvasPanel>();
    Fill(Root,CombatPanel);
    HudText=MakeText(WidgetTree,TEXT(""),22);
    UCanvasPanelSlot* HudSlot=CombatPanel->AddChildToCanvas(HudText);
    HudSlot->SetAnchors(FAnchors(0,0,1,0));
    HudSlot->SetOffsets(FMargin(24,18,24,36));

    UHorizontalBox* Controls=WidgetTree->ConstructWidget<UHorizontalBox>();
    UCanvasPanelSlot* ControlSlot=CombatPanel->AddChildToCanvas(Controls);
    ControlSlot->SetAnchors(FAnchors(0,1,1,1));
    ControlSlot->SetOffsets(FMargin(24,-82,24,48));
    UTextBlock* Prefix=MakeText(WidgetTree,TEXT("y = "),23);
    Controls->AddChildToHorizontalBox(Prefix)->SetPadding(FMargin(4,8));
    ExpressionBox=WidgetTree->ConstructWidget<UEditableTextBox>();
    StyleInput(ExpressionBox);
    ExpressionBox->SetText(FText::FromString(TEXT("x")));
    ExpressionBox->SetHintText(FText::FromString(TEXT("输入函数，例如 2x+1")));
    UHorizontalBoxSlot* ExprSlot=Controls->AddChildToHorizontalBox(ExpressionBox);
    ExprSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    ExprSlot->SetPadding(FMargin(3));
    Controls->AddChildToHorizontalBox(MakeText(WidgetTree,TEXT("x0"),18))->SetPadding(FMargin(8,9,3,0));
    StartBox=WidgetTree->ConstructWidget<UEditableTextBox>();
    StyleInput(StartBox);
    StartBox->SetHintText(FText::FromString(TEXT("自动")));
    StartBox->SetMinDesiredWidth(90);
    Controls->AddChildToHorizontalBox(StartBox)->SetPadding(FMargin(3));
    FireButton=MakeButton(WidgetTree,TEXT("发射"));
    Controls->AddChildToHorizontalBox(FireButton)->SetPadding(FMargin(6,2));
    UButton* HelpButton=MakeButton(WidgetTree,TEXT("函数帮助"));
    Controls->AddChildToHorizontalBox(HelpButton)->SetPadding(FMargin(6,2));
    StatusText=MakeText(WidgetTree,TEXT(""),15);
    UCanvasPanelSlot* StatusSlot=CombatPanel->AddChildToCanvas(StatusText);
    StatusSlot->SetAnchors(FAnchors(0,1,1,1));
    StatusSlot->SetOffsets(FMargin(26,-30,24,24));

    MenuPanel=WidgetTree->ConstructWidget<UCanvasPanel>();
    Fill(Root,MenuPanel);
    Backdrop(WidgetTree,MenuPanel);
    UVerticalBox* Menu=WidgetTree->ConstructWidget<UVerticalBox>();
    Center(MenuPanel,Menu,FVector2D(460,280));
    Menu->AddChildToVerticalBox(MakeText(WidgetTree,TEXT("函数攻击"),38))->SetPadding(FMargin(0,0,0,16));
    Menu->AddChildToVerticalBox(MakeText(WidgetTree,TEXT("选择难度，输入函数，守住左侧防线。"),18))->SetPadding(FMargin(0,0,0,12));
    UButton* Easy=MakeButton(WidgetTree,TEXT("简单模式 · 撞障碍物不扣次数"));
    UButton* Hard=MakeButton(WidgetTree,TEXT("困难模式 · 三次撞击后失败"));
    AddButton(Menu,Easy);
    AddButton(Menu,Hard);

    ResultPanel=WidgetTree->ConstructWidget<UCanvasPanel>();
    Fill(Root,ResultPanel);
    Backdrop(WidgetTree,ResultPanel);
    UVerticalBox* Result=WidgetTree->ConstructWidget<UVerticalBox>();
    Center(ResultPanel,Result,FVector2D(480,300));
    ResultText=MakeText(WidgetTree,TEXT(""),24);
    ResultText->SetAutoWrapText(true);
    Result->AddChildToVerticalBox(ResultText)->SetPadding(FMargin(0,0,0,18));
    UButton* Retry=MakeButton(WidgetTree,TEXT("重新挑战"));
    NextButton=MakeButton(WidgetTree,TEXT("进入下一关"));
    UButton* Back=MakeButton(WidgetTree,TEXT("返回难度选择"));
    AddButton(Result,Retry); AddButton(Result,NextButton); AddButton(Result,Back);

    HelpPanel=WidgetTree->ConstructWidget<UCanvasPanel>();
    Fill(Root,HelpPanel);
    Backdrop(WidgetTree,HelpPanel);
    UVerticalBox* Help=WidgetTree->ConstructWidget<UVerticalBox>();
    UCanvasPanelSlot* HelpSlot=HelpPanel->AddChildToCanvas(Help);
    HelpSlot->SetAnchors(FAnchors(0.12f,0.08f,0.88f,0.92f));
    HelpSlot->SetOffsets(FMargin(0));
    Help->AddChildToVerticalBox(MakeText(WidgetTree,TEXT("函数帮助"),30))->SetPadding(FMargin(0,0,0,12));
    UScrollBox* Scroll=WidgetTree->ConstructWidget<UScrollBox>();
    Help->AddChildToVerticalBox(Scroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
    const FString Guide=
        TEXT("只输入 y= 后面的表达式；唯一变量是 x。支持空格和英文大小写。\n\n")
        TEXT("运算：+  -  *  /  ^，括号；2x、2(x+1)、(x+1)(x-1)、2sin(x) 可省略乘号。")
        TEXT("幂右结合，-x^2 等于 -(x^2)，2^-2 有效。常量：pi、π、e；支持科学计数法。\n\n")
        TEXT("根与数值：sqrt(a) 要求 a≥0；cbrt(a) 接受任意实数；root(a,n) 的 n 必须是非零整数，")
        TEXT("负数只能开奇次方根，负次数不能用于零。例如 root(-8,3)。\n")
        TEXT("abs(a)、sign(a) 各 1 个参数；min(a,b)、max(a,b) 各 2 个；")
        TEXT("clamp(a,下限,上限) 共 3 个，要求下限≤上限。例如 max(x,0)。\n\n")
        TEXT("指数与对数：exp(x)、ln(x)、log(x)、log10(x)、lg(x)、log2(x)、log(b,x)。")
        TEXT("除 log(b,x) 外各 1 个参数；单参数 log 是自然对数。对数真数必须大于零，")
        TEXT("双参数 log 的底数大于零且不等于一。例如 log(2,x)。\n\n")
        TEXT("三角（弧度）：sin、cos、tan、cot、sec、csc；反三角：asin、acos、atan 和 arcsin、arccos、arctan。")
        TEXT("它们各有 1 个参数；tan、sec 要求 cos 不为零，cot、csc 要求 sin 不为零；")
        TEXT("asin、acos 的参数须在 [-1,1]。例：sin(pi/2)、asin(0.5)。\n")
        TEXT("双曲：sinh、cosh、tanh、asinh、acosh、atanh，各 1 个参数。")
        TEXT("acosh 要求参数≥1，atanh 要求参数在 (-1,1)。例：tanh(x)。\n\n")
        TEXT("离散与分段：floor、ceil、round、trunc；if(条件,真值,假值) 仅计算选中分支。")
        TEXT("取整函数各 1 个参数；if 有 3 个参数，条件支持 < <= > >= == !=。")
        TEXT("例：floor(x)、if(x<0,-x,x)。\n\n")
        TEXT("轨迹从方块出发并向右前进：纵向显示 f(x0+s)-f(x0)。")
        TEXT("因此 x 与 x-2 的轨迹相同。x=0 无定义时会搜索可用 x0；也可在输入框旁手动指定。")
        TEXT("到达定义域中断、跳跃或精度上限时停止，不跨越断点。\n\n")
        TEXT("例子：x、2x+1、x^2、1/x、ln(x)、log(2,x)、3sin(5x)、abs(x-3)。")
        TEXT("除零、0^0、超出定义域、未知名称和过长表达式不能发射；负数的非整数幂可改用 root。")
        TEXT("连续区间过短或采样精度不足时可调整函数或 x0 后重试。");
    UTextBlock* HelpText=MakeText(WidgetTree,Guide,18);
    HelpText->SetAutoWrapText(true);
    Scroll->AddChild(HelpText);
    UButton* Close=MakeButton(WidgetTree,TEXT("关闭帮助"));
    AddButton(Help,Close);

    ExpressionBox->OnTextChanged.AddDynamic(this,&UFunctionGameWidget::OnExpressionChanged);
    StartBox->OnTextChanged.AddDynamic(this,&UFunctionGameWidget::OnStartChanged);
    FireButton->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnFire);
    Easy->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnEasy);
    Hard->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnHard);
    HelpButton->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnHelp);
    Close->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnCloseHelp);
    Retry->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnRetry);
    NextButton->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnNext);
    Back->OnClicked.AddDynamic(this,&UFunctionGameWidget::OnMenu);
}

void UFunctionGameWidget::Refresh()
{
    if (!Game || !MenuPanel) return;
    const bool bPlaying=Game->IsPlaying();
    const bool bFinished=Game->IsFinished();
    MenuPanel->SetVisibility(!bPlaying && !bFinished ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    CombatPanel->SetVisibility(bPlaying ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    ResultPanel->SetVisibility(bFinished ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    HelpPanel->SetVisibility(Game->IsHelpOpen() ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    HudText->SetText(FText::FromString(Game->GetHudText()));
    StatusText->SetText(FText::FromString(Game->GetExpressionStatus()));
    ResultText->SetText(FText::FromString(Game->GetResultText()));
    FireButton->SetIsEnabled(Game->CanFire());
    NextButton->SetIsEnabled(Game->IsWon());
}

void UFunctionGameWidget::Revalidate()
{
    if (Game && ExpressionBox && StartBox) Game->UpdateExpression(ExpressionBox->GetText().ToString(),StartBox->GetText().ToString());
}
void UFunctionGameWidget::OnExpressionChanged(const FText&) { Revalidate(); }
void UFunctionGameWidget::OnStartChanged(const FText&) { Revalidate(); }
void UFunctionGameWidget::OnFire() { if(Game) Game->Fire(); }
void UFunctionGameWidget::OnEasy() { if(Game) Game->StartLevel(false,1); }
void UFunctionGameWidget::OnHard() { if(Game) Game->StartLevel(true,1); }
void UFunctionGameWidget::OnHelp() { if(Game) Game->SetHelpOpen(true); }
void UFunctionGameWidget::OnCloseHelp() { if(Game) Game->SetHelpOpen(false); }
void UFunctionGameWidget::OnRetry() { if(Game) Game->StartLevel(Game->IsHard(),Game->GetLevel()); }
void UFunctionGameWidget::OnNext() { if(Game && Game->IsWon()) Game->StartLevel(Game->IsHard(),Game->GetLevel()+1); }
void UFunctionGameWidget::OnMenu() { if(Game) Game->ReturnToMenu(); }
