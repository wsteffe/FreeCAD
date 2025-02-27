/***************************************************************************
 *   Copyright (c) 2014 Yorik van Havre <yorik@uncreated.net>              *
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


#ifndef PATH_ViewProviderPathCompound_H
#define PATH_ViewProviderPathCompound_H

#include "ViewProviderPath.h"

namespace PathGui
{

class PathGuiExport ViewProviderPathCompound: public ViewProviderPath
{
    PROPERTY_HEADER(PathGui::ViewProviderPathCompound);

public:
    ViewProviderPathCompound();

    std::vector<App::DocumentObject*> claimChildren(void)const;
    virtual bool canDragObjects() const;
    virtual void dragObject(App::DocumentObject*);
    virtual bool canDropObjects() const;
    virtual bool canDropObject(App::DocumentObject* obj) const;
    virtual void dropObject(App::DocumentObject*);
    virtual bool canReorderObject(App::DocumentObject* obj, App::DocumentObject* before);
    virtual bool reorderObjects(const std::vector<App::DocumentObject*> &objs, App::DocumentObject* before);

protected:
    virtual bool setEdit(int ModNum);
    virtual void unsetEdit(int ModNum);

};

typedef Gui::ViewProviderPythonFeatureT<ViewProviderPathCompound> ViewProviderPathCompoundPython;

} //namespace PathGui


#endif // PATH_ViewProviderPathCompound_H
