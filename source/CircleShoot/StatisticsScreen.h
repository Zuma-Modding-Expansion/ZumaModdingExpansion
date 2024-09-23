#ifndef __STATISTICSSCREEN_H__
#define __STATISTICSSCREEN_H__

#include <SexyAppFramework/Widget.h>
#include <SexyAppFramework/HyperlinkWidget.h>
#include <SexyAppFramework/ButtonListener.h>
#include <SexyAppFramework/CheckboxListener.h>

namespace Sexy
{
    class CircleButton;
    class Image;

    class StatisticsScreen : public Widget, public ButtonListener
    {
    private:
    public:
        StatisticsScreen();
        virtual ~StatisticsScreen();

        virtual void Draw(Graphics* g);
        virtual void Update();
        virtual void Resize(int x, int y, int theWidth, int theHeight);
        virtual void AddedToManager(WidgetManager* theWidgetManager);
        virtual void RemovedFromManager(WidgetManager* theWidgetManager);
        virtual void ButtonDepress(int theId);

        Image* mBackgroundImage;
        CircleButton* mMainMenuButton;
    };

};

#endif