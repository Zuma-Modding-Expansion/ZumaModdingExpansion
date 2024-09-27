#include "Zuma_Prefix.pch"

#include <SexyAppFramework/Common.h>
#include <SexyAppFramework/Debug.h>
#include <SexyAppFramework/SexyAppBase.h>
#include <SexyAppFramework/ButtonWidget.h>
#include <SexyAppFramework/ButtonListener.h>
#include <SexyAppFramework/Font.h>
#include <SexyAppFramework/WidgetManager.h>
#include <SexyAppFramework/SexyMatrix.h>

#include "Ball.h"
#include "Board.h"
#include "Bullet.h"
#include "CircleShootApp.h"
#include "CircleButton.h"
#include "CircleCommon.h"
#include "CurveMgr.h"
#include "DataSync.h"
#include "LevelParser.h"
#include "Gun.h"
#include "HighScoreMgr.h"
#include "ParticleMgr.h"
#include "ProfileMgr.h"
#include "SoundMgr.h"
#include "SpriteMgr.h"
#include "StatsDialog.h"
#include "TransitionMgr.h"
#include "Res.h"
#include "WayPoint.h"
#include "WorkerThread.h"


using namespace Sexy;

class BoardOverlay : public Widget
{
public:
    BoardOverlay(Board *board) : Widget()
    {
        mBoard = board;
        mMouseVisible = false;
        mHasTransparencies = true;
        mHasAlpha = true;
    }

    virtual void Draw(Graphics *g)
    {
        mBoard->DrawOverlay(g);
    }

    Board *mBoard;
};

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

GameStats::GameStats()
{
    mTimePlayed = 0;
    mNumBallsCleared = 0;
    mNumGemsCleared = 0;
    mNumGaps = 0;
    mNumCombos = 0;
    mMaxCombo = -1;
    mMaxComboScore = 0;
    mMaxInARow = 0;
    mMaxInARowScore = 0;
}

void GameStats::Reset()
{
    mTimePlayed = 0;
    mNumBallsCleared = 0;
    mNumGemsCleared = 0;
    mNumGaps = 0;
    mNumCombos = 0;
    mMaxCombo = -1;
    mMaxComboScore = 0;
    mMaxInARow = 0;
    mMaxInARowScore = 0;
}

void GameStats::Add(const GameStats &theStats)
{
    mTimePlayed += theStats.mTimePlayed;
    mNumBallsCleared += theStats.mNumBallsCleared;
    mNumGemsCleared += theStats.mNumGemsCleared;
    mNumCombos += theStats.mNumCombos;
    mNumGaps += theStats.mNumGaps;

    if (theStats.mMaxCombo > mMaxCombo || theStats.mMaxCombo == mMaxCombo && theStats.mMaxComboScore > mMaxComboScore)
    {
        mMaxCombo = theStats.mMaxCombo;
        mMaxComboScore = theStats.mMaxComboScore;
    }

    if (theStats.mMaxInARow > this->mMaxInARow)
    {
        mMaxInARow = theStats.mMaxInARow;
        mMaxInARowScore = theStats.mMaxInARowScore;
    }
}

void GameStats::SyncState(DataSync &theSync)
{
    theSync.SyncLong(mTimePlayed);
    theSync.SyncLong(mNumBallsCleared);
    theSync.SyncLong(mNumGemsCleared);
    theSync.SyncLong(mNumGaps);
    theSync.SyncLong(mNumCombos);
    theSync.SyncLong(mMaxCombo);
    theSync.SyncLong(mMaxComboScore);
    theSync.SyncLong(mMaxInARow);
    theSync.SyncLong(mMaxInARowScore);
}

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////

Board::Board(CircleShootApp *theApp)
{
    mApp = theApp;
    mGun = new Gun();

    mMenuButton = new ButtonWidget(0, this);
    mContinueButton = MakeButton(1, this, "CONTINUE", CircleButton::CB_ClickSound, NULL, 3);
    mContinueButton->SetVisible(false);

    mOverlayWidget = new BoardOverlay(this);

    mMenuButton->mButtonImage = Sexy::IMAGE_MENU_BUTTON;
    Sexy::Image *menuImage = mMenuButton->mButtonImage;
    int menuWidth = menuImage->mWidth / 3;
    int menuHeight = menuImage->mHeight;
    mMenuButton->mNormalRect = Rect(0, 0, menuWidth, menuHeight);
    mMenuButton->mOverRect = Rect(menuWidth, 0, menuWidth, menuHeight);
    mMenuButton->mDownRect = Rect(menuWidth * 2, 0, menuWidth, menuHeight);

    mMenuButton->Resize(540, 3, menuWidth, menuHeight);
    mContinueButton->Resize(410, 450 - mContinueButton->mHeight, 200, mContinueButton->mHeight);

    mOverlayWidget->Resize(0, 0, CIRCLE_WINDOW_WIDTH, CIRCLE_WINDOW_HEIGHT);

    mScore = 0;
    mScoreDisplay = 0;
    mLevel = 0;
    mPauseCount = 0;
    mDialogCount = 0;
    mPauseUpdateCnt = 0;
    mLastBallClickTick = 0;
    mLastExplosionTick = 0;
    mStartLevel = 0;
    mLevelBeginScore = 0;
    mLives = 3;
    mDifficultyDiff = 0;
    mFlashCount = 30;
    mLastPauseTick = 0;
    mMaxStage = 0;
    mGameState = GameState_None;
    mSlowMo = 0;
    mDoGuide = 0;
    mIsSavedGame = false;
    mShowBallsDuringPause = false;
    mShowParTimer = false;

    mSpriteMgr = new SpriteMgr();
    mNextSpriteMgr = new SpriteMgr();
    mParticleMgr = new ParticleMgr(this);
    mTransitionMgr = new TransitionMgr(this);
    mSoundMgr = new SoundMgr();
    mLevelDesc = new LevelDesc();
    mNextLevelDesc = new LevelDesc();
    mIsEndless = false;
}

Board::~Board()
{
    WaitForLoadingThread();
    DeleteBullets();
    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        delete mCurveMgr.at(i);
    }
    for (int i = 0; i < mNextCurveMgr.size(); i++)
    {
        delete mNextCurveMgr.at(i);
    }
    delete mSpriteMgr;
    delete mNextSpriteMgr;
    delete mParticleMgr;
    delete mTransitionMgr;
    delete mSoundMgr;
    delete mGun;
    delete mLevelDesc;
    delete mNextLevelDesc;
    delete mMenuButton;
    delete mContinueButton;
    delete mOverlayWidget;
}

void Board::StaticLoadProc(void *theData)
{
    ((Board *)theData)->LoadProc();
}

void Board::LoadProc()
{
    if (mLoadingThreadParam == 0)
    {
        Sexy::MirrorPoint(mNextLevelDesc->mGunX, mNextLevelDesc->mGunY, MirrorType_None);

        for (TreasurePointList::iterator anItr = mNextLevelDesc->mTreasurePoints.begin();
             anItr != mNextLevelDesc->mTreasurePoints.end();
             anItr++)
        {
            Sexy::MirrorPoint(anItr->x, anItr->y, MirrorType_None);
        }

        mNextSpriteMgr->SetupLevel(*mNextLevelDesc, MirrorType_None);

        for (int i = 0; i < mNextLevelDesc->GetNumCurves(); i++)
        {
            if (i >= mNextCurveMgr.size())
                mNextCurveMgr.push_back(new CurveMgr(this));
            mNextCurveMgr.at(i)->SetupLevel(mNextLevelDesc, mNextSpriteMgr, i, MirrorType_None);
        }

        CurveDrawer aCurveDrawer(mSpriteMgr);
        for (int i = 0; i < mNextLevelDesc->GetNumCurves(); i++)
        {
            if (mNextLevelDesc->mCurveDesc.at(i).mDrawCurve && mNextLevelDesc->mCurveDesc.at(i).mDrawTunnels)
                mNextCurveMgr.at(i)->DrawTunnel(aCurveDrawer);
        }

        for (int i = 0; i < mNextLevelDesc->GetNumCurves(); i++)
        {
            if (mNextLevelDesc->mCurveDesc.at(i).mDrawCurve)
                mNextCurveMgr.at(i)->DrawCurve(aCurveDrawer);
        }

        aCurveDrawer.AddCurvesToMgr();
        CreateThumbnail(mNextSpriteMgr, mNextLevelDesc);
    }
    else if (mLoadingThreadParam == 1)
    {
        LevelDesc *aLevelDesc = mLevelDesc;
        if (aLevelDesc->mBackgroundAlpha.empty())
            aLevelDesc = &mApp->mLevelParser->GetBackgroundTransition();

        mSpriteMgr->GenerateBackgroundTransitionSprites(*aLevelDesc);
    }
}

void Board::DoAccuracy(bool accuracy)
{
    mRecalcGuide = accuracy;
    mShowGuide = accuracy;
    mDoGuide = accuracy;

    if (accuracy)
    {
        mGun->SetFireSpeed(15.0f);
    }
    else
    {
        mAccuracyCount = 0;
        mGun->SetFireSpeed(mLevelDesc->mFireSpeed);
    }
}

void Board::PlayBallClick(int theSound)
{
    int aTick = Sexy::BoardGetTickCount();
    if (aTick - mLastBallClickTick >= 250)
    {
        mApp->PlaySample(theSound);
        mLastBallClickTick = aTick;
    }
}

void Board::StartLevel()
{
    mGameState = GameState_Playing;
    mStateCount = 0;
    mSpriteMgr->ClearHoleFlashes();
    mParticleMgr->Clear();
    mTransitionMgr->Clear();
    mGun->SetFireSpeed(mLevelDesc->mFireSpeed);
    if (mLevelDesc->mGunType == 0)
        mGun->SetPos(mLevelDesc->mGunX, mLevelDesc->mGunY);
    mGun->SetGunType(mLevelDesc->mGunType);
    mSoundMgr->PlayLoop(LoopType_RollIn);
    for (CurveMgr* i : mCurveMgr) {
        i->StartLevel();
    }
    mLevelBeginning = true;
}

void Board::SetLosing()
{
    mApp->mDoPlayCount = true;
    mSoundMgr->StopLoop(LoopType_RollIn);

    ResetInARowBonus();
    mLevelStats.mTimePlayed = mStateCount;
    mGameStats.Add(mLevelStats);
    mLevelStats.Reset();
    DeleteBullets();

    if (!mGun->StartFire(false))
    {
        mGun->EmptyBullets();
    }

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        mCurveMgr[i]->SetLosing();
    }

    mSoundMgr->PlayLoop(LoopType_RollOut);
    mGameState = GameState_Losing;
    mStateCount = 0;
    mLives = mLives - 1;
    GetCircleShootApp()->mProfile->mUserStats.mLiveLost++;

    if (mLives <= 0)
    {
        mApp->PlaySong(39, false, 0.01);
        CheckHighScore();
    }

    mTransitionMgr->DoLosing();
}

void Board::DoLevelUp(bool playSounds, bool isCheat)
{
    mApp->mDoPlayCount = true;
    if (mPracticeBoard.empty())
    {
        if (mNextLevelDesc->mStage > mApp->mProfile->mMaxStage)
        {
            mApp->mProfile->mMaxStage = mNextLevelDesc->mStage;
            mApp->SaveProfile();
        }
    }
    else
    {
        mApp->mProfile->UpdateMaxLevel(mOriginalPracticeBoard, mLevel + 1);
        mApp->SaveProfile();
    }

    if (mIsEndless)
    {
        int aTreasure = (mCurTreasure != NULL) ? mCurTreasureNum : -1;
        mLevel++;

        GetLevel(mLevel, mLevelDesc, mLevelDesc->mName.c_str());

        if (aTreasure >= 0)
        {
            TreasurePointList::iterator anItr = mLevelDesc->mTreasurePoints.begin();
            std::advance(anItr, mCurTreasureNum);

            mCurTreasure = &*anItr;
        }

        mLevelString = Sexy::StrFormat("%s %d", gSmallGauntletStages[mLevelDesc->mStage], mLevelDesc->mLevel + 1);
        mVerboseLevelString = Sexy::StrFormat("%s %d", gGauntletStages[mLevelDesc->mStage], mLevelDesc->mLevel + 1);

        int aTextWidth = Sexy::FONT_HUGE->StringWidth(mVerboseLevelString);

        mParticleMgr->AddFloatingText(600 - aTextWidth, 440, 0xFFFFFF, mVerboseLevelString, Sexy::FONT_HUGE_ID, 0, 0, 150, true);

        for (int i = 0; i < mCurveMgr.size(); i++)
        {
            mCurveMgr[i]->DoEndlessZumaEffect();
            mCurveMgr[i]->SetupLevelDesc(mLevelDesc);
            IncScore(100 * (100 - mCurveMgr[i]->GetFarthestBallPercent()));
        }

        mLevelBeginScore = mScore;
        mScoreTarget = mScore + 1250;
        return;
    }

    mSoundMgr->StopLoop(LoopType_RollIn);
    mTargetBarSize = 256;
    mCurBarSize = 256;

    if (!isCheat)
    {
        std::string aLvlString = GetUniqueLevelString();
        mApp->mHighScoreMgr->SubmitLowTime(aLvlString, mApp->mProfile->mName, mStateCount / 100);
    }

    if (mNumClearsInARow > mLevelStats.mMaxInARow)
    {
        mLevelStats.mMaxInARow = mNumClearsInARow;
        mLevelStats.mMaxInARowScore = mCurInARowBonus;
    }

    ResetInARowBonus();
    mLevelStats.mTimePlayed = mStateCount;
    mGameStats.Add(mLevelStats);

    mApp->PlaySong(38, false, 0.01);
    DoAccuracy(false);
    WaitForLoadingThread();

    if (mPracticeMode != PracticeMode_Single)
    {
        DoGenerateBackgroundTransitions();
    }

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        mCurveMgr[i]->DeleteBalls();
    }

    DeleteBullets();

    if (!mGun->StartFire(false))
        mGun->EmptyBullets();

    mGun->CreateCachedGunImage();
    mGameState = GameState_LevelUp;
    mTransitionMgr->DoLevelUp();
    mStateCount = 0;

    MarkDirty();
}

void Board::CheckEndConditions()
{
    if (!mBulletList.empty() || mGun->IsFiring())
        return;

    int i;
    int mNumCurves = mCurveMgr.size();
    for (i = 0; i < mNumCurves; i++)
    {
        if (!mCurveMgr[i]->IsWinning())
        {
            break;
        }
    }

    if (i == mNumCurves && mNumCurves > 0)
    {
        DoLevelUp(true, false);
        return;
    }

    for (i = 0; i < mNumCurves; i++)
    {
        if (mCurveMgr[i]->IsLosing())
        {
            break;
        }
    }

    if (i != mNumCurves)
    {
        SetLosing();
        return;
    }

    i = 0;
    for (i = 0; i < mNumCurves; i++)
    {
        if (mCurveMgr[i]->IsInDanger())
            break;
    }

    mApp->SwitchSong((i == mNumCurves) ? 0 : 36);
}

void Board::SyncPracticeMode()
{
    if (mPracticeBoard.empty())
    {
        mPracticeMode = PracticeMode_None;
    }
    else if (mPracticeBoard == "random")
    {
        mPracticeMode = PracticeMode_Random;
    }
    else if (mPracticeBoard == "sequence")
    {
        mPracticeMode = PracticeMode_Sequence;
    }
    else
    {
        mPracticeMode = PracticeMode_Single;
    }
}

void Board::AdvanceFreeBullet(BulletList::iterator &theBulletItr)
{
    Bullet *aBullet = *theBulletItr;
    aBullet->Update();

    if (mCurTreasure != NULL)
    {
        float dx = aBullet->GetX() - mCurTreasure->x - 15;
        float dy = aBullet->GetY() - mCurTreasure->y - 15;
        float r = aBullet->GetRadius() + 20;

        if (dx * dx + dy * dy < r * r)
        {
            ++mLevelStats.mNumGemsCleared;
            ++GetCircleShootApp()->mProfile->mUserStats.mCoinAmount;
            FloatingTextHelper aFloat;

            int bonus = ((mScoreTarget - mLevelBeginScore) / 600) * 100;

            if (mIsEndless)
            {
                if (bonus < 200)
                {
                    bonus = 200;
                }
            }
            else
            {
                if (bonus < 500)
                {
                    bonus = 500;
                }
            }

            aFloat.AddText(StrFormat("BONUS +%d", bonus), Sexy::FONT_FLOAT_ID, 0xFFFF00);
            aFloat.AddToMgr(mParticleMgr, mCurTreasure->x, mCurTreasure->y);
            IncScore(bonus);

            mApp->PlaySample(Sexy::SOUND_JEWEL_HIT);

            mTreasureZoomFrame = 1;
            mTreasureZoomX = mCurTreasure->x;
            mTreasureZoomY = mCurTreasure->y;

            for (int i = 0; i < 10; i++)
            {
                float vx = cosf(i * 360 / 10 * SEXY_PI / 180.0f);
                float vy = sinf(i * 360 / 10 * SEXY_PI / 180.0f);
                uint color = gBrightBallColors[Sexy::Rand() % 6];

                mParticleMgr->AddSparkle(mCurTreasure->x + vx * 16.0f, mCurTreasure->y + vx * 16.0f, 2 * vx, 2 * vy, 5, 0, Sexy::Rand() % 5, color);
            }

            mCurTreasure = NULL;

            delete aBullet;
            theBulletItr = mBulletList.erase(theBulletItr);
            return;
        }
    }

    if (gCheckCollision)
    {
        for (int i = 0; i < mCurveMgr.size(); i++)
        {
            if (mCurveMgr[i]->CheckCollision(aBullet))
            {
                theBulletItr = mBulletList.erase(theBulletItr);
                return;
            }
        }
    }

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        mCurveMgr[i]->CheckGapShot(aBullet);
    }

    if (aBullet->GetX() >= 0.0f && aBullet->GetY() >= 0.0f &&
        (aBullet->GetX() - aBullet->GetRadius()) < mWidth &&
        (aBullet->GetY() - aBullet->GetRadius()) < mHeight)
    {
        ++theBulletItr;
    }
    else
    {
        ResetInARowBonus();
        delete aBullet;
        theBulletItr = mBulletList.erase(theBulletItr);
    }
}

void Board::UpdateBullets()
{
    for (BulletList::iterator aBulletItr = mBulletList.begin(); aBulletItr != mBulletList.end();)
    {
        AdvanceFreeBullet(aBulletItr);
    }
}

void Board::UpdatePlaying()
{
    Bullet *aBullet = mGun->GetFiredBullet();
    if (aBullet) // CANNON TRISHOT
    {
        mBulletList.push_back(aBullet);
        if (aBullet->GetPowerType() == PowerType_CannonShot)
        {
            Bullet* bBullet = new Bullet();
            bBullet->SetPowerType(PowerType_CannonShot, false);
            float rad = mGun->GetAngle() - (SEXY_PI / 2) + 0.33f;
            float vx = cosf(rad);
            float vy = -sinf(rad);

            bBullet->SetVelocity(vx * mGun->GetFireSpeed(), vy * mGun->GetFireSpeed());
            bBullet->SetPos(aBullet->GetX(), aBullet->GetY());
            mBulletList.push_back(bBullet);

            Bullet* cBullet = new Bullet();
            cBullet->SetPowerType(PowerType_CannonShot, false);
            rad = mGun->GetAngle() - (SEXY_PI / 2) - 0.33f;
            vx = cosf(rad);
            vy = -sinf(rad);

            cBullet->SetVelocity(vx * mGun->GetFireSpeed(), vy * mGun->GetFireSpeed());
            cBullet->SetPos(aBullet->GetX(), aBullet->GetY());
            mBulletList.push_back(cBullet);

            for (int i = 0; i < mCurveMgr.size(); i++)
            {
                aBullet->AddCurCurvePoint(0);
                bBullet->AddCurCurvePoint(0);
                cBullet->AddCurCurvePoint(0);
            }
            CheckReload();
        }
        else
        {
            for (int i = 0; i < mCurveMgr.size(); i++)
            {
                aBullet->AddCurCurvePoint(0);
            }
            CheckReload();
        }
    }

    UpdateBullets();
    UpdateTreasure();

    if (mLevelBeginning)
    {
        bool stillStarting = false;

        for (int i = 0; i < mCurveMgr.size(); ++i)
        {
            if (!mCurveMgr[i]->HasReachedCruisingSpeed())
            {
                stillStarting = true;
            }
        }

        if (!stillStarting || mStateCount > 500)
        {
            mLevelBeginning = false;
            mSoundMgr->StopLoop(LoopType_RollIn);
        }
    }

    if (!mHaveReachedTarget && mCurBarSize == 256 && mScore >= mScoreTarget)
    {
        mBarBlinkCount = 224;
        mFlashCount = 50;
        mApp->PlaySample(Sexy::SOUND_BAR_FULL);

        if (mIsEndless)
        {
            DoLevelUp();
        }
        else
        {
            mHaveReachedTarget = true;
            for (int i = 0; i < mCurveMgr.size(); ++i)
            {
                mCurveMgr[i]->SetStopAddingBalls(true);

                if (!mDestroyAll)
                {
                    mCurveMgr[i]->DetonateBalls();
                }
            }
        }

        if (!mDestroyAll)
        {
            mApp->PlaySample(Sexy::SOUND_EXPLODE);
        }
    }

    int aScoreDiff = mScoreTarget - mLevelBeginScore;
    if (aScoreDiff > 0)
    {
        int aScoreRemaining = mScoreTarget - mScore;
        if (aScoreRemaining < 0)
        {
            if (mLevelEndFrame != 0)
            {
                if (mStateCount - 3000 == mLevelEndFrame)
                {
                    for (int i = 0; i < mCurveMgr.size(); i++)
                    {
                        mLevelDesc->mCurveDesc[i].mPowerUpFreq[PowerType_Bomb] = 500;
                        mLevelDesc->mCurveDesc[i].mPowerUpFreq[PowerType_SlowDown] = 0;
                        mLevelDesc->mCurveDesc[i].mPowerUpFreq[PowerType_Accuracy] = 0;
                        mLevelDesc->mCurveDesc[i].mPowerUpFreq[PowerType_MoveBackwards] = 0;
                        mLevelDesc->mCurveDesc[i].mAccelerationRate = 0.0003;
                    }
                }
            }
            else if (mBallColorMap.size() == 2)
            {
                mLevelEndFrame = mStateCount;
            }
        }

        mTargetBarSize = 256 - (aScoreRemaining * 256 / aScoreDiff);
    }

    for (int i = 0; i < mCurveMgr.size(); ++i)
    {
        mCurveMgr[i]->UpdatePlaying();
    }

    CheckEndConditions();
    if (mStateCount > 50)
    {
        CheckReload();
    }

    MarkDirty();
}

void Board::UpdateLosing()
{
    MarkDirty();
    mTransitionMgr->Update();
}

void Board::UpdateLevelUp()
{
    MarkDirty();
    mTransitionMgr->Update();
}

void Board::UpdateLevelBegin()
{
    MarkDirty();
    mTransitionMgr->Update();
}

void Board::CheckReload()
{
    if (mBallColorMap.empty())
        return;

    Bullet *aBullet = mGun->GetBullet();

    if (aBullet != NULL && mBallColorMap.find(aBullet->GetType()) == mBallColorMap.end())
    {
        int rand = Sexy::AppRand() % mBallColorMap.size();
        ColorMap::iterator it = mBallColorMap.begin();

        for (int i = 0; i < rand; i++)
            it++;

        mGun->SetBulletType(it->first);
    }

    aBullet = mGun->GetNextBullet();

    if (aBullet != NULL && mBallColorMap.find(aBullet->GetType()) == mBallColorMap.end())
    {
        int rand = Sexy::AppRand() % mBallColorMap.size();
        ColorMap::iterator it = mBallColorMap.begin();

        for (int i = 0; i < rand; i++)
            it++;

        mGun->SetNextBulletType(it->first);
    }

    while (mGun->NeedsReload())
    {
        int rand = Sexy::AppRand() % mBallColorMap.size();
        ColorMap::iterator it = mBallColorMap.begin();

        for (int i = 0; i < rand; i++)
            it++;

        mGun->Reload(it->first, true, Sexy::PowerType_Max);
    }
}

void Board::UpdateBallColorMap(Ball *theBall, bool added)
{
    if (added)
    {
        mBallColorMap[theBall->GetType()]++;
    }
    else
    {
        ColorMap::iterator anItr = mBallColorMap.find(theBall->GetType());

        if (anItr != mBallColorMap.end())
        {
            anItr->second--;

            if (anItr->second <= 0)
            {
                mBallColorMap.erase(anItr);
            }
        }
    }
}

void Board::UpdateGuide()
{
    float anAngle = mGun->GetAngle() - (SEXY_PI / 2);
    float dx = sinf(anAngle);
    float dy = cosf(anAngle);
    float dx2 = dx;
    float dy2 = dy;
    float dx3 = dx * 16.0f;
    float dy3 = dy * 16.0f;
    // int dist = 50; this was probably there ???

    const SexyVector3 aCenter(
        (float)mGun->GetCenterX() + dy * 50.0f,
        (float)mGun->GetCenterY() + dx * -50.0f,
        0.0f);

    SexyVector3 g1(aCenter.x - dx3, aCenter.y - dy3, 0.0f);
    SexyVector3 g2(aCenter.x + dx3, aCenter.y + dy3, 0.0f);
    SexyVector3 v1(cosf(anAngle), -sinf(anAngle), 0.0f);
    float t = 10000000.0f;

    Sexy::Ball *aBall = NULL;

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        Sexy::Ball *anIntersectBall = mCurveMgr[i]->CheckBallIntersection(g1, v1, t);
        if (anIntersectBall)
            aBall = anIntersectBall;
    }

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        Sexy::Ball *anIntersectBall = mCurveMgr[i]->CheckBallIntersection(g2, v1, t);
        if (anIntersectBall)
            aBall = anIntersectBall;
    }

    if (aBall == NULL)
    {
        t = 1000.0f / v1.Magnitude();
    }

    mGuideBall = aBall;

    SexyVector3 aGuide = aCenter + v1 * t;

    if (!mRecalcGuide && mShowGuide && (mGuideCenter - aGuide).Magnitude() < 20.0f)
        return;

    mGuideCenter = aGuide;
    mShowGuide = true;
    mRecalcGuide = false;

    mGuide[0].mX = g1.x + dx3 * 0.5f;
    mGuide[0].mY = g1.y + dy3 * 0.5f;
    mGuide[1].mX = g2.x + dx3 * -0.5f;
    mGuide[1].mY = g2.y + dy3 * -0.5f;
    mGuide[2].mX = aGuide.x + dx2;
    mGuide[2].mY = aGuide.y + dy2;
    mGuide[3].mX = aGuide.x - dx2;
    mGuide[3].mY = aGuide.y - dy2;
}

int GetTreasurePitch(int x)
{
    return 3000 * (x - 320) / 320;
}

void Board::UpdateTreasure()
{
    if (mTreasureZoomFrame != 0)
    {
        mTreasureZoomFrame++;
        if (mTreasureZoomFrame > 40)
        {
            mTreasureZoomFrame = 0;
        }
    }

    if (mCurTreasure != NULL && !gForceTreasure)
    {
        if (mStateCount > mTreasureEndFrame)
        {
            mApp->PlaySample(Sexy::SOUND_JEWEL_DISAPPEAR, GetTreasurePitch(mCurTreasure->x));
            mCurTreasure = NULL;
        }
        return;
    }

    if (!gForceTreasure)
    {
        if (mScore >= mScoreTarget)
            return;

        if (mLevelDesc->mTreasurePoints.empty())
            return;

        int aRnd = Sexy::AppRand() % mLevelDesc->mTreasureFreq;
        if (aRnd != 0 || mLevelDesc->mTreasureFreq >= mStateCount - mTreasureEndFrame)
            return;
    }

    for (int i = 0; i < mLevelDesc->mTreasurePoints.size() * 2; i++)
    {
        int aTreasureNum;
        if (gForceTreasure)
        {
            aTreasureNum = gForceTreasureNum - 1;
        }
        else
        {
            aTreasureNum = Sexy::AppRand();
        }

        mCurTreasureNum = aTreasureNum % mLevelDesc->mTreasurePoints.size();

        TreasurePointList::iterator anItr = mLevelDesc->mTreasurePoints.begin();
        std::advance(anItr, mCurTreasureNum);

        mCurTreasure = &*anItr;

        if (!gForceTreasure)
        {
            for (int j = 0; j < mCurveMgr.size(); j++)
            {
                if (j >= mCurTreasure->mCurveDist.size())
                    continue;

                float aFarthestBallPercent = mCurveMgr[j]->GetFarthestBallPercent();
                if (mCurTreasure->mCurveDist[j] > aFarthestBallPercent)
                {
                    mCurTreasure = NULL;
                    gForceTreasure = false;
                    return;
                }
            }
        }

        if (mCurTreasure != NULL)
        {
            mApp->PlaySample(Sexy::SOUND_JEWEL_APPEAR, GetTreasurePitch(mCurTreasure->x));
            mTreasureEndFrame = mStateCount + 1000;

            for (int j = 0; j < 360; j += 90)
            {
                uint aColor = gBrightBallColors[Sexy::Rand() % 6];
                int aRand = Sexy::Rand() % 5;

                float y = mCurTreasure->y + sinf(j * SEXY_PI / 180.0) * 4.0f - 15.0f;
                float x = mCurTreasure->x + cosf(j * SEXY_PI / 180.0) * 4.0f + 10.0f;

                mParticleMgr->AddSparkle(x, y, 0.0f, 0.25f, 5, 0, aRand, aColor);
            }
            break;
        }
    }

    gForceTreasure = false;
}

void Board::UpdateMiscStuff()
{
    bool flag;

    if (mScoreDisplay == mScore)
    {
        flag = false;
    }
    else
    {
        if (mScoreDisplay < mScore)
            mScoreDisplay += ((mScore - mScoreDisplay) / 50.0) + 2.0;

        if (mScoreDisplay > mScore)
            mScoreDisplay = mScore;

        flag = true;
    }

    if (mCurBarSize != mTargetBarSize)
    {
        if (mCurBarSize < mTargetBarSize)
            mCurBarSize++;

        if (mCurBarSize > mTargetBarSize)
            mCurBarSize--;

        flag = true;
    }

    if (mBarBlinkCount > 0)
    {
        mBarBlinkCount--;
        flag = true;
    }

    if (mFlashCount > 0)
    {
        gBallBlink = true;

        if (--mFlashCount == 0)
            gBallBlink = false;

        flag = true;
    }

    if (mLivesBlinkCount > 0)
    {
        mLivesBlinkCount--;
        flag = true;
    }

    if (mAccuracyCount > 0)
    {
        if (--mAccuracyCount == 0)
            DoAccuracy(false);
    }

    if (!flag)
        return;

    MarkDirty();
}

void Board::DrawTreasure(Graphics *g)
{
    if (mCurTreasure != NULL)
    {
        int x = mCurTreasure->x;
        int y = mCurTreasure->y;
        int v10 = (mStateCount / 4) % 30;

        g->SetColorizeImages(true);
        g->SetColor(Color(0, 0, 0, 96));
        g->DrawImageCel(Sexy::IMAGE_COIN, x - 28, y - 8, v10);
        g->SetColorizeImages(false);
        g->DrawImageCel(Sexy::IMAGE_COIN, x - 18, y - 18, v10);
    }

    if (mTreasureZoomFrame != 0)
    {
        float aScale = 1.0f + mTreasureZoomFrame / 30.0f;
        int x = mTreasureZoomX + aScale * -18.0f;
        int y = mTreasureZoomY + aScale * -18.0f;

        int width = Sexy::IMAGE_COIN->mWidth;
        int height = Sexy::IMAGE_COIN->mHeight / Sexy::IMAGE_COIN->mNumRows;

        int offY = mTreasureZoomFrame + ((mStateCount - mTreasureZoomFrame + 1) / 4);

        Rect rc1(0, height * (offY % 30), width, height);
        Rect rc2(x, y, width * aScale, height * aScale);

        bool hadFastStretch = g->GetFastStretch();

        if (mApp->Is3DAccelerated())
        {
            Rect a3(x + aScale * -10.0f, y + aScale * 10.0f, width * aScale, height * aScale);

            g->SetColorizeImages(true);
            g->SetColor(Color(0, 0, 0, 96 - 96 * mTreasureZoomFrame / 40));
            g->DrawImage(Sexy::IMAGE_COIN, a3, rc1);
            g->SetFastStretch(false);
            g->SetColor(Color(255, 255, 255, 255 - 255 * mTreasureZoomFrame / 40));
        }
        else
        {
            g->SetFastStretch(true);
        }

        g->DrawImage(Sexy::IMAGE_COIN, rc2, rc1);
        g->SetFastStretch(hadFastStretch);
        g->SetColorizeImages(false);
    }
}

void Board::DrawPlaying(Graphics *g)
{
    static BallDrawer aDrawer;

    mSpriteMgr->DrawBackground(g);
    aDrawer.Reset();

    if (mPauseCount == 0 || mShowBallsDuringPause)
    {
        for (int i = 0; i < mCurveMgr.size(); i++)
        {
            mCurveMgr[i]->DrawBalls(aDrawer);
        }
    }

    aDrawer.Draw(g, mSpriteMgr, mParticleMgr);
    mGun->Draw(g);
    DrawTreasure(g);

    if (mShowGuide)
    {
        int anAlpha = 120;

        if (mAccuracyCount <= 300)
        {
            anAlpha = (120 * mAccuracyCount) / 300 + 8;
        }

        g->SetColor(Color(0, 255, 255, anAlpha));
        g->PolyFill(mGuide, 4, false);
    }

    DrawBullets(g);

    if (mGun->GetBullet() != NULL && mGun->GetBullet()->GetPowerType() == PowerType_CannonShot)
    {
        const int anim_time = 150;
        int frame = GetBoardStateCount() % anim_time;
        float anim_percent = (float)frame / (float)anim_time;

        g->SetColorizeImages(true);
        for (int i = 0; i < 3; i++) {
            int alpha = 255;
            if (i == 2) {
                alpha = 255 * (1 - pow(1.0 - anim_percent, 4.0)); // apply ease
            }
            else if (i == 0) {
                alpha = 255 - 255 * (anim_percent * anim_percent * anim_percent * anim_percent); // apply ease
            }
            g->SetColor(Color(255, 255, 255, alpha));

            float distancefromGun = (200.0f - 75.0f * i) + 75.0f * anim_percent; // offset from frog
            float gunX = (float)mGun->GetCenterX();
            float gunY = (float)mGun->GetCenterY();
            float angle = mGun->GetAngle();

            float scale = (0.35f * anim_percent) + 1.15f - 0.35f * i;
            float scalex = scale * (1 - (0.3f * i) + 0.3f * anim_percent);
            float scaley = scale;

            float posX = gunX + distancefromGun * sin(angle);
            float posY = gunY + distancefromGun * cos(angle);

            if (mApp->Is3DAccelerated()) {
                Sexy::Transform aTrans = Sexy::Transform();
                aTrans.Scale(scalex, scaley);
                aTrans.RotateRad(angle + SEXY_PI);
                aTrans.Translate(posX, posY);
                g->DrawImageTransformF(Sexy::IMAGE_CANNON_VISUAL, aTrans);
            }
            else {
                g->DrawImageRotatedF(Sexy::IMAGE_CANNON_VISUAL, posX - 20, posY - 20, (float)angle + SEXY_PI);
            }

            float lateralOffset = (48.75f - 24.375f * i) + 24.375 * anim_percent; // angle offset
            float cDistance = distancefromGun * -0.1f; // closer distance for 2nd and 3rd 

            float posX2 = posX + cDistance * sin(angle) - lateralOffset * cos(angle);
            float posY2 = posY + cDistance * cos(angle) + lateralOffset * sin(angle);

            if (mApp->Is3DAccelerated()) {
                Sexy::Transform aTrans = Sexy::Transform();
                aTrans.Scale(scalex, scaley);
                aTrans.RotateRad(angle + SEXY_PI - 0.35f);
                aTrans.Translate(posX2, posY2);
                g->DrawImageTransformF(Sexy::IMAGE_CANNON_VISUAL, aTrans);
            }
            else {
                g->DrawImageRotatedF(Sexy::IMAGE_CANNON_VISUAL, posX2 - 20, posY2 - 20, (float)angle + SEXY_PI - 0.35f);
            }


            float posX3 = posX + cDistance * sin(angle) + lateralOffset * cos(angle);
            float posY3 = posY + cDistance * cos(angle) - lateralOffset * sin(angle);


            if (mApp->Is3DAccelerated()) {
                Sexy::Transform aTrans = Sexy::Transform();
                aTrans.Scale(scalex, scaley);
                aTrans.RotateRad(angle + SEXY_PI + 0.35f);
                aTrans.Translate(posX3, posY3);
                g->DrawImageTransformF(Sexy::IMAGE_CANNON_VISUAL, aTrans);
            }
            else {
                g->DrawImageRotatedF(Sexy::IMAGE_CANNON_VISUAL, posX3 - 20, posY3 - 20, (float)angle + SEXY_PI + 0.35f);
            }
        }
        g->SetColorizeImages(false);
    }
}

void Board::DrawLosing(Graphics *g)
{
    mTransitionMgr->Draw(g);
}

void Board::DrawLevelUp(Graphics *g)
{
    mTransitionMgr->Draw(g);
}

void Board::DrawLevelBegin(Graphics *g)
{
    mTransitionMgr->Draw(g);
}

void Board::DrawBalls(Graphics *g)
{
}

void Board::DrawBullets(Graphics *g)
{
    for (BulletList::iterator anItr = mBulletList.begin(); anItr != mBulletList.end(); anItr++)
        (*anItr)->DrawShadow(g);

    for (BulletList::iterator anItr = mBulletList.begin(); anItr != mBulletList.end(); anItr++)
        (*anItr)->Draw(g);
}

void Board::DrawText(Graphics *g)
{
    std::string aText;
    bool v19 = false;
    int v20 = 0;

    g->SetFont(Sexy::FONT_DIALOG);

    if ((mLivesBlinkCount & 0x10) == 0)
    {
        int aLives = mLives - 1;

        if (mGameState == GameState_Losing)
        {
            aLives = mLives;
        }

        if (mIsEndless)
        {
            g->SetColor(Color(0xFFFF00));
            aText = "Survival";
            int aTextWidth = Sexy::FONT_DIALOG->StringWidth(aText);
            g->DrawString(aText, 64 - aTextWidth / 2, 20);

            v19 = false;
            v20 = 0;
        }
        else
        {
            if (aLives == 0)
            {
                g->SetColor(Color(0xFFFF00));
                aText = "Last Life";
                int aTextWidth = Sexy::FONT_DIALOG->StringWidth(aText);
                g->DrawString(aText, 64 - aTextWidth / 2, 20);
                v19 = false;
                v20 = 0;
            }
            else if (aLives <= 3)
            {
                v20 = aLives;
                v19 = false;
            }
            else
            {
                v20 = 1;
                v19 = true;
            }
        }

        int aFrogX = 28;
        for (int i = 0; i < v20; i++)
        {
            g->DrawImage(Sexy::IMAGE_FROG_LIVES, aFrogX, 3);
            aFrogX += 26;
        }

        if (v19)
        {
            g->SetColor(Color(0xFFFF00));
            g->SetFont(Sexy::FONT_FLOAT);
            std::string aText = Sexy::StrFormat("x%d", aLives);
            g->DrawString(aText, aFrogX + 4, 22);
            g->SetFont(Sexy::FONT_DIALOG);
        }
    }

    g->SetColor(Color(0xFFFF00));
    int aLvlWidth = Sexy::FONT_DIALOG->StringWidth(mLevelString);
    g->DrawString(mLevelString, 200 - aLvlWidth / 2, 15);

    g->SetFont(Sexy::FONT_MONODIGIT);
    std::string aScoreTxt = Sexy::StrFormat("%d", mScoreDisplay);
    int aScoreWidth = Sexy::FONT_MONODIGIT->StringWidth(aScoreTxt);
    g->DrawString(aScoreTxt, 360 - aScoreWidth, 17);
    g->SetFont(Sexy::FONT_DIALOG);
    g->SetColor(Color(0xFFFF00));

    if (mScoreTarget - mLevelBeginScore > 0)
    {
        Image *aBarImage = Sexy::IMAGE_ZUMA_BAR;
        int aBarWidth = Sexy::IMAGE_ZUMA_BAR->mWidth;

        if ((mBarBlinkCount & 0x20) == 0)
        {
            int v11 = mCurBarSize * aBarWidth;
            int v12 = v11 + 255;
            if (v11 > -1)
            {
                v12 = v11;
            }

            int v13 = v12 >> 8;
            if (aBarWidth <= v13 || mGameState == GameState_LevelUp)
            {
                aBarImage = Sexy::IMAGE_ZUMA_BAR_DONE;
                v13 = aBarWidth;
            }
            else if (mBarBlinkCount != 0)
            {
                aBarImage = Sexy::IMAGE_ZUMA_BAR_DONE;
            }

            Rect aRect(0, 0, v13, aBarImage->mHeight);
            g->DrawImage(aBarImage, 409, 2, aRect);
        }
    }

    if (mShowParTimer && mGameState == GameState_Playing && !mIsEndless)
    {
        int aTime = mLevelDesc->mParTime - mStateCount / 100;
        bool aOverTime = false;

        if (aTime < 0)
        {
            aTime = mStateCount / 100 - mLevelDesc->mParTime;
            aOverTime = true;
        }

        std::string aTimeText = Sexy::GetTimeString(aTime);
        if (aOverTime)
        {
            aTimeText = "-" + aTimeText;
        }

        g->SetFont(Sexy::FONT_MAIN10OUTLINE);
        g->SetColor(Color(0xFFFF00));
        g->DrawString(aTimeText, 580, 460);
    }
}

void Board::DrawOverlay(Graphics *g)
{
    mParticleMgr->DrawTopMost(g);

    if (mPauseCount == 0)
        return;

    if (!mShowBallsDuringPause && mLives > 0)
    {
        g->SetColor(Color(0, 0, 0, 255 * mPauseFade / 100));
        g->FillRect(0, 0, CIRCLE_WINDOW_WIDTH, CIRCLE_WINDOW_HEIGHT);
    }

    if (mDialogCount == 0)
    {
        g->SetFont(Sexy::FONT_HUGE);

        std::string pauseText = "PAUSED";
        int textY = ((mHeight - Sexy::FONT_HUGE->GetHeight()) / 2) + Sexy::FONT_HUGE->GetAscent() - 20;
        int textWidth = Sexy::FONT_HUGE->StringWidth(pauseText);

        g->SetColor(Color(0xFFFFFF));
        g->DrawString(pauseText, (mWidth - textWidth) / 2, textY);
    }
}

void Board::Update()
{
    static bool inUpdate = false;

    Widget::Update();

    if (mSlowMo && (mUpdateCnt & 3) != 0)
        return;

    if (gSpeedUp && !inUpdate)
    {
        inUpdate = true;
        for (int i = 0; i < 10; i++)
        {
            Update();
        }
        inUpdate = false;
        return;
    }

    if (mPauseCount != 0)
    {
        if (mApp->GetDialog(DialogType_Stats) != NULL)
        {
            if (mParticleMgr->Update())
            {
                MarkDirty();
            }

            mSoundMgr->Update();
            UpdateMiscStuff();
        }

        if (mPauseFade <= 50)
        {
            mPauseFade++;
            MarkDirty();
        }
    }
    else
    {
        mApp->mUnk29++;
        mGun->Update();
        mSpriteMgr->Update();
        mParticleMgr->Update();
        mSoundMgr->Update();

        mStateCount++;

        switch (mGameState)
        {
        case GameState_Playing:
            UpdatePlaying();
            break;
        case GameState_Losing:
            UpdateLosing();
            break;
        case GameState_LevelUp:
            UpdateLevelUp();
            break;
        case GameState_LevelBegin:
            UpdateLevelBegin();
            break;
        default:
            break;
        }

        if (mDoGuide)
        {
            UpdateGuide();
        }

        UpdateMiscStuff();
    }
}

void Board::Draw(Graphics *g)
{
    if (mApp->mWorkerThread->mTaskProc)
    {
        mApp->mWorkerThread->WaitForTask();
    }

    if (mGameState == GameState_Losing)
    {
        DrawLosing(g);
    }
    else if (mGameState == GameState_LevelUp)
    {
        DrawLevelUp(g);
    }
    else if (mGameState == GameState_LevelBegin)
    {
        DrawLevelBegin(g);
    }
    else
    {
        DrawPlaying(g);
    }

    mSpriteMgr->DrawBorder(g);
    DrawText(g);
}

void Board::MouseMove(int x, int y)
{
    Widget::MouseMove(x, y);

    if (mPauseCount != 0)
        return;

    switch (mLevelDesc->mGunType)
    {
        case 0:
        {
            int fromCenterX = x - mGun->GetCenterX(); /// RADIAL
            float angle;

            if (fromCenterX != 0)
            {
                float v4 = atanf((float)(mGun->GetCenterY() - y) / (float)fromCenterX);
                if (fromCenterX < 0)
                {
                    v4 += SEXY_PI;
                }

                angle = v4 + (SEXY_PI / 2.0);
            }
            else
            {
                angle = 0.0f;
                if (y < mGun->GetCenterY())
                {
                    angle = SEXY_PI;
                }
            }
            mGun->SetAngle(angle);
            break;
        }
        case 1:
        {
            if (x < mLevelDesc->mGunX + mLevelDesc->mGunWidth / 2 && x > mLevelDesc->mGunX - mLevelDesc->mGunWidth / 2 && mGameState == GameState_Playing)
                mGun->SetPos(x, mGun->GetCenterY()); // HORIZ
            if (y < mGun->GetCenterY())
            {
                mGun->SetAngle(SEXY_PI);
            }
            else
            {
                mGun->SetAngle(SEXY_PI * 2);
            }
            break;
        }
        case 2:
        {
            if (y < mLevelDesc->mGunY + mLevelDesc->mGunHeight / 2 && y > mLevelDesc->mGunY - mLevelDesc->mGunHeight / 2 && mGameState == GameState_Playing)
                mGun->SetPos(mGun->GetCenterX(), y); // VERT

            if (x < mGun->GetCenterX())
            {
                mGun->SetAngle(SEXY_PI * 1.5f);
            }
            else
            {
                mGun->SetAngle(SEXY_PI / 2.0f);
            }
            break;
        }
    }
    mRecalcGuide = true;
    MarkDirty();
}

void Board::MouseDrag(int x, int y)
{
    Widget::MouseDrag(x, y);
    Widget::MouseMove(x, y);
}

void Board::MouseDown(int x, int y, int theClickCount)
{
    Widget::MouseDown(x, y, theClickCount);

    if (mPauseCount != 0)
    {
        Pause(false);
    }
    else if (mPauseUpdateCnt != mUpdateCnt)
    {
        if (mGameState == GameState_Playing)
        {
            for (int i = 0; i < mCurveMgr.size(); i++)
            {
                if (!mCurveMgr[i]->CanFire())
                    return;
            }

            if (theClickCount >= 0)
            {
                if (mGun->StartFire())
                {
                    if (mGun->GetBullet()->GetPowerType() == PowerType_CannonShot)
                    {
                        mApp->PlaySample(Sexy::SOUND_CANNONFIRE);
                        return;
                    }
                    mApp->PlaySample(Sexy::SOUND_BALLFIRE);
                }
            }
            else
            {
                if (mGun->GetBullet()->GetPowerType() != PowerType_CannonShot)
                    mGun->SwapBullets();
            }
        }
        else
        {
            mTransitionMgr->MouseDown(x, y, theClickCount);
        }
    }
}

extern bool gForceStageComplete;

void Board::KeyChar(char theChar)
{
    char c = toupper(theChar);
    if (theChar == ' ' && mDialogCount == 0)
    {
        int aTickCount = Sexy::AppGetTickCount();
        if (aTickCount - mLastPauseTick >= 100)
        {
            mLastPauseTick = aTickCount;
            Pause(mPauseCount == 0, false);
        }
    }
    else if (c == 'T')
    {
        mShowParTimer = !mShowParTimer;
        MarkDirty();
    }
    else if (c == 'S')
    {
        mSpriteMgr->ToggleSpaceScroll();
    }
#ifndef RELEASEFINAL
    else if (c == 'P') // TODO THIS IS EXTRA DEBUG FUNCTIONALITY
    {
        mScore = mScoreTarget;
        mCurBarSize = mTargetBarSize;

        for (int i = 0; i < mCurveMgr.size(); i++)
        {
            mCurveMgr[i]->DetonateBalls();
        }
    }
    else if (c == 'O') // TODO THIS IS EXTRA DEBUG FUNCTIONALITY
    {
        SetLosing();
    }
	 //LHR DEBUG KEY ADDS
    else if (c == 'B') // Next bullet type, this needs work
    {
        mGun->SetNextBulletType(1);
    }
    else if (c == 'D') // Draw stats, I like the slide compared to AX but if you don't change the first true to false, A in ZR but D is cooler
    {
        mApp->DoStatsDialog(true, false);
    }
    else if (c == 'C') // Custom
    {
        gForceTreasureNum++; //Probably a simple variable increment as the original spawns use RNG, For RNG, set this to Rand(3);
        if (gForceTreasure == 2)
            gForceTreasureNum = 0;
        gForceTreasure = true;
    }
    else if (c == 'E') // Extra life, I like the SFX but if you don't comment those lines (ZD Flash)
    {
        mLives += 1;
        mLivesBlinkCount = 150;
        mApp->PlaySample(Sexy::SOUND_EXTRA_LIFE);
        mSoundMgr->AddSound(Sexy::SOUND_EXTRA_LIFE, 30);
        mSoundMgr->AddSound(Sexy::SOUND_EXTRA_LIFE, 60);
    }
    else if (c == 'F') // Add force stage complete (Custom)
    {
        gForceStageComplete = true;
        DoLevelUp(true, true);
    }
    else if (c == 'G') // Game over cheat why not
    {
        mLives = 1; //Set this to 1 so the right sound plays
        SetLosing();
    }
    else if (c == 'K') // Kill frog, I like K more
    {
        SetLosing();
    }
    else if (c == 'L') // Instant win
    {
        //Board::IncScore(mScoreTarget - mScore);
        DoLevelUp(true, true);
    }
    else if (c == 'M' || c == '[') // Prev level
    {
        if (mLevel > 0)
        {
            mStartLevel = mLevel - 1;
            Reset(false, true);
        }
    }
    else if (c == 'N' || c == ']') // Next level
    {
        Board::DoLevelUp(false, true);
        if (mGameState == GameState_LevelUp)
        {
            Reset(false, false);
        }
    }
    else if (c == 'R') // Reset
    {
        Reset(false, true);
    }
    else if (c == '+') // Score up 1000 from ZR Flash
    {
        mScore += 1000;
        //Board::DoLevelUp(false, true);
    }
    /*else if (c == 'U') //AX upsell
    {

    }*/
    else if (c == '|') //Speed up
    {
        gSpeedUp ^= true;
    }
    else if (c == 'W') // Lose life from AX
    {
        mLives -= 1;
        mLivesBlinkCount = 150;
    }
    else if (c == 'X') //Toggle collision detection from AX
    {
        if (gCheckCollision)
        {
            gCheckCollision = false;
        }
        else
        {
            gCheckCollision = true;
        }
    }
    else if (c == 'Z') // Zuma score from Flash
    {
        IncScore(mScoreTarget - mScore);
    }
    else if (c >= '1' && theChar <= '9') //Ported from Flash, why are they detecting 1-9 were there more powers that got cut?
    {
        if (mGameState == GameState_Playing)
        {
            if (c - 49 >= 0 && c - 49 < PowerType_Max)
                mCurveMgr[Rand((int)mCurveMgr.size())]->AddPowerUp((PowerType)(c - 49));
        }   
    }
    else if (c == 127) // Delete highlighted ball
    {
        //todo
    }
    else if (c == 'A') // Color override (Custom)
    {
        if (gColorOverride <= gNumColors)
        {
            gColorOverride += 1;
        }
        else
        {
            gColorOverride = 0;
        }
    }
    else if (c == 'Q') // CANNON OVERRIDE
    {
        if (mGun->GetBullet() != NULL)
            mGun->GetBullet()->SetPowerType(PowerType_CannonShot, false);
    }
#endif
}

void Board::KeyDown(KeyCode theKey)
{
    Widget::KeyDown(theKey);
}

void Board::AddedToManager(WidgetManager *theManager)
{
    Widget::AddedToManager(theManager);

    theManager->AddWidget(mMenuButton);
    theManager->AddWidget(mContinueButton);
    theManager->AddWidget(mOverlayWidget);
}

void Board::RemovedFromManager(WidgetManager *theManager)
{
    Widget::RemovedFromManager(theManager);

    theManager->RemoveWidget(mMenuButton);
    theManager->RemoveWidget(mContinueButton);
    theManager->RemoveWidget(mOverlayWidget);
}

void Board::ButtonPress(int theId)
{
    mApp->PlaySample(Sexy::SOUND_BUTTON1);
}

void Board::ButtonDepress(int theId)
{
    if (theId == 0)
    {
        mApp->DoOptionsDialog();
    }
    else if (theId == 1)
    {
        mIsWinning = true;

        int aStage = mLevelDesc->mStage + 1;

        mContinueButton->SetVisible(false);

        if (aStage == 13)
        {
            mApp->DoStatsDialog(true, true);
            if (!mApp->mProfile->mHasWon)
            {
                mApp->mProfile->mHasWon = true;
                mApp->SaveProfile();
            }
        }
        else if (aStage == 12 || aStage <= mMaxStage)
        {
            Reset(false, false);
        }
        else
        {
            mVerboseLevelString = Sexy::StrFormat("LeVeL %d-%d", mNextLevelDesc->mStage + 1, mNextLevelDesc->mLevel + 1);
            SaveGame(GetSaveGameName(false, mApp->mProfile->mId));
            mApp->ShowAdventureScreen(false, true);
        }
    }
}

void Board::ActivatePower(Ball *theBall)
{
    switch (theBall->GetPowerTypeWussy())
    {
    case PowerType_Bomb:
    {
        int aTicks = Sexy::BoardGetTickCount();
        if (aTicks - mLastExplosionTick > 250)
        {
            mLastExplosionTick = aTicks;
            mApp->PlaySample(Sexy::SOUND_EXPLODE);
        }
        break;
    }
    case PowerType_MoveBackwards:
        mApp->PlaySample(Sexy::SOUND_BACKWARDS_BALL);
        break;
    case PowerType_SlowDown:
        mApp->PlaySample(Sexy::SOUND_SLOWDOWN_BALL);
        break;
    case PowerType_Accuracy:
        mApp->PlaySample(Sexy::SOUND_ACCURACY_BALL);
        mAccuracyCount = 2000;
        DoAccuracy(true);
        break;
    case PowerType_Cannon:
        if (mGun->GetBullet() != NULL)
        {
            mApp->PlaySample(Sexy::SOUND_CANNONPOWER);
            mGun->GetBullet()->SetPowerType(PowerType_CannonShot, false);
        }
        break;
    }

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        mCurveMgr[i]->ActivatePower(theBall);
    }
}

void Board::WaitForLoadingThread()
{
    mApp->mWorkerThread->WaitForTask();
    mApp->ClearUpdateBacklog();
}

void Board::Reset(bool gameOver, bool isLevelReset)
{
    mStateCount = 0;
    bool aSave = false;
    gBallBlink = 0;

    DoAccuracy(false);
    mSoundMgr->KillAllSounds();
    mContinueButton->SetVisible(false);

    WaitForLoadingThread();

    if (mPracticeMode != PracticeMode_Single && !isLevelReset) {
        for (CurveMgr* curve : mCurveMgr) {
            delete curve;
        }
        mCurveMgr.clear();
    }

    mParticleMgr->Clear();
    mTransitionMgr->Clear();
    DeleteBullets();

    if (gameOver)
    {
        mGameStats.Reset();

        mScore = 0;
        mScoreDisplay = 0;
        mCurBarSize = 0;
        mDifficultyDiff = 0;
        mLevel = mStartLevel;
        mLives = 2 * (!mIsEndless) + 1;

        mOldCurveList.clear();
        SyncPracticeMode();

        mApp->DemoAddMarker("NewGame");
        mGun->EmptyBullets();
    }
    else
    {
        if (!isLevelReset)
            mLevel++;

        mApp->DemoAddMarker(Sexy::StrFormat("NewLevel: %d", mLevel));
    }

    mLevelStats.Reset();

    mShowGuide = false;
    mRecalcGuide = false;

    gWasReset = isLevelReset;

    if (isLevelReset)
    {
        GetLevel(mLevel, mLevelDesc, mLevelDesc->mName.c_str());
    }
    else
    {
        if (gameOver)
        {
            SetupNextLevel(mPracticeMode == PracticeMode_Single ? 0 : mLevel);

            if (mPracticeMode == PracticeMode_Single)
            {
                SetLevelToNextLevel();
                mDifficultyDiff = mLevelDesc->mDifficulty;
            }
        }

        if (mPracticeMode == PracticeMode_Single)
        {
            GetLevel(mLevel, mLevelDesc);
            GetLevel(mLevel + 1, mNextLevelDesc);
        }
        else
        {
            SetupNextLevel(mLevel + 1);
        }
    }

    mGun->SetPos(mLevelDesc->mGunX, mLevelDesc->mGunY);
    mBallColorMap.clear();
    mGun->EmptyBullets();

    mDestroyAll = true;
    int aScoreTarget = 0;

    for (int i = 0; i < mLevelDesc->mCurveDesc.size(); i++)
    {
        if (aScoreTarget < mLevelDesc->mCurveDesc[i].mScoreTarget)
            aScoreTarget = mLevelDesc->mCurveDesc[i].mScoreTarget;

        if (!mLevelDesc->mCurveDesc[i].mDestroyAll)
            mDestroyAll = false;
    }

    if (aScoreTarget < 0)
    {
        mScoreTarget = 0;
    }
    else
    {
        mScoreTarget = mScore + aScoreTarget;
    }

    if (mIsEndless)
    {
        mScoreTarget = mScore + 1000;
        mNextDifficultyIncFrame = 3000;
    }

    if (!mPracticeBoard.empty())
    {
        mLevelString = Sexy::StrFormat("%s %d", gSmallGauntletStages[mLevelDesc->mStage], mLevelDesc->mLevel + 1);
        mVerboseLevelString = Sexy::StrFormat("%s %d", gGauntletStages[mLevelDesc->mStage], mLevelDesc->mLevel + 1);
    }
    else
    {
        mLevelString = Sexy::StrFormat("LVL %d-%d", mLevelDesc->mStage + 1, mLevelDesc->mLevel + 1);
        mVerboseLevelString = Sexy::StrFormat("LeVeL %d-%d", mLevelDesc->mStage + 1, mLevelDesc->mLevel + 1);
    }

    mHaveReachedTarget = false;
    mLevelBeginScore = mScore;
    mTargetBarSize = 0;
    mCurTreasure = 0;
    mTreasureEndFrame = 0;
    mTreasureZoomX = 0;
    mTreasureZoomY = 0;
    mTreasureZoomFrame = 0;
    mBarBlinkCount = 0;
    mAccuracyCount = 0;
    mLivesBlinkCount = 0;
    mNumClearsInARow = 0;
    mCurInARowBonus = 0;
    mFlashCount = 0;
    mLevelEndFrame = 0;
    mIsWinning = false;
    mGameState = GameState_LevelBegin;

    mSpriteMgr->ClearHoleFlashes();
    mTransitionMgr->DoLevelBegin(gameOver);

    if (mLevel > mApp->mProfile->mMaxLevel)
    {
        mApp->mProfile->mMaxLevel = mLevel;
        aSave = true;
    }

    if (mPracticeBoard.empty())
    {
        if (mApp->mProfile->UpdateMaxLevel(mLevelDesc->mName, 0))
            aSave = true;

        if (mLevelDesc->mStage > mApp->mProfile->mMaxStage)
        {
            mApp->mProfile->mMaxStage = mLevelDesc->mStage;
            aSave = true;
        }
    }

    if (aSave)
    {
        mApp->SaveProfile();
    }

    mMaxStage = mApp->mProfile->mMaxStage;
}

void Board::Pause(bool pause, bool becauseOfDialog)
{
    if (pause)
    {
        if (mPauseCount == 0)
        {
            mPauseFade = 0;
            mSoundMgr->Mute(true);
        }

        if (becauseOfDialog)
        {
            mDialogCount++;
        }

        mPauseCount++;
    }
    else
    {
        if (becauseOfDialog)
        {
            mDialogCount--;
        }
        mPauseCount--;
    }

    if (mPauseCount == 0)
    {
        mShowBallsDuringPause = false;
        mSoundMgr->Mute(false);
        mPauseUpdateCnt = mUpdateCnt;
        MouseMove(mWidgetManager->mLastMouseX, mWidgetManager->mLastMouseY);
    }

    MarkDirty();
}

void Board::SyncState(DataSync &theSync)
{
    DataReader *aReader = theSync.mReader;
    DataWriter *aWriter = theSync.mWriter;

    mGun->SyncState(theSync);
    mParticleMgr->SyncState(theSync);
    mTransitionMgr->SyncState(theSync);
    mSpriteMgr->SyncState(theSync);

    for (int i = 0; i < mCurveMgr.size(); i++)
    {
        mCurveMgr[i]->SyncState(theSync);
    }

    theSync.SyncLong(mScore);
    theSync.SyncLong(mScoreDisplay);
    theSync.SyncLong(mStateCount);
    theSync.SyncShort(mLives);
    theSync.SyncLong(mCurBarSize);
    theSync.SyncLong(mTargetBarSize);
    theSync.SyncLong(mBarBlinkCount);
    theSync.SyncLong(mFlashCount);
    theSync.SyncLong(mLivesBlinkCount);
    theSync.SyncLong(mAccuracyCount);
    theSync.SyncLong(mDifficultyDiff);

    if (theSync.mReader)
    {
        theSync.SyncBytes(&mGameState, 4);

#ifdef CIRCLE_ENDIAN_SWAP_ENABLED
        mGameState = (GameState)ByteSwap(mGameState);
#endif
    }
    else
    {
#ifdef CIRCLE_ENDIAN_SWAP_ENABLED
        mGameState = (GameState)ByteSwap(mGameState);
#endif

        theSync.SyncBytes(&mGameState, 4);
    }

    theSync.SyncLong(mLastBallClickTick);
    theSync.SyncLong(mLastExplosionTick);
    theSync.SyncBool(mDoGuide);
    theSync.SyncBool(mIsWinning);
    theSync.SyncLong(mTreasureEndFrame);
    theSync.SyncLong(mTreasureZoomX);
    theSync.SyncLong(mTreasureZoomY);
    theSync.SyncLong(mTreasureZoomFrame);
    theSync.SyncLong(mLevelEndFrame);
    theSync.SyncLong(mLevelBeginScore);
    theSync.SyncLong(mScoreTarget);
    theSync.SyncBool(mDestroyAll);
    theSync.SyncLong(mPauseFade);
    theSync.SyncShort(mNumClearsInARow);
    theSync.SyncLong(mCurInARowBonus);
    theSync.SyncBool(mHaveReachedTarget);
    theSync.SyncBool(mLevelBeginning);
    mLevelStats.SyncState(theSync);
    mGameStats.SyncState(theSync);
    theSync.SyncString(mLevelString);
    theSync.SyncString(mVerboseLevelString);
    theSync.SyncLong(mNextDifficultyIncFrame);
    theSync.SyncShort(mMaxStage);
    theSync.SyncBool(mIsEndless);
    theSync.SyncString(mOriginalPracticeBoard);

    if (aReader)
    {
        mGuideBall = NULL;
        mShowGuide = false;
        mRecalcGuide = true;

        DeleteBullets();
        mBallColorMap.clear();

        int aColorMapCount = aReader->ReadShort();
        for (int i = 0; i < aColorMapCount; i++)
        {
            int aColor = aReader->ReadByte();
            int aCount = aReader->ReadShort();
            mBallColorMap[aColor] = aCount;
        }

        int aBulletCount = aReader->ReadShort();
        for (int i = 0; i < aBulletCount; i++)
        {
            Bullet *aBullet = new Bullet();
            for (int j = 0; j < mCurveMgr.size(); j++) {
                aBullet->AddCurCurvePoint(0);
            }
            aBullet->SyncState(theSync);
            mBulletList.push_back(aBullet);
        }

        mCurTreasureNum = (signed char)aReader->ReadByte();
        if (mCurTreasureNum < 0)
        {
            mCurTreasure = 0;
        }
        else
        {
            TreasurePointList::iterator anItr = mLevelDesc->mTreasurePoints.begin();
            std::advance(anItr, mCurTreasureNum);

            mCurTreasure = &*anItr;
        }

        mOldCurveList.clear();
        int aOldCurveCount = aReader->ReadShort();
        for (int i = 0; i < aOldCurveCount; i++)
        {
            std::string aCurveName;
            aReader->ReadString(aCurveName);
            mOldCurveList.push_back(aCurveName);
        }
    }
    else
    {
        aWriter->WriteShort(mBallColorMap.size());
        for (ColorMap::iterator anItr = mBallColorMap.begin();
             anItr != mBallColorMap.end();
             anItr++)
        {
            aWriter->WriteByte(anItr->first);
            aWriter->WriteShort(anItr->second);
        }

        aWriter->WriteShort(mBulletList.size());
        for (BulletList::iterator anItr = mBulletList.begin();
             anItr != mBulletList.end();
             anItr++)
        {
            (*anItr)->SyncState(theSync);
        }

        signed char aCurTreasureNum = mCurTreasure != NULL ? mCurTreasureNum : -1;
        aWriter->WriteByte((uchar)aCurTreasureNum);

        aWriter->WriteShort(mOldCurveList.size());
        for (LevelParser::StringList::iterator anItr = mOldCurveList.begin();
             anItr != mOldCurveList.end();
             anItr++)
        {
            aWriter->WriteString(*anItr);
        }
    }
}

void Board::SaveGame(const std::string &theFilePath)
{
    WaitForLoadingThread();
    DataWriter aWriter;
    // idk what this is for, some debugging leftover?
    // just gets written to in the original code, but never read anywhere.
    int aUnkState = 0;
    aWriter.OpenMemory(0x20);

    StatsDialog *aDialog = reinterpret_cast<StatsDialog *>(mApp->GetDialog(DialogType_Stats));
    if (aDialog)
        aDialog->FinishScoreInc();

    aWriter.WriteLong(gSaveGameVersion);

    Sexy::LevelDesc *aLevelDesc = mIsWinning ? mNextLevelDesc : mLevelDesc;

    aWriter.WriteBool(!mPracticeBoard.empty());
    aWriter.WriteString(mVerboseLevelString);
    aWriter.WriteString(aLevelDesc->mDisplayName);
    aWriter.WriteLong(mScore);
    aWriter.WriteString(mPracticeBoard);
    aWriter.WriteShort(mLevel);
    aWriter.WriteBool(aDialog != NULL);
    aWriter.WriteString(mLevelDesc->mName);
    aWriter.WriteString(mNextLevelDesc->mName);

    {
        DataSync aDataSync(aWriter);
        aUnkState = 1;

        SyncState(aDataSync);
        aDataSync.SyncPointers();
        mApp->WriteBytesToFile(theFilePath, aWriter.mMemoryHandle, aWriter.mMemoryPosition);
        mApp->ClearUpdateBacklog();
        aUnkState = 0;
    }

    aUnkState = -1;
}

// probably not original code, not present in known versions.
void Board::LoadGame(const std::string &theFilePath)
{
    Buffer aBuffer;
    if (mApp->ReadBufferFromFile(theFilePath, &aBuffer))
        LoadGame(aBuffer);
}

void Board::LoadGame(Buffer &theBuffer)
{
    this->mIsSavedGame = true;

    DataReader aReader;
    // Same thing as in Board::SaveGame()
    int aUnkState = 0;
    aReader.OpenMemory(theBuffer.GetDataPtr(), theBuffer.GetDataLen(), false);

    {
        DataSync aDataSync(aReader);
        aUnkState = 2;

        // all of those get their return values ignored for some reason.
        int version = aReader.ReadLong();
        std::string aLevelString;
        aUnkState = 3;

        bool isPractice = aReader.ReadBool();
        aReader.ReadString(aLevelString);
        aReader.ReadString(aLevelString);
        int aScore = aReader.ReadLong();

        aReader.ReadString(mPracticeBoard);
        mLevel = aReader.ReadShort();
        bool isStatsDialogOpen = aReader.ReadBool();

        {
            std::string aLevelName;
            std::string aNextLevelName;
            aUnkState = 5;

            aReader.ReadString(aLevelName);
            aReader.ReadString(aNextLevelName);
            SyncPracticeMode();

            SetupNextLevel(mLevel, aLevelName.c_str());

            if (mPracticeMode == PracticeMode_Single)
            {
                SetLevelToNextLevel();
                GetLevel(mLevel + 1, mNextLevelDesc);
            }
            else
            {
                SetupNextLevel(mLevel + 1, aNextLevelName.c_str());
            }

            SyncState(aDataSync);
            aDataSync.SyncPointers();

            if (mIsWinning && mLevelDesc->mStage < 13)
                Reset(false);

            if (isStatsDialogOpen)
            {
                mApp->DoStatsDialog(false, false);
                mPauseFade = 50;
            }
        }

        aUnkState = 1;
        mApp->ClearUpdateBacklog();
        aUnkState = 0;
    }

    aUnkState = -1;
}

void Board::IncScore(int theInc, bool delayDisplay)
{
    if (theInc <= 0)
        return;

    int aScore = mScore;
    mScore += theInc;
    if (aScore / 50000 < (aScore + theInc) / 50000 && !mIsEndless && !mIsWinning)
    {
        mLives += (aScore + theInc) / 50000 - aScore / 50000;
        GetCircleShootApp()->mProfile->mUserStats.mLiveEarned += (aScore + theInc) / 50000 - aScore / 50000;

        mLivesBlinkCount = 150;
        mApp->PlaySample(Sexy::SOUND_EXTRA_LIFE);
        mSoundMgr->AddSound(Sexy::SOUND_EXTRA_LIFE, 30);
        mSoundMgr->AddSound(Sexy::SOUND_EXTRA_LIFE, 60);
    }

    if (!delayDisplay)
        mScoreDisplay = mScore;
}

bool Board::NeedSaveGame()
{
    if (mIsWinning)
    {
        return mApp->GetDialog(DialogType_Stats) != NULL;
    }

    if (mGameState == GameState_None)
    {
        return false;
    }

    if (mGameState == GameState_Losing)
    {
        return mLives > 0;
    }

    return mScore > 0;
}

bool Board::IsGameOver()
{
    return mGameState == GameState_Losing && mLives <= 0 || mIsWinning;
}

int Board::GetCurrentStage()
{
    return mLevelDesc->mStage;
}

int Board::GetParBonus(int theLevelTime, int theParTime)
{
    if (theLevelTime > theParTime)
    {
        return 0;
    }

    return 100 * int(((1.0f - ((float)theLevelTime / theParTime)) * 25000.0f) / 100) + 100;
}

void Board::ResetInARowBonus()
{
    if (mNumClearsInARow > mLevelStats.mMaxInARow)
    {
        mLevelStats.mMaxInARow = mNumClearsInARow;
        mLevelStats.mMaxInARowScore = mCurInARowBonus;
    }

    mNumClearsInARow = 0;
    mCurInARowBonus = 0;
}

std::string Board::GetUniqueLevelString()
{
    std::string result;
    if (mIsEndless)
        result += "e";

    if (mPracticeBoard.empty())
    {
        result += Sexy::StrFormat("a_%d", mLevel);
    }
    else
    {
        result += Sexy::StrFormat("p_%s_%d_%d", mLevelDesc->mName.c_str(), mLevelDesc->mDifficulty, mLevelDesc->mCurveDesc[0].mNumColors);
    }

    return result;
}

void Board::DoSetupNextLevel()
{
    WaitForLoadingThread();
    mLoadingThreadParam = 0;
    mApp->mWorkerThread->DoTask(Board::StaticLoadProc, (void *)this);
}

void Board::DoGenerateBackgroundTransitions()
{
    WaitForLoadingThread();
    mLoadingThreadParam = 1;

    mApp->mWorkerThread->DoTask(Board::StaticLoadProc, (void *)this);
}

void Board::SetLevelToNextLevel()
{
    WaitForLoadingThread();

    // LevelDesc *anOldLevelDesc = mLevelDesc;
    // mLevelDesc = mNextLevelDesc;
    // mNextLevelDesc = anOldLevelDesc;
    std::swap(mLevelDesc, mNextLevelDesc);
        // CurveMgr *aCurveMgr = mCurveMgr[i];
        // mCurveMgr[i] = mNextCurveMgr[i];
        // mNextCurveMgr[i] = aCurveMgr;
    std::swap(mCurveMgr, mNextCurveMgr);
    //for (int i = 0; i < mNextCurveMgr.size(); i++) {
        //mCurveMgr.at(i)->StartLevel();
    //}

    /*for (int i = 0; i < mLevelDesc->GetNumCurves(); i++)
    {
        CurveMgr* curve = new CurveMgr(this);
        curve->SetupLevel(mLevelDesc, mSpriteMgr, i);
        curve->StartLevel();
        mCurveMgr.push_back(curve);
    }*/

    // SpriteMgr *aOldSprMgr = mSpriteMgr;
    // mSpriteMgr = mNextSpriteMgr;
    // mNextSpriteMgr = aOldSprMgr;
    std::swap(mSpriteMgr, mNextSpriteMgr);
}

void Board::GetLevel(int theLevel, LevelDesc *theLevelDesc, const char *theLevelName)
{
    int aNumColors = 0;

    if (!mPracticeBoard.empty())
    {
        int aLevel = theLevel % 7;
        int aStage = theLevel / 7;
        int aDifficulty = mDifficultyDiff + aLevel + 2 * aStage;

        for (aNumColors = aStage + 4; aNumColors > 6; --aNumColors)
            aDifficulty += 5;

        if (theLevelName != NULL)
        {
            *theLevelDesc = mApp->mLevelParser->GetLevelByName(theLevelName, aDifficulty);
        }
        else if (mPracticeMode == PracticeMode_Single)
        {
            *theLevelDesc = mApp->mLevelParser->GetLevelByName(mPracticeBoard, aDifficulty);
        }
        else if (mPracticeMode == PracticeMode_Random)
        {
            int aShuffleCount = mApp->mProfile->mMaxBoardLevel.size() - 5;
            if (aShuffleCount > 10)
            {
                aShuffleCount = 10;
            }
            else if (aShuffleCount < 1)
            {
                aShuffleCount = 1;
            }

            while ((int)mOldCurveList.size() > aShuffleCount)
            {
                mOldCurveList.pop_back();
            }

            *theLevelDesc = mApp->mLevelParser->GetRandomBoard(aDifficulty, theLevel, mApp->mProfile, mOldCurveList);
            mOldCurveList.push_front(theLevelDesc->mName);

            if (mIsEndless)
            {
                mPracticeBoard = theLevelDesc->mName;
                mPracticeMode = PracticeMode_Single;
            }
        }
        else
        {
            *theLevelDesc = mApp->mLevelParser->GetSequenceBoard(aDifficulty, theLevel, mApp->mProfile);
        }

        while (aStage > 3)
        {
            aStage--;
            aLevel += 7;
        }

        theLevelDesc->mStage = aStage;
        theLevelDesc->mLevel = aLevel;
        theLevelDesc->mParTime += 10 * aNumColors - 40;
    }
    else
    {
        *theLevelDesc = mApp->mLevelParser->GetStageLevel(theLevel);
        aNumColors = theLevelDesc->mStage / 3 + 4;

        if (aNumColors > 6)
            aNumColors = 6;
    }

    for (int i = 0; i < theLevelDesc->GetNumCurves(); i++)
    {
        theLevelDesc->mCurveDesc[i].mNumColors = aNumColors;
    }
}

void Board::SetupNextLevel(int theLevel, const char *theLevelName)
{
    WaitForLoadingThread();
    SetLevelToNextLevel();
    GetLevel(theLevel, mNextLevelDesc, theLevelName);
    DoSetupNextLevel();
}

void Board::DeleteBullets()
{
    for (BulletList::iterator i = mBulletList.begin(); i != mBulletList.end(); i++)
    {
        delete *i;
    }

    mBulletList.clear();
}

void Board::CheckHighScore() {}

void Board::CreateThumbnail(SpriteMgr *theSpriteMgr, LevelDesc *theLevelDesc) {}