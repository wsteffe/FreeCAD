/***************************************************************************
 *   Copyright (c) 2011 Jürgen Riegel <juergen.riegel@web.de>              *
 *   Copyright (c) 2011 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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


#include "PreCompiled.h"

#ifndef _PreComp_
# include <assert.h>
# include <string>
# include <boost_bind_bind.hpp>
# include <QApplication>
# include <QString>
# include <QStatusBar>
# include <QTextStream>
#endif

#include <boost/algorithm/string/predicate.hpp>

/// Here the FreeCAD includes sorted by Base,App,Gui......
#include "Application.h"
#include "Document.h"
#include "Selection.h"
#include "SelectionFilter.h"
#include "View3DInventor.h"
#include <Base/Exception.h>
#include <Base/Console.h>
#include <Base/Tools.h>
#include <Base/Interpreter.h>
#include <Base/UnitsApi.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <App/DocumentObjectPy.h>
#include <App/GeoFeature.h>
#include <App/DocumentObserver.h>
#include <Gui/SelectionObjectPy.h>
#include "MainWindow.h"
#include "Tree.h"
#include "TreeParams.h"
#include "ViewParams.h"
#include "ViewProviderDocumentObject.h"
#include "Macro.h"
#include "Command.h"
#include "Widgets.h"

FC_LOG_LEVEL_INIT("Selection",false,true,true)

using namespace Gui;
using namespace std;
namespace bp = boost::placeholders;

SelectionGateFilterExternal::SelectionGateFilterExternal(const char *docName, const char *objName) {
    if(docName) {
        DocName = docName;
        if(objName)
            ObjName = objName;
    }
}

bool SelectionGateFilterExternal::allow(App::Document *doc ,App::DocumentObject *obj, const char*) {
    if(!doc || !obj)
        return true;
    if(DocName.size() && doc->getName()!=DocName)
        notAllowedReason = "Cannot select external object";
    else if(ObjName.size() && ObjName==obj->getNameInDocument())
        notAllowedReason = "Cannot select self";
    else
        return true;
    return false;
}

//////////////////////////////////////////////////////////////////////////////////////////

SelectionObserver::SelectionObserver(bool attach,int resolve)
    :resolve(resolve),blockSelection(false)
{
    if(attach)
        attachSelection();
}

SelectionObserver::SelectionObserver(const ViewProviderDocumentObject *vp,bool attach,int resolve)
    :resolve(resolve),blockSelection(false)
{
    if(vp && vp->getObject() && vp->getObject()->getDocument()) {
        filterDocName = vp->getObject()->getDocument()->getName();
        filterObjName = vp->getObject()->getNameInDocument();
    }
    if(attach)
        attachSelection();
}


SelectionObserver::~SelectionObserver()
{
    detachSelection();
}

bool SelectionObserver::blockConnection(bool block)
{
    bool ok = blockSelection;
    if (block)
        blockSelection = true;
    else
        blockSelection = false;
    return ok;
}

bool SelectionObserver::isConnectionBlocked() const
{
    return blockSelection;
}

bool SelectionObserver::isConnectionAttached() const
{
    return connectSelection.connected();
}

void SelectionObserver::attachSelection()
{
    if (!connectSelection.connected()) {
        auto &signal = resolve > 1 ? Selection().signalSelectionChanged3 :
                       resolve     ? Selection().signalSelectionChanged2 :
                                     Selection().signalSelectionChanged  ;
        connectSelection = signal.connect(boost::bind
            (&SelectionObserver::_onSelectionChanged, this, bp::_1));

        if (filterDocName.size()) {
            Selection().addSelectionGate(
                    new SelectionGateFilterExternal(filterDocName.c_str(),filterObjName.c_str()));
        }
    }
}

void SelectionObserver::_onSelectionChanged(const SelectionChanges& msg) {
    try {
        if (blockSelection)
            return;
        onSelectionChanged(msg);
    } catch (Base::Exception &e) {
        e.ReportException();
        FC_ERR("Unhandled Base::Exception caught in selection observer");
    } catch (Py::Exception &) {
        Base::PyGILStateLocker lock;
        Base::PyException e;
        e.ReportException();
        FC_ERR("Unhandled Python exception caught in selection observer");
    } catch (std::exception &e) {
        FC_ERR("Unhandled std::exception caught in selection observer: " << e.what());
    } catch (...) {
        FC_ERR("Unhandled unknown exception caught in selection observer");
    }
}

void SelectionObserver::detachSelection()
{
    if (connectSelection.connected()) {
        connectSelection.disconnect();
        if(filterDocName.size())
            Selection().rmvSelectionGate();
    }
}

// -------------------------------------------

std::vector<SelectionObserverPython*> SelectionObserverPython::_instances;

SelectionObserverPython::SelectionObserverPython(const Py::Object& obj, int resolve)
    : SelectionObserver(true,resolve),inst(obj)
{
#undef FC_PY_ELEMENT
#define FC_PY_ELEMENT(_name) FC_PY_GetCallable(obj.ptr(),#_name,py_##_name);
    FC_PY_SEL_OBSERVER
}

SelectionObserverPython::~SelectionObserverPython()
{
}

void SelectionObserverPython::addObserver(const Py::Object& obj, int resolve)
{
    _instances.push_back(new SelectionObserverPython(obj,resolve));
}

void SelectionObserverPython::removeObserver(const Py::Object& obj)
{
    SelectionObserverPython* obs=0;
    for (std::vector<SelectionObserverPython*>::iterator it =
        _instances.begin(); it != _instances.end(); ++it) {
        if ((*it)->inst == obj) {
            obs = *it;
            _instances.erase(it);
            break;
        }
    }

    delete obs;
}

void SelectionObserverPython::onSelectionChanged(const SelectionChanges& msg)
{
    switch (msg.Type)
    {
    case SelectionChanges::AddSelection:
        addSelection(msg);
        break;
    case SelectionChanges::RmvSelection:
        removeSelection(msg);
        break;
    case SelectionChanges::SetSelection:
        setSelection(msg);
        break;
    case SelectionChanges::ClrSelection:
        clearSelection(msg);
        break;
    case SelectionChanges::SetPreselect:
        setPreselection(msg);
        break;
    case SelectionChanges::RmvPreselect:
        removePreselection(msg);
        break;
    case SelectionChanges::PickedListChanged:
        pickedListChanged();
        break;
    default:
        break;
    }
}

void SelectionObserverPython::pickedListChanged()
{
    if(py_pickedListChanged.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Callable(py_pickedListChanged).apply(Py::Tuple());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

void SelectionObserverPython::addSelection(const SelectionChanges& msg)
{
    if(py_addSelection.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Tuple args(4);
        args.setItem(0, Py::String(msg.pDocName ? msg.pDocName : ""));
        args.setItem(1, Py::String(msg.pObjectName ? msg.pObjectName : ""));
        args.setItem(2, Py::String(msg.pSubName ? msg.pSubName : ""));
        Py::Tuple tuple(3);
        tuple[0] = Py::Float(msg.x);
        tuple[1] = Py::Float(msg.y);
        tuple[2] = Py::Float(msg.z);
        args.setItem(3, tuple);
        Base::pyCall(py_addSelection.ptr(),args.ptr());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

void SelectionObserverPython::removeSelection(const SelectionChanges& msg)
{
    if(py_removeSelection.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Tuple args(3);
        args.setItem(0, Py::String(msg.pDocName ? msg.pDocName : ""));
        args.setItem(1, Py::String(msg.pObjectName ? msg.pObjectName : ""));
        args.setItem(2, Py::String(msg.pSubName ? msg.pSubName : ""));
        Base::pyCall(py_removeSelection.ptr(),args.ptr());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

void SelectionObserverPython::setSelection(const SelectionChanges& msg)
{
    if(py_setSelection.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Tuple args(1);
        args.setItem(0, Py::String(msg.pDocName ? msg.pDocName : ""));
        Base::pyCall(py_setSelection.ptr(),args.ptr());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

void SelectionObserverPython::clearSelection(const SelectionChanges& msg)
{
    if(py_clearSelection.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Tuple args(1);
        args.setItem(0, Py::String(msg.pDocName ? msg.pDocName : ""));
        Base::pyCall(py_clearSelection.ptr(),args.ptr());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

void SelectionObserverPython::setPreselection(const SelectionChanges& msg)
{
    if(py_setPreselection.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Tuple args(3);
        args.setItem(0, Py::String(msg.pDocName ? msg.pDocName : ""));
        args.setItem(1, Py::String(msg.pObjectName ? msg.pObjectName : ""));
        args.setItem(2, Py::String(msg.pSubName ? msg.pSubName : ""));
        Base::pyCall(py_setPreselection.ptr(),args.ptr());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

void SelectionObserverPython::removePreselection(const SelectionChanges& msg)
{
    if(py_removePreselection.isNone())
        return;
    Base::PyGILStateLocker lock;
    try {
        Py::Tuple args(3);
        args.setItem(0, Py::String(msg.pDocName ? msg.pDocName : ""));
        args.setItem(1, Py::String(msg.pObjectName ? msg.pObjectName : ""));
        args.setItem(2, Py::String(msg.pSubName ? msg.pSubName : ""));
        Base::pyCall(py_removePreselection.ptr(),args.ptr());
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        e.ReportException();
    }
}

// -------------------------------------------

static int _DisableTopParentCheck;

SelectionNoTopParentCheck::SelectionNoTopParentCheck() {
    ++_DisableTopParentCheck;
}

SelectionNoTopParentCheck::~SelectionNoTopParentCheck() {
    if(_DisableTopParentCheck>0)
        --_DisableTopParentCheck;
}

bool SelectionNoTopParentCheck::enabled() {
    return _DisableTopParentCheck>0;
}

// -------------------------------------------

static int _PauseNotification;

SelectionPauseNotification::SelectionPauseNotification() {
    ++_PauseNotification;
}

SelectionPauseNotification::~SelectionPauseNotification() {
    if(_PauseNotification>0) {
        --_PauseNotification;
        Selection().flushNotifications();
    }
}

bool SelectionPauseNotification::enabled() {
    return _PauseNotification>0;
}
// -------------------------------------------

SelectionContext::SelectionContext(const App::SubObjectT &sobj)
{
    Selection().pushContext(sobj);
}

SelectionContext::~SelectionContext()
{
    Selection().popContext();
}

// -------------------------------------------
bool SelectionSingleton::hasSelection() const
{
    return !_SelList.empty();
}

bool SelectionSingleton::hasPreselection() const {
    return !CurrentPreselection.Object.getObjectName().empty();
}

std::vector<SelectionSingleton::SelObj> SelectionSingleton::getCompleteSelection(int resolve) const
{
    return getSelection("*",resolve);
}

std::vector<App::SubObjectT> SelectionSingleton::getSelectionT(
        const char* pDocName, int resolve, bool single) const
{
    auto sels = getSelection(pDocName,resolve,single);
    std::vector<App::SubObjectT> res;
    res.reserve(sels.size());
    for(auto &sel : sels)
        res.emplace_back(sel.pObject,sel.SubName);
    return res;
}

std::vector<SelectionSingleton::SelObj> SelectionSingleton::getSelection(const char* pDocName,
        int resolve, bool single) const
{
    std::vector<SelObj> temp;
    if(single) temp.reserve(1);
    SelObj tempSelObj;

    App::Document *pcDoc = 0;
    if(!pDocName || strcmp(pDocName,"*")!=0) {
        pcDoc = getDocument(pDocName);
        if (!pcDoc)
            return temp;
    }

    std::map<App::DocumentObject*,std::set<std::string> > objMap;

    for(const auto &sel : _SelList.get<0>()) {
        if(!sel.pDoc) continue;
        const char *subelement = 0;
        auto obj = getObjectOfType(sel,App::DocumentObject::getClassTypeId(),resolve,&subelement);
        if(!obj || (pcDoc && sel.pObject->getDocument()!=pcDoc))
            continue;

        // In case we are resolving objects, make sure no duplicates
        if(resolve && !objMap[obj].insert(std::string(subelement?subelement:"")).second)
            continue;

        if(single && temp.size()) {
            temp.clear();
            break;
        }

        tempSelObj.DocName  = obj->getDocument()->getName();
        tempSelObj.FeatName = obj->getNameInDocument();
        tempSelObj.SubName = subelement;
        tempSelObj.TypeName = obj->getTypeId().getName();
        tempSelObj.pObject  = obj;
        tempSelObj.pResolvedObject  = sel.pResolvedObject;
        tempSelObj.pDoc     = obj->getDocument();
        tempSelObj.x        = sel.x;
        tempSelObj.y        = sel.y;
        tempSelObj.z        = sel.z;

        temp.push_back(tempSelObj);
    }

    return temp;
}

bool SelectionSingleton::hasSelection(const char* doc, bool resolve) const
{
    App::Document *pcDoc = 0;
    if(!doc || strcmp(doc,"*")!=0) {
        pcDoc = getDocument(doc);
        if (!pcDoc)
            return false;
    }
    for(const auto &sel : _SelList.get<0>()) {
        if(!sel.pDoc) continue;
        auto obj = getObjectOfType(sel,App::DocumentObject::getClassTypeId(),resolve);
        if(obj && (!pcDoc || sel.pObject->getDocument()==pcDoc)) {
            return true;
        }
    }

    return false;
}

bool SelectionSingleton::hasSubSelection(const char* doc, bool subElement) const
{
    App::Document *pcDoc = 0;
    if(!doc || strcmp(doc,"*")!=0) {
        pcDoc = getDocument(doc);
        if (!pcDoc)
            return false;
    }
    for(auto &sel : _SelList.get<0>()) {
        if(pcDoc && pcDoc != sel.pDoc)
            continue;
        if(sel.SubName.empty())
            continue;
        if(subElement && sel.SubName.back()!='.')
            return true;
        if(sel.pObject != sel.pResolvedObject)
            return true;
    }

    return false;
}

std::vector<App::SubObjectT> SelectionSingleton::getPickedList(const char* pDocName) const
{
    std::vector<App::SubObjectT> res;

    App::Document *pcDoc = 0;
    if(!pDocName || strcmp(pDocName,"*")!=0) {
        pcDoc = getDocument(pDocName);
        if (!pcDoc)
            return res;
    }

    res.reserve(_PickedList.size());
    for(auto &sel : _PickedList) {
        if (!pcDoc || sel.pDoc == pcDoc) {
            res.emplace_back(sel.DocName.c_str(), sel.FeatName.c_str(), sel.SubName.c_str());
        }
    }
    return res;
}

template<class T>
std::vector<SelectionObject>
SelectionSingleton::getObjectList(const char* pDocName,
                                  Base::Type typeId,
                                  T &objList,
                                  int resolve,
                                  bool single) const
{
    std::vector<SelectionObject> temp;
    if(single) temp.reserve(1);
    std::map<App::DocumentObject*,size_t> SortMap;

    // check the type
    if (typeId == Base::Type::badType())
        return temp;

    App::Document *pcDoc = 0;
    if(!pDocName || strcmp(pDocName,"*")!=0) {
        pcDoc = getDocument(pDocName);
        if (!pcDoc)
            return temp;
    }

    for (const auto &sel : objList) {
        if(!sel.pDoc) continue;
        const char *subelement = 0;
        auto obj = getObjectOfType(sel,typeId,resolve,&subelement);
        if(!obj || (pcDoc && sel.pObject->getDocument()!=pcDoc))
            continue;
        auto it = SortMap.find(obj);
        if(it!=SortMap.end()) {
            // only add sub-element
            if (subelement && *subelement) {
                auto &entry = temp[it->second];
                if(resolve && !entry._SubNameSet.insert(subelement).second)
                    continue;
                if (entry.SubNames.empty()) {
                    // It means there is previous whole object selection. Don't
                    // loose that information.
                    entry.SubNames.emplace_back();
                    entry.SelPoses.emplace_back(0, 0, 0);
                }
                entry.SubNames.push_back(subelement);
                entry.SelPoses.emplace_back(sel.x,sel.y,sel.z);
            }
        }
        else {
            if(single && temp.size()) {
                temp.clear();
                break;
            }
            // create a new entry
            temp.emplace_back(obj);
            if (subelement && *subelement) {
                temp.back().SubNames.push_back(subelement);
                temp.back().SelPoses.emplace_back(sel.x,sel.y,sel.z);
                if(resolve)
                    temp.back()._SubNameSet.insert(subelement);
            }
            SortMap.insert(std::make_pair(obj,temp.size()-1));
        }
    }

    return temp;
}

std::vector<SelectionObject> SelectionSingleton::getSelectionEx(
        const char* pDocName, Base::Type typeId, int resolve, bool single) const {
    return getObjectList(pDocName,typeId,_SelList.get<0>(),resolve,single);
}

std::vector<SelectionObject> SelectionSingleton::getPickedListEx(const char* pDocName, Base::Type typeId) const {
    return getObjectList(pDocName,typeId,_PickedList,false);
}

bool SelectionSingleton::needPickedList() const {
    return _needPickedList;
}

void SelectionSingleton::enablePickedList(bool enable) {
    if(enable != _needPickedList) {
        _needPickedList = enable;
        _PickedList.clear();
        notify(SelectionChanges(SelectionChanges::PickedListChanged));
    }
}

void SelectionSingleton::notify(SelectionChanges &&Chng) {
    if(Notifying || SelectionPauseNotification::enabled()) {
        switch(Chng.Type) {
        // In case of ClrSelection or SetSelection, we can erase any
        // Add/RmvSelection from tail up.
        case SelectionChanges::ClrSelection:
        case SelectionChanges::SetSelection:
            while(NotificationQueue.size()) {
                auto &entry = NotificationQueue.back();
                switch(entry.Type) {
                case SelectionChanges::AddSelection:
                case SelectionChanges::RmvSelection:
                case SelectionChanges::ClrSelection:
                case SelectionChanges::SetSelection:
                    if (Chng.Object.getDocumentName().empty()
                            || Chng.Object.getDocument() != entry.Object.getDocument())
                    {
                        NotificationQueue.pop_back();
                        continue;
                    }
                    // fall through
                default:
                    break;
                }
                break;
            }
            NotificationQueue.emplace_back(std::move(Chng));
            return;
        // In case the queued Add/RmvSelection message exceed the limit,
        // replace them with a SetSelection message. The expected response to
        // this message for the observer is to recheck the entire selections.
        case SelectionChanges::AddSelection:
        case SelectionChanges::RmvSelection:
            if (NotificationQueue.size()
                    && ViewParams::getMaxSelectionNotification()
                    && ++PendingAddSelection >= ViewParams::getMaxSelectionNotification())
            {
                if (NotificationQueue.back().Type != SelectionChanges::SetSelection)
                    notify(SelectionChanges(SelectionChanges::SetSelection));
                return;
            }
        default:
            break;
        }

        long limit = std::max(1000l, ViewParams::getMaxSelectionNotification());
        if (++NotificationRecursion > limit) {
            if (NotificationRecursion == limit + 1)
                FC_WARN("Discard selection notification " << Chng.Type);
            return;
        }

        NotificationQueue.push_back(std::move(Chng));
        return;
    }

    NotificationRecursion = 0;
    PendingAddSelection = 0;
    Base::FlagToggler<bool> flag(Notifying);
    NotificationQueue.push_back(std::move(Chng));
    while(NotificationQueue.size()) {
        const auto &msg = NotificationQueue.front();
        bool notify;
        switch(msg.Type) {
        case SelectionChanges::AddSelection:
            notify = isSelected(msg.pDocName,msg.pObjectName,msg.pSubName,0);
            break;
        case SelectionChanges::RmvSelection:
            notify = !isSelected(msg.pDocName,msg.pObjectName,msg.pSubName,0);
            break;
        case SelectionChanges::SetPreselect:
            notify = CurrentPreselection.Type==SelectionChanges::SetPreselect
                && CurrentPreselection.Object == msg.Object;
            break;
        case SelectionChanges::RmvPreselect:
            notify = CurrentPreselection.Type==SelectionChanges::ClrSelection;
            break;
        default:
            notify = true;
        }
        if(notify) {
            Notify(msg);
            try {
                signalSelectionChanged(msg);
            }
            catch (const boost::exception&) {
                // reported by code analyzers
                Base::Console().Warning("notify: Unexpected boost exception\n");
            }
        }
        NotificationQueue.pop_front();
    }
    NotificationRecursion = 0;
}

void SelectionSingleton::flushNotifications()
{
    if(Notifying || SelectionPauseNotification::enabled()
                 || NotificationQueue.empty())
        return;
    auto chg = NotificationQueue.back();
    NotificationQueue.pop_back();
    notify(std::move(chg));

    _selStackPush(_SelList.size() > 0);
}

bool SelectionSingleton::hasPickedList() const {
    return _PickedList.size();
}

int SelectionSingleton::getAsPropertyLinkSubList(App::PropertyLinkSubList &prop) const
{
    std::vector<Gui::SelectionObject> sel = this->getSelectionEx();
    std::vector<App::DocumentObject*> objs; objs.reserve(sel.size()*2);
    std::vector<std::string> subs; subs.reserve(sel.size()*2);
    for (std::size_t iobj = 0; iobj < sel.size(); iobj++) {
        Gui::SelectionObject &selitem = sel[iobj];
        App::DocumentObject* obj = selitem.getObject();
        const std::vector<std::string> &subnames = selitem.getSubNames();
        if (subnames.size() == 0){//whole object is selected
            objs.push_back(obj);
            subs.emplace_back();
        } else {
            for (std::size_t isub = 0; isub < subnames.size(); isub++) {
                objs.push_back(obj);
                subs.push_back(subnames[isub]);
            }
        }
    }
    assert(objs.size()==subs.size());
    prop.setValues(objs, subs);
    return objs.size();
}

App::DocumentObject *SelectionSingleton::getObjectOfType(const _SelObj &sel,
        Base::Type typeId, int resolve, const char **subelement)
{
    auto obj = sel.pObject;
    if(!obj || !obj->getNameInDocument())
        return 0;
    const char *subname = sel.SubName.c_str();
    if(resolve) {
        obj = sel.pResolvedObject;
        if((resolve==2 || sel.elementName.second.empty())
                && sel.elementName.first.size())
            subname = sel.elementName.first.c_str();
        else
            subname = sel.elementName.second.c_str();
    }
    if(!obj)
        return 0;
    if(!obj->isDerivedFrom(typeId) &&
       (resolve!=3 || !obj->getLinkedObject(true)->isDerivedFrom(typeId)))
        return 0;
    if(subelement) *subelement = subname;
    return obj;
}

vector<App::DocumentObject*> SelectionSingleton::getObjectsOfType(const Base::Type& typeId, const char* pDocName, int resolve) const
{
    std::vector<App::DocumentObject*> temp;

    App::Document *pcDoc = 0;
    if(!pDocName || strcmp(pDocName,"*")!=0) {
        pcDoc = getDocument(pDocName);
        if (!pcDoc)
            return temp;
    }

    std::set<App::DocumentObject*> objs;
    for(const auto &sel : _SelList.get<0>()) {
        if(pcDoc && pcDoc!=sel.pDoc) continue;
        App::DocumentObject *pObject = getObjectOfType(sel,typeId,resolve);
        if (pObject) {
            auto ret = objs.insert(pObject);
            if(ret.second)
                temp.push_back(pObject);
        }
    }

    return temp;
}

std::vector<App::DocumentObject*> SelectionSingleton::getObjectsOfType(const char* typeName, const char* pDocName, int resolve) const
{
    Base::Type typeId = Base::Type::fromName(typeName);
    if (typeId == Base::Type::badType())
        return std::vector<App::DocumentObject*>();
    return getObjectsOfType(typeId, pDocName, resolve);
}

unsigned int SelectionSingleton::countObjectsOfType(const Base::Type& typeId, const char* pDocName, int resolve) const
{
    unsigned int iNbr=0;
    App::Document *pcDoc = 0;
    if(!pDocName || strcmp(pDocName,"*")!=0) {
        pcDoc = getDocument(pDocName);
        if (!pcDoc)
            return 0;
    }

    for (const auto &sel : _SelList.get<0>()) {
        if((!pcDoc||pcDoc==sel.pDoc) && getObjectOfType(sel,typeId,resolve))
            iNbr++;
    }

    return iNbr;
}

unsigned int SelectionSingleton::countObjectsOfType(const char* typeName, const char* pDocName, int resolve) const
{
    if (!typeName)
        return size();
    Base::Type typeId = Base::Type::fromName(typeName);
    if (typeId == Base::Type::badType())
        return 0;
    return countObjectsOfType(typeId, pDocName, resolve);
}


void SelectionSingleton::slotSelectionChanged(const SelectionChanges& msg) {
    if(msg.Type == SelectionChanges::ShowSelection ||
       msg.Type == SelectionChanges::HideSelection)
        return;

    if(msg.Object.getSubName().size()) {
        auto pParent = msg.Object.getObject();
        if(!pParent) return;
        std::pair<std::string,std::string> elementName;
        auto &newElementName = elementName.first;
        auto &oldElementName = elementName.second;
        auto pObject = App::GeoFeature::resolveElement(pParent,msg.pSubName,elementName);
        if (!pObject) return;
        SelectionChanges msg2(msg.Type,pObject->getDocument()->getName(),
                pObject->getNameInDocument(),
                newElementName.size()?newElementName.c_str():oldElementName.c_str(),
                pObject->getTypeId().getName(), msg.x,msg.y,msg.z);

        try {
            msg2.pOriginalMsg = &msg;
            signalSelectionChanged3(msg2);

            msg2.Object.setSubName(oldElementName.c_str());
            msg2.pSubName = msg2.Object.getSubName().c_str();
            signalSelectionChanged2(msg2);
        }
        catch (const boost::exception&) {
            // reported by code analyzers
            Base::Console().Warning("slotSelectionChanged: Unexpected boost exception\n");
        }
    }
    else {
        try {
            signalSelectionChanged3(msg);
            signalSelectionChanged2(msg);
        }
        catch (const boost::exception&) {
            // reported by code analyzers
            Base::Console().Warning("slotSelectionChanged: Unexpected boost exception\n");
        }
    }
}

int SelectionSingleton::pushContext(const App::SubObjectT &sobj)
{
    ContextObjectStack.push_back(sobj);
    return (int)ContextObjectStack.size();
}

int SelectionSingleton::popContext()
{
    if (ContextObjectStack.empty())
        return -1;
    ContextObjectStack.pop_back();
    return (int)ContextObjectStack.size();
}

int SelectionSingleton::setContext(const App::SubObjectT &sobj)
{
    if (ContextObjectStack.size())
        ContextObjectStack.back() = sobj;
    return (int)ContextObjectStack.size();
}

const App::SubObjectT &SelectionSingleton::getContext(int pos) const
{
    if (pos >= 0 && pos < (int)ContextObjectStack.size())
        return ContextObjectStack[ContextObjectStack.size() - 1 - pos];
    static App::SubObjectT dummy;
    return dummy;
}

App::SubObjectT SelectionSingleton::getExtendedContext(App::DocumentObject *obj) const
{
    auto checkSel = [this, obj](const App::SubObjectT &sel) {
        auto sobj = sel.getSubObject();
        if (!sobj)
            return false;
        if (!obj || obj == sobj)
            return true;
        if (obj && ContextObjectStack.size()) {
            auto objs = ContextObjectStack.back().getSubObjectList();
            if (std::find(objs.begin(), objs.end(), obj) != objs.end())
                return true;
        }
        return false;
    };

    if (ContextObjectStack.size() && checkSel(ContextObjectStack.back()))
        return ContextObjectStack.back();

    if (checkSel(CurrentPreselection.Object))
        return CurrentPreselection.Object;

    if (!obj) {
        auto sels = getSelectionT(nullptr, 0, true);
        if (sels.size())
            return sels.front();
    } else {
        auto objs = getSelectionT(nullptr, 0);
        for (auto &sel : objs) {
            auto sobj = sel.getSubObject();
            if (sobj == obj)
                return sel;
        }
        for (auto &sel : objs) {
            auto sobjs = sel.getSubObjectList();
            if (std::find(sobjs.begin(), sobjs.end(), obj) != sobjs.end())
                return sel;
        }
    }

    auto gdoc = Application::Instance->editDocument();
    if (gdoc && gdoc == Application::Instance->activeDocument()) {
        auto objT = gdoc->getInEditT();
        if (checkSel(objT))
            return objT;
    }

    return App::SubObjectT();
}

int SelectionSingleton::setPreselect(const char* pDocName, const char* pObjectName, const char* pSubName,
                                     float x, float y, float z, int signal, bool msg)
{
    if(!pDocName || !pObjectName) {
        rmvPreselect();
        return 0;
    }
    if(!pSubName) pSubName = "";

    if(DocName==pDocName && FeatName==pObjectName && SubName==pSubName) {
        if(hx!=x || hy!=y || hz!=z) {
            hx = x;
            hy = y;
            hz = z;
            CurrentPreselection.x = x;
            CurrentPreselection.y = y;
            CurrentPreselection.z = z;

            if (msg)
                format(0,0,0,x,y,z,true);

            // MovePreselect is likely going to slow down large scene rendering.
            // Disable it for now.
#if 0
            SelectionChanges Chng(SelectionChanges::MovePreselect,
                    DocName,FeatName,SubName,std::string(),x,y,z);
            notify(Chng);
#endif
        }
        return -1;
    }

    // Do not restore cursor to prevent causing flash of cursor
    rmvPreselect(/*restoreCursor*/false);

    if (ActiveGate && signal!=1) {
        App::Document* pDoc = getDocument(pDocName);
        if (!pDoc || !pObjectName) {
            ActiveGate->restoreCursor();
            return 0;
        }
        std::pair<std::string,std::string> elementName;
        auto pObject = pDoc->getObject(pObjectName);
        if(!pObject) {
            ActiveGate->restoreCursor();
            return 0;
        }

        const char *subelement = pSubName;
        if(gateResolve) {
            auto &newElementName = elementName.first;
            auto &oldElementName = elementName.second;
            pObject = App::GeoFeature::resolveElement(pObject,pSubName,elementName);
            if (!pObject) {
                ActiveGate->restoreCursor();
                return 0;
            }
            if(gateResolve > 1)
                subelement = newElementName.size()?newElementName.c_str():oldElementName.c_str();
            else
                subelement = oldElementName.c_str();
        }
        if (!ActiveGate->allow(pObject->getDocument(),pObject,subelement)) {
            QString msg;
            if (ActiveGate->notAllowedReason.length() > 0){
                msg = QObject::tr(ActiveGate->notAllowedReason.c_str());
            } else {
                msg = QCoreApplication::translate("SelectionFilter","Not allowed:");
            }
            auto sobjT = App::SubObjectT(pDocName, pObjectName, pSubName);
            msg.append(QStringLiteral(" %1").arg(
                        QString::fromUtf8(sobjT.getSubObjectFullName().c_str())));

            if (getMainWindow()) {
                getMainWindow()->showMessage(msg);
                ActiveGate->setOverrideCursor();

                DocName = pDocName; // So that rmvPreselect() will restore the cursor
            }
            return -2;
        }
        ActiveGate->restoreCursor();
    }

    DocName = pDocName;
    FeatName= pObjectName;
    SubName = pSubName;
    hx = x;
    hy = y;
    hz = z;

    App::SubObjectT sobjT(pDocName, pObjectName, pSubName);
    if (signal == 2 && !SelectionNoTopParentCheck::enabled())
        checkTopParent(sobjT);

    // set up the change object
    SelectionChanges Chng(SelectionChanges::SetPreselect,
                          sobjT, x, y, z, signal);

    CurrentPreselection = Chng;

    auto vp = Application::Instance->getViewProvider(Chng.Object.getObject());
    if (vp) {
        // Trigger populating bounding box cache. This also makes sure the
        // invisible object gets their geometry visual populated.
        vp->getBoundingBox(Chng.Object.getSubNameNoElement().c_str());
    }

    if (msg)
        format(0,0,0,x,y,z,true);

    FC_TRACE("preselect " << sobjT.getSubObjectFullName());
    notify(Chng);

    // It is possible the preselect is removed during notification
    return DocName.empty()?0:1;
}

void SelectionSingleton::setPreselectCoord( float x, float y, float z)
{
    // if nothing is in preselect ignore
    if(CurrentPreselection.Object.getObjectName().empty()) return;

    CurrentPreselection.x = x;
    CurrentPreselection.y = y;
    CurrentPreselection.z = z;

    format(0,0,0,x,y,z,true);
}

QString SelectionSingleton::format(App::DocumentObject *obj,
                                   const char *subname, 
                                   float x, float y, float z,
                                   bool show)
{
    if (!obj || !obj->getNameInDocument())
        return QString();
    return format(obj->getDocument()->getName(), obj->getNameInDocument(), subname, x, y, z, show);
}

QString SelectionSingleton::format(const char *docname,
                                   const char *objname,
                                   const char *subname, 
                                   float x, float y, float z,
                                   bool show)
{
    App::SubObjectT objT(docname?docname:DocName.c_str(),
                         objname?objname:FeatName.c_str(),
                         subname?subname:SubName.c_str());

    QString text;
    QTextStream ts(&text);

    auto sobj = objT.getSubObject();
    if (sobj) {
        int index = -1;
        std::string element = objT.getOldElementName(&index);
        ts << QString::fromUtf8(sobj->getNameInDocument());
        if (index > 0)
            ts << "." << QString::fromUtf8(element.c_str()) << index;
        ts << " | ";
        if (sobj->Label.getStrValue() != sobj->getNameInDocument())
            ts << QString::fromUtf8(sobj->Label.getValue()) << " | ";
    }
    if(x != 0. || y != 0. || z != 0.) {
        auto fmt = [this](double v) -> QString {
            Base::Quantity q(v, Base::Quantity::MilliMetre.getUnit());
            double factor;
            QString unit;
            Base::UnitsApi::schemaTranslate(q, factor, unit);
            QLocale Lc;
            const Base::QuantityFormat& format = q.getFormat();
            if (format.option != Base::QuantityFormat::None) {
                uint opt = static_cast<uint>(format.option);
                Lc.setNumberOptions(static_cast<QLocale::NumberOptions>(opt));
            }
            return QStringLiteral("%1 %2").arg(
                        Lc.toString(v/factor, format.toFormat(),
                                    fmtDecimal<0 ? format.precision : fmtDecimal),
                        unit);
        };
        if (QApplication::queryKeyboardModifiers() == Qt::AltModifier) {
            ts << qSetRealNumberPrecision(std::numeric_limits<double>::digits10 + 1);
            ts << x << "; " << y << "; " << z;
        } else
            ts << fmt(x) << "; " << fmt(y) << "; " << fmt(z);
        ts << QStringLiteral(" | ");
    }

    ts << QString::fromUtf8(objT.getDocumentName().c_str()) << "#" 
       << QString::fromUtf8(objT.getObjectName().c_str()) << "."
       << QString::fromUtf8(objT.getSubName().c_str());

    PreselectionText.clear();
    if (show && getMainWindow()) {
        getMainWindow()->showMessage(text);

        std::vector<std::string> cmds;
        const auto &cmdmap = Gui::Application::Instance->commandManager().getCommands();
        for (auto it = cmdmap.upper_bound("Std_Macro_Presel"); it != cmdmap.end(); ++it) {
            if (!boost::starts_with(it->first, "Std_Macro_Presel"))
                break;
            auto cmd = dynamic_cast<MacroCommand*>(it->second);
            if (cmd && cmd->isPreselectionMacro() && cmd->getOption())
                cmds.push_back(it->first);
        }

        for (auto &name : cmds) {
            auto cmd = dynamic_cast<MacroCommand*>(
                    Gui::Application::Instance->commandManager().getCommandByName(name.c_str()));
            if (cmd)
                cmd->invoke(2);
        }
        if (cmds.empty()) 
            ToolTip::hideText();
        else {
            QPoint pt(ViewParams::getPreselectionToolTipOffsetX(),
                      ViewParams::getPreselectionToolTipOffsetY());
            ToolTip::showText(pt,
                              QString::fromUtf8(PreselectionText.c_str()),
                              Application::Instance->activeView(),
                              true,
                              (ToolTip::Corner)ViewParams::getPreselectionToolTipCorner());
        }
    }

    return text;
}

const std::string &SelectionSingleton::getPreselectionText() const
{
    return PreselectionText;
}

void SelectionSingleton::setPreselectionText(const std::string &txt) 
{
    PreselectionText = txt;
}

void SelectionSingleton::setFormatDecimal(int d)
{
    fmtDecimal = d;
}

void SelectionSingleton::rmvPreselect(bool restoreCursor)
{
    if (DocName == "")
        return;

    SelectionChanges Chng(SelectionChanges::RmvPreselect,DocName,FeatName,SubName);

    // reset the current preselection
    CurrentPreselection = SelectionChanges();

    DocName = "";
    FeatName= "";
    SubName = "";
    hx = 0;
    hy = 0;
    hz = 0;

    if (restoreCursor && ActiveGate) {
        ActiveGate->restoreCursor();
    }

    FC_TRACE("rmv preselect");
    ToolTip::hideText();

    // notify observing objects
    notify(std::move(Chng));

}

const SelectionChanges &SelectionSingleton::getPreselection(void) const
{
    return CurrentPreselection;
}

// add a SelectionGate to control what is selectable
void SelectionSingleton::addSelectionGate(Gui::SelectionGate *gate, int resolve)
{
    if (ActiveGate)
        rmvSelectionGate();

    ActiveGate = gate;
    gateResolve = resolve;
}

// remove the active SelectionGate
void SelectionSingleton::rmvSelectionGate(void)
{
    if (ActiveGate) {
        // Delay deletion to avoid recursion
        std::unique_ptr<Gui::SelectionGate> guard(ActiveGate);
        ActiveGate = nullptr;
        Gui::Document* doc = Gui::Application::Instance->activeDocument();
        if (doc) {
            // if a document is about to be closed it has no MDI view any more
            Gui::MDIView* mdi = doc->getActiveView();
            if (mdi)
                mdi->restoreOverrideCursor();
        }
    }
}

void SelectionGate::setOverrideCursor()
{
    if (Gui::Document* doc = Gui::Application::Instance->activeDocument()) {
        if (Gui::MDIView* mdi = doc->getActiveView())
            mdi->setOverrideCursor(Qt::ForbiddenCursor);
    }
}

void SelectionGate::restoreCursor()
{
    if (Gui::Document* doc = Gui::Application::Instance->activeDocument()) {
        if (Gui::MDIView* mdi = doc->getActiveView())
            mdi->restoreOverrideCursor();
    }
}

App::Document* SelectionSingleton::getDocument(const char* pDocName) const
{
    if (pDocName && pDocName[0])
        return App::GetApplication().getDocument(pDocName);
    else
        return App::GetApplication().getActiveDocument();
}

int SelectionSingleton::disableCommandLog() {
    if(!logDisabled)
        logHasSelection = hasSelection();
    return ++logDisabled;
}

int SelectionSingleton::enableCommandLog(bool silent) {
    --logDisabled;
    if(!logDisabled && !silent) {
        auto manager = Application::Instance->macroManager();
        if(!hasSelection()) {
            if(logHasSelection)
                manager->addLine(MacroManager::Cmt, "Gui.Selection.clearSelection()");
        }else{
            for(const auto &sel : _SelList.get<0>())
                sel.log();
        }
    }
    return logDisabled;
}

void SelectionSingleton::_SelObj::log(bool remove, bool clearPreselect) const {
    if(logged && !remove)
        return;
    logged = true;
    std::ostringstream ss;
    ss << "Gui.Selection." << (remove?"removeSelection":"addSelection")
        << "('" << DocName  << "','" << FeatName << "'";
    if(SubName.size()) {
        // Use old style indexed based selection may have ambiguity, e.g in
        // sketch, where the editing geometry and shape geometry may have the
        // same sub element name but refers to different geometry element.
#if 1
        if(elementName.second.size() && elementName.first.size())
            ss << ",'" << SubName.substr(0,SubName.size()-elementName.first.size())
                << elementName.second << "'";
        else
#endif
            ss << ",'" << SubName << "'";
    }
    if(!remove && (x || y || z || !clearPreselect)) {
        if(SubName.empty())
            ss << ",''";
        ss << ',' << x << ',' << y << ',' << z;
        if(!clearPreselect)
            ss << ",False";
    }
    ss << ')';
    Application::Instance->macroManager()->addLine(MacroManager::Cmt, ss.str().c_str());
}

static bool _SelStackLock;

bool SelectionSingleton::addSelection(const char* pDocName, const char* pObjectName,
        const char* pSubName, float x, float y, float z,
        const std::vector<SelObj> *pickedList, bool clearPreselect)
{
    if(pickedList) {
        _PickedList.clear();
        for(const auto &sel : *pickedList) {
            _PickedList.emplace_back();
            auto &s = _PickedList.back();
            s.DocName = sel.DocName;
            s.FeatName = sel.FeatName;
            s.SubName = sel.SubName;
            s.TypeName = sel.TypeName;
            s.pObject = sel.pObject;
            s.pDoc = sel.pDoc;
            s.x = sel.x;
            s.y = sel.y;
            s.z = sel.z;
        }
        notify(SelectionChanges(SelectionChanges::PickedListChanged));
    }

    _SelObj temp;
    int ret = checkSelection(pDocName,pObjectName,pSubName,0,temp);
    if(ret!=0)
        return false;

    temp.x        = x;
    temp.y        = y;
    temp.z        = z;

    // check for a Selection Gate
    if (ActiveGate) {
        const char *subelement = 0;
        auto pObject = getObjectOfType(temp,App::DocumentObject::getClassTypeId(),gateResolve,&subelement);
        if (!ActiveGate->allow(pObject?pObject->getDocument():temp.pDoc,pObject,subelement)) {
            if (getMainWindow()) {
                QString msg;
                if (ActiveGate->notAllowedReason.length() > 0) {
                    msg = QObject::tr(ActiveGate->notAllowedReason.c_str());
                } else {
                    msg = QCoreApplication::translate("SelectionFilter","Selection not allowed by filter");
                }
                getMainWindow()->showMessage(msg);
                ActiveGate->setOverrideCursor();
            }
            ActiveGate->notAllowedReason.clear();
            QApplication::beep();
            return false;
        }
    }

    if(!logDisabled)
        temp.log(false,clearPreselect);

    _SelList.get<0>().push_back(temp);

    _SelStackForward.clear();
    if (TreeParams::getRecordSelection() && !_SelStackLock)
        _selStackPush(_SelList.size() > 1);

    if(clearPreselect)
        rmvPreselect();

    SelectionChanges Chng(SelectionChanges::AddSelection,
            temp.DocName,temp.FeatName,temp.SubName,temp.TypeName, x,y,z);

    FC_LOG("Add Selection "<<Chng.pDocName<<'#'<<Chng.pObjectName<<'.'<<Chng.pSubName
            << " (" << x << ", " << y << ", " << z << ')');

    auto vp = Application::Instance->getViewProvider(Chng.Object.getObject());
    if (vp) {
        // Trigger populating bounding box cache. This also makes sure the
        // invisible object gets their geometry visual populated.
        vp->getBoundingBox(Chng.Object.getSubNameNoElement().c_str());
    }

    notify(std::move(Chng));

    getMainWindow()->updateActions();

    // There is a possibility that some observer removes or clears selection
    // inside signal handler, hence the check here
    return isSelected(temp.DocName.c_str(),temp.FeatName.c_str(), temp.SubName.c_str());
}

void SelectionSingleton::selStackPush(bool clearForward, bool overwrite) {
    // Change of behavior. If tree view record selection option is active, we'll
    // manage selection recording internally and bypass any external call of
    // stack push.
    if (!TreeParams::getRecordSelection()) {
        if (clearForward)
            _SelStackForward.clear();
        _selStackPush(overwrite);
    }
}

void SelectionSingleton::_selStackPush(bool overwrite) {
    if (SelectionPauseNotification::enabled() || _SelList.empty())
        return;
    if(_SelStackBack.size() >= ViewParams::getSelectionStackSize())
        _SelStackBack.pop_front();
    SelStackItem item;
    for(auto &sel : _SelList.get<0>())
        item.emplace_back(sel.DocName.c_str(),sel.FeatName.c_str(),sel.SubName.c_str());
    if(_SelStackBack.size() && _SelStackBack.back()==item)
        return;
    if(!overwrite || _SelStackBack.empty())
        _SelStackBack.emplace_back();
    _SelStackBack.back() = std::move(item);
}

void SelectionSingleton::selStackGoBack(int count, const std::vector<int> &indices, bool skipEmpty)
{
    Base::StateLocker guard(_SelStackLock);
    if((int)_SelStackBack.size()<count)
        count = _SelStackBack.size();
    if(count<=0)
        return;
    if(_SelList.size()) {
        _selStackPush(true);
        clearCompleteSelection();
    } else
        --count;
    for(int i=0;i<count;++i) {
        _SelStackForward.push_front(std::move(_SelStackBack.back()));
        _SelStackBack.pop_back();
    }
    std::deque<SelStackItem> tmpStack;
    _SelStackForward.swap(tmpStack);
    while(_SelStackBack.size()) {
        bool found = !skipEmpty || !indices.empty();
        int idx = -1;
        for(auto &sobjT : _SelStackBack.back()) {
            ++idx;
            if (!indices.empty() && std::find(indices.begin(), indices.end(), idx) == indices.end())
                continue;
            if(sobjT.getSubObject()) {
                addSelection(sobjT.getDocumentName().c_str(),
                            sobjT.getObjectName().c_str(),
                            sobjT.getSubName().c_str());
                found = true;
            }
        }
        if(found)
            break;
        tmpStack.push_front(std::move(_SelStackBack.back()));
        _SelStackBack.pop_back();
    }
    _SelStackForward = std::move(tmpStack);
    getMainWindow()->updateActions();
    if (TreeParams::getSyncSelection())
        TreeWidget::scrollItemToTop();
}

void SelectionSingleton::selStackGoForward(int count, const std::vector<int> &indices, bool skipEmpty)
{
    Base::StateLocker guard(_SelStackLock);
    if((int)_SelStackForward.size()<count)
        count = _SelStackForward.size();
    if(count<=0)
        return;
    if(_SelList.size()) {
        _selStackPush(true);
        clearCompleteSelection();
    }
    for(int i=0;i<count;++i) {
        _SelStackBack.push_back(_SelStackForward.front());
        _SelStackForward.pop_front();
    }
    std::deque<SelStackItem> tmpStack;
    _SelStackForward.swap(tmpStack);
    while(1) {
        bool found = !skipEmpty || !indices.empty();
        int idx = -1;
        for(auto &sobjT : _SelStackBack.back()) {
            ++idx;
            if (!indices.empty() && std::find(indices.begin(), indices.end(), idx) == indices.end())
                continue;
            if(sobjT.getSubObject()) {
                addSelection(sobjT.getDocumentName().c_str(),
                            sobjT.getObjectName().c_str(),
                            sobjT.getSubName().c_str());
                found = true;
            }
        }
        if(found || tmpStack.empty())
            break;
        _SelStackBack.push_back(tmpStack.front());
        tmpStack.pop_front();
    }
    _SelStackForward = std::move(tmpStack);
    getMainWindow()->updateActions();
    if (TreeParams::getSyncSelection())
        TreeWidget::scrollItemToTop();
}

std::vector<SelectionObject> SelectionSingleton::selStackGet(
        const char* pDocName, int resolve, int index) const
{
    if (!pDocName)
        pDocName = "*";
    const SelStackItem *item = 0;
    if(index>=0) {
        if(index>=(int)_SelStackBack.size())
            return {};
        item = &_SelStackBack[_SelStackBack.size()-1-index];
    }else{
        index = -index-1;
        if(index>=(int)_SelStackForward.size())
            return {};
        item = &_SelStackForward[index];
    }

    SelContainer selList;
    for(auto &sobjT : *item) {
        _SelObj sel;
        if(checkSelection(sobjT.getDocumentName().c_str(),
                          sobjT.getObjectName().c_str(),
                          sobjT.getSubName().c_str(),
                          0,
                          sel,
                          &selList)!=-1)
        {
            selList.get<0>().push_back(sel);
        }
    }

    return getObjectList(pDocName,App::DocumentObject::getClassTypeId(),selList.get<0>(),resolve);
}

std::vector<App::SubObjectT> SelectionSingleton::selStackGetT(
        const char* pDocName, int resolve, int index) const
{
    std::vector<App::SubObjectT> res;
    for (const auto &sel : selStackGet(pDocName, resolve, index)) {
        if (sel.getSubNames().empty())
            res.emplace_back(sel.getDocName(), sel.getFeatName(), "");
        else {
            for (const auto &sub : sel.getSubNames())
                res.emplace_back(sel.getDocName(), sel.getFeatName(), sub.c_str());
        }
    }
    return res;
}

int SelectionSingleton::addSelections(const std::vector<App::SubObjectT> &objs)
{
    if(!logDisabled) {
        std::ostringstream ss;
        ss << "Gui.Selection.addSelections([";
        size_t count = 0;
        for (const auto &objT : objs) {
            ss << objT.getSubObjectPython(false) << ", ";
            if (++count >= 10)
                break;
        }
        ss << "])";
        if (objs.size() > count)
            ss << " # with more objects not shown...";
        Application::Instance->macroManager()->addLine(MacroManager::Cmt, ss.str().c_str());
    }
    int count = 0;
    SelectionPauseNotification guard;
    SelectionLogDisabler disabler(true);
    for (const auto &objT : objs) {
        if (addSelection(objT))
            ++count;
    }
    return count;
}

int SelectionSingleton::addSelections(const char* pDocName, const char* pObjectName, const std::vector<std::string>& pSubNames)
{
    std::vector<App::SubObjectT> objs;
    App::SubObjectT objT(pDocName, pObjectName, "");
    for (const auto &sub : pSubNames) {
        objT.setSubName(sub);
        objs.push_back(objT);
    }
    return addSelections(objs);
}

bool SelectionSingleton::updateSelection(bool show, const char* pDocName,
                            const char* pObjectName, const char* pSubName)
{
    if(!pDocName || !pObjectName)
        return false;
    if(!pSubName)
        pSubName = "";
    auto pDoc = getDocument(pDocName);
    if(!pDoc) return false;
    auto pObject = pDoc->getObject(pObjectName);
    if(!pObject) return false;
    _SelObj sel;
    if(checkSelection(pDocName,pObjectName,pSubName,0,sel,&_SelList)<=0)
        return false;
    if(DocName==sel.DocName && FeatName==sel.FeatName && SubName==sel.SubName) {
        if(show) {
            FC_TRACE("preselect signal");
            notify(SelectionChanges(SelectionChanges::SetPreselect,DocName,FeatName,SubName));
        }else
            rmvPreselect();
    }
    SelectionChanges Chng(show?SelectionChanges::ShowSelection:SelectionChanges::HideSelection,
            sel.DocName, sel.FeatName, sel.SubName, sel.pObject->getTypeId().getName());

    FC_LOG("Update Selection "<<Chng.pDocName << '#' << Chng.pObjectName << '.' <<Chng.pSubName);

    notify(std::move(Chng));

    return true;
}

bool SelectionSingleton::addSelection(const SelectionObject& obj,bool clearPreselect)
{
    const std::vector<std::string>& subNames = obj.getSubNames();
    const std::vector<Base::Vector3d> points = obj.getPickedPoints();
    if (!subNames.empty() && subNames.size() == points.size()) {
        bool ok = true;
        for (std::size_t i=0; i<subNames.size(); i++) {
            const std::string& name = subNames[i];
            const Base::Vector3d& pnt = points[i];
            ok &= addSelection(obj.getDocName(), obj.getFeatName(), name.c_str(),
                               static_cast<float>(pnt.x),
                               static_cast<float>(pnt.y),
                               static_cast<float>(pnt.z),0,clearPreselect);
        }
        return ok;
    }
    else if (!subNames.empty()) {
        bool ok = true;
        for (std::size_t i=0; i<subNames.size(); i++) {
            const std::string& name = subNames[i];
            ok &= addSelection(obj.getDocName(), obj.getFeatName(), name.c_str());
        }
        return ok;
    }
    else {
        return addSelection(obj.getDocName(), obj.getFeatName());
    }
}


void SelectionSingleton::rmvSelection(const char* pDocName, const char* pObjectName, const char* pSubName,
        const std::vector<SelObj> *pickedList)
{
    if(pickedList) {
        _PickedList.clear();
        for(const auto &sel : *pickedList) {
            _PickedList.emplace_back();
            auto &s = _PickedList.back();
            s.DocName = sel.DocName;
            s.FeatName = sel.FeatName;
            s.SubName = sel.SubName;
            s.TypeName = sel.TypeName;
            s.pObject = sel.pObject;
            s.pDoc = sel.pDoc;
            s.x = sel.x;
            s.y = sel.y;
            s.z = sel.z;
        }
        notify(SelectionChanges(SelectionChanges::PickedListChanged));
    }

    if(!pDocName) return;

    _SelObj temp;
    int ret = checkSelection(pDocName,pObjectName,pSubName,0,temp);
    if(ret<0)
        return;

    std::vector<SelectionChanges> changes;
    for(auto It=_SelList.get<0>().begin(),ItNext=It;It!=_SelList.get<0>().end();It=ItNext) {
        ++ItNext;
        if(It->DocName!=temp.DocName || It->FeatName!=temp.FeatName)
            continue;

        // If no sub-element (e.g. Face, Edge) is specified, remove all
        // sub-element selection of the matching sub-object.

        if(!temp.elementName.second.empty()) {
            if (!boost::equals(It->SubName, temp.SubName))
                continue;
        } else if (!boost::starts_with(It->SubName,temp.SubName))
            continue;
        else if (auto element = Data::ComplexGeoData::findElementName(It->SubName.c_str())) {
            if (element - It->SubName.c_str() != (int)temp.SubName.size())
                continue;
        }

        It->log(true);

        changes.emplace_back(SelectionChanges::RmvSelection,
                It->DocName,It->FeatName,It->SubName,It->TypeName);

        // destroy the _SelObj item
        _SelList.get<0>().erase(It);
    }

    // NOTE: It can happen that there are nested calls of rmvSelection()
    // so that it's not safe to invoke the notifications inside the loop
    // as this can invalidate the iterators and thus leads to undefined
    // behaviour.
    // So, the notification is done after the loop, see also #0003469
    if(changes.size()) {
        if (!_SelList.empty()) {
            _SelStackForward.clear();
            if (TreeParams::getRecordSelection() && !_SelStackLock)
                _selStackPush(true);
        }

        for(auto &Chng : changes) {
            FC_LOG("Rmv Selection "<<Chng.pDocName<<'#'<<Chng.pObjectName<<'.'<<Chng.pSubName);
            notify(std::move(Chng));
        }
        getMainWindow()->updateActions();
    }
}

struct SelInfo {
    std::string DocName;
    std::string FeatName;
    std::string SubName;
    SelInfo(const std::string &docName,
            const std::string &featName,
            const std::string &subName)
        :DocName(docName)
        ,FeatName(featName)
        ,SubName(subName)
    {}
};

void SelectionSingleton::setVisible(VisibleState vis, const std::vector<App::SubObjectT> &_sels) {
    std::set<std::pair<App::DocumentObject*,App::DocumentObject*> > filter;
    int visible;
    switch(vis) {
    case VisShow:
        visible = 1;
        break;
    case VisToggle:
        visible = -1;
        break;
    default:
        visible = 0;
    }

    const auto &sels = _sels.size()?_sels:getSelectionT(0,0);
    for(auto &sel : sels) {
        App::DocumentObject *obj = sel.getObject();
        if(!obj) continue;

        // get parent object
        App::DocumentObject *parent = 0;
        std::string elementName;
        obj = obj->resolve(sel.getSubName().c_str(),&parent,&elementName);
        if (!obj || !obj->getNameInDocument() || (parent && !parent->getNameInDocument()))
            continue;
        // try call parent object's setElementVisible
        if (parent) {
            // prevent setting the same object visibility more than once
            if (!filter.insert(std::make_pair(obj,parent)).second)
                continue;
            int visElement = parent->isElementVisible(elementName.c_str());
            if (visElement >= 0) {
                if (visElement > 0)
                    visElement = 1;
                if (visible >= 0) {
                    if (visElement == visible)
                        continue;
                    visElement = visible;
                }
                else {
                    visElement = !visElement;
                }

                if(!visElement)
                    updateSelection(false,
                                    sel.getDocumentName().c_str(),
                                    sel.getObjectName().c_str(),
                                    sel.getSubName().c_str());

                parent->setElementVisible(elementName.c_str(),visElement?true:false);

                if(visElement && ViewParams::getUpdateSelectionVisual())
                    updateSelection(true,
                                    sel.getDocumentName().c_str(),
                                    sel.getObjectName().c_str(),
                                    sel.getSubName().c_str());
                continue;
            }

            // Fall back to direct object visibility setting
        }
        if(!filter.insert(std::make_pair(obj,(App::DocumentObject*)0)).second){
            continue;
        }

        auto vp = Application::Instance->getViewProvider(obj);

        if(vp) {
            bool visObject;
            if(visible>=0)
                visObject = visible ? true : false;
            else
                visObject = !vp->isShow();

            SelectionNoTopParentCheck guard;
            if(visObject) {
                vp->show();
                if(ViewParams::getUpdateSelectionVisual())
                    updateSelection(true,
                                    sel.getDocumentName().c_str(),
                                    sel.getObjectName().c_str(),
                                    sel.getSubName().c_str());
            } else {
                updateSelection(false,
                                sel.getDocumentName().c_str(),
                                sel.getObjectName().c_str(),
                                sel.getSubName().c_str());
                vp->hide();
            }
        }
    }
}

void SelectionSingleton::setSelection(const std::vector<App::DocumentObject*>& sel)
{
    std::vector<App::SubObjectT> objs;
    for (const auto obj : sel)
        objs.emplace_back(obj);
    addSelections(objs);
}

void SelectionSingleton::clearSelection(const char* pDocName, bool clearPreSelect)
{
    // Because the introduction of external editing, it is best to make
    // clearSelection(0) behave as clearCompleteSelection(), which is the same
    // behavior of python Selection.clearSelection(None)
    if (!pDocName || !pDocName[0] || strcmp(pDocName,"*")==0) {
        clearCompleteSelection(clearPreSelect);
        return;
    }

    if (_PickedList.size()) {
        _PickedList.clear();
        notify(SelectionChanges(SelectionChanges::PickedListChanged));
    }

    App::Document* pDoc;
    pDoc = getDocument(pDocName);
    if (pDoc) {
        std::string docName = pDocName;
        if (clearPreSelect && DocName == docName)
            rmvPreselect();

        bool touched = false;
        for (auto it=_SelList.get<0>().begin();it!=_SelList.get<0>().end();) {
            if (it->DocName == docName) {
                touched = true;
                it = _SelList.get<0>().erase(it);
            }
            else {
                ++it;
            }
        }

        if (!touched)
            return;

        if (!logDisabled) {
            std::ostringstream ss;
            ss << "Gui.Selection.clearSelection('" << docName << "'";
            if (!clearPreSelect)
                ss << ", False";
            ss << ')';
            Application::Instance->macroManager()->addLine(MacroManager::Cmt,ss.str().c_str());
        }

        notify(SelectionChanges(SelectionChanges::ClrSelection,docName.c_str()));

        getMainWindow()->updateActions();
    }
}

void SelectionSingleton::clearCompleteSelection(bool clearPreSelect)
{
    if(_PickedList.size()) {
        _PickedList.clear();
        notify(SelectionChanges(SelectionChanges::PickedListChanged));
    }

    if(clearPreSelect)
        rmvPreselect();

    if(_SelList.empty())
        return;

    if(!logDisabled)
        Application::Instance->macroManager()->addLine(MacroManager::Cmt,
                clearPreSelect?"Gui.Selection.clearSelection()"
                              :"Gui.Selection.clearSelection(False)");

    _SelList.clear();

    SelectionChanges Chng(SelectionChanges::ClrSelection);

    FC_LOG("Clear selection");

    notify(std::move(Chng));
    getMainWindow()->updateActions();
}

bool SelectionSingleton::isSelected(const char* pDocName,
        const char* pObjectName, const char* pSubName, int resolve) const
{
    _SelObj sel;
    return checkSelection(pDocName,pObjectName,pSubName,resolve,sel,&_SelList)>0;
}

bool SelectionSingleton::isSelected(App::DocumentObject* pObject, const char* pSubName, int resolve) const
{
    if(!pObject || !pObject->getNameInDocument() || !pObject->getDocument())
        return false;
    _SelObj sel;
    return checkSelection(pObject->getDocument()->getName(),
            pObject->getNameInDocument(),pSubName,resolve,sel,&_SelList)>0;
}

void SelectionSingleton::checkTopParent(App::DocumentObject *&obj, std::string &subname) {
    TreeWidget::checkTopParent(obj,subname);
}

bool SelectionSingleton::checkTopParent(App::SubObjectT &sobjT) {
    auto obj = sobjT.getObject();
    auto subname = sobjT.getSubName();
    auto parent = obj;
    TreeWidget::checkTopParent(parent,subname);
    if (parent != obj) {
        sobjT = App::SubObjectT(parent, subname.c_str());
        return true;
    }
    return false;
}

int SelectionSingleton::checkSelection(const char *pDocName, const char *pObjectName,
        const char *pSubName, int resolve, _SelObj &sel, const SelContainer *selList) const
{
    sel.pDoc = getDocument(pDocName);
    if(!sel.pDoc) {
        if(!selList)
            FC_ERR("Cannot find document");
        return -1;
    }
    pDocName = sel.pDoc->getName();
    sel.DocName = pDocName;

    if(pObjectName)
        sel.pObject = sel.pDoc->getObject(pObjectName);
    else
        sel.pObject = 0;
    if (!sel.pObject) {
        if(!selList)
            FC_ERR("Object not found");
        return -1;
    }
    if(sel.pObject->testStatus(App::ObjectStatus::Remove))
        return -1;
    if(pSubName)
       sel.SubName = pSubName;
    if(!resolve && !SelectionNoTopParentCheck::enabled())
        checkTopParent(sel.pObject,sel.SubName);
    pSubName = sel.SubName.size()?sel.SubName.c_str():0;
    sel.FeatName = sel.pObject->getNameInDocument();
    sel.TypeName = sel.pObject->getTypeId().getName();
    const char *element = 0;
    sel.pResolvedObject = App::GeoFeature::resolveElement(sel.pObject,
            pSubName,sel.elementName,false,App::GeoFeature::Normal,0,&element);
    if(!sel.pResolvedObject) {
        if(!selList)
            FC_ERR("Sub-object " << sel.DocName << '#' << sel.FeatName << '.' << sel.SubName << " not found");
        return -1;
    }
    if(sel.pResolvedObject->testStatus(App::ObjectStatus::Remove))
        return -1;
    std::string subname;
    std::string prefix;
    if(pSubName && element) {
        prefix = std::string(pSubName, element-pSubName);
        if(sel.elementName.first.size()) {
            // make sure the selected sub name is a new style if available
            subname = prefix + sel.elementName.first;
            pSubName = subname.c_str();
            sel.SubName = subname;
        }
    }
    if(!selList)
        selList = &_SelList;

    if(!pSubName)
        pSubName = "";

    const auto &selMap = selList->get<1>();
    auto it = selMap.lower_bound(sel);
    if (it != selMap.end()) {
        auto &s = *it;
        if (s.DocName==pDocName && s.FeatName==sel.FeatName) {
            if(s.SubName==pSubName)
                return 1;
            if(resolve>1 && boost::starts_with(s.SubName,prefix))
                return 1;
        }
    }
    if(resolve==1) {
        const auto &resolvedMap = selList->get<2>();
        for (auto it = resolvedMap.lower_bound(sel.pResolvedObject); it != resolvedMap.end(); ++it) {
            const auto &s = *it;
            if(s.pResolvedObject != sel.pResolvedObject)
                break;
            if(!pSubName[0])
                return 1;
            if(s.elementName.first.size()) {
                if(s.elementName.first == sel.elementName.first)
                    return 1;
            }else if(s.SubName == sel.elementName.second)
                return 1;
        }
    }
    return 0;
}

const char *SelectionSingleton::getSelectedElement(App::DocumentObject *obj, const char* pSubName) const
{
    if (!obj) return 0;

    std::pair<std::string,std::string> elementName;
    if(!App::GeoFeature::resolveElement(obj,pSubName,elementName,true))
        return 0;

    if(elementName.first.size())
        pSubName = elementName.first.c_str();

    for(auto It = _SelList.get<0>().begin();It != _SelList.get<0>().end();++It) {
        if (It->pObject == obj) {
            auto len = It->SubName.length();
            if(!len)
                return "";
            if (pSubName && strncmp(pSubName,It->SubName.c_str(),It->SubName.length())==0){
                if(pSubName[len]==0 || pSubName[len-1] == '.')
                    return It->SubName.c_str();
            }
        }
    }
    return 0;
}

void SelectionSingleton::slotDeletedObject(const App::DocumentObject& Obj)
{
    if(!Obj.getNameInDocument()) return;

    // For safety reason, don't bother checking
    rmvPreselect();

    auto pObj = const_cast<App::DocumentObject*>(&Obj);

    // Remove also from the selection, if selected
    // We don't walk down the hierarchy for each selection, so there may be stray selection
    std::vector<SelectionChanges> changes;
    for(auto it=_SelList.get<2>().lower_bound(pObj); it!=_SelList.get<2>().end(); ) {
        if (it->pResolvedObject != pObj)
            break;
        changes.emplace_back(SelectionChanges::RmvSelection,
                it->DocName,it->FeatName,it->SubName,it->TypeName);
        it = _SelList.get<2>().erase(it);
    }
    for(auto it=_SelList.get<3>().lower_bound(pObj); it!=_SelList.get<3>().end(); ) {
        if (it->pObject != pObj)
            break;
        changes.emplace_back(SelectionChanges::RmvSelection,
                it->DocName,it->FeatName,it->SubName,it->TypeName);
        it = _SelList.get<3>().erase(it);
    }
    if(changes.size()) {
        for(auto &Chng : changes) {
            FC_LOG("Rmv Selection "<<Chng.pDocName<<'#'<<Chng.pObjectName<<'.'<<Chng.pSubName);
            notify(std::move(Chng));
        }
        getMainWindow()->updateActions();
    }

    if(_PickedList.size()) {
        bool changed = false;
        for(auto it=_PickedList.begin(),itNext=it;it!=_PickedList.end();it=itNext) {
            ++itNext;
            auto &sel = *it;
            if(sel.pObject == &Obj) {
                changed = true;
                _PickedList.erase(it);
            }
        }
        if(changed)
            notify(SelectionChanges(SelectionChanges::PickedListChanged));
    }
}


//**************************************************************************
// Construction/Destruction

/**
 * A constructor.
 * A more elaborate description of the constructor.
 */
SelectionSingleton::SelectionSingleton()
    :CurrentPreselection(SelectionChanges::ClrSelection)
    ,_needPickedList(false)
{
    hx = 0;
    hy = 0;
    hz = 0;
    ActiveGate = 0;
    gateResolve = 1;
    App::GetApplication().signalDeletedObject.connect(boost::bind(&Gui::SelectionSingleton::slotDeletedObject, this, bp::_1));
    signalSelectionChanged.connect(boost::bind(&Gui::SelectionSingleton::slotSelectionChanged, this, bp::_1));

    auto hGrp = App::GetApplication().GetParameterGroupByPath(
            "User parameter:BaseApp/Preferences/Units");
    fmtDecimal = hGrp->GetInt("DecimalsPreSel",-1);
}

/**
 * A destructor.
 * A more elaborate description of the destructor.
 */
SelectionSingleton::~SelectionSingleton()
{
}

SelectionSingleton* SelectionSingleton::_pcSingleton = NULL;

SelectionSingleton& SelectionSingleton::instance(void)
{
    if (_pcSingleton == NULL)
        _pcSingleton = new SelectionSingleton;
    return *_pcSingleton;
}

void SelectionSingleton::destruct (void)
{
    if (_pcSingleton != NULL)
        delete _pcSingleton;
    _pcSingleton = 0;
}

//**************************************************************************
// Python stuff

// SelectionSingleton Methods  // Methods structure
PyMethodDef SelectionSingleton::Methods[] = {
    {"addSelection",         (PyCFunction) SelectionSingleton::sAddSelection, METH_VARARGS,
     "Add an object to the selection\n"
     "addSelection(object,[string,float,float,float]\n"
     "--\n"
     "where string is the sub-element name and the three floats represent a 3d point"},
    {"addSelections",        (PyCFunction) SelectionSingleton::sAddSelections, METH_VARARGS,
     "Add a number of objects to the selection\n"
     "addSelections(sels : Sequence[DocumentObject|Tuple[DocumentObject, String]])"},
    {"updateSelection",      (PyCFunction) SelectionSingleton::sUpdateSelection, METH_VARARGS,
     "update an object in the selection\n"
     "updateSelection(show,object,[string])\n"
     "--"
     "where string is the sub-element name and the three floats represent a 3d point"},
    {"removeSelection",      (PyCFunction) SelectionSingleton::sRemoveSelection, METH_VARARGS,
     "Remove an object from the selection"
     "removeSelection(object)"},
    {"clearSelection"  ,     (PyCFunction) SelectionSingleton::sClearSelection, METH_VARARGS,
     "Clear the selection\n"
     "clearSelection(docName='',clearPreSelect=True)\n"
     "--\n"
     "Clear the selection to the given document name. If no document is\n"
     "given the complete selection is cleared."},
    {"isSelected",           (PyCFunction) SelectionSingleton::sIsSelected, METH_VARARGS,
     "Check if a given object is selected\n"
     "isSelected(object,resolve=True)"},
    {"setPreselection",      reinterpret_cast<PyCFunction>(reinterpret_cast<void (*) (void)>( SelectionSingleton::sSetPreselection )), METH_VARARGS|METH_KEYWORDS,
     "Set preselected object\n"
     "setPreselection()"},
    {"getPreselection",      (PyCFunction) SelectionSingleton::sGetPreselection, METH_VARARGS,
     "Get preselected object\n"
     "getPreselection()"},
    {"clearPreselection",   (PyCFunction) SelectionSingleton::sRemPreselection, METH_VARARGS,
     "Clear the preselection\n"
     "clearPreselection()"},
    {"setPreselectionText", (PyCFunction) SelectionSingleton::sSetPreselectionText, METH_VARARGS,
     "setPreselectionText() -- Set preselection message text"},
    {"getPreselectionText", (PyCFunction) SelectionSingleton::sGetPreselectionText, METH_VARARGS,
     "getPreselectionText() -- Get preselected message text"},
    {"countObjectsOfType",   (PyCFunction) SelectionSingleton::sCountObjectsOfType, METH_VARARGS,
     "Get the number of selected objects\n"
     "countObjectsOfType(string, [string],[resolve=1])\n"
     "--\n"
     "The first argument defines the object type e.g. \"Part::Feature\" and the\n"
     "second argumeht defines the document name. If no document name is given the\n"
     "currently active document is used"},
    {"getSelection",         (PyCFunction) SelectionSingleton::sGetSelection, METH_VARARGS,
     "Return a list of selected objects\n"
     "getSelection(docName='',resolve=1,single=False)\n"
     "--\n"
     "docName - document name. Empty string means the active document, and '*' means all document\n"
     "resolve - whether to resolve the subname references.\n"
     "          0: do not resolve, 1: resolve, 2: resolve with element map\n"
     "single - only return if there is only one selection"},
    {"getPickedList",         (PyCFunction) SelectionSingleton::sGetPickedList, 1,
     "Return a list of objects under the last mouse click\n"
     "getPickedList(docName='')\n"
     "--\n"
     "docName - document name. Empty string means the active document, and '*' means all document"},
    {"enablePickedList",      (PyCFunction) SelectionSingleton::sEnablePickedList, METH_VARARGS,
     "Enable/disable picked list\n"
     "enablePickedList(boolean)"},
    {"needPickedList",      (PyCFunction) SelectionSingleton::sNeedPickedList, METH_VARARGS,
     "Check whether picked list is enabled\n"
     "needPickedList() -> boolean"},
    {"getCompleteSelection", (PyCFunction) SelectionSingleton::sGetCompleteSelection, METH_VARARGS,
     "Return a list of selected objects of all documents.\n"
     "getCompleteSelection(resolve=1)"},
    {"getSelectionEx",         (PyCFunction) SelectionSingleton::sGetSelectionEx, METH_VARARGS,
     "Return a list of SelectionObjects\n"
     "getSelectionEx(docName='',resolve=1, single=False)\n"
     "--\n"
     "docName - document name. Empty string means the active document, and '*' means all document\n"
     "resolve - whether to resolve the subname references.\n"
     "          0: do not resolve, 1: resolve, 2: resolve with element map\n"
     "single - only return if there is only one selection\n"
     "The SelectionObjects contain a variety of information about the selection, e.g. sub-element names."},
    {"getSelectionObject",  (PyCFunction) SelectionSingleton::sGetSelectionObject, METH_VARARGS,
     "Return a SelectionObject\n"
     "getSelectionObject(doc,obj,sub,(x,y,z))"},
    {"addObserver",         (PyCFunction) SelectionSingleton::sAddSelObserver, METH_VARARGS,
     "Install an observer\n"
     "addObserver(Object, resolve=1)"},
    {"removeObserver",      (PyCFunction) SelectionSingleton::sRemSelObserver, METH_VARARGS,
     "Uninstall an observer\n"
     "removeObserver(Object)"},
    {"addSelectionGate",      (PyCFunction) SelectionSingleton::sAddSelectionGate, METH_VARARGS,
     "activate the selection gate.\n"
     "addSelectionGate(String|Filter|Gate, resolve=1)\n"
     "--\n"
     "The selection gate will prohibit all selections which do not match\n"
     "the given selection filter string.\n"
     " Examples strings are:\n"
     "'SELECT Part::Feature SUBELEMENT Edge',\n"
     "'SELECT Robot::RobotObject'\n"
     "\n"
     "You can also set an instance of SelectionFilter:\n"
     "filter = Gui.Selection.Filter('SELECT Part::Feature SUBELEMENT Edge')\n"
     "Gui.Selection.addSelectionGate(filter)\n"
     "\n"
     "And the most flexible approach is to write your own selection gate class\n"
     "that implements the method 'allow'\n"
     "class Gate:\n"
     "  def allow(self,doc,obj,sub):\n"
     "    return (sub[0:4] == 'Face')\n"
     "Gui.Selection.addSelectionGate(Gate())"},
    {"removeSelectionGate",      (PyCFunction) SelectionSingleton::sRemoveSelectionGate, METH_VARARGS,
     "remove the active selection gate\n"
     "removeSelectionGate()"},
    {"setVisible",            (PyCFunction) SelectionSingleton::sSetVisible, METH_VARARGS,
     "set visibility of all selection items\n"
     "setVisible(visible=None)\n"
     "--\n"
     "If 'visible' is None, then toggle visibility"},
    {"pushSelStack",      (PyCFunction) SelectionSingleton::sPushSelStack, METH_VARARGS,
     "push current selection to stack\n"
     "pushSelStack(clearForward=True, overwrite=False)\n"
     "--\n"
     "clearForward: whether to clear the forward selection stack.\n"
     "overwrite: overwrite the top back selection stack with current selection."},
    {"hasSelection",      (PyCFunction) SelectionSingleton::sHasSelection, METH_VARARGS,
     "check if there is any selection\n"
     "hasSelection(docName='', resolve=False)"},
    {"hasSubSelection",   (PyCFunction) SelectionSingleton::sHasSubSelection, METH_VARARGS,
     "check if there is any selection with subname\n"
     "hasSubSelection(docName='',subElement=False)"},
    {"getSelectionFromStack",(PyCFunction) SelectionSingleton::sGetSelectionFromStack, METH_VARARGS,
     "Return a list of SelectionObjects from selection stack\n"
     "getSelectionFromStack(docName='',resolve=1,index=0)\n"
     "--\n"
     "docName - document name. Empty string means the active document, and '*' means all document\n"
     "resolve - whether to resolve the subname references.\n"
     "          0: do not resolve, 1: resolve, 2: resolve with element map\n"
     "index - select stack index, 0 is the last pushed selection, positive index to trace further back,\n"
     "          and negative for forward stack item"},
    {"checkTopParent",   (PyCFunction) SelectionSingleton::sCheckTopParent, METH_VARARGS,
     "checkTopParent(obj, subname='')\n\n"
     "Check object hierarchy to find the top parent of the given (sub)object.\n"
     "Returns (topParent,subname)\n"},
    {"pushContext",   (PyCFunction) SelectionSingleton::sPushContext, METH_VARARGS,
     "pushContext(obj, subname='') -> Int\n\n"
     "Push a context object into stack for use by ViewObject.doubleClicked/setupContextMenu().\n"
     "Return the stack size after push."},
    {"popContext",   (PyCFunction) SelectionSingleton::sPopContext, METH_VARARGS,
     "popContext() -> Int\n\n"
      "Pop context object. Return the context stack size after push."},
    {"setContext",   (PyCFunction) SelectionSingleton::sSetContext, METH_VARARGS,
     "setContext(obj, subname='') -> Int\n\n"
     "Set the context object at the top of the stack. No effect if stack is empty.\n"
     "Return the current stack size."},
    {"getContext",   (PyCFunction) SelectionSingleton::sGetContext, METH_VARARGS,
     "getContext(extended = False) -> (obj, subname)\n\n"
     "Obtain the current context sub-object.\n"
     "If extended is True, then try various options in the following order,\n"
     " * Explicitly set context by calling pushContext(),\n"
     " * Current pre-selected object,\n"
     " * If there is only one selection in the active document,\n"
     " * If the active document is in editing, then return the editing object."},
    {NULL, NULL, 0, NULL}  /* Sentinel */
};

PyObject *SelectionSingleton::sAddSelection(PyObject * /*self*/, PyObject *args)
{
    SelectionLogDisabler disabler(true);
    PyObject *clearPreselect = Py_True;
    char *objname;
    char *docname;
    char* subname=0;
    float x=0,y=0,z=0;
    if (PyArg_ParseTuple(args, "ss|sfffO!", &docname, &objname ,
                &subname,&x,&y,&z,&PyBool_Type,&clearPreselect))
    {
        Selection().addSelection(docname,objname,subname,x,y,z,0,PyObject_IsTrue(clearPreselect));
        Py_Return;
    }
    PyErr_Clear();

    PyObject *object;
    subname = 0;
    x=0,y=0,z=0;
    if (PyArg_ParseTuple(args, "O!|sfffO!", &(App::DocumentObjectPy::Type),&object,
                &subname,&x,&y,&z,&PyBool_Type,&clearPreselect))
    {
        App::DocumentObjectPy* docObjPy = static_cast<App::DocumentObjectPy*>(object);
        App::DocumentObject* docObj = docObjPy->getDocumentObjectPtr();
        if (!docObj || !docObj->getNameInDocument()) {
            PyErr_SetString(Base::BaseExceptionFreeCADError, "Cannot check invalid object");
            return NULL;
        }

        Selection().addSelection(docObj->getDocument()->getName(),
                                 docObj->getNameInDocument(),
                                 subname,x,y,z,0,PyObject_IsTrue(clearPreselect));
        Py_Return;
    }

    PyErr_Clear();
    PyObject *sequence;
    if (PyArg_ParseTuple(args, "O!O|O!", &(App::DocumentObjectPy::Type),&object,
                &sequence,&PyBool_Type,&clearPreselect))
    {
        App::DocumentObjectPy* docObjPy = static_cast<App::DocumentObjectPy*>(object);
        App::DocumentObject* docObj = docObjPy->getDocumentObjectPtr();
        if (!docObj || !docObj->getNameInDocument()) {
            PyErr_SetString(Base::BaseExceptionFreeCADError, "Cannot check invalid object");
            return NULL;
        }

        try {
            if (PyTuple_Check(sequence) || PyList_Check(sequence)) {
                Py::Sequence list(sequence);
                for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
                    std::string subname = static_cast<std::string>(Py::String(*it));
                    Selection().addSelection(docObj->getDocument()->getName(),
                                             docObj->getNameInDocument(),
                                             subname.c_str(),0,0,0,0,PyObject_IsTrue(clearPreselect));
                }

                Py_Return;
            }
        }
        catch (const Py::Exception&) {
            // do nothing here
        }
    }

    PyErr_SetString(PyExc_ValueError, "type must be 'DocumentObject[,subname[,x,y,z]]' or 'DocumentObject, list or tuple of subnames'");
    return 0;
}

PyObject *SelectionSingleton::sAddSelections(PyObject * /*self*/, PyObject *args)
{
    SelectionLogDisabler disabler(true);
    PyObject *sequence;
    if (!PyArg_ParseTuple(args, "O", &sequence))
        return nullptr;

    const char *errmsg = "Expect the first argument to be of type Sequence[DocumentObject | Tuple(DocumentObject, String)]";
    if (!PySequence_Check(sequence)) {
        PyErr_SetString(PyExc_TypeError, errmsg);
        return nullptr;
    }

    std::vector<App::SubObjectT> objs;
    Py::Sequence seq(sequence);
    for (Py_ssize_t i=0;i<seq.size();++i) {
        std::string subname;
        PyObject *pyObj = seq[i].ptr();
        if (PySequence_Check(pyObj)) {
            Py::Sequence item(pyObj);
            if (item.size() != 2) {
                PyErr_SetString(PyExc_TypeError, errmsg);
                return nullptr;
            }
            pyObj = item[0].ptr();
            if (!PyUnicode_Check(item[1].ptr())) {
                PyErr_SetString(PyExc_TypeError, errmsg);
                return nullptr;
            }
            subname = PyUnicode_AsUTF8(item[1].ptr());
        }
        if (!PyObject_TypeCheck(pyObj, &App::DocumentObjectPy::Type)) {
            PyErr_SetString(PyExc_TypeError, errmsg);
            return nullptr;
        }
        objs.emplace_back(static_cast<App::DocumentObjectPy*>(pyObj)->getDocumentObjectPtr(), subname.c_str());
    }
    PY_TRY {
        Selection().addSelections(objs);
        Py_Return;
    } PY_CATCH
}

PyObject *SelectionSingleton::sUpdateSelection(PyObject * /*self*/, PyObject *args)
{
    PyObject *show;
    PyObject *object;
    char* subname=0;
    if(!PyArg_ParseTuple(args, "OO!|s", &show,&(App::DocumentObjectPy::Type),&object,&subname))
        return 0;
    App::DocumentObjectPy* docObjPy = static_cast<App::DocumentObjectPy*>(object);
    App::DocumentObject* docObj = docObjPy->getDocumentObjectPtr();
    if (!docObj || !docObj->getNameInDocument()) {
        PyErr_SetString(Base::BaseExceptionFreeCADError, "Cannot check invalid object");
        return NULL;
    }

    Selection().updateSelection(PyObject_IsTrue(show),
            docObj->getDocument()->getName(), docObj->getNameInDocument(), subname);
    Py_Return;
}


PyObject *SelectionSingleton::sRemoveSelection(PyObject * /*self*/, PyObject *args)
{
    SelectionLogDisabler disabler(true);
    char *docname,*objname;
    char* subname=0;
    if(PyArg_ParseTuple(args, "ss|s", &docname,&objname,&subname)) {
        Selection().rmvSelection(docname,objname,subname);
        Py_Return;
    }
    PyErr_Clear();

    PyObject *object;
    subname = 0;
    if (!PyArg_ParseTuple(args, "O!|s", &(App::DocumentObjectPy::Type),&object,&subname))
        return NULL;

    App::DocumentObjectPy* docObjPy = static_cast<App::DocumentObjectPy*>(object);
    App::DocumentObject* docObj = docObjPy->getDocumentObjectPtr();
    if (!docObj || !docObj->getNameInDocument()) {
        PyErr_SetString(Base::BaseExceptionFreeCADError, "Cannot check invalid object");
        return NULL;
    }

    Selection().rmvSelection(docObj->getDocument()->getName(),
                             docObj->getNameInDocument(),
                             subname);

    Py_Return;
}

PyObject *SelectionSingleton::sClearSelection(PyObject * /*self*/, PyObject *args)
{
    SelectionLogDisabler disabler(true);
    PyObject *clearPreSelect = Py_True;
    char *documentName=0;
    if (!PyArg_ParseTuple(args, "|O!", &PyBool_Type, &clearPreSelect)) {
        PyErr_Clear();
        if (!PyArg_ParseTuple(args, "|sO!", &documentName, &PyBool_Type, &clearPreSelect))
            return NULL;
    }
    Selection().clearSelection(documentName,PyObject_IsTrue(clearPreSelect));
    Py_Return;
}

PyObject *SelectionSingleton::sIsSelected(PyObject * /*self*/, PyObject *args)
{
    PyObject *object;
    char* subname=0;
    PyObject *resolve = Py_True;
    if (!PyArg_ParseTuple(args, "O!|sO", &(App::DocumentObjectPy::Type), &object, &subname,&resolve))
        return NULL;

    App::DocumentObjectPy* docObj = static_cast<App::DocumentObjectPy*>(object);
    bool ok = Selection().isSelected(docObj->getDocumentObjectPtr(), subname,PyObject_IsTrue(resolve));
    return Py_BuildValue("O", (ok ? Py_True : Py_False));
}

PyObject *SelectionSingleton::sCountObjectsOfType(PyObject * /*self*/, PyObject *args)
{
    char* objecttype=0;
    char* document=0;
    int resolve = 1;
    if (!PyArg_ParseTuple(args, "|ssi", &objecttype, &document,&resolve))
        return NULL;

    unsigned int count = Selection().countObjectsOfType(objecttype, document, resolve);
    return PyLong_FromLong(count);
}

PyObject *SelectionSingleton::sGetSelection(PyObject * /*self*/, PyObject *args)
{
    char *documentName=0;
    int resolve = 1;
    PyObject *single=Py_False;
    if (!PyArg_ParseTuple(args, "|siO", &documentName,&resolve,&single))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    std::vector<SelectionSingleton::SelObj> sel;
    sel = Selection().getSelection(documentName,resolve,PyObject_IsTrue(single));

    try {
        std::set<App::DocumentObject*> noduplicates;
        std::vector<App::DocumentObject*> selectedObjects; // keep the order of selection
        Py::List list;
        for (std::vector<SelectionSingleton::SelObj>::iterator it = sel.begin(); it != sel.end(); ++it) {
            if (noduplicates.insert(it->pObject).second) {
                selectedObjects.push_back(it->pObject);
            }
        }
        for (std::vector<App::DocumentObject*>::iterator it = selectedObjects.begin(); it != selectedObjects.end(); ++it) {
            list.append(Py::asObject((*it)->getPyObject()));
        }
        return Py::new_reference_to(list);
    }
    catch (Py::Exception&) {
        return 0;
    }
}

PyObject *SelectionSingleton::sEnablePickedList(PyObject * /*self*/, PyObject *args)
{
    PyObject *enable = Py_True;
    if (!PyArg_ParseTuple(args, "|O", &enable))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    Selection().enablePickedList(PyObject_IsTrue(enable));
    Py_Return;
}

PyObject *SelectionSingleton::sNeedPickedList(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    return Py::new_reference_to(Py::Boolean(Selection().needPickedList()));
}

PyObject *SelectionSingleton::sSetPreselection(PyObject * /*self*/, PyObject *args, PyObject *kwd)
{
    PyObject *object;
    char* subname=0;
    float x=0,y=0,z=0;
    int type=1;
    static char *kwlist[] = {"obj","subname","x","y","z","tp",0};
    if (PyArg_ParseTupleAndKeywords(args, kwd, "O!|sfffi", kwlist,
                &(App::DocumentObjectPy::Type),&object,&subname,&x,&y,&z,&type)) {
        App::DocumentObjectPy* docObjPy = static_cast<App::DocumentObjectPy*>(object);
        App::DocumentObject* docObj = docObjPy->getDocumentObjectPtr();
        if (!docObj || !docObj->getNameInDocument()) {
            PyErr_SetString(Base::BaseExceptionFreeCADError, "Cannot check invalid object");
            return NULL;
        }

        Selection().setPreselect(docObj->getDocument()->getName(),
                                 docObj->getNameInDocument(),
                                 subname,x,y,z,type);
        Py_Return;
    }

    PyErr_SetString(PyExc_ValueError, "type must be 'DocumentObject[,subname[,x,y,z]]'");
    return 0;
}

PyObject *SelectionSingleton::sGetPreselection(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    const SelectionChanges& sel = Selection().getPreselection();
    SelectionObject obj(sel);
    return obj.getPyObject();
}

PyObject *SelectionSingleton::sRemPreselection(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Selection().rmvPreselect();
    Py_Return;
}

PyObject *SelectionSingleton::sGetCompleteSelection(PyObject * /*self*/, PyObject *args)
{
    int resolve = 1;
    if (!PyArg_ParseTuple(args, "|i",&resolve))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    std::vector<SelectionSingleton::SelObj> sel;
    sel = Selection().getCompleteSelection(resolve);

    try {
        Py::List list;
        for (std::vector<SelectionSingleton::SelObj>::iterator it = sel.begin(); it != sel.end(); ++it) {
            list.append(Py::asObject(it->pObject->getPyObject()));
        }
        return Py::new_reference_to(list);
    }
    catch (Py::Exception&) {
        return 0;
    }
}

PyObject *SelectionSingleton::sGetSelectionEx(PyObject * /*self*/, PyObject *args)
{
    char *documentName=0;
    int resolve=1;
    PyObject *single = Py_False;
    if (!PyArg_ParseTuple(args, "|siO", &documentName,&resolve,&single))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    std::vector<SelectionObject> sel;
    sel = Selection().getSelectionEx(documentName,
            App::DocumentObject::getClassTypeId(),resolve,PyObject_IsTrue(single));

    try {
        Py::List list;
        for (std::vector<SelectionObject>::iterator it = sel.begin(); it != sel.end(); ++it) {
            list.append(Py::asObject(it->getPyObject()));
        }
        return Py::new_reference_to(list);
    }
    catch (Py::Exception&) {
        return 0;
    }
}

PyObject *SelectionSingleton::sGetPickedList(PyObject * /*self*/, PyObject *args)
{
    char *documentName=0;
    if (!PyArg_ParseTuple(args, "|s", &documentName))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    std::vector<SelectionObject> sel;
    sel = Selection().getPickedListEx(documentName);

    try {
        Py::List list;
        for (std::vector<SelectionObject>::iterator it = sel.begin(); it != sel.end(); ++it) {
            list.append(Py::asObject(it->getPyObject()));
        }
        return Py::new_reference_to(list);
    }
    catch (Py::Exception&) {
        return 0;
    }
}

PyObject *SelectionSingleton::sGetSelectionObject(PyObject * /*self*/, PyObject *args)
{
    char *docName, *objName, *subName;
    PyObject* tuple=0;
    if (!PyArg_ParseTuple(args, "sss|O!", &docName, &objName, &subName,
                                          &PyTuple_Type, &tuple))
        return NULL;

    try {
        SelectionObject selObj;
        selObj.DocName  = docName;
        selObj.FeatName = objName;
        std::string sub = subName;
        if (!sub.empty()) {
            selObj.SubNames.push_back(sub);
            if (tuple) {
                Py::Tuple t(tuple);
                double x = (double)Py::Float(t.getItem(0));
                double y = (double)Py::Float(t.getItem(1));
                double z = (double)Py::Float(t.getItem(2));
                selObj.SelPoses.emplace_back(x,y,z);
            }
        }

        return selObj.getPyObject();
    }
    catch (const Py::Exception&) {
        return 0;
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(Base::BaseExceptionFreeCADError, e.what());
        return 0;
    }
}

PyObject *SelectionSingleton::sAddSelObserver(PyObject * /*self*/, PyObject *args)
{
    PyObject* o;
    int resolve = 1;
    if (!PyArg_ParseTuple(args, "O|i",&o,&resolve))
        return NULL;
    PY_TRY {
        SelectionObserverPython::addObserver(Py::Object(o),resolve);
        Py_Return;
    } PY_CATCH;
}

PyObject *SelectionSingleton::sRemSelObserver(PyObject * /*self*/, PyObject *args)
{
    PyObject* o;
    if (!PyArg_ParseTuple(args, "O",&o))
        return NULL;
    PY_TRY {
        SelectionObserverPython::removeObserver(Py::Object(o));
        Py_Return;
    } PY_CATCH;
}

PyObject *SelectionSingleton::sAddSelectionGate(PyObject * /*self*/, PyObject *args)
{
    char* filter;
    int resolve = 1;
    if (PyArg_ParseTuple(args, "s|i",&filter,&resolve)) {
        PY_TRY {
            Selection().addSelectionGate(new SelectionFilterGate(filter),resolve);
            Py_Return;
        } PY_CATCH;
    }

    PyErr_Clear();
    PyObject* filterPy;
    if (PyArg_ParseTuple(args, "O!|i",SelectionFilterPy::type_object(),&filterPy,resolve)) {
        PY_TRY {
            Selection().addSelectionGate(new SelectionFilterGatePython(
                        static_cast<SelectionFilterPy*>(filterPy)),resolve);
            Py_Return;
        } PY_CATCH;
    }

    PyErr_Clear();
    PyObject* gate;
    if (PyArg_ParseTuple(args, "O|i",&gate,&resolve)) {
        PY_TRY {
            Selection().addSelectionGate(new SelectionGatePython(Py::Object(gate, false)),resolve);
            Py_Return;
        } PY_CATCH;
    }

    PyErr_SetString(PyExc_ValueError, "Argument is neither string nor SelectionFiler nor SelectionGate");
    return 0;
}

PyObject *SelectionSingleton::sRemoveSelectionGate(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    PY_TRY {
        Selection().rmvSelectionGate();
    } PY_CATCH;

    Py_Return;
}

PyObject *SelectionSingleton::sSetVisible(PyObject * /*self*/, PyObject *args)
{
    PyObject *visible = Py_None;
    if (!PyArg_ParseTuple(args, "|O",&visible))
        return NULL;                             // NULL triggers exception

    PY_TRY {
        int vis;
        if(visible == Py_None) {
            vis = -1;
        }
        else if(PyLong_Check(visible)) {
            vis = PyLong_AsLong(visible);
        }
        else {
            vis = PyObject_IsTrue(visible)?1:0;
        }
        if(vis<0)
            Selection().setVisible(VisToggle);
        else
            Selection().setVisible(vis==0?VisHide:VisShow);
    } PY_CATCH;

    Py_Return;
}

PyObject *SelectionSingleton::sPushSelStack(PyObject * /*self*/, PyObject *args)
{
    PyObject *clear = Py_True;
    PyObject *overwrite = Py_False;
    if (!PyArg_ParseTuple(args, "|OO",&clear,&overwrite))
        return NULL;                             // NULL triggers exception

    Selection().selStackPush(PyObject_IsTrue(clear),PyObject_IsTrue(overwrite));
    Py_Return;
}

PyObject *SelectionSingleton::sHasSelection(PyObject * /*self*/, PyObject *args)
{
    const char *doc = 0;
    PyObject *resolve = Py_False;
    if (!PyArg_ParseTuple(args, "|sO",&doc,&resolve))
        return NULL;                             // NULL triggers exception

    PY_TRY {
        bool ret;
        if(doc || PyObject_IsTrue(resolve))
            ret = Selection().hasSelection(doc,PyObject_IsTrue(resolve));
        else
            ret = Selection().hasSelection();
        return Py::new_reference_to(Py::Boolean(ret));
    } PY_CATCH;
}

PyObject *SelectionSingleton::sHasSubSelection(PyObject * /*self*/, PyObject *args)
{
    const char *doc = 0;
    PyObject *subElement = Py_False;
    if (!PyArg_ParseTuple(args, "|sO!",&doc,&PyBool_Type,&subElement))
        return NULL;                             // NULL triggers exception

    PY_TRY {
        return Py::new_reference_to(
                Py::Boolean(Selection().hasSubSelection(doc,PyObject_IsTrue(subElement))));
    } PY_CATCH;
}

PyObject *SelectionSingleton::sGetSelectionFromStack(PyObject * /*self*/, PyObject *args)
{
    char *documentName=0;
    int resolve=1;
    int index=0;
    if (!PyArg_ParseTuple(args, "|sii", &documentName,&resolve,&index))     // convert args: Python->C
        return NULL;                             // NULL triggers exception

    PY_TRY {
        Py::List list;
        for(auto &sel : Selection().selStackGet(documentName, resolve, index))
            list.append(Py::asObject(sel.getPyObject()));
        return Py::new_reference_to(list);
    } PY_CATCH;
}

PyObject* SelectionSingleton::sCheckTopParent(PyObject *, PyObject *args) {
    PyObject *pyObj;
    const char *subname = 0;
    if (!PyArg_ParseTuple(args, "O!|s", &App::DocumentObjectPy::Type,&pyObj,&subname))
        return 0;
    PY_TRY {
        std::string sub;
        if(subname)
            sub = subname;
        App::DocumentObject *obj = static_cast<App::DocumentObjectPy*>(pyObj)->getDocumentObjectPtr();
        checkTopParent(obj,sub);
        return Py::new_reference_to(Py::TupleN(Py::asObject(obj->getPyObject()),Py::String(sub)));
    } PY_CATCH;
}

PyObject *SelectionSingleton::sGetContext(PyObject *, PyObject *args)
{
    PyObject *extended = Py_False;
    int pos = 0;
    if (!PyArg_ParseTuple(args, "|Oi", &extended, &pos))
        return 0;
    App::SubObjectT sobjT;
    if (PyObject_IsTrue(extended))
        sobjT = Selection().getExtendedContext();
    else
        sobjT = Selection().getContext(pos);
    auto obj = sobjT.getObject();
    if (!obj)
        Py_Return;
    return Py::new_reference_to(Py::TupleN(Py::asObject(obj->getPyObject()),
                                           Py::String(sobjT.getSubName().c_str())));
}

PyObject *SelectionSingleton::sPushContext(PyObject *, PyObject *args)
{
    PyObject *pyObj;
    char *subname = "";
    if (!PyArg_ParseTuple(args, "O!|s", &App::DocumentObjectPy::Type,&pyObj,&subname))
        return 0;
    return Py::new_reference_to(Py::Int(Selection().pushContext(App::SubObjectT(
            static_cast<App::DocumentObjectPy*>(pyObj)->getDocumentObjectPtr(), subname))));
}

PyObject *SelectionSingleton::sPopContext(PyObject *, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return 0;
    return Py::new_reference_to(Py::Int(Selection().popContext()));
}

PyObject *SelectionSingleton::sSetContext(PyObject *, PyObject *args)
{
    PyObject *pyObj;
    char *subname = "";
    if (!PyArg_ParseTuple(args, "O!|s", &App::DocumentObjectPy::Type,&pyObj,&subname))
        return 0;
    return Py::new_reference_to(Py::Int(Selection().setContext(App::SubObjectT(
            static_cast<App::DocumentObjectPy*>(pyObj)->getDocumentObjectPtr(), subname))));
}

PyObject *SelectionSingleton::sGetPreselectionText(PyObject *, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return 0;
    return Py::new_reference_to(Py::String(Selection().getPreselectionText()));
}

PyObject *SelectionSingleton::sSetPreselectionText(PyObject *, PyObject *args)
{
    const char *subname;
    if (!PyArg_ParseTuple(args, "s", &subname))
        return 0;
    Selection().setPreselectionText(subname);
    Py_Return;
}
