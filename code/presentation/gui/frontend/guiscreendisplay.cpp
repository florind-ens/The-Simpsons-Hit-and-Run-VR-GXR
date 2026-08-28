//===========================================================================
// Copyright (C) 2003 Radical Entertainment Ltd.  All rights reserved.
//
// Component:   CGuiScreenDisplay
//
// Description: Implementation of the CGuiScreenDisplay class.
//
// Authors:     Tony Chu
//
// Revisions		Date			Author	    Revision
//                  2003/06/16      TChu        Created for SRR2
//
//===========================================================================

//===========================================================================
// Includes
//===========================================================================
#include <presentation/gui/frontend/guiscreendisplay.h>
#include <presentation/gui/guimenu.h>

#include <data/config/gameconfigmanager.h>
#include <main/win32platform.h>
#include <memory/srrmemory.h>
#include <render/RenderFlow/renderflow.h>
#ifdef RAD_ANDROID
#include <vr/openxrmanager.h>
#endif

#include <raddebug.hpp> // Foundation
#include <Screen.h>
#include <Page.h>
#include <Group.h>
#include <Text.h>
#ifdef RAD_ANDROID
#include <Sprite.h>
#include <FePage.h>
#include <FeGroup.h>
#include <FeText.h>
#include <FeSprite.h>
#endif

//===========================================================================
// Global Data, Local Data, Local Classes
//===========================================================================


const char* DISPLAY_MENU_ITEMS[] =
{
    "Resolution",
    "ColourDepth",
    "DisplayMode",
    "Gamma",
    "ApplyChanges",

    ""
};

const float SLIDER_ICON_SCALE = 0.5f;

//===========================================================================
// Public Member Functions
//===========================================================================

//===========================================================================
// CGuiScreenDisplay::CGuiScreenDisplay
//===========================================================================
// Description: Constructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CGuiScreenDisplay::CGuiScreenDisplay
(
    Scrooby::Screen* pScreen,
    CGuiEntity* pParent
)
:   CGuiScreen( pScreen, pParent, GUI_SCREEN_ID_DISPLAY ),
    m_pMenu( NULL ),
    m_changedGamma( false )
#ifdef RAD_ANDROID
    , m_pRenderScaleLabel( NULL )
    , m_pRefreshRateLabel( NULL )
#endif
{
MEMTRACK_PUSH_GROUP( "CGuiScreenDisplay" );
    // Retrieve the Scrooby drawing elements.
    //
    Scrooby::Page* pPage = m_pScroobyScreen->GetPage( "Display" );
    rAssert( pPage != NULL );

    // Create a menu.
    //
    m_pMenu = new CGuiMenu( this, NUM_MENU_ITEMS );
    rAssert( m_pMenu != NULL );

    // Add menu items
    //
#ifdef RAD_ANDROID
    // OpenXR owns the resolution, colour depth and fullscreen state on Quest.
    // Keep Display as a real settings screen, but populate it with the
    // platform-specific graphics option instead of the inapplicable PC rows.
    Scrooby::Group* menuGroup = pPage->GetGroup( "Menu" );
    Scrooby::Group* csmGroup = pPage->GetGroup( "DisplayMode" );
    if( csmGroup == NULL && menuGroup != NULL )
    {
        csmGroup = menuGroup->GetGroup( "DisplayMode" );
    }
    rAssert( csmGroup != NULL );

    Scrooby::Text* csmLabel = csmGroup->GetText( "DisplayMode" );
    Scrooby::Text* csmValue = csmGroup->GetText( "DisplayMode_Value" );
    rAssert( csmLabel != NULL && csmValue != NULL );
    csmLabel->SetString( 0, "CSM Shadows" );
    csmValue->SetString( 0, "Off" );
    csmValue->SetString( 1, "On" );

    m_pMenu->AddMenuItem( csmLabel,
                          csmValue,
                          NULL,
                          NULL,
                          csmGroup->GetSprite( "DisplayMode_ArrowL" ),
                          csmGroup->GetSprite( "DisplayMode_ArrowR" ),
                          SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
    m_pMenu->SetSelectionValueCount( MENU_ITEM_CSM, 2 );

    Scrooby::Group* materialsGroup = pPage->GetGroup( "ColourDepth" );
    if( materialsGroup == NULL && menuGroup != NULL ) materialsGroup = menuGroup->GetGroup( "ColourDepth" );
    rAssert( materialsGroup != NULL );
    Scrooby::Text* materialsLabel = materialsGroup->GetText( "ColourDepth" );
    Scrooby::Text* materialsValue = materialsGroup->GetText( "ColourDepth_Value" );
    rAssert( materialsLabel != NULL && materialsValue != NULL );
    materialsLabel->SetString( 0, "Enhanced Materials" );
    materialsValue->SetString( 0, "Off" );
    materialsValue->SetString( 1, "On" );
    m_pMenu->AddMenuItem( materialsLabel,
                          materialsValue,
                          NULL,
                          NULL,
                          materialsGroup->GetSprite( "ColourDepth_ArrowL" ),
                          materialsGroup->GetSprite( "ColourDepth_ArrowR" ),
                          SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
    m_pMenu->SetSelectionValueCount( MENU_ITEM_ENHANCED_MATERIALS, 2 );

    Scrooby::Group* gtaoGroup = pPage->GetGroup( "Resolution" );
    if( gtaoGroup == NULL && menuGroup != NULL ) gtaoGroup = menuGroup->GetGroup( "Resolution" );
    rAssert( gtaoGroup != NULL );
    Scrooby::Text* gtaoLabel = gtaoGroup->GetText( "Resolution" );
    Scrooby::Text* gtaoValue = gtaoGroup->GetText( "Resolution_Value" );
    rAssert( gtaoLabel != NULL && gtaoValue != NULL );
    gtaoLabel->SetString( 0, "Vehicle Lights" );
    gtaoValue->SetString( 0, "Off" );
    gtaoValue->SetString( 1, "Optimized" );
    gtaoValue->SetString( 2, "Max" );
    m_pMenu->AddMenuItem( gtaoLabel,
                          gtaoValue,
                          NULL,
                          NULL,
                          gtaoGroup->GetSprite( "Resolution_ArrowL" ),
                          gtaoGroup->GetSprite( "Resolution_ArrowR" ),
                          SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
    m_pMenu->SetSelectionValueCount( MENU_ITEM_VEHICLE_LIGHTS, 3 );

    // ApplyChanges is authored above the Gamma row in PauseDisplay. Add it
    // first so controller selection order follows the visible top-to-bottom
    // order instead of jumping down to Render Scale and back up.
    if( menuGroup != NULL )
    {
        Scrooby::Text* applyText = menuGroup->GetText( "ApplyChanges" );
        if( applyText != NULL )
        {
            applyText->SetString( 0, "Refresh Rate" );
            applyText->SetVisible( true );

            Scrooby::Text* refreshValue = NULL;
            Scrooby::Text* refreshLabel = applyText;
            Scrooby::Sprite* refreshLeft = NULL;
            Scrooby::Sprite* refreshRight = NULL;
            FePage* pageImpl = dynamic_cast<FePage*>( pPage );
            FeText* valueTemplate = dynamic_cast<FeText*>( csmValue );
            if( pageImpl != NULL && valueTemplate != NULL )
            {
                int labelX,labelY,valueX,valueY,valueW,valueH;
                int materialsX,materialsY,templateLabelX,templateLabelY;
                int templateLabelW,templateLabelH;
                applyText->GetOriginPosition( labelX, labelY );
                materialsGroup->GetOriginPosition( materialsX, materialsY );
                materialsLabel->GetOriginPosition( templateLabelX, templateLabelY );
                materialsLabel->GetBoundingBoxSize( templateLabelW, templateLabelH );
                materialsValue->GetOriginPosition( valueX, valueY );
                materialsValue->GetBoundingBoxSize( valueW, valueH );
                int refreshLabelY=labelY;
                Scrooby::Group* gammaLayoutGroup=pPage->GetGroup("Gamma");
                if(gammaLayoutGroup==NULL) gammaLayoutGroup=menuGroup->GetGroup("Gamma");
                Scrooby::Text* gammaLayoutLabel=gammaLayoutGroup?
                    gammaLayoutGroup->GetText("Gamma"):NULL;
                if(gammaLayoutLabel)
                {
                    int gammaX,gammaY,gammaLabelX,gammaLabelY;
                    gammaLayoutGroup->GetOriginPosition(gammaX,gammaY);
                    gammaLayoutLabel->GetOriginPosition(gammaLabelX,gammaLabelY);
                    // The new group is page-owned, just like Gamma. Use its
                    // page coordinates directly and one fixed authored row;
                    // do not add Menu's transform or retain ApplyChanges' Y.
                    refreshLabelY=gammaY+gammaLabelY-52;
                }

                FeGroup* controls = pageImpl->AddGroup( "RefreshRateControls" );
                FeText* labelTemplate = dynamic_cast<FeText*>( csmLabel );
                if( labelTemplate != NULL )
                {
                    FeText* label = controls->AddText( "RefreshRate_Label",
                                                       materialsX + templateLabelX,
                                                       refreshLabelY );
                    label->SetBoundingBoxSize( templateLabelW, templateLabelH );
                    label->SetTextStyle( labelTemplate->GetTextStyleResourceId() );
                    label->SetHorizontalJustification( csmLabel->GetHorizontalJustification() );
                    label->SetVerticalJustification( csmLabel->GetVerticalJustification() );
                    label->SetTextMode( labelTemplate->GetTextMode() );
                    label->SetColour( materialsLabel->GetColour() );
                    label->SetDisplayOutline( labelTemplate->IsDisplayingOutline() );
                    label->SetOutlineColour( labelTemplate->GetOutlineColour() );
                    label->AddHardCodedString( "Refresh Rate" );
                    refreshLabel = label;
                    applyText->SetVisible( false );
                }
                FeText* value = controls->AddText( "RefreshRate_Value",
                                                   materialsX + valueX,
                                                   refreshLabelY +
                                                       (valueY - templateLabelY) );
                value->SetBoundingBoxSize( valueW, valueH );
                value->SetTextStyle( valueTemplate->GetTextStyleResourceId() );
                value->SetHorizontalJustification( Scrooby::Center );
                value->SetVerticalJustification( csmValue->GetVerticalJustification() );
                value->SetTextMode( valueTemplate->GetTextMode() );
                value->SetColour( materialsValue->GetColour() );
                value->SetDisplayOutline( valueTemplate->IsDisplayingOutline() );
                value->SetOutlineColour( valueTemplate->GetOutlineColour() );
                // Label the row from the rates the runtime actually offers.
                // Headsets differ, and a runtime without the refresh-rate
                // extension offers none at all.
                const unsigned rateCount = SharOpenXR::GetSupportedRefreshRateCount();
                if( rateCount == 0 )
                {
                    value->AddHardCodedString( "Default" );
                }
                for( unsigned rateIndex = 0; rateIndex < rateCount; ++rateIndex )
                {
                    char rateText[ 16 ];
                    std::sprintf( rateText, "%.0f Hz",
                                  SharOpenXR::GetSupportedRefreshRate( rateIndex ) );
                    value->AddHardCodedString( rateText );
                }
                refreshValue = value;

                const char* names[2]={"ColourDepth_ArrowL","ColourDepth_ArrowR"};
                const char* newNames[2]={"RefreshRate_ArrowL","RefreshRate_ArrowR"};
                Scrooby::Sprite** outputs[2]={&refreshLeft,&refreshRight};
                for(int arrowIndex=0;arrowIndex<2;++arrowIndex)
                {
                    FeSprite* source=dynamic_cast<FeSprite*>(materialsGroup->GetSprite(names[arrowIndex]));
                    if(!source) continue;
                    int x,y,w,h;
                    source->GetOriginPosition(x,y);
                    source->GetBoundingBoxSize(w,h);
                    FeSprite* arrow=controls->AddSprite(newNames[arrowIndex],
                                                        materialsX+x,
                                                        refreshLabelY+
                                                            (y-templateLabelY));
                    arrow->SetHorizontalJustification(source->GetHorizontalJustification());
                    arrow->SetVerticalJustification(source->GetVerticalJustification());
                    arrow->SetColour(source->GetColour());
                    arrow->CopyImagesFrom(*source);
                    arrow->SetBoundingBoxSize(w,h);
                    *outputs[arrowIndex]=arrow;
                }
                controls->Show();
            }

            m_pRefreshRateLabel = refreshValue;
            m_pMenu->AddMenuItem( refreshLabel, refreshValue,
                                  NULL,NULL,refreshLeft,refreshRight,
                                  SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
            // The item is always added so the eMenuItem indices stay fixed;
            // a single value simply makes it non-adjustable.
            const unsigned refreshValueCount = SharOpenXR::GetSupportedRefreshRateCount();
            m_pMenu->SetSelectionValueCount( MENU_ITEM_REFRESH_RATE,
                                             refreshValueCount > 0 ?
                                                 static_cast< int >( refreshValueCount ) : 1 );
        }
    }

    Scrooby::Group* scaleGroup = pPage->GetGroup( "Gamma" );
    if( scaleGroup == NULL && menuGroup != NULL ) scaleGroup = menuGroup->GetGroup( "Gamma" );
    rAssert( scaleGroup != NULL );
    Scrooby::Text* scaleLabel = scaleGroup->GetText( "Gamma" );
    Scrooby::Group* scaleSliderGroup = scaleGroup->GetGroup( "Gamma_Slider" );
    rAssert( scaleLabel != NULL && scaleSliderGroup != NULL );
    m_pRenderScaleLabel = scaleLabel;
    scaleSliderGroup->ResetTransformation();
    m_pMenu->AddMenuItem( scaleLabel,
                          NULL,
                          NULL,
                          scaleSliderGroup->GetSprite( "Gamma_Slider" ),
                          NULL,
                          NULL,
                          SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
    m_pMenu->GetMenuItem( MENU_ITEM_RENDER_SCALE )->m_slider.m_type = Slider::HORIZONTAL_SLIDER_LEFT;
    Scrooby::Sprite* scaleIcon = scaleGroup->GetSprite( "Gamma_Icon" );
    if( scaleIcon != NULL ) scaleIcon->SetVisible( false );
    UpdateVrDisplayLabels();
#else
    char itemName[ 32 ];

    for( int i = 0; i < MENU_ITEM_GAMMA; i++ )
    {
        Scrooby::Group* group = pPage->GetGroup( DISPLAY_MENU_ITEMS[ i ] );
        rAssert( group != NULL );

        sprintf( itemName, "%s_Value", DISPLAY_MENU_ITEMS[ i ] );
        Scrooby::Text* pTextValue = group->GetText( itemName );

        sprintf( itemName, "%s_ArrowL", DISPLAY_MENU_ITEMS[ i ] );
        Scrooby::Sprite* pLArrow = group->GetSprite( itemName );

        sprintf( itemName, "%s_ArrowR", DISPLAY_MENU_ITEMS[ i ] );
        Scrooby::Sprite* pRArrow = group->GetSprite( itemName );

        m_pMenu->AddMenuItem( group->GetText( DISPLAY_MENU_ITEMS[ i ] ),
                              pTextValue,
                              NULL,
                              NULL,
                              pLArrow,
                              pRArrow,
                              SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );
    }

    // Add the gamma slider
    Scrooby::Group* pgroup = pPage->GetGroup( "Gamma" );
    rAssert(pgroup  != NULL );

    Scrooby::Text* pText = pgroup->GetText( "Gamma" );

    Scrooby::Group* sliderGroup = pgroup->GetGroup( "Gamma_Slider" );
    rAssert( sliderGroup != NULL );

    sliderGroup->ResetTransformation();

    m_pMenu->AddMenuItem( pText,
                          NULL,
                          NULL,
                          sliderGroup->GetSprite( "Gamma_Slider" ),
                          NULL,
                          NULL,
                          SELECTION_ENABLED | VALUES_WRAPPED | TEXT_OUTLINE_ENABLED );

    m_pMenu->GetMenuItem( MENU_ITEM_GAMMA )->m_slider.m_type = Slider::HORIZONTAL_SLIDER_ABOUT_CENTER;

    Scrooby::Sprite* soundOnIcon = pgroup->GetSprite( "Gamma_Icon" );
    soundOnIcon->ScaleAboutCenter( SLIDER_ICON_SCALE );

    // Add the apply changes button

    pgroup = pPage->GetGroup( "Menu" );
    rAssert( pgroup != NULL );

    m_pMenu->AddMenuItem( pgroup->GetText( "ApplyChanges" ) );
#endif

MEMTRACK_POP_GROUP("CGuiScreenDisplay");
}


//===========================================================================
// CGuiScreenDisplay::~CGuiScreenDisplay
//===========================================================================
// Description: Destructor.
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
CGuiScreenDisplay::~CGuiScreenDisplay()
{
    if( m_pMenu != NULL )
    {
        delete m_pMenu;
        m_pMenu = NULL;
    }
}


//===========================================================================
// CGuiScreenDisplay::HandleMessage
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenDisplay::HandleMessage
(
	eGuiMessage message, 
	unsigned int param1,
	unsigned int param2 
)
{
    if( m_state == GUI_WINDOW_STATE_RUNNING )
    {
#ifdef RAD_ANDROID
        // PauseDisplay's authored navigation has its vertical axis opposite
        // to PauseSettings (used by VR). Normalize it here so both screens
        // react identically to the Quest stick.
        if( message == GUI_MSG_CONTROLLER_UP ) message = GUI_MSG_CONTROLLER_DOWN;
        else if( message == GUI_MSG_CONTROLLER_DOWN ) message = GUI_MSG_CONTROLLER_UP;

        // Use the same latched-axis navigation as the dedicated VR screen.
        // The OpenXR stick must return to neutral before a perpendicular menu
        // direction can be accepted.
        if( ( message == GUI_MSG_CONTROLLER_UP || message == GUI_MSG_CONTROLLER_DOWN ) &&
            SharOpenXR::IsHorizontalMenuInputDominant() )
        {
            message = GUI_MSG_UPDATE;
            param1 = 0;
        }
        if( ( message == GUI_MSG_CONTROLLER_LEFT || message == GUI_MSG_CONTROLLER_RIGHT ) &&
            SharOpenXR::IsVerticalMenuInputDominant() )
        {
            message = GUI_MSG_UPDATE;
            param1 = 0;
        }
#endif
        switch( message )
        {
            case GUI_MSG_MENU_SELECTION_MADE:
            {
                switch( param1 )
                {
#ifdef RAD_ANDROID
                    case MENU_ITEM_REFRESH_RATE:
                    {
                        const unsigned count=SharOpenXR::GetSupportedRefreshRateCount();
                        if(count>1)
                        {
                            const unsigned next=(SharOpenXR::GetRefreshRateIndex()+1)%count;
                            SharOpenXR::SetRefreshRate(SharOpenXR::GetSupportedRefreshRate(next));
                        }
                        UpdateVrDisplayLabels();
                        break;
                    }
#else
                    case MENU_ITEM_APPLY_CHANGES:
                    {
                        ApplySettings();
                        break;
                    }
#endif
                }
                break;
            }
            case GUI_MSG_MENU_SELECTION_VALUE_CHANGED:
            {
                rAssert( m_pMenu );
                GuiMenuItem* currentItem = m_pMenu->GetMenuItem( param1 );
                rAssert( currentItem );

                switch( param1 )
                {
#ifdef RAD_ANDROID
                    case MENU_ITEM_REFRESH_RATE:
                    {
                        if(param2<SharOpenXR::GetSupportedRefreshRateCount())
                            SharOpenXR::SetRefreshRate(SharOpenXR::GetSupportedRefreshRate(param2));
                        UpdateVrDisplayLabels();
                        break;
                    }
                    case MENU_ITEM_CSM:
                    {
                        SharOpenXR::SetCsmEnabled( param2 != 0 );
                        break;
                    }
                    case MENU_ITEM_ENHANCED_MATERIALS:
                    {
                        SharOpenXR::SetEnhancedMaterialsEnabled( param2 != 0 );
                        break;
                    }
                    case MENU_ITEM_VEHICLE_LIGHTS:
                    {
                        SharOpenXR::SetVehicleLightMode( static_cast<int>(param2) );
                        break;
                    }
#else
                    case MENU_ITEM_GAMMA:
                    {
                        float gamma = 2 * currentItem->m_slider.m_value + 0.5f;
                        GetRenderFlow()->SetGamma( gamma );
                        m_changedGamma = true;

                        break;
                    }
#endif
                }
                break;
            }
#ifdef RAD_ANDROID
            case GUI_MSG_MENU_SLIDER_NOT_CHANGING:
            {
                if( param1 == MENU_ITEM_RENDER_SCALE )
                {
                    const float slider=m_pMenu->GetMenuItem( MENU_ITEM_RENDER_SCALE )->m_slider.m_value;
                    SharOpenXR::SetRenderScale( 0.10f + slider * 1.90f );
                    UpdateVrDisplayLabels();
                }
                break;
            }
#endif
        }

        // relay message to menu
        if( m_pMenu != NULL )
        {
            m_pMenu->HandleMessage( message, param1, param2 );
        }
    }

	// Propogate the message up the hierarchy.
	//
	CGuiScreen::HandleMessage( message, param1, param2 );
}

//===========================================================================
// CGuiScreenDisplay::InitIntro
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenDisplay::InitIntro()
{
#ifdef RAD_ANDROID
    m_pMenu->SetSelectionValue( MENU_ITEM_CSM,
                                SharOpenXR::IsCsmEnabled() ? 1 : 0 );
    m_pMenu->SetSelectionValue( MENU_ITEM_ENHANCED_MATERIALS,
                                SharOpenXR::IsEnhancedMaterialsEnabled() ? 1 : 0 );
    m_pMenu->SetSelectionValue( MENU_ITEM_VEHICLE_LIGHTS,
                                SharOpenXR::GetVehicleLightMode() );
    m_pMenu->GetMenuItem( MENU_ITEM_RENDER_SCALE )->m_slider.SetValue(
        ( SharOpenXR::GetRenderScale() - 0.10f ) / 1.90f );
    UpdateVrDisplayLabels();
#else
    // update settings
    //
    Win32Platform* plat = Win32Platform::GetInstance();

    Win32Platform::Resolution res = plat->GetResolution();
    m_pMenu->SetSelectionValue( MENU_ITEM_RESOLUTION,
                                res );

    int bpp = plat->GetBPP();
    m_pMenu->SetSelectionValue( MENU_ITEM_COLOUR_DEPTH,
                                bpp == 16 ? 0: 1 );

    bool fullscreen = plat->IsFullscreen();
    m_pMenu->SetSelectionValue( MENU_ITEM_DISPLAY_MODE,
                                fullscreen ? 1 : 0 );

    GuiMenuItem* menuItem = m_pMenu->GetMenuItem( MENU_ITEM_GAMMA );
    rAssert( menuItem );
    menuItem->m_slider.SetValue( ( GetRenderFlow()->GetGamma() - 0.5f ) / 2.0f );
#endif
}


//===========================================================================
// CGuiScreenDisplay::InitRunning
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenDisplay::InitRunning()
{
}


//===========================================================================
// CGuiScreenDisplay::InitOutro
//===========================================================================
// Description: 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenDisplay::InitOutro()
{
#ifndef RAD_ANDROID
    // Save the config if we've changed the gamma settings
    if( m_changedGamma )
    {
        GetGameConfigManager()->SaveConfigFile();
        m_changedGamma = false;
    }
#endif
}


//---------------------------------------------------------------------
// Private Functions
//---------------------------------------------------------------------

//===========================================================================
// CGuiScreenDisplay::ApplySettings
//===========================================================================
// Description: Applies the current display settings to teh game. 
//
// Constraints:	None.
//
// Parameters:	None.
//
// Return:      N/A.
//
//===========================================================================
void CGuiScreenDisplay::ApplySettings()
{
#ifndef RAD_ANDROID
    // Retrieve the settings.
    //
    Win32Platform::Resolution res = static_cast< Win32Platform::Resolution >( m_pMenu->GetSelectionValue( MENU_ITEM_RESOLUTION ) );

    int bpp = m_pMenu->GetSelectionValue( MENU_ITEM_COLOUR_DEPTH ) ? 32: 16;

    bool fullscreen = m_pMenu->GetSelectionValue( MENU_ITEM_DISPLAY_MODE ) == 1;

    // Set the resolution.
    Win32Platform::GetInstance()->SetResolution( res, bpp, fullscreen );

    // Save the change to the config file.
    GetGameConfigManager()->SaveConfigFile();
    m_changedGamma = false;
#endif
}

#ifdef RAD_ANDROID
void CGuiScreenDisplay::UpdateVrDisplayLabels()
{
    char text[64];
    if( m_pRenderScaleLabel != NULL )
    {
        std::sprintf( text, "Render Scale: %.0f%%", SharOpenXR::GetRenderScale() * 100.0f );
        m_pRenderScaleLabel->SetString( 0, text );
        m_pRenderScaleLabel->SetIndex( 0 );
    }
    if( m_pRefreshRateLabel != NULL )
    {
        const int selection=static_cast<int>(SharOpenXR::GetRefreshRateIndex());
        // SetSelectionValue emits GUI_MSG_MENU_SELECTION_VALUE_CHANGED.  Do
        // not emit it again while handling that same notification.
        if(m_pMenu->GetSelectionValue(MENU_ITEM_REFRESH_RATE)!=selection)
            m_pMenu->SetSelectionValue(MENU_ITEM_REFRESH_RATE,selection);
    }
}
#endif
