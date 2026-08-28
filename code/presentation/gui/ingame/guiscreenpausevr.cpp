#include <presentation/gui/ingame/guiscreenpausevr.h>
#include <presentation/gui/guimenu.h>
#include <vr/openxrmanager.h>

#include <raddebug.hpp>
#include <cstdio>
#include <Screen.h>
#include <Page.h>
#include <Layer.h>
#include <Group.h>
#include <Text.h>
#include <Sprite.h>
#include <FePage.h>
#include <FeGroup.h>
#include <FeText.h>
#include <FeSprite.h>

namespace
{
const char* const RowNames[7]={"Camera","JumpCamera","IntersectNavSystem","Radar","Tutorial","VrHud","Foveation"};
const char* const Labels[7]={"Mode","Seated Mode","Turn Mode","Turn Speed","Vehicle Control","Developer Menus","Foveation"};
const float SmoothSpeeds[5]={45.0f,90.0f,120.0f,180.0f,240.0f};
const float SnapAngles[5]={15.0f,30.0f,45.0f,60.0f,90.0f};
const int FoveationRow=6;

// Row 3 shows a number the left/right arrows step through rather than a list
// of named values, so it declares a single value.
int ValueCount(int row)
{
    switch(row)
    {
        case 3: return 1;
        case 4: return 3;
        case FoveationRow: return 4;
        default: return 2;
    }
}

// Applies a row's value strings through whichever of the two text APIs suits
// the caller: freshly created rows have to add strings, authored ones already
// have slots to overwrite.
template<typename SetString>
void FillValues(int row, SetString set)
{
    switch(row)
    {
        case 0: set(0,"Original"); set(1,"VR"); break;
        case 2: set(0,"Smooth"); set(1,"Snap"); break;
        case 3: set(0,"120"); break;
        case 4: set(0,"Stick"); set(1,"VR Wheel"); set(2,"Third Person"); break;
        case FoveationRow:
            set(0,"Off"); set(1,"Low"); set(2,"Medium"); set(3,"High"); break;
        default: set(0,"Off"); set(1,"On"); break;
    }
}

int Closest(const float* values,float value)
{
    int best=0;
    for(int i=1;i<5;++i)
        if(rmt::Fabs(values[i]-value)<rmt::Fabs(values[best]-value)) best=i;
    return best;
}
}

CGuiScreenPauseVR::CGuiScreenPauseVR(Scrooby::Screen* screen,CGuiEntity* parent)
: CGuiScreen(screen,parent,GUI_SCREEN_ID_VR),m_pMenu(NULL),m_pPage(NULL),
  m_numRows(5),m_frontendLayout(false)
{
    for(int i=0;i<6;++i){ m_pRows[i]=NULL; m_pValues[i]=NULL; }
    m_numericValues[0]=Closest(SmoothSpeeds,SharOpenXR::GetSmoothTurnSpeed());
    m_numericValues[1]=Closest(SnapAngles,SharOpenXR::GetSnapTurnAngle());
    m_pPage=m_pScroobyScreen->GetPage("PauseSettings");
    const bool frontendLayout=m_pPage==NULL;
    m_frontendLayout=frontendLayout;
    // The Foveation row is only meaningful where the runtime supports it, and
    // it is last, so leaving it out shifts nothing else.
    m_numRows=frontendLayout?(SharOpenXR::IsFoveationAvailable()?7:6):5;
    if(frontendLayout) m_pPage=m_pScroobyScreen->GetPage("Controller");
    rAssert(m_pPage);
    Scrooby::Group* menuGroup=m_pPage->GetGroup("Menu");
    m_pMenu=new CGuiMenu(this,m_numRows,GUI_TEXT_MENU,MENU_SFX_NONE);
    rAssert(m_pMenu);
    if(frontendLayout)
    {
        // frontend.p3d has no PauseSettings page.  Build an independent VR
        // menu on the Controller page instead of treating its incomplete
        // Configuration_Value field as a real settings row.
        FePage* page=dynamic_cast<FePage*>(m_pPage);
        Scrooby::Text* styleLabel=menuGroup?menuGroup->GetText("Display"):NULL;
        FeText* style=dynamic_cast<FeText*>(styleLabel);
        rAssert(page && styleLabel && style);
        if(menuGroup) menuGroup->SetVisible(false);
        // Controller is only a canvas for the Android VR settings screen.
        // Hide the controller diagram and its platform-specific text pages;
        // otherwise they are drawn underneath the dynamically-created rows.
        const char* const controllerPages[]={
            "ControllerPC","ControllerImage","ControllerXBOX",
            "ControllerPS2","ControllerGC"
        };
        for(unsigned int pageIndex=0;
            pageIndex<sizeof(controllerPages)/sizeof(controllerPages[0]);
            ++pageIndex)
        {
            Scrooby::Page* controllerPage=
                m_pScroobyScreen->GetPage(controllerPages[pageIndex]);
            if(!controllerPage) continue;
            for(int layerIndex=0;
                layerIndex<controllerPage->GetNumberOfLayers();++layerIndex)
            {
                Scrooby::Layer* layer=controllerPage->GetLayerByIndex(layerIndex);
                if(layer) layer->SetVisible(false);
            }
        }
        int styleW,styleH;
        styleLabel->GetBoundingBoxSize(styleW,styleH);
        // Hold the authored vertical band fixed as rows are added, so an
        // extra row tightens the spacing instead of running off the page.
        const int firstY=135;
        const int lastY=395;
        const int spacing=(m_numRows>1)?((lastY-firstY)/(m_numRows-1)):0;
        for(int i=0;i<m_numRows;++i)
        {
            FeGroup* row=page->AddGroup(RowNames[i]);
            const int y=firstY+i*spacing;
            FeText* label=row->AddText(RowNames[i],120,0);
            label->SetBoundingBoxSize(230,styleH);
            label->SetTextStyle(style->GetTextStyleResourceId());
            label->SetHorizontalJustification(Scrooby::Left);
            label->SetVerticalJustification(styleLabel->GetVerticalJustification());
            label->SetTextMode(style->GetTextMode());
            label->SetColour(styleLabel->GetColour());
            label->SetDisplayOutline(true);
            label->SetOutlineColour(tColour(0,0,0,192));
            label->AddHardCodedString(Labels[i]);
            char valueName[64];
            std::sprintf(valueName,"%s_Value",RowNames[i]);
            FeText* value=row->AddText(valueName,350,0);
            value->SetBoundingBoxSize(180,styleH);
            value->SetTextStyle(style->GetTextStyleResourceId());
            value->SetHorizontalJustification(Scrooby::Center);
            value->SetVerticalJustification(styleLabel->GetVerticalJustification());
            value->SetTextMode(style->GetTextMode());
            value->SetColour(styleLabel->GetColour());
            value->SetDisplayOutline(true);
            value->SetOutlineColour(tColour(0,0,0,192));
            FillValues(i,[value](int,const char* text){ value->AddHardCodedString(text); });
            row->Show();
            row->SetPosition(0,y);
            m_pRows[i]=row;
        }
    }
    for(int i=0;i<m_numRows;++i)
    {
        const char* rowName=RowNames[i];
        if(!frontendLayout) m_pRows[i]=m_pPage->GetGroup(rowName);
        if(!m_pRows[i] && menuGroup) m_pRows[i]=menuGroup->GetGroup(rowName);
        if(!m_pRows[i]) continue;
        Scrooby::Text* label=m_pRows[i]->GetText(rowName);
        char valueName[64],leftName[64],rightName[64];
        std::sprintf(valueName,"%s_Value",rowName);
        std::sprintf(leftName,"%s_LArrow",rowName);
        std::sprintf(rightName,"%s_RArrow",rowName);
        Scrooby::Text* value=m_pRows[i]->GetText(valueName);
        Scrooby::Sprite* left=m_pRows[i]->GetSprite(leftName);
        Scrooby::Sprite* right=m_pRows[i]->GetSprite(rightName);
        if(!label || !value) continue;

        // Every VR row uses Tutorial as the single visual template.  Preserve
        // each row's vertical coordinate, but standardize horizontal origins,
        // boxes, font metrics, justification and arrow geometry.
        Scrooby::Group* styleRow=m_pPage->GetGroup("Tutorial");
        Scrooby::Text* styleLabel=styleRow?styleRow->GetText("Tutorial"):NULL;
        Scrooby::Text* styleValue=styleRow?styleRow->GetText("Tutorial_Value"):NULL;
        FeText* labelImpl=dynamic_cast<FeText*>(label);
        FeText* valueImpl=dynamic_cast<FeText*>(value);
        FeText* styleLabelImpl=dynamic_cast<FeText*>(styleLabel);
        FeText* styleValueImpl=dynamic_cast<FeText*>(styleValue);
        if(labelImpl && valueImpl && styleLabelImpl && styleValueImpl)
        {
            int x,y,w,h,ownX,ownY;
            label->GetOriginPosition(ownX,ownY);
            styleLabel->GetOriginPosition(x,y); styleLabel->GetBoundingBoxSize(w,h);
            label->SetPosition(x,ownY); label->SetBoundingBoxSize(w,h);
            labelImpl->SetTextStyle(styleLabelImpl->GetTextStyleResourceId());
            label->SetHorizontalJustification(Scrooby::Left);
            label->SetVerticalJustification(styleLabel->GetVerticalJustification());
            value->GetOriginPosition(ownX,ownY);
            styleValue->GetOriginPosition(x,y); styleValue->GetBoundingBoxSize(w,h);
            value->SetPosition(x,ownY); value->SetBoundingBoxSize(w,h);
            valueImpl->SetTextStyle(styleValueImpl->GetTextStyleResourceId());
            value->SetHorizontalJustification(Scrooby::Center);
            value->SetVerticalJustification(styleValue->GetVerticalJustification());
        }
        const char* arrowSuffix[2]={"LArrow","RArrow"};
        Scrooby::Sprite* arrows[2]={left,right};
        for(int a=0;a<2;++a)
        {
            char templateName[32];
            std::sprintf(templateName,"Tutorial_%s",arrowSuffix[a]);
            Scrooby::Sprite* styleArrow=styleRow?styleRow->GetSprite(templateName):NULL;
            if(!arrows[a] || !styleArrow) continue;
            int x,y,w,h,ownX,ownY;
            arrows[a]->GetOriginPosition(ownX,ownY);
            styleArrow->GetOriginPosition(x,y); styleArrow->GetBoundingBoxSize(w,h);
            arrows[a]->SetPosition(x,ownY);
            arrows[a]->SetBoundingBoxSize(w,h);
            arrows[a]->SetHorizontalJustification(styleArrow->GetHorizontalJustification());
            arrows[a]->SetVerticalJustification(styleArrow->GetVerticalJustification());
            FeSprite* arrowImpl=dynamic_cast<FeSprite*>(arrows[a]);
            FeSprite* styleArrowImpl=dynamic_cast<FeSprite*>(styleArrow);
            if(arrowImpl && styleArrowImpl)
            {
                arrows[a]->ResetTransformation();
                arrowImpl->ReplaceImagesFrom(*styleArrowImpl);
                arrows[a]->SetBoundingBoxSize(w,h);
            }
        }
        m_pValues[i]=value;
        // This Scrooby page is shared with the legacy Settings screen, whose
        // menu may already have left its selected row yellow. Normalize the
        // authored drawables before our menu records their default style.
        label->SetColour(tColour(255,255,255));
        value->SetColour(tColour(255,255,255));
        label->SetDisplayOutline(true);
        value->SetDisplayOutline(true);
        label->SetOutlineColour(tColour(0,0,0,192));
        value->SetOutlineColour(tColour(0,0,0,192));
        label->SetTextMode(Scrooby::TEXT_WRAP);
        value->SetTextMode(Scrooby::TEXT_WRAP);
        if(i==4 && value->GetNumOfStrings()<3)
        {
            FeText* valueText=dynamic_cast<FeText*>(value);
            if(valueText) valueText->AddHardCodedString("Third Person");
        }
        label->SetString(0,Labels[i]);
        FillValues(i,[value](int slot,const char* text){ value->SetString(slot,text); });
        m_pMenu->AddMenuItem(label,value,NULL,NULL,left,right,
                             SELECTION_ENABLED|VALUES_WRAPPED|TEXT_OUTLINE_ENABLED);
        m_pMenu->SetSelectionValueCount(i,ValueCount(i));
    }

    SetVrLayoutVisible(false);
}

CGuiScreenPauseVR::~CGuiScreenPauseVR(){ delete m_pMenu; m_pMenu=NULL; }

void CGuiScreenPauseVR::HandleMessage(eGuiMessage message,unsigned int param1,unsigned int param2)
{
    if(m_state==GUI_WINDOW_STATE_RUNNING)
    {
        if(m_frontendLayout)
        {
            // The authored Controller screen has the opposite vertical axis
            // convention to the frontend options menus.
            if(message==GUI_MSG_CONTROLLER_UP) message=GUI_MSG_CONTROLLER_DOWN;
            else if(message==GUI_MSG_CONTROLLER_DOWN) message=GUI_MSG_CONTROLLER_UP;
        }
        if((message==GUI_MSG_CONTROLLER_UP || message==GUI_MSG_CONTROLLER_DOWN) &&
           SharOpenXR::IsHorizontalMenuInputDominant())
        {
            // A diagonal Touch stick deflection must change a value or move
            // between rows, never do both during the same input gesture.
            message=GUI_MSG_UPDATE;
            param1=0;
        }
        if((message==GUI_MSG_CONTROLLER_LEFT || message==GUI_MSG_CONTROLLER_RIGHT) &&
           SharOpenXR::IsVerticalMenuInputDominant())
        {
            message=GUI_MSG_UPDATE;
            param1=0;
        }
        if((message==GUI_MSG_CONTROLLER_LEFT || message==GUI_MSG_CONTROLLER_RIGHT) &&
           m_pMenu && m_pMenu->GetSelection()==3)
        {
            const int row=3;
            int& value=m_numericValues[SharOpenXR::IsSnapTurnEnabled()?1:0];
            value+=(message==GUI_MSG_CONTROLLER_RIGHT)?1:-1;
            if(value<0) value=4;
            if(value>4) value=0;
            if(SharOpenXR::IsSnapTurnEnabled()) SharOpenXR::SetSnapTurnAngle(SnapAngles[value]);
            else SharOpenXR::SetSmoothTurnSpeed(SmoothSpeeds[value]);
            UpdateNumericValue(row);
            message=GUI_MSG_UPDATE;
            param1=0;
        }
        if(message==GUI_MSG_CONTROLLER_START) m_pParent->HandleMessage(GUI_MSG_UNPAUSE_INGAME);
        else if(message==GUI_MSG_MENU_SELECTION_VALUE_CHANGED)
        {
            if(param1==0)
            {
                SharOpenXR::SetVrModeEnabled(param2!=0);
            }
            else if(param1==1) SharOpenXR::SetSeatedMode(param2==1);
            else if(param1==2)
            {
                SharOpenXR::SetSnapTurnEnabled(param2==1);
                UpdateNumericValue(3);
            }
            else if(param1==3 && param2<5) SharOpenXR::SetSmoothTurnSpeed(SmoothSpeeds[param2]);
            else if(param1==4) SharOpenXR::SetVehicleControlMode(static_cast<int>(param2));
            else if(param1==5) SharOpenXR::SetDeveloperMenusEnabled(param2==1);
            else if(param1==FoveationRow)
                SharOpenXR::SetFoveationLevel(static_cast<int>(param2));
        }
        if(m_pMenu)
        {
            m_pMenu->HandleMessage(message,param1,param2);
            // Only the currently selected setting is adjustable, so only its
            // arrows should be visible.  Do this explicitly because these
            // authored groups are shared with PauseSettings.
            const int selected=m_pMenu->GetSelection();
            for(int i=0;i<m_numRows;++i)
            {
                if(!m_pRows[i]) continue;
                char name[64];
                std::sprintf(name,"%s_LArrow",RowNames[i]);
                Scrooby::Sprite* arrow=m_pRows[i]->GetSprite(name);
                if(arrow) arrow->SetVisible(i==selected);
                std::sprintf(name,"%s_RArrow",RowNames[i]);
                arrow=m_pRows[i]->GetSprite(name);
                if(arrow) arrow->SetVisible(i==selected);
            }
        }
    }
    CGuiScreen::HandleMessage(message,param1,param2);
}

void CGuiScreenPauseVR::SetVrLayoutVisible(bool visible)
{
    for(int i=0;i<m_numRows;++i)
        SetRowVisible(i,visible);
}

void CGuiScreenPauseVR::SetRowVisible(int row,bool visible)
{
        if(row<0 || row>=m_numRows || !m_pRows[row]) return;
        const char* rowName=RowNames[row];
        m_pRows[row]->SetVisible(visible);
        // PauseSettings and VR share the authored row groups, so restore each
        // child explicitly when this screen becomes active.
        Scrooby::Text* label=m_pRows[row]->GetText(rowName);
        if(label) label->SetVisible(visible);
        if(m_pValues[row]) m_pValues[row]->SetVisible(visible);
        char name[64];
        std::sprintf(name,"%s_LArrow",rowName);
        Scrooby::Sprite* left=m_pRows[row]->GetSprite(name);
        const bool showArrows=visible && m_pMenu && m_pMenu->GetSelection()==row;
        if(left) left->SetVisible(showArrows);
        std::sprintf(name,"%s_RArrow",rowName);
        Scrooby::Sprite* right=m_pRows[row]->GetSprite(name);
        if(right) right->SetVisible(showArrows);
}

void CGuiScreenPauseVR::UpdateNumericValue(int row)
{
    if(row!=3 || !m_pValues[row]) return;
    const bool snap=SharOpenXR::IsSnapTurnEnabled();
    Scrooby::Text* label=m_pRows[row]?m_pRows[row]->GetText(RowNames[row]):NULL;
    if(label) label->SetString(0,snap?"Snap Angle":"Smooth Speed");
    char text[16];
    const float value=snap?SnapAngles[m_numericValues[1]]:SmoothSpeeds[m_numericValues[0]];
    std::sprintf(text,"%.0f",value);
    m_pValues[row]->SetString(0,text);
    m_pValues[row]->SetIndex(0);
}

void CGuiScreenPauseVR::InitIntro()
{
    SetVrLayoutVisible(true);
    m_pMenu->SetSelectionValue(0,SharOpenXR::IsVrModeEnabled()?1:0);
    m_pMenu->SetSelectionValue(1,SharOpenXR::IsSeatedMode()?1:0);
    m_pMenu->SetSelectionValue(2,SharOpenXR::IsSnapTurnEnabled()?1:0);
    m_pMenu->SetSelectionValue(4,SharOpenXR::GetVehicleControlMode());
    if(m_frontendLayout)
        m_pMenu->SetSelectionValue(5,SharOpenXR::IsDeveloperMenusEnabled()?1:0);
    if(m_numRows>FoveationRow)
        m_pMenu->SetSelectionValue(FoveationRow,SharOpenXR::GetFoveationLevel());
    m_numericValues[0]=Closest(SmoothSpeeds,SharOpenXR::GetSmoothTurnSpeed());
    m_numericValues[1]=Closest(SnapAngles,SharOpenXR::GetSnapTurnAngle());
    UpdateNumericValue(3);
}
void CGuiScreenPauseVR::InitRunning(){}
void CGuiScreenPauseVR::InitOutro()
{
    SetVrLayoutVisible(false);
}
