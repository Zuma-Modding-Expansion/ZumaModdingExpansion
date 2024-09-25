#include "Zuma_Prefix.pch"

#include <SexyAppFramework/SexyAppBase.h>
#include <SexyAppFramework/Font.h>
#include <SexyAppFramework/ButtonListener.h>
#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/Widget.h>
#include <SexyAppFramework/ResourceManager.h>
#include <SexyAppFramework/DDImage.h>

#include "CircleShootApp.h"
#include "Res.h"
#include "CircleCommon.h"
#include "CircleButton.h"
#include "StatisticsScreen.h"
#include "SpriteMgr.h"
#include "ProfileMgr.h"

#include <math.h>

using namespace Sexy;
const int yOffset = 19;

StatisticsScreen::StatisticsScreen()
{
	//mBackgroundImage = gSexyAppBase->GetImage("images/statistics_screen", 0);
	mMainMenuButton = MakeButton(0, this, "Main Menu", CircleButton::CB_ClickSound, NULL, 3);
	GetCircleShootApp()->SaveProfile();
}

StatisticsScreen::~StatisticsScreen()
{
	//delete mBackgroundImage;
	delete mMainMenuButton;
}

void StatisticsScreen::Draw(Graphics* g)
{
	Widget::Draw(g);
	//g->DrawImage(mBackgroundImage, 0, 0);

	UserStatistics* stat = &GetCircleShootApp()->mProfile->mUserStats;

	std::string fireBallAmount = "Fired Ball Amount: " + std::to_string(stat->mFiredBallAmount);
	std::string chainAmount = "Chain Amount: " + std::to_string(stat->mChainAmount);
	std::string maxChainAmount = "Max Chain Amount: " + std::to_string(stat->mMaxChain);
	std::string comboAmount = "Combo Amount: " + std::to_string(stat->mComboAmount);
	std::string maxComboAmount = "Max Combo Amount: " + std::to_string(stat->mMaxCombo + 1);
	std::string gapAmount = "Gap Amount: " + std::to_string(stat->mGapAmount);
	std::string coinAmount = "Coin Amount: " + std::to_string(stat->mCoinAmount);
	std::string liveEarned = "Live Earned: " + std::to_string(stat->mLiveEarned);
	std::string liveLost = "Live Lost: " + std::to_string(stat->mLiveLost);
	int fireBallAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(fireBallAmount);
	int chainAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(chainAmount);
	int maxChainAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(maxChainAmount);
	int comboAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(comboAmount);
	int maxComboAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(maxComboAmount);
	int gapAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(gapAmount);
	int coinAmountWidth = Sexy::FONT_BROWNTITLE->StringWidth(coinAmount);
	int liveEarnedWidth = Sexy::FONT_BROWNTITLE->StringWidth(liveEarned);
	int liveLostWidth = Sexy::FONT_BROWNTITLE->StringWidth(liveLost);

	g->SetColor(Sexy::Color(0xFFFFFF));
	g->SetFont(Sexy::FONT_BROWNTITLE);

	g->DrawString(fireBallAmount, (CIRCLE_WINDOW_WIDTH - fireBallAmountWidth) / 2, 128+yOffset);
	g->DrawString(chainAmount, (CIRCLE_WINDOW_WIDTH - chainAmountWidth) / 2, 160+yOffset);
	g->DrawString(maxChainAmount, (CIRCLE_WINDOW_WIDTH - maxChainAmountWidth) / 2, 192 + yOffset);
	g->DrawString(comboAmount, (CIRCLE_WINDOW_WIDTH - comboAmountWidth) / 2, 224 + yOffset);
	g->DrawString(maxComboAmount, (CIRCLE_WINDOW_WIDTH - maxComboAmountWidth) / 2, 256 + yOffset);
	g->DrawString(gapAmount, (CIRCLE_WINDOW_WIDTH - gapAmountWidth) / 2, 288 + yOffset);
	g->DrawString(coinAmount, (CIRCLE_WINDOW_WIDTH - coinAmountWidth) / 2, 320 + yOffset);
	g->DrawString(liveEarned, (CIRCLE_WINDOW_WIDTH - liveEarnedWidth) / 2, 352 + yOffset);
	g->DrawString(liveLost, (CIRCLE_WINDOW_WIDTH - liveLostWidth) / 2, 384 + yOffset);
}

void StatisticsScreen::Update()
{
	Widget::Update();
	//
}

void StatisticsScreen::Resize(int x, int y, int theWidth, int theHeight)
{
	Widget::Resize(x, y, theWidth, theHeight);
	mMainMenuButton->Resize(x + 10, y + CIRCLE_WINDOW_HEIGHT - mMainMenuButton->mHeight, 120, mMainMenuButton->mHeight);
	//mMainMenuButton->Resize(x, y + mHeight - 50, mMainMenuButton->mWidth, mMainMenuButton->mHeight);
}

void StatisticsScreen::AddedToManager(WidgetManager* theWidgetManager)
{
	Widget::AddedToManager(theWidgetManager);
	mMainMenuButton->Layout(48, this, 0, mHeight - 50, 100, 50);
	theWidgetManager->AddWidget(mMainMenuButton);
}

void StatisticsScreen::RemovedFromManager(WidgetManager* theWidgetManager)
{
	Widget::RemovedFromManager(theWidgetManager);
	theWidgetManager->RemoveWidget(mMainMenuButton);
}

void StatisticsScreen::ButtonDepress(int theId)
{
	if (theId == 0)
	{
		GetCircleShootApp()->ShowMainMenu();
	}
}