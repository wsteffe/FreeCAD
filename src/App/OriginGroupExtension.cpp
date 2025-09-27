/***************************************************************************
 *   Copyright (c) 2015 Alexander Golubev (Fat-Zer) <fatzer2@gmail.com>    *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include <App/Document.h>
#include <Base/Console.h>
#include <Base/Exception.h>
#include <Base/Tools.h>
#include <Base/Placement.h>

#include "OriginGroupExtension.h"
#include "GeoFeature.h"
#include "Link.h"
#include "Origin.h"


FC_LOG_LEVEL_INIT("App", true, true)

using namespace App;

EXTENSION_PROPERTY_SOURCE(App::OriginGroupExtension, App::GeoFeatureGroupExtension)

OriginGroupExtension::OriginGroupExtension()
{

    initExtensionType(OriginGroupExtension::getExtensionClassTypeId());

    EXTENSION_ADD_PROPERTY_TYPE(Origin,
                                (nullptr),
                                0,
                                App::Prop_Hidden,
                                "Origin linked to the group");
    Origin.setScope(LinkScope::Child);
}

OriginGroupExtension::~OriginGroupExtension() = default;

App::Origin* OriginGroupExtension::getOrigin() const
{
    App::DocumentObject* originObj = Origin.getValue();

    if (!originObj) {
        std::stringstream err;
        err << "Can't find Origin for \"" << getExtendedObject()->getFullName() << "\"";
        throw Base::RuntimeError(err.str().c_str());
    }
    else if (!originObj->isDerivedFrom<App::Origin>()) {
        std::stringstream err;
        err << "Bad object \"" << originObj->getFullName() << "\"("
            << originObj->getTypeId().getName() << ") linked to the Origin of \""
            << getExtendedObject()->getFullName() << "\"";
        throw Base::RuntimeError(err.str().c_str());
    }
    else {
        return static_cast<App::Origin*>(originObj);
    }
}


bool OriginGroupExtension::extensionGetSubObject(App::DocumentObject*& ret,
                                                 const char* subname,
                                                 PyObject** pyObj,
                                                 Base::Matrix4D* mat,
                                                 bool transform,
                                                 int depth) const
{
    int debug = false;
    if (debug)
    {
        Base::Console().message(
            "OGE extGetSubObject owner=%s sub='%s' mat=%p transform=%d depth=%d\n",
            getExtendedObject()->getNameInDocument(),
            subname ? subname : "",
            (void*)mat,
            int(transform),
            depth);

        if (mat && transform) {
            Base::Placement P;
            P.fromMatrix(*mat);
            Base::Vector3d pos = P.getPosition();
            double yaw = 0, pitch = 0, roll = 0;
            P.getRotation().getYawPitchRoll(yaw, pitch, roll);
            Base::Console().message(
                "mat(accum) -> Pos=(%.6f, %.6f, %.6f), YPR=(%.6f, %.6f, %.6f)\n",
                pos.x,
                pos.y,
                pos.z,
                yaw,
                pitch,
                roll);
        }
    }

    // Let the base do its usual work (it multiplies by this container's Placement)
    App::DocumentObject* originObj = Origin.getValue();
    const char* dot;
    if (originObj && originObj->isAttachedToDocument() && subname && (dot = strchr(subname, '.'))) {
        bool subObjectIsOrigin=false;
        if (subname[0] == '$') {
            subObjectIsOrigin = std::string(subname + 1, dot) == originObj->Label.getValue();
        }
        else {
            subObjectIsOrigin = std::string(subname, dot) == originObj->getNameInDocument();
        }
        if (subObjectIsOrigin) {
            if (mat && transform) {
                *mat *= const_cast<OriginGroupExtension*>(this)->placement().getValue().toMatrix();
            }
            ret = originObj->getSubObject(dot + 1, pyObj, mat, true, depth + 1);
            return true;
        }
    }

    ret = nullptr;
    // --- Empty subname: return this container (ONLY container Placement, as upstream) ---
    if (!subname || *subname == 0) {
        auto obj = dynamic_cast<const App::DocumentObject*>(getExtendedContainer());
        ret = const_cast<App::DocumentObject*>(obj);
        if (mat && transform) {
            // Upstream behavior: container Placement only
            *mat *= const_cast<OriginGroupExtension*>(this)->placement().getValue().toMatrix();
        }
        return true;
    }
    else if ((dot = strchr(subname, '.'))) {
        if (subname[0] != '$') {
            ret = Group.findUsingMap(std::string(subname, dot));
        }
        else {
            std::string name = std::string(subname + 1, dot);
            for (auto child : Group.getValues()) {
                if (name == child->Label.getStrValue()) {
                    ret = child;
                    break;
                }
            }
        }
        if (ret) {
            ++dot;
            if (*dot && !ret->hasExtension(App::LinkBaseExtension::getExtensionClassTypeId())
                && !ret->hasExtension(App::GeoFeatureGroupExtension::getExtensionClassTypeId())) {
                // Consider this
                // Body
                //  | -- Pad
                //        | -- Sketch
                //
                // Suppose we want to getSubObject(Body,"Pad.Sketch.")
                // Because of the special property of geo feature group, both
                // Pad and Sketch are children of Body, so we can't call
                // getSubObject(Pad,"Sketch.") as this will transform Sketch
                // using Pad's placement.
                //
                const char* next = strchr(dot, '.');
                if (next) {
                    App::DocumentObject* nret = nullptr;
                    extensionGetSubObject(nret, dot, pyObj, mat, transform, depth + 1);
                    if (nret) {
                        ret = nret;
                        return true;
                    }
                }
            }
            if (mat && transform) {
                *mat *= const_cast<OriginGroupExtension*>(this)->placement().getValue().toMatrix();
                if (auto* orgObj = Origin.getValue()) {
                    if (auto* org = dynamic_cast<App::Origin*>(orgObj)) {
                        if (orgObj->isAttachedToDocument()) {
                            *mat *= org->Placement.getValue().toMatrix();
                        }
                    }
                }
            }
            ret = ret->getSubObject(dot, pyObj, mat, true, depth + 1);
        }
    }
    return true;
}


bool OriginGroupExtension::extensionGetGlobalPlacement(Base::Placement& out) const
{
    try {
        // Container first
        Base::Placement C =
            const_cast<OriginGroupExtension*>(this)->placement().getValue();

        // Then this container's local Origin
        Base::Placement O; // identity
        if (auto* orgObj = Origin.getValue()) {
            if (auto* org = dynamic_cast<App::Origin*>(orgObj)) {
                if (org->isAttachedToDocument())
                    O = org->Placement.getValue();
            }
        }

        out = C * O;   // keep per-level order consistent with the matrix path
        return true;
    } catch (...) {
        return false;
    }
}


App::DocumentObject* OriginGroupExtension::getGroupOfObject(const DocumentObject* obj)
{

    if (!obj) {
        return nullptr;
    }

    bool isOriginFeature = obj->isDerivedFrom<App::DatumElement>();

    auto list = obj->getInList();
    for (auto o : list) {
        if (o->hasExtension(App::OriginGroupExtension::getExtensionClassTypeId())) {
            return o;
        }
        else if (isOriginFeature
                 && o->isDerivedFrom<App::LocalCoordinateSystem>()) {
            auto result = getGroupOfObject(o);
            if (result) {
                return result;
            }
        }
    }

    return nullptr;
}

short OriginGroupExtension::extensionMustExecute()
{
    if (Origin.isTouched()) {
        return 1;
    }
    else {
        return GeoFeatureGroupExtension::extensionMustExecute();
    }
}

App::DocumentObjectExecReturn* OriginGroupExtension::extensionExecute()
{
    // >>> cablaggio tardivo: ora il Body è davvero nel Part
    wireParentOrigin_();

    try {  // try to find all base axis and planes in the origin
        getOrigin();
    }
    catch (const Base::Exception& ex) {
        // getExtendedObject()->setError ();
        return new App::DocumentObjectExecReturn(ex.what());
    }

    return GeoFeatureGroupExtension::extensionExecute();
}

App::DocumentObject* OriginGroupExtension::getLocalizedOrigin(App::Document* doc)
{
    auto* originObject = doc->addObject<App::Origin>("Origin");
    QByteArray byteArray = tr("Origin").toUtf8();
    originObject->Label.setValue(byteArray.constData());
    return originObject;
}


void OriginGroupExtension::wireParentOrigin_()  // non-const
{
    auto* owner = getExtendedObject();
    App::DocumentObject* parentOriginObj = nullptr;

    if (owner) {
        for (auto* in : owner->getInList()) {
            if (!in) {
                continue;
            }
            if (!in->hasExtension(App::GroupExtension::getExtensionClassTypeId())) {
                continue;
            }

            if (auto* gext = static_cast<App::GroupExtension*>(
                    in->getExtension(App::GroupExtension::getExtensionClassTypeId()))) {
                if (!gext || !gext->hasObject(owner)) {
                    continue;
                }

                if (in->hasExtension(App::OriginGroupExtension::getExtensionClassTypeId())) {
                    if (auto* pOge = static_cast<OriginGroupExtension*>(
                            in->getExtension(OriginGroupExtension::getExtensionClassTypeId()))) {
                        try {
                            parentOriginObj = pOge->getOrigin();
                        }
                        catch (...) {
                            parentOriginObj = nullptr;
                        }
                    }
                    break;
                }
            }
        }
    }

    if (auto* childOrg = dynamic_cast<App::Origin*>(Origin.getValue())) {
        // Only set link if the parent origin is clearly valid & attached
        App::DocumentObject* newTarget =
            (parentOriginObj && parentOriginObj->isAttachedToDocument())
            ? static_cast<App::DocumentObject*>(parentOriginObj)
            : nullptr;

        if (childOrg->ParentOrigin.getValue() != newTarget) {
            childOrg->ParentOrigin.setValue(newTarget);
        }
    }

    // Do NOT call owner->touch() or childOrg->touch() here.
}




void OriginGroupExtension::onExtendedSetupObject()
{
    wireParentOrigin_();
    enforcePlacementVisibility_();
    App::Document* doc = getExtendedObject()->getDocument();

    App::DocumentObject* originObj = getLocalizedOrigin(doc);

    assert(originObj && originObj->isDerivedFrom<App::Origin>());
    Origin.setValue(originObj);

    GeoFeatureGroupExtension::onExtendedSetupObject();
}

void OriginGroupExtension::onExtendedUnsetupObject()
{
    App::DocumentObject* origin = Origin.getValue();
    if (origin && !origin->isRemoving()) {
        origin->getDocument()->removeObject(origin->getNameInDocument());
    }

    GeoFeatureGroupExtension::onExtendedUnsetupObject();
}


void OriginGroupExtension::extensionOnChanged(const App::Property* p)
{
    App::DocumentObject* owner = getExtendedObject();

    // 1) Cambia il link all'Origin del container → rewire + visibilità + gestione Importing
    if (p == &Origin) {
        wireParentOrigin_();
        enforcePlacementVisibility_();

        App::DocumentObject* origin = Origin.getValue();
        if (origin && owner && owner->getDocument()
            && owner->getDocument()->testStatus(Document::Importing)) {
            for (auto o : origin->getInList()) {
                if (o != owner
                    && o->hasExtension(App::OriginGroupExtension::getExtensionClassTypeId())) {
                    App::Document* document = owner->getDocument();
                    Base::ObjectStatusLocker<Document::Status, Document> guard(
                        Document::Restoring, document, false);
                    Origin.setValue(getLocalizedOrigin(document));
                    FC_WARN("Reset origin in " << owner->getFullName());
                    return;
                }
            }
        }
        // non ritorniamo: più sotto gestiamo l’eventuale frameChanged
    }


    if (p == &Group) {
        // We are the parent; rewire all OGE children just added/removed
        for (auto* ch : getAllChildren()) {
            if (!ch) continue;
            if (!ch->hasExtension(App::OriginGroupExtension::getExtensionClassTypeId()))
                continue;
            if (auto* oge = static_cast<OriginGroupExtension*>(
                    ch->getExtension(OriginGroupExtension::getExtensionClassTypeId()))) {
                try { oge->wireParentOrigin_(); } catch (...) {}
                oge->wireParentOrigin_();   // sets ch.Origin.ParentOrigin -> this.Origin
            }
        }
    }


    // 3) Decidi se il frame del container è cambiato
    bool frameChanged = false;

    // 3a) Placement del container (Part/Body) cambiata?
    if (p == &const_cast<OriginGroupExtension*>(this)->placement())
        frameChanged = true;

    // 3b) Placement dell’Origin del container cambiata?
    if (auto* org = dynamic_cast<App::Origin*>(Origin.getValue())) {
        if (p == &org->Placement)
            frameChanged = true;
    }

    // NB: NON controlliamo più ParentOrigin qui, perché ora vive su App::Origin
    //     e la sua variazione farà ricomputare l'Origin stesso.

    if (frameChanged) {
        // Tocca SOLO il container (niente figli)
        if (owner && !owner->isTouched())
            owner->touch();
    }

    // 4) Mantieni comportamento base
    GeoFeatureGroupExtension::extensionOnChanged(p);
}




void OriginGroupExtension::relinkToOrigin(App::DocumentObject* obj)
{
    // we get all links and replace the origin objects if needed (subnames need not to change, they
    // would stay the same)
    std::vector<App::DocumentObject*> result;
    std::vector<App::Property*> list;
    obj->getPropertyList(list);
    auto isOriginFeature = [](App::DocumentObject* obj) -> bool {
        // Check if the object is a DatumElement
        if (auto* datumElement = dynamic_cast<App::DatumElement*>(obj)) {
            // Check if the DatumElement is an origin
            return datumElement->isOriginFeature();
        }
        return false;
    };

    for (App::Property* prop : list) {
        if (prop->isDerivedFrom<App::PropertyLink>()) {

            auto p = static_cast<App::PropertyLink*>(prop);
            if (!p->getValue() || !isOriginFeature(p->getValue())) {
                continue;
            }

            p->setValue(getOrigin()->getDatumElement(
                static_cast<DatumElement*>(p->getValue())->Role.getValue()));
        }
        else if (prop->isDerivedFrom<App::PropertyLinkList>()) {
            auto p = static_cast<App::PropertyLinkList*>(prop);
            auto vec = p->getValues();
            std::vector<App::DocumentObject*> result;
            bool changed = false;
            for (App::DocumentObject* o : vec) {
                if (!isOriginFeature(o)) {
                    result.push_back(o);
                }
                else {
                    result.push_back(getOrigin()->getDatumElement(
                        static_cast<DatumElement*>(o)->Role.getValue()));
                    changed = true;
                }
            }
            if (changed) {
                static_cast<App::PropertyLinkList*>(prop)->setValues(result);
            }
        }
        else if (prop->isDerivedFrom<App::PropertyLinkSub>()) {
            auto p = static_cast<App::PropertyLinkSub*>(prop);
            if (!p->getValue() || !isOriginFeature(p->getValue())) {
                continue;
            }

            std::vector<std::string> subValues = p->getSubValues();
            p->setValue(getOrigin()->getDatumElement(
                            static_cast<DatumElement*>(p->getValue())->Role.getValue()),
                        subValues);
        }
        else if (prop->isDerivedFrom<App::PropertyLinkSubList>()) {
            auto p = static_cast<App::PropertyLinkSubList*>(prop);
            auto vec = p->getSubListValues();
            bool changed = false;
            for (auto& v : vec) {
                if (isOriginFeature(v.first)) {
                    v.first = getOrigin()->getDatumElement(
                        static_cast<DatumElement*>(v.first)->Role.getValue());
                    changed = true;
                }
            }
            if (changed) {
                p->setSubListValues(vec);
            }
        }
    }
}

std::vector<DocumentObject*> OriginGroupExtension::addObjects(std::vector<DocumentObject*> objs)
{

    for (auto obj : objs) {
        relinkToOrigin(obj);
    }

    return App::GeoFeatureGroupExtension::addObjects(objs);
}

bool OriginGroupExtension::hasObject(const DocumentObject* obj, bool recursive) const
{

    if (Origin.getValue() && (obj == getOrigin() || getOrigin()->hasObject(obj))) {
        return true;
    }

    return App::GroupExtension::hasObject(obj, recursive);
}

bool OriginGroupExtension::actsAsGroupBoundary() const { return false; }

// Python feature ---------------------------------------------------------

namespace App
{
EXTENSION_PROPERTY_SOURCE_TEMPLATE(App::OriginGroupExtensionPython, App::OriginGroupExtension)

// explicit template instantiation
template class AppExport ExtensionPythonT<GroupExtensionPythonT<OriginGroupExtension>>;
}  // namespace App

Base::Placement OriginGroupExtension::getChainedPlacement() const
{
    // Access non-const placement() from a const method (same pattern used elsewhere)
    Base::Placement groupP = const_cast<OriginGroupExtension*>(this)->placement().getValue();

    App::DocumentObject* originObj = Origin.getValue();
    if (originObj && originObj->isAttachedToDocument()) {
        if (auto* org = dynamic_cast<App::Origin*>(originObj)) {
            try {
                return org->Placement.getValue() * groupP;
            } catch (...) {
                return groupP;
            }
        }
    }
    return groupP;
}

void OriginGroupExtension::enforcePlacementVisibility_() const
{
    // Container (owner) placement: Hidden = true, ReadOnly = false
    if (auto* owner = const_cast<OriginGroupExtension*>(this)->getExtendedObject()) {
        if (auto* prop = owner->getPropertyByName("Placement")) {
            try {
                prop->setStatus(App::Property::Hidden, true);
            } catch (...) {}
            try {
                prop->setStatus(App::Property::ReadOnly, false);
            } catch (...) {}
        }
    }

    // Origin placement: Hidden = false, ReadOnly = false
    try {
        if (auto* org = const_cast<OriginGroupExtension*>(this)->getOrigin()) {
            if (auto* prop = org->getPropertyByName("Placement")) {
                try {
                    prop->setStatus(App::Property::Hidden, false);
                } catch (...) {}
                try {
                    prop->setStatus(App::Property::ReadOnly, false);
                } catch (...) {}
            }
        }
    } catch (...) {
        // getOrigin() may throw if Origin isn't present/initialized yet
    }
}
