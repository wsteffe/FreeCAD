/****************************************************************************
 *   Copyright (c) 2018 Zheng Lei (realthunder) <realthunder.dev@gmail.com> *
 *                                                                          *
 *   This file is part of the FreeCAD CAx development system.               *
 *                                                                          *
 *   This library is free software; you can redistribute it and/or          *
 *   modify it under the terms of the GNU Library General Public            *
 *   License as published by the Free Software Foundation; either           *
 *   version 2 of the License, or (at your option) any later version.       *
 *                                                                          *
 *   This library  is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of         *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the          *
 *   GNU Library General Public License for more details.                   *
 *                                                                          *
 *   You should have received a copy of the GNU Library General Public      *
 *   License along with this library; see the file COPYING.LIB. If not,     *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,          *
 *   Suite 330, Boston, MA  02111-1307, USA                                 *
 *                                                                          *
 ****************************************************************************/

#include "PreCompiled.h"
#ifndef _PreComp_
# include <QFont>
# include <QApplication>
# include <QEvent>
# include <QWidget>
#endif

/*[[[cog
import ViewParams
ViewParams.define()
]]]*/

#include <unordered_map>
#include <App/Application.h>
#include <App/DynamicProperty.h>
#include "ViewParams.h"

using namespace Gui;

class ViewParamsP: public ParameterGrp::ObserverType {
public:
    ParameterGrp::handle handle;
    std::unordered_map<const char *,void(*)(ViewParamsP*),App::CStringHasher,App::CStringHasher> funcs;

    bool UseNewSelection;
    bool UseSelectionRoot;
    bool EnableSelection;
    long RenderCache;
    bool RandomColor;
    unsigned long BoundingBoxColor;
    unsigned long AnnotationTextColor;
    long MarkerSize;
    unsigned long DefaultLinkColor;
    unsigned long DefaultShapeLineColor;
    unsigned long DefaultShapeVertexColor;
    unsigned long DefaultShapeColor;
    long DefaultShapeLineWidth;
    long DefaultShapePointSize;
    bool CoinCycleCheck;
    bool EnablePropertyViewForInactiveDocument;
    bool ShowSelectionBoundingBox;
    long DefaultFontSize;

    ViewParamsP() {
        handle = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/View");
        handle->Attach(this);

        UseNewSelection = handle->GetBool("UseNewSelection", true);
        funcs["UseNewSelection"] = &ViewParamsP::updateUseNewSelection;
        UseSelectionRoot = handle->GetBool("UseSelectionRoot", true);
        funcs["UseSelectionRoot"] = &ViewParamsP::updateUseSelectionRoot;
        EnableSelection = handle->GetBool("EnableSelection", true);
        funcs["EnableSelection"] = &ViewParamsP::updateEnableSelection;
        RenderCache = handle->GetInt("RenderCache", 0);
        funcs["RenderCache"] = &ViewParamsP::updateRenderCache;
        RandomColor = handle->GetBool("RandomColor", false);
        funcs["RandomColor"] = &ViewParamsP::updateRandomColor;
        BoundingBoxColor = handle->GetUnsigned("BoundingBoxColor", 4294967295);
        funcs["BoundingBoxColor"] = &ViewParamsP::updateBoundingBoxColor;
        AnnotationTextColor = handle->GetUnsigned("AnnotationTextColor", 4294967295);
        funcs["AnnotationTextColor"] = &ViewParamsP::updateAnnotationTextColor;
        MarkerSize = handle->GetInt("MarkerSize", 9);
        funcs["MarkerSize"] = &ViewParamsP::updateMarkerSize;
        DefaultLinkColor = handle->GetUnsigned("DefaultLinkColor", 1728053247);
        funcs["DefaultLinkColor"] = &ViewParamsP::updateDefaultLinkColor;
        DefaultShapeLineColor = handle->GetUnsigned("DefaultShapeLineColor", 421075455);
        funcs["DefaultShapeLineColor"] = &ViewParamsP::updateDefaultShapeLineColor;
        DefaultShapeVertexColor = handle->GetUnsigned("DefaultShapeVertexColor", 421075455);
        funcs["DefaultShapeVertexColor"] = &ViewParamsP::updateDefaultShapeVertexColor;
        DefaultShapeColor = handle->GetUnsigned("DefaultShapeColor", 3435973887);
        funcs["DefaultShapeColor"] = &ViewParamsP::updateDefaultShapeColor;
        DefaultShapeLineWidth = handle->GetInt("DefaultShapeLineWidth", 2);
        funcs["DefaultShapeLineWidth"] = &ViewParamsP::updateDefaultShapeLineWidth;
        DefaultShapePointSize = handle->GetInt("DefaultShapePointSize", 2);
        funcs["DefaultShapePointSize"] = &ViewParamsP::updateDefaultShapePointSize;
        CoinCycleCheck = handle->GetBool("CoinCycleCheck", true);
        funcs["CoinCycleCheck"] = &ViewParamsP::updateCoinCycleCheck;
        EnablePropertyViewForInactiveDocument = handle->GetBool("EnablePropertyViewForInactiveDocument", true);
        funcs["EnablePropertyViewForInactiveDocument"] = &ViewParamsP::updateEnablePropertyViewForInactiveDocument;
        ShowSelectionBoundingBox = handle->GetBool("ShowSelectionBoundingBox", false);
        funcs["ShowSelectionBoundingBox"] = &ViewParamsP::updateShowSelectionBoundingBox;
        DefaultFontSize = handle->GetInt("DefaultFontSize", 0);
        funcs["DefaultFontSize"] = &ViewParamsP::updateDefaultFontSize;
    }

    ~ViewParamsP() {
    }

    void OnChange(Base::Subject<const char*> &, const char* sReason) {
        if(!sReason)
            return;
        auto it = funcs.find(sReason);
        if(it == funcs.end())
            return;
        it->second(this);
    }


    static void updateUseNewSelection(ViewParamsP *self) {
        self->UseNewSelection = self->handle->GetBool("UseNewSelection", true);
    }
    static void updateUseSelectionRoot(ViewParamsP *self) {
        self->UseSelectionRoot = self->handle->GetBool("UseSelectionRoot", true);
    }
    static void updateEnableSelection(ViewParamsP *self) {
        self->EnableSelection = self->handle->GetBool("EnableSelection", true);
    }
    static void updateRenderCache(ViewParamsP *self) {
        self->RenderCache = self->handle->GetInt("RenderCache", 0);
    }
    static void updateRandomColor(ViewParamsP *self) {
        self->RandomColor = self->handle->GetBool("RandomColor", false);
    }
    static void updateBoundingBoxColor(ViewParamsP *self) {
        self->BoundingBoxColor = self->handle->GetUnsigned("BoundingBoxColor", 4294967295);
    }
    static void updateAnnotationTextColor(ViewParamsP *self) {
        self->AnnotationTextColor = self->handle->GetUnsigned("AnnotationTextColor", 4294967295);
    }
    static void updateMarkerSize(ViewParamsP *self) {
        self->MarkerSize = self->handle->GetInt("MarkerSize", 9);
    }
    static void updateDefaultLinkColor(ViewParamsP *self) {
        self->DefaultLinkColor = self->handle->GetUnsigned("DefaultLinkColor", 1728053247);
    }
    static void updateDefaultShapeLineColor(ViewParamsP *self) {
        self->DefaultShapeLineColor = self->handle->GetUnsigned("DefaultShapeLineColor", 421075455);
    }
    static void updateDefaultShapeVertexColor(ViewParamsP *self) {
        self->DefaultShapeVertexColor = self->handle->GetUnsigned("DefaultShapeVertexColor", 421075455);
    }
    static void updateDefaultShapeColor(ViewParamsP *self) {
        self->DefaultShapeColor = self->handle->GetUnsigned("DefaultShapeColor", 3435973887);
    }
    static void updateDefaultShapeLineWidth(ViewParamsP *self) {
        self->DefaultShapeLineWidth = self->handle->GetInt("DefaultShapeLineWidth", 2);
    }
    static void updateDefaultShapePointSize(ViewParamsP *self) {
        self->DefaultShapePointSize = self->handle->GetInt("DefaultShapePointSize", 2);
    }
    static void updateCoinCycleCheck(ViewParamsP *self) {
        self->CoinCycleCheck = self->handle->GetBool("CoinCycleCheck", true);
    }
    static void updateEnablePropertyViewForInactiveDocument(ViewParamsP *self) {
        self->EnablePropertyViewForInactiveDocument = self->handle->GetBool("EnablePropertyViewForInactiveDocument", true);
    }
    static void updateShowSelectionBoundingBox(ViewParamsP *self) {
        self->ShowSelectionBoundingBox = self->handle->GetBool("ShowSelectionBoundingBox", false);
    }
    static void updateDefaultFontSize(ViewParamsP *self) {
        auto v = self->handle->GetInt("DefaultFontSize", 0);
        if (self->DefaultFontSize != v) {
            self->DefaultFontSize = v;
            ViewParams::onDefaultFontSizeChanged();
        }
    }
};

ViewParamsP *instance() {
    static ViewParamsP *inst;
    if (!inst)
        inst = new ViewParamsP;
    return inst;
}

ParameterGrp::handle ViewParams::getHandle() {
    return instance()->handle;
}

const char *ViewParams::docUseNewSelection() {
    return "";
}
const bool & ViewParams::UseNewSelection() {
    return instance()->UseNewSelection;
}
const bool & ViewParams::defaultUseNewSelection() {
    const static bool def = true;
    return def;
}
void ViewParams::setUseNewSelection(const bool &v) {
    instance()->handle->SetBool("UseNewSelection",v);
    instance()->UseNewSelection = v;
}
void ViewParams::removeUseNewSelection() {
    instance()->handle->RemoveBool("UseNewSelection");
}

const char *ViewParams::docUseSelectionRoot() {
    return "";
}
const bool & ViewParams::UseSelectionRoot() {
    return instance()->UseSelectionRoot;
}
const bool & ViewParams::defaultUseSelectionRoot() {
    const static bool def = true;
    return def;
}
void ViewParams::setUseSelectionRoot(const bool &v) {
    instance()->handle->SetBool("UseSelectionRoot",v);
    instance()->UseSelectionRoot = v;
}
void ViewParams::removeUseSelectionRoot() {
    instance()->handle->RemoveBool("UseSelectionRoot");
}

const char *ViewParams::docEnableSelection() {
    return "";
}
const bool & ViewParams::EnableSelection() {
    return instance()->EnableSelection;
}
const bool & ViewParams::defaultEnableSelection() {
    const static bool def = true;
    return def;
}
void ViewParams::setEnableSelection(const bool &v) {
    instance()->handle->SetBool("EnableSelection",v);
    instance()->EnableSelection = v;
}
void ViewParams::removeEnableSelection() {
    instance()->handle->RemoveBool("EnableSelection");
}

const char *ViewParams::docRenderCache() {
    return "";
}
const long & ViewParams::RenderCache() {
    return instance()->RenderCache;
}
const long & ViewParams::defaultRenderCache() {
    const static long def = 0;
    return def;
}
void ViewParams::setRenderCache(const long &v) {
    instance()->handle->SetInt("RenderCache",v);
    instance()->RenderCache = v;
}
void ViewParams::removeRenderCache() {
    instance()->handle->RemoveInt("RenderCache");
}

const char *ViewParams::docRandomColor() {
    return "";
}
const bool & ViewParams::RandomColor() {
    return instance()->RandomColor;
}
const bool & ViewParams::defaultRandomColor() {
    const static bool def = false;
    return def;
}
void ViewParams::setRandomColor(const bool &v) {
    instance()->handle->SetBool("RandomColor",v);
    instance()->RandomColor = v;
}
void ViewParams::removeRandomColor() {
    instance()->handle->RemoveBool("RandomColor");
}

const char *ViewParams::docBoundingBoxColor() {
    return "";
}
const unsigned long & ViewParams::BoundingBoxColor() {
    return instance()->BoundingBoxColor;
}
const unsigned long & ViewParams::defaultBoundingBoxColor() {
    const static unsigned long def = 4294967295;
    return def;
}
void ViewParams::setBoundingBoxColor(const unsigned long &v) {
    instance()->handle->SetUnsigned("BoundingBoxColor",v);
    instance()->BoundingBoxColor = v;
}
void ViewParams::removeBoundingBoxColor() {
    instance()->handle->RemoveUnsigned("BoundingBoxColor");
}

const char *ViewParams::docAnnotationTextColor() {
    return "";
}
const unsigned long & ViewParams::AnnotationTextColor() {
    return instance()->AnnotationTextColor;
}
const unsigned long & ViewParams::defaultAnnotationTextColor() {
    const static unsigned long def = 4294967295;
    return def;
}
void ViewParams::setAnnotationTextColor(const unsigned long &v) {
    instance()->handle->SetUnsigned("AnnotationTextColor",v);
    instance()->AnnotationTextColor = v;
}
void ViewParams::removeAnnotationTextColor() {
    instance()->handle->RemoveUnsigned("AnnotationTextColor");
}

const char *ViewParams::docMarkerSize() {
    return "";
}
const long & ViewParams::MarkerSize() {
    return instance()->MarkerSize;
}
const long & ViewParams::defaultMarkerSize() {
    const static long def = 9;
    return def;
}
void ViewParams::setMarkerSize(const long &v) {
    instance()->handle->SetInt("MarkerSize",v);
    instance()->MarkerSize = v;
}
void ViewParams::removeMarkerSize() {
    instance()->handle->RemoveInt("MarkerSize");
}

const char *ViewParams::docDefaultLinkColor() {
    return "";
}
const unsigned long & ViewParams::DefaultLinkColor() {
    return instance()->DefaultLinkColor;
}
const unsigned long & ViewParams::defaultDefaultLinkColor() {
    const static unsigned long def = 1728053247;
    return def;
}
void ViewParams::setDefaultLinkColor(const unsigned long &v) {
    instance()->handle->SetUnsigned("DefaultLinkColor",v);
    instance()->DefaultLinkColor = v;
}
void ViewParams::removeDefaultLinkColor() {
    instance()->handle->RemoveUnsigned("DefaultLinkColor");
}

const char *ViewParams::docDefaultShapeLineColor() {
    return "";
}
const unsigned long & ViewParams::DefaultShapeLineColor() {
    return instance()->DefaultShapeLineColor;
}
const unsigned long & ViewParams::defaultDefaultShapeLineColor() {
    const static unsigned long def = 421075455;
    return def;
}
void ViewParams::setDefaultShapeLineColor(const unsigned long &v) {
    instance()->handle->SetUnsigned("DefaultShapeLineColor",v);
    instance()->DefaultShapeLineColor = v;
}
void ViewParams::removeDefaultShapeLineColor() {
    instance()->handle->RemoveUnsigned("DefaultShapeLineColor");
}

const char *ViewParams::docDefaultShapeVertexColor() {
    return "";
}
const unsigned long & ViewParams::DefaultShapeVertexColor() {
    return instance()->DefaultShapeVertexColor;
}
const unsigned long & ViewParams::defaultDefaultShapeVertexColor() {
    const static unsigned long def = 421075455;
    return def;
}
void ViewParams::setDefaultShapeVertexColor(const unsigned long &v) {
    instance()->handle->SetUnsigned("DefaultShapeVertexColor",v);
    instance()->DefaultShapeVertexColor = v;
}
void ViewParams::removeDefaultShapeVertexColor() {
    instance()->handle->RemoveUnsigned("DefaultShapeVertexColor");
}

const char *ViewParams::docDefaultShapeColor() {
    return "";
}
const unsigned long & ViewParams::DefaultShapeColor() {
    return instance()->DefaultShapeColor;
}
const unsigned long & ViewParams::defaultDefaultShapeColor() {
    const static unsigned long def = 3435973887;
    return def;
}
void ViewParams::setDefaultShapeColor(const unsigned long &v) {
    instance()->handle->SetUnsigned("DefaultShapeColor",v);
    instance()->DefaultShapeColor = v;
}
void ViewParams::removeDefaultShapeColor() {
    instance()->handle->RemoveUnsigned("DefaultShapeColor");
}

const char *ViewParams::docDefaultShapeLineWidth() {
    return "";
}
const long & ViewParams::DefaultShapeLineWidth() {
    return instance()->DefaultShapeLineWidth;
}
const long & ViewParams::defaultDefaultShapeLineWidth() {
    const static long def = 2;
    return def;
}
void ViewParams::setDefaultShapeLineWidth(const long &v) {
    instance()->handle->SetInt("DefaultShapeLineWidth",v);
    instance()->DefaultShapeLineWidth = v;
}
void ViewParams::removeDefaultShapeLineWidth() {
    instance()->handle->RemoveInt("DefaultShapeLineWidth");
}

const char *ViewParams::docDefaultShapePointSize() {
    return "";
}
const long & ViewParams::DefaultShapePointSize() {
    return instance()->DefaultShapePointSize;
}
const long & ViewParams::defaultDefaultShapePointSize() {
    const static long def = 2;
    return def;
}
void ViewParams::setDefaultShapePointSize(const long &v) {
    instance()->handle->SetInt("DefaultShapePointSize",v);
    instance()->DefaultShapePointSize = v;
}
void ViewParams::removeDefaultShapePointSize() {
    instance()->handle->RemoveInt("DefaultShapePointSize");
}

const char *ViewParams::docCoinCycleCheck() {
    return "";
}
const bool & ViewParams::CoinCycleCheck() {
    return instance()->CoinCycleCheck;
}
const bool & ViewParams::defaultCoinCycleCheck() {
    const static bool def = true;
    return def;
}
void ViewParams::setCoinCycleCheck(const bool &v) {
    instance()->handle->SetBool("CoinCycleCheck",v);
    instance()->CoinCycleCheck = v;
}
void ViewParams::removeCoinCycleCheck() {
    instance()->handle->RemoveBool("CoinCycleCheck");
}

const char *ViewParams::docEnablePropertyViewForInactiveDocument() {
    return "";
}
const bool & ViewParams::EnablePropertyViewForInactiveDocument() {
    return instance()->EnablePropertyViewForInactiveDocument;
}
const bool & ViewParams::defaultEnablePropertyViewForInactiveDocument() {
    const static bool def = true;
    return def;
}
void ViewParams::setEnablePropertyViewForInactiveDocument(const bool &v) {
    instance()->handle->SetBool("EnablePropertyViewForInactiveDocument",v);
    instance()->EnablePropertyViewForInactiveDocument = v;
}
void ViewParams::removeEnablePropertyViewForInactiveDocument() {
    instance()->handle->RemoveBool("EnablePropertyViewForInactiveDocument");
}

const char *ViewParams::docShowSelectionBoundingBox() {
    return QT_TRANSLATE_NOOP("ViewParams", "Show selection bounding box");
}
const bool & ViewParams::ShowSelectionBoundingBox() {
    return instance()->ShowSelectionBoundingBox;
}
const bool & ViewParams::defaultShowSelectionBoundingBox() {
    const static bool def = false;
    return def;
}
void ViewParams::setShowSelectionBoundingBox(const bool &v) {
    instance()->handle->SetBool("ShowSelectionBoundingBox",v);
    instance()->ShowSelectionBoundingBox = v;
}
void ViewParams::removeShowSelectionBoundingBox() {
    instance()->handle->RemoveBool("ShowSelectionBoundingBox");
}

const char *ViewParams::docDefaultFontSize() {
    return "";
}
const long & ViewParams::DefaultFontSize() {
    return instance()->DefaultFontSize;
}
const long & ViewParams::defaultDefaultFontSize() {
    const static long def = 0;
    return def;
}
void ViewParams::setDefaultFontSize(const long &v) {
    instance()->handle->SetInt("DefaultFontSize",v);
    instance()->DefaultFontSize = v;
}
void ViewParams::removeDefaultFontSize() {
    instance()->handle->RemoveInt("DefaultFontSize");
}
//[[[end]]]

void ViewParams::init() {
    onDefaultFontSizeChanged();
}

int ViewParams::appDefaultFontSize() {
    static int defaultSize;
    if (!defaultSize) {
        QFont font;
        defaultSize = font.pointSize();
    }
    return defaultSize;
}

void ViewParams::onDefaultFontSizeChanged() {
    int defaultSize = appDefaultFontSize();
    int fontSize = DefaultFontSize();
    if (fontSize <= 0)
        fontSize = defaultSize;
    else if (fontSize < 8)
        fontSize = 8;
    QFont font = QApplication::font();
    if (font.pointSize() != fontSize) {
        font.setPointSize(fontSize);
        QApplication::setFont(font);
        QEvent e(QEvent::ApplicationFontChange);
        for (auto w : QApplication::allWidgets())
            QApplication::sendEvent(w, &e);
    }
}
