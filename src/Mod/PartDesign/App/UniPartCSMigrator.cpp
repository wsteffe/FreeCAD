/***************************************************************************
 *   Copyright (c) 2025 Walter Steffè <walter.steffe@hierarchical-electromagnetics.com> *
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

#include "UniPartCSMigrator.h"

#include <App/Application.h>        // App::GetApplication(), signals
#include <App/Document.h>
#include <Base/Console.h>

#include "Body.h"                
#include "ResetBodyPlacement.h"  

using Base::Console;

namespace PartDesign {

UniPartCSMigrator* UniPartCSMigrator::_instance = nullptr;

static App::Document* getAppDocByName(const std::string& name) {
    return App::GetApplication().getDocument(name.c_str());
}

bool UniPartCSMigrator::needsMigration(App::Document* doc)
{
    if (!doc) return false;
    auto bodies = doc->getObjectsOfType(PartDesign::Body::getClassTypeId());
    for (auto* o : bodies) {
        auto* body = static_cast<PartDesign::Body*>(o);
        if (!body->Placement.getValue().isIdentity())
            return true;
    }
    return false;
}

UniPartCSMigrator::UniPartCSMigrator()
{
    // mirror WorkflowManager: connect a member slot to App signal
    connectFinishOpenDocument =
        App::GetApplication().signalFinishOpenDocument.connect(
            std::bind(&UniPartCSMigrator::slotFinishOpenDocument, this));
}

UniPartCSMigrator::~UniPartCSMigrator()
{
    if (connectFinishOpenDocument.connected())
        connectFinishOpenDocument.disconnect();
}

void UniPartCSMigrator::slotFinishOpenDocument()
{
    for (App::Document* doc : App::GetApplication().getDocuments()) {
        if (!doc) continue;

        const std::string name = doc->getName();

        // guard: run at most once per document per session
        if (migratedDocs_.count(name))
            continue;

        migratedDocs_.insert(name);  // mark first to avoid reentrancy repeats

        if (needsMigration(doc)) {
            PartDesign::resetBodiesPlacements(doc);  // idempotent; recompute only if changed
            Console().message("[PD-Migrate] UniPartCS migrated: %s\n", name.c_str());
        }
    }
}

// ------- singleton lifecycle (like WorkflowManager) -------
void UniPartCSMigrator::init()
{
    if (!_instance)
        _instance = new UniPartCSMigrator();
}

UniPartCSMigrator* UniPartCSMigrator::instance()
{
    if (!_instance)
        UniPartCSMigrator::init();
    return _instance;
}

void UniPartCSMigrator::destruct()
{
    if (_instance) {
        delete _instance;
        _instance = nullptr;
    }
}

} // namespace PartDesign

