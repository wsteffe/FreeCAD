/***************************************************************************
 *   Copyright (c) 2005 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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
# include <QApplication>
# include <qfileinfo.h>
# include <qdir.h>
# include <QPrinter>
# include <QFileInfo>
# include <QMessageBox>
# include <Inventor/SoInput.h>
# include <Inventor/actions/SoGetPrimitiveCountAction.h>
# include <Inventor/nodes/SoSeparator.h>
# include <QInputDialog>
#endif

#include <xercesc/util/XMLString.hpp>
#include <xercesc/util/TranscodingException.hpp>
#include <boost/regex.hpp>

#include "Action.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Command.h"
#include "Document.h"
#include "MainWindow.h"
#include "Macro.h"
#include "EditorView.h"
#include "PythonEditor.h"
#include "SoFCDB.h"
#include "View3DInventor.h"
#include "SplitView3DInventor.h"
#include "ViewProvider.h"
#include "WaitCursor.h"
#include "PythonWrapper.h"
#include "WidgetFactory.h"
#include "Workbench.h"
#include "WorkbenchManager.h"
#include "Language/Translator.h"
#include "DownloadManager.h"
#include "DlgPreferencesImp.h"
#include "DocumentObserverPython.h"
#include "Action.h"
#include "FileDialog.h"
#include <App/DocumentObjectPy.h>
#include <App/DocumentPy.h>
#include <App/PropertyFile.h>
#include <App/ExpressionParser.h>
#include <Base/Interpreter.h>
#include <Base/Console.h>
#include <CXX/Objects.hxx>
#include <Inventor/MarkerBitmaps.h>

FC_LOG_LEVEL_INIT("Gui", true, true)

using namespace Gui;

// FCApplication Methods						// Methods structure
PyMethodDef Application::Methods[] = {
  {"activateWorkbench",(PyCFunction) Application::sActivateWorkbenchHandler, METH_VARARGS,
   "activateWorkbench(string) -> None\n\n"
   "Activate the workbench by name"},
  {"addWorkbench",     (PyCFunction) Application::sAddWorkbenchHandler, METH_VARARGS,
   "addWorkbench(string, object) -> None\n\n"
   "Add a workbench under a defined name."},
  {"removeWorkbench",  (PyCFunction) Application::sRemoveWorkbenchHandler, METH_VARARGS,
   "removeWorkbench(string) -> None\n\n"
   "Remove the workbench with name"},
  {"getWorkbench",     (PyCFunction) Application::sGetWorkbenchHandler, METH_VARARGS,
   "getWorkbench(string) -> object\n\n"
   "Get the workbench by its name"},
  {"listWorkbenches",   (PyCFunction) Application::sListWorkbenchHandlers, METH_VARARGS,
   "listWorkbenches() -> list\n\n"
   "Show a list of all workbenches"},
  {"activeWorkbench", (PyCFunction) Application::sActiveWorkbenchHandler, METH_VARARGS,
   "activeWorkbench() -> object\n\n"
   "Return the active workbench object"},
  {"addResourcePath",             (PyCFunction) Application::sAddResPath, METH_VARARGS,
   "addResourcePath(string) -> None\n\n"
   "Add a new path to the system where to find resource files\n"
   "like icons or localization files"},
  {"addLanguagePath",             (PyCFunction) Application::sAddLangPath, METH_VARARGS,
   "addLanguagePath(string) -> None\n\n"
   "Add a new path to the system where to find language files"},
  {"addIconPath",             (PyCFunction) Application::sAddIconPath, METH_VARARGS,
   "addIconPath(string) -> None\n\n"
   "Add a new path to the system where to find icon files"},
  {"addIcon",                 (PyCFunction) Application::sAddIcon, METH_VARARGS,
   "addIcon(string, string or list) -> None\n\n"
   "Add an icon as file name or in XPM format to the system"},
  {"getIcon",                 (PyCFunction) Application::sGetIcon, METH_VARARGS,
   "getIcon(key:string, original=False) -> QIcon\n\n"
   "Get an icon in the system"},
  {"getIconContext",          (PyCFunction) Application::sGetIconContext, METH_VARARGS,
   "getIconContext(string) -> list(string)\n\n"
   "Get user defined icon usage context"},
  {"addIconContext",          (PyCFunction) Application::sAddIconContext, METH_VARARGS,
   "addIconContext(string) -> None\n\n"
   "Set user defined icon usage context"},
  {"isIconCached",           (PyCFunction) Application::sIsIconCached, METH_VARARGS,
   "isIconCached(String) -> Bool\n\n"
   "Check if an icon with the given name is cached"},
  {"getIconNames",                 (PyCFunction) Application::sGetIconNames, METH_VARARGS,
   "getIconNames() -> list(string)\n\n"
   "Get all cached icon names in the system"},
  {"getMainWindow",           (PyCFunction) Application::sGetMainWindow, METH_VARARGS,
   "getMainWindow() -> QMainWindow\n\n"
   "Return the main window instance"},
  {"updateGui",               (PyCFunction) Application::sUpdateGui, METH_VARARGS,
   "updateGui() -> None\n\n"
   "Update the main window and all its windows"},
  {"updateLocale",            (PyCFunction) Application::sUpdateLocale, METH_VARARGS,
   "updateLocale() -> None\n\n"
   "Update the localization"},
  {"getLocale",            (PyCFunction) Application::sGetLocale, METH_VARARGS,
   "getLocale() -> string\n\n"
   "Returns the locale currently used by FreeCAD"},
  {"setLocale",            (PyCFunction) Application::sSetLocale, METH_VARARGS,
   "getLocale(string) -> None\n\n"
   "Sets the locale used by FreeCAD. You can set it by\n"
   "top-level domain (e.g. \"de\") or the language name (e.g. \"German\")"},
  {"supportedLocales", (PyCFunction) Application::sSupportedLocales, METH_VARARGS,
   "supportedLocales() -> dict\n\n"
   "Returns a dict of all supported languages/top-level domains"},
  {"createDialog",            (PyCFunction) Application::sCreateDialog, METH_VARARGS,
   "createDialog(string) -- Open a UI file"},
  {"addPreferencePage",       (PyCFunction) Application::sAddPreferencePage, METH_VARARGS,
   "addPreferencePage(string,string) -- Add a UI form to the\n"
   "preferences dialog. The first argument specifies the file name"
   "and the second specifies the group name"},
  {"addCommand",              (PyCFunction) Application::sAddCommand, METH_VARARGS,
   "addCommand(string, object) -> None\n\n"
   "Add a command object"},
  {"runCommand",              (PyCFunction) Application::sRunCommand, METH_VARARGS,
   "runCommand(string) -> None\n\n"
   "Run command with name"},
  {"SendMsgToActiveView",     (PyCFunction) Application::sSendActiveView, METH_VARARGS,
   "deprecated -- use class View"},
  {"sendMsgToFocusView",     (PyCFunction) Application::sSendFocusView, METH_VARARGS,
   "send message to the focused view"},
  {"hide",                    (PyCFunction) Application::sHide, METH_VARARGS,
   "deprecated"},
  {"show",                    (PyCFunction) Application::sShow, METH_VARARGS,
   "deprecated"},
  {"hideObject",              (PyCFunction) Application::sHideObject, METH_VARARGS,
   "hideObject(object) -> None\n\n"
   "Hide the view provider to the given object"},
  {"showObject",              (PyCFunction) Application::sShowObject, METH_VARARGS,
   "showObject(object) -> None\n\n"
   "Show the view provider to the given object"},
  {"open",                    (PyCFunction) Application::sOpen, METH_VARARGS,
   "Open a macro, Inventor or VRML file"},
  {"insert",                  (PyCFunction) Application::sInsert, METH_VARARGS,
   "Open a macro, Inventor or VRML file"},
  {"export",                  (PyCFunction) Application::sExport, METH_VARARGS,
   "save scene to Inventor or VRML file"},
  {"activeDocument",          (PyCFunction) Application::sActiveDocument, METH_VARARGS,
   "activeDocument() -> object or None\n\n"
   "Return the active document or None if no one exists"},
  {"setActiveDocument",       (PyCFunction) Application::sSetActiveDocument, METH_VARARGS,
   "setActiveDocument(string or App.Document) -> None\n\n"
   "Activate the specified document"},
  {"activeView", (PyCFunction)Application::sActiveView, METH_VARARGS,
   "activeView(typename=None) -> object or None\n\n"
   "Return the active view of the active document or None if no one exists" },
  {"activateView", (PyCFunction)Application::sActivateView, METH_VARARGS,
   "activateView(type)\n\n"
   "Activate a view of the given type of the active document"},
  {"editDocument", (PyCFunction)Application::sEditDocument, METH_VARARGS,
   "editDocument() -> object or None\n\n"
   "Return the current editing document or None if no one exists" },
  {"resetEdit", (PyCFunction)Application::sResetEdit, METH_VARARGS,
   "resetEdit()\n\n"
   "Reset current editing document if there is one" },
  {"getDocument",             (PyCFunction) Application::sGetDocument, METH_VARARGS,
   "getDocument(string) -> object\n\n"
   "Get a document by its name"},
  {"doCommand",               (PyCFunction) Application::sDoCommand, METH_VARARGS,
   "doCommand(string) -> None\n\n"
   "Prints the given string in the python console and runs it"},
  {"doCommandGui",               (PyCFunction) Application::sDoCommandGui, METH_VARARGS,
   "doCommandGui(string) -> None\n\n"
   "Prints the given string in the python console and runs it but doesn't record it in macros"},
  {"addModule",               (PyCFunction) Application::sAddModule, METH_VARARGS,
   "addModule(string) -> None\n\n"
   "Prints the given module import only once in the macro recording"},
  {"showDownloads",               (PyCFunction) Application::sShowDownloads, METH_VARARGS,
   "showDownloads() -> None\n\n"
   "Shows the downloads manager window"},
  {"showPreferences",               (PyCFunction) Application::sShowPreferences, METH_VARARGS,
   "showPreferences([string,int]) -> None\n\n"
   "Shows the preferences window. If string and int are provided, the given page index in the given group is shown."},
  {"createViewer",               (PyCFunction) Application::sCreateViewer, METH_VARARGS,
   "createViewer([int]) -> View3DInventor/SplitView3DInventor\n\n"
   "shows and returns a viewer. If the integer argument is given and > 1: -> splitViewer"},

  {"getMarkerIndex", (PyCFunction) Application::sGetMarkerIndex, METH_VARARGS,
   "Get marker index according to marker size setting"},

    {"addDocumentObserver",  (PyCFunction) Application::sAddDocObserver, METH_VARARGS,
     "addDocumentObserver() -> None\n\n"
     "Add an observer to get notified about changes on documents."},
    {"removeDocumentObserver",  (PyCFunction) Application::sRemoveDocObserver, METH_VARARGS,
     "removeDocumentObserver() -> None\n\n"
     "Remove an added document observer."},
    
    {"listUserEditModes", (PyCFunction) Application::sListUserEditModes, METH_VARARGS,
     "listUserEditModes() -> list\n\n"
     "List available user edit modes"},
     
    {"getUserEditMode", (PyCFunction) Application::sGetUserEditMode, METH_VARARGS,
     "getUserEditMode() -> string\n\n"
     "Get current user edit mode"},
     
    {"setUserEditMode", (PyCFunction) Application::sSetUserEditMode, METH_VARARGS,
     "setUserEditMode(string=mode) -> Bool\n\n"
     "Set user edit mode to 'mode', returns True if exists, false otherwise"},

  {"reload",                    (PyCFunction) Application::sReload, METH_VARARGS,
   "reload(name) -> doc\n\n"
   "Reload a partial opened document"},

  {"loadFile",   reinterpret_cast<PyCFunction>(reinterpret_cast<void (*) (void)>( Application::sLoadFile )), METH_VARARGS|METH_KEYWORDS,
   "loadFile(string=filename,[string=module]) -> None\n\n"
   "Loads an arbitrary file by delegating to the given Python module:\n"
   "* If no module is given it will be determined by the file extension.\n"
   "* If more than one module can load a file the first one will be taken.\n"
   "* If no module exists to load the file an exception will be raised."},

  {"coinRemoveAllChildren",     (PyCFunction) Application::sCoinRemoveAllChildren, METH_VARARGS,
   "Remove all children from a group node"},

  {"_setExecFile", (PyCFunction) Application::sSetExecFile, METH_VARARGS,
   "Internal use to inform the file used for exec()"},

  {NULL, NULL, 0, NULL}		/* Sentinel */
};

PyObject* Gui::Application::sEditDocument(PyObject * /*self*/, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))     // convert args: Python->C
		return NULL;                       // NULL triggers exception

	Document *pcDoc = Instance->editDocument();
	if (pcDoc) {
		return pcDoc->getPyObject();
	}
	else {
		Py_Return;
	}
}

PyObject* Gui::Application::sResetEdit(PyObject * /*self*/, PyObject *args)
{
	if (!PyArg_ParseTuple(args, ""))     // convert args: Python->C 
		return NULL;                       // NULL triggers exception 

    PY_TRY {
        Instance->setEditDocument(nullptr);
	    Py_Return;
    } PY_CATCH
}

PyObject* Gui::Application::sActiveDocument(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Document *pcDoc = Instance->activeDocument();
    if (pcDoc) {
        return pcDoc->getPyObject();
    }
    else {
        Py_Return;
    }
}

PyObject* Gui::Application::sActiveView(PyObject * /*self*/, PyObject *args)
{
    const char *typeName=0;
    if (!PyArg_ParseTuple(args, "|s", &typeName))
        return NULL;

    PY_TRY {
        Base::Type type;
        if(typeName) {
            type = Base::Type::fromName(typeName);
            if(type.isBad()) {
                PyErr_Format(PyExc_TypeError, "Invalid type '%s'", typeName);
                return 0;
            }
        }

        Gui::MDIView* mdiView = Instance->activeView();
        if (mdiView && (type.isBad() || mdiView->isDerivedFrom(type))) {
            auto res = Py::asObject(mdiView->getPyObject());
            if(!res.isNone() || !type.isBad())
                return Py::new_reference_to(res);
        }

        if(type.isBad())
            type = Gui::View3DInventor::getClassTypeId();
        Instance->activateView(type, true);
        mdiView = Instance->activeView();
        if (mdiView)
            return mdiView->getPyObject();

        Py_Return;

    } PY_CATCH
}

PyObject* Gui::Application::sActivateView(PyObject * /*self*/, PyObject *args)
{
    char* typeStr;
    PyObject *create = Py_False;
    if (!PyArg_ParseTuple(args, "sO!", &typeStr, &PyBool_Type, &create))
        return NULL;

    Base::Type type = Base::Type::fromName(typeStr);
    Instance->activateView(type, PyObject_IsTrue(create) ? true : false);
    Py_Return;
}

PyObject* Gui::Application::sSetActiveDocument(PyObject * /*self*/, PyObject *args)
{
    Document *pcDoc = 0;

    do {
        char *pstr=0;
        if (PyArg_ParseTuple(args, "s", &pstr)) {
            pcDoc = Instance->getDocument(pstr);
            if (!pcDoc) {
                PyErr_Format(PyExc_NameError, "Unknown document '%s'", pstr);
                return 0;
            }
            break;
        }

        PyErr_Clear();
        PyObject* doc;
        if (PyArg_ParseTuple(args, "O!", &(App::DocumentPy::Type), &doc)) {
            pcDoc = Instance->getDocument(static_cast<App::DocumentPy*>(doc)->getDocumentPtr());
            if (!pcDoc) {
                PyErr_Format(PyExc_KeyError, "Unknown document instance");
                return 0;
            }
            break;
        }
    }
    while(false);

    if (!pcDoc) {
        PyErr_SetString(PyExc_TypeError, "Either string or App.Document expected");
        return 0;
    }

    if (Instance->activeDocument() != pcDoc) {
        Gui::MDIView* view = pcDoc->getActiveView();
        getMainWindow()->setActiveWindow(view);
    }
    Py_Return;
}

PyObject* Application::sGetDocument(PyObject * /*self*/, PyObject *args)
{
    char *pstr=0;
    if (PyArg_ParseTuple(args, "s", &pstr)) {
        Document *pcDoc = Instance->getDocument(pstr);
        if (!pcDoc) {
            PyErr_Format(PyExc_NameError, "Unknown document '%s'", pstr);
            return 0;
        }
        return pcDoc->getPyObject();
    }

    PyErr_Clear();
    PyObject* doc;
    if (PyArg_ParseTuple(args, "O!", &(App::DocumentPy::Type), &doc)) {
        Document *pcDoc = Instance->getDocument(static_cast<App::DocumentPy*>(doc)->getDocumentPtr());
        if (!pcDoc) {
            PyErr_Format(PyExc_KeyError, "Unknown document instance");
            return 0;
        }
        return pcDoc->getPyObject();
    }

    PyErr_SetString(PyExc_TypeError, "Either string or App.Document exprected");
    return 0;
}

PyObject* Application::sHide(PyObject * /*self*/, PyObject *args)
{
    char *psFeatStr;
    if (!PyArg_ParseTuple(args, "s;Name of the object to hide has to be given!",&psFeatStr))
        return NULL;

    Document *pcDoc = Instance->activeDocument();

    if (pcDoc)
        pcDoc->setHide(psFeatStr);

    Py_Return;
}

PyObject* Application::sShow(PyObject * /*self*/, PyObject *args)
{
    char *psFeatStr;
    if (!PyArg_ParseTuple(args, "s;Name of the object to show has to be given!",&psFeatStr))
        return NULL;

    Document *pcDoc = Instance->activeDocument();

    if (pcDoc)
        pcDoc->setShow(psFeatStr);

    Py_Return;
}

PyObject* Application::sHideObject(PyObject * /*self*/, PyObject *args)
{
    PyObject *object;
    if (!PyArg_ParseTuple(args, "O!",&(App::DocumentObjectPy::Type),&object))
        return 0;

    App::DocumentObject* obj = static_cast<App::DocumentObjectPy*>(object)->getDocumentObjectPtr();
    Instance->hideViewProvider(obj);

    Py_Return;
}

PyObject* Application::sShowObject(PyObject * /*self*/, PyObject *args)
{
    PyObject *object;
    if (!PyArg_ParseTuple(args, "O!",&(App::DocumentObjectPy::Type),&object))
        return 0;

    App::DocumentObject* obj = static_cast<App::DocumentObjectPy*>(object)->getDocumentObjectPtr();
    Instance->showViewProvider(obj);

    Py_Return;
}

PyObject* Application::sOpen(PyObject * /*self*/, PyObject *args)
{
    // only used to open Python files
    char* Name;
    if (!PyArg_ParseTuple(args, "et","utf-8",&Name))
        return NULL;
    std::string Utf8Name = std::string(Name);
    PyMem_Free(Name);
    PY_TRY {
        QString fileName = QString::fromUtf8(Utf8Name.c_str());
        QFileInfo fi;
        fi.setFile(fileName);
        QString ext = fi.suffix().toLower();
        QList<EditorView*> views = getMainWindow()->findChildren<EditorView*>();
        for (QList<EditorView*>::Iterator it = views.begin(); it != views.end(); ++it) {
            if ((*it)->fileName() == fileName) {
                (*it)->setFocus();
                Py_Return;
            }
        }

        if (ext == QStringLiteral("iv")) {
            if (!Application::Instance->activeDocument())
                App::GetApplication().newDocument();
            //QString cmd = QString("Gui.activeDocument().addAnnotation(\"%1\",\"%2\")").arg(fi.baseName()).arg(fi.absoluteFilePath());
            QString cmd = QStringLiteral(
                "App.ActiveDocument.addObject(\"App::InventorObject\",\"%1\")."
                "FileName=\"%2\"\n"
                "App.ActiveDocument.ActiveObject.Label=\"%1\"\n"
                "App.ActiveDocument.recompute()")
                .arg(fi.baseName(), fi.absoluteFilePath());
            Base::Interpreter().runString(cmd.toUtf8());
        }
        else if (ext == QStringLiteral("wrl") ||
                 ext == QStringLiteral("vrml") ||
                 ext == QStringLiteral("wrz")) {
            if (!Application::Instance->activeDocument())
                App::GetApplication().newDocument();

            // Add this to the search path in order to read inline files (#0002029)
            QByteArray path = fi.absolutePath().toUtf8();
            SoInput::addDirectoryFirst(path.constData());

            //QString cmd = QString("Gui.activeDocument().addAnnotation(\"%1\",\"%2\")").arg(fi.baseName()).arg(fi.absoluteFilePath());
            QString cmd = QStringLiteral(
                "App.ActiveDocument.addObject(\"App::VRMLObject\",\"%1\")."
                "VrmlFile=\"%2\"\n"
                "App.ActiveDocument.ActiveObject.Label=\"%1\"\n"
                "App.ActiveDocument.recompute()")
                .arg(fi.baseName(), fi.absoluteFilePath());
            Base::Interpreter().runString(cmd.toUtf8());
            SoInput::removeDirectory(path.constData());
        }
        else if (ext == QStringLiteral("py") || ext == QStringLiteral("fcmacro") ||
                 ext == QStringLiteral("fcscript")) {
            PythonEditor* editor = new PythonEditor();
            editor->setWindowIcon(Gui::BitmapFactory().iconFromTheme("applications-python"));
            PythonEditorView* edit = new PythonEditorView(editor, getMainWindow());
            edit->open(fileName);
            edit->resize(400, 300);
            getMainWindow()->addWindow( edit );
        }
        else {
            Base::Console().Error("File type '%s' not supported\n", ext.toUtf8().constData());
        }
    } PY_CATCH;

    Py_Return;
}

PyObject* Application::sInsert(PyObject * /*self*/, PyObject *args)
{
    char* Name;
    char* DocName=0;
    if (!PyArg_ParseTuple(args, "et|s","utf-8",&Name,&DocName))
        return NULL;
    std::string Utf8Name = std::string(Name);
    PyMem_Free(Name);

    PY_TRY {
        QString fileName = QString::fromUtf8(Utf8Name.c_str());
        QFileInfo fi;
        fi.setFile(fileName);
        QString ext = fi.suffix().toLower();
        if (ext == QStringLiteral("iv")) {
            App::Document *doc = 0;
            if (DocName)
                doc = App::GetApplication().getDocument(DocName);
            else
                doc = App::GetApplication().getActiveDocument();
            if (!doc)
                doc = App::GetApplication().newDocument(DocName);

            App::DocumentObject* obj = doc->addObject("App::InventorObject",
                (const char*)fi.baseName().toUtf8());
            obj->Label.setValue((const char*)fi.baseName().toUtf8());
            static_cast<App::PropertyString*>(obj->getPropertyByName("FileName"))
                ->setValue((const char*)fi.absoluteFilePath().toUtf8());
            doc->recompute();
        }
        else if (ext == QStringLiteral("wrl") ||
                 ext == QStringLiteral("vrml") ||
                 ext == QStringLiteral("wrz")) {
            App::Document *doc = 0;
            if (DocName)
                doc = App::GetApplication().getDocument(DocName);
            else
                doc = App::GetApplication().getActiveDocument();
            if (!doc)
                doc = App::GetApplication().newDocument(DocName);

            // Add this to the search path in order to read inline files (#0002029)
            QByteArray path = fi.absolutePath().toUtf8();
            SoInput::addDirectoryFirst(path.constData());

            App::DocumentObject* obj = doc->addObject("App::VRMLObject",
                (const char*)fi.baseName().toUtf8());
            obj->Label.setValue((const char*)fi.baseName().toUtf8());
            static_cast<App::PropertyFileIncluded*>(obj->getPropertyByName("VrmlFile"))
                ->setValue((const char*)fi.absoluteFilePath().toUtf8());
            doc->recompute();

            SoInput::removeDirectory(path.constData());
        }
        else if (ext == QStringLiteral("py") || ext == QStringLiteral("fcmacro") ||
                 ext == QStringLiteral("fcscript")) {
            PythonEditor* editor = new PythonEditor();
            editor->setWindowIcon(Gui::BitmapFactory().iconFromTheme("applications-python"));
            PythonEditorView* edit = new PythonEditorView(editor, getMainWindow());
            edit->open(fileName);
            edit->resize(400, 300);
            getMainWindow()->addWindow( edit );
        }
        else {
            Base::Console().Error("File type '%s' not supported\n", ext.toUtf8().constData());
        }
    } PY_CATCH;

    Py_Return;
}

PyObject* Application::sExport(PyObject * /*self*/, PyObject *args)
{
    PyObject* object;
    char* Name;
    if (!PyArg_ParseTuple(args, "Oet",&object,"utf-8",&Name))
        return NULL;
    std::string Utf8Name = std::string(Name);
    PyMem_Free(Name);

    PY_TRY {
        App::Document* doc = 0;
        Py::Sequence list(object);
        for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
            PyObject* item = (*it).ptr();
            if (PyObject_TypeCheck(item, &(App::DocumentObjectPy::Type))) {
                App::DocumentObject* obj = static_cast<App::DocumentObjectPy*>(item)->getDocumentObjectPtr();
                doc = obj->getDocument();
                break;
            }
        }

        QString fileName = QString::fromUtf8(Utf8Name.c_str());
        QFileInfo fi;
        fi.setFile(fileName);
        QString ext = fi.suffix().toLower();
        if (ext == QStringLiteral("iv") ||
            ext == QStringLiteral("wrl") ||
            ext == QStringLiteral("vrml") ||
            ext == QStringLiteral("wrz") ||
            ext == QStringLiteral("x3d") ||
            ext == QStringLiteral("x3dz") ||
            ext == QStringLiteral("xhtml")) {

            // build up the graph
            SoSeparator* sep = new SoSeparator();
            sep->ref();

            for (Py::Sequence::iterator it = list.begin(); it != list.end(); ++it) {
                PyObject* item = (*it).ptr();
                if (PyObject_TypeCheck(item, &(App::DocumentObjectPy::Type))) {
                    App::DocumentObject* obj = static_cast<App::DocumentObjectPy*>(item)->getDocumentObjectPtr();

                    Gui::ViewProvider* vp = Gui::Application::Instance->getViewProvider(obj);
                    if (vp) {
                        sep->addChild(vp->getRoot());
                    }
                }
            }


            SoGetPrimitiveCountAction action;
            action.setCanApproximate(true);
            action.apply(sep);

            bool binary = false;
            if (action.getTriangleCount() > 100000 ||
                action.getPointCount() > 30000 ||
                action.getLineCount() > 10000)
                binary = true;

            SoFCDB::writeToFile(sep, Utf8Name.c_str(), binary);
            sep->unref();
        }
        else if (ext == QStringLiteral("pdf")) {
            // get the view that belongs to the found document
            Gui::Document* gui_doc = Application::Instance->getDocument(doc);
            if (gui_doc) {
                Gui::MDIView* view = gui_doc->getActiveView();
                if (view) {
                    View3DInventor* view3d = qobject_cast<View3DInventor*>(view);
                    if (view3d)
                        view3d->viewAll();
                    QPrinter printer(QPrinter::ScreenResolution);
                    printer.setOutputFormat(QPrinter::PdfFormat);
                    printer.setOutputFileName(fileName);
                    view->print(&printer);
                }
            }
        }
        else {
            Base::Console().Error("File type '%s' not supported\n", ext.toUtf8().constData());
        }
    } PY_CATCH;

    Py_Return;
}

PyObject* Application::sSendActiveView(PyObject * /*self*/, PyObject *args)
{
    char *psCommandStr;
    PyObject *suppress=Py_False;
    if (!PyArg_ParseTuple(args, "s|O!",&psCommandStr,&PyBool_Type,&suppress))
        return NULL;

    const char* ppReturn=0;
    if (!Instance->sendMsgToActiveView(psCommandStr,&ppReturn)) {
        if (!PyObject_IsTrue(suppress))
            Base::Console().Warning("Unknown view command: %s\n",psCommandStr);
    }

    // Print the return value to the output
    if (ppReturn) {
        return Py_BuildValue("s",ppReturn);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sSendFocusView(PyObject * /*self*/, PyObject *args)
{
    char *psCommandStr;
    PyObject *suppress=Py_False;
    if (!PyArg_ParseTuple(args, "s|O!",&psCommandStr,&PyBool_Type,&suppress))
        return NULL;

    const char* ppReturn=0;
    if (!Instance->sendMsgToFocusView(psCommandStr,&ppReturn)) {
        if (!PyObject_IsTrue(suppress))
            Base::Console().Warning("Unknown view command: %s\n",psCommandStr);
    }

    // Print the return value to the output
    if (ppReturn) {
        return Py_BuildValue("s",ppReturn);
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sGetMainWindow(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    PythonWrapper wrap;
    if (!wrap.loadCoreModule() ||
        !wrap.loadGuiModule() ||
        !wrap.loadWidgetsModule()) {
        PyErr_SetString(PyExc_RuntimeError, "Failed to load Python wrapper for Qt");
        return 0;
    }
    try {
        return Py::new_reference_to(wrap.fromQWidget(Gui::getMainWindow(), "QMainWindow"));
    }
    catch (const Py::Exception&) {
        return 0;
    }
}

PyObject* Application::sUpdateGui(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    qApp->processEvents(QEventLoop::ExcludeUserInputEvents);

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sUpdateLocale(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Translator::instance()->refresh();

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sGetLocale(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    std::string locale = Translator::instance()->activeLanguage();
    return PyUnicode_FromString(locale.c_str());
}

PyObject* Application::sSetLocale(PyObject * /*self*/, PyObject *args)
{
    char* name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    std::string cname(name);
    TStringMap map = Translator::instance()->supportedLocales();
    map["English"] = "en";
    for (const auto& it : map) {
        if (it.first == cname || it.second == cname) {
            Translator::instance()->activateLanguage(it.first.c_str());
            break;
        }
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sSupportedLocales(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    TStringMap map = Translator::instance()->supportedLocales();
    Py::Dict dict;
    dict.setItem(Py::String("English"), Py::String("en"));
    for (const auto& it : map) {
        Py::String key(it.first);
        Py::String val(it.second);
        dict.setItem(key, val);
    }
    return Py::new_reference_to(dict);
}

PyObject* Application::sCreateDialog(PyObject * /*self*/, PyObject *args)
{
    char* fn = 0;
    if (!PyArg_ParseTuple(args, "s", &fn))
        return NULL;

    PyObject* pPyResource=0L;
    try{
        pPyResource = new PyResource();
        ((PyResource*)pPyResource)->load(fn);
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(PyExc_AssertionError, e.what());
        return NULL;
    }

    return pPyResource;
}

PyObject* Application::sAddPreferencePage(PyObject * /*self*/, PyObject *args)
{
    char *fn, *grp;
    if (PyArg_ParseTuple(args, "ss", &fn,&grp)) {
        QFileInfo fi(QString::fromUtf8(fn));
        if (!fi.exists()) {
            PyErr_SetString(PyExc_RuntimeError, "UI file does not exist");
            return 0;
        }

        // add to the preferences dialog
        new PrefPageUiProducer(fn, grp);

        Py_INCREF(Py_None);
        return Py_None;
    }
    PyErr_Clear();

    PyObject* dlg;
    // old style classes
    if (PyArg_ParseTuple(args, "O!s", &PyType_Type, &dlg, &grp)) {
        // add to the preferences dialog
        new PrefPagePyProducer(Py::Object(dlg), grp);

        Py_INCREF(Py_None);
        return Py_None;
    }
    PyErr_Clear();

    // new style classes
    if (PyArg_ParseTuple(args, "O!s", &PyType_Type, &dlg, &grp)) {
        // add to the preferences dialog
        new PrefPagePyProducer(Py::Object(dlg), grp);

        Py_INCREF(Py_None);
        return Py_None;
    }

    return 0;
}

PyObject* Application::sActivateWorkbenchHandler(PyObject * /*self*/, PyObject *args)
{
    char*       psKey;
    if (!PyArg_ParseTuple(args, "s", &psKey))
        return NULL;

    // search for workbench handler from the dictionary
    PyObject* pcWorkbench = PyDict_GetItemString(Instance->_pcWorkbenchDictionary, psKey);
    if (!pcWorkbench) {
        PyErr_Format(PyExc_KeyError, "No such workbench '%s'", psKey);
        return NULL;
    }

    try {
        bool ok = Instance->activateWorkbench(psKey);
        return Py::new_reference_to(Py::Boolean(ok));
    }
    catch (const Base::Exception& e) {
        std::stringstream err;
        err << psKey << ": " << e.what();
        PyErr_SetString(Base::BaseExceptionFreeCADError, err.str().c_str());
        return 0;
    }
    catch (const XERCES_CPP_NAMESPACE_QUALIFIER TranscodingException& e) {
        std::stringstream err;
        char *pMsg = XERCES_CPP_NAMESPACE_QUALIFIER XMLString::transcode(e.getMessage());
        err << "Transcoding exception in Xerces-c:\n\n"
            << "Transcoding exception raised in activateWorkbench.\n"
            << "Check if your user configuration file is valid.\n"
            << "  Exception message:"
            << pMsg;
        XERCES_CPP_NAMESPACE_QUALIFIER XMLString::release(&pMsg);
        PyErr_SetString(PyExc_RuntimeError, err.str().c_str());
        return 0;
    }
    catch (...) {
        std::stringstream err;
        err << "Unknown C++ exception raised in activateWorkbench('" << psKey << "')";
        PyErr_SetString(Base::BaseExceptionFreeCADError, err.str().c_str());
        return 0;
    }
}

static std::string _getCurrentPythonFile(const std::string &execFile)
{
    Py::Module mod(PyImport_ImportModule("inspect"), true);
    if (mod.isNull()) {
        PyErr_SetString(PyExc_ImportError, "Cannot load inspect module");
        return std::string();
    }
    Py::Callable inspect(mod.getAttr("stack"));
    Py::List list(inspect.apply());

    std::string file;
    // usually this is the file name of the calling script
#if (PY_MAJOR_VERSION > 3 || (PY_MAJOR_VERSION==3 && PY_MINOR_VERSION>=5))
    Py::Object info = list.getItem(0);
    PyObject *pyfile = PyStructSequence_GET_ITEM(*info,1);
    if(!pyfile)
        throw Py::Exception();
    file = Py::Object(pyfile).as_string();
#else
    Py::Tuple info = list.getItem(0);
    file = info.getItem(1).as_string();
#endif

    if (file == "<string>" && execFile.size())
        return execFile;
    return file;
}

class StdCmdWorkbenchItem: public Command
{
public:
    StdCmdWorkbenchItem(const char *wb, Py::Object pyObj)
        :Command(wb)
    {
        sGroup = QT_TR_NOOP("Workbench");
        _cmdName = cmdName(wb);
        sName = _cmdName.c_str();
        eType = NoTransaction;

        _pixmap = std::string("Icon_") + wb;
        sPixmap = _pixmap.c_str();
        // sPixmap stores the key to the icon cache. Calling workbenchIcon()
        // below to make sure the cache is populated
        Application::Instance->workbenchIcon(QString::fromUtf8(workbenchName()));

        try {
            Py::Object member = pyObj.getAttr(std::string("MenuText"));
            if (member.isString()) {
                _menuText = Py::String(member).as_std_string("utf-8");
                sMenuText = _menuText.c_str();
            }
        } catch (Py::Exception &e) {
            e.clear();
        }
        try {
            Py::Object member = pyObj.getAttr(std::string("ToolTip"));
            if (member.isString()) {
                _toolTips = Py::String(member).as_std_string("utf-8");
                sStatusTip = sToolTipText = _toolTips.c_str();
            }
        } catch (Py::Exception &e) {
            e.clear();
        }
    }

    const char *className() const {return "StdCmdWorkbenchItem";}

    Action *createAction()
    {
        Action *pcAction = new Action(this, getMainWindow());
        pcAction->setText(QString::fromUtf8(_menuText.c_str()));
        QString toolTips = QString::fromUtf8(_toolTips.c_str());
        pcAction->setToolTip(toolTips);
        pcAction->setStatusTip(toolTips);
        pcAction->setWhatsThis(QString::fromUtf8(_cmdName.c_str()));
        pcAction->setIcon(BitmapFactory().pixmap(_pixmap.c_str()));
        return pcAction;
    }

    bool isActive()
    {
        return true;
    }

    void activated(int)
    {
        try {
            Workbench* w = WorkbenchManager::instance()->active();
            if (w && w->name() == workbenchName())
                return;
            doCommand(Gui, "Gui.activateWorkbench(\"%s\")", workbenchName());
        }
        catch(const Base::Exception& e) {
            e.ReportException();
            QString msg(QString::fromUtf8(e.what()));
            // ignore '<type 'exceptions.*Error'>' prefixes
            QRegExp rx;
            rx.setPattern(QStringLiteral("^\\s*<type 'exceptions.\\w*'>:\\s*"));
            int pos = rx.indexIn(msg);
            if (pos != -1)
                msg = msg.mid(rx.matchedLength());
            QMessageBox::critical(getMainWindow(), QObject::tr("Cannot load workbench"), msg); 
        }
        catch(...) {
            QMessageBox::critical(getMainWindow(), QObject::tr("Cannot load workbench"), 
                QObject::tr("A general error occurred while loading the workbench")); 
        }
    }

    static std::string cmdName(const char *wb)
    {
        if (!wb || !wb[0])
            return std::string();
        return std::string("Std_Workbench_") + wb;
    }

    const char *workbenchName()
    {
        if (_cmdName.size() < 14)
            return "";
        return _cmdName.c_str() + 14;
    }

    static void add(const char *wb, const Py::Object &pyObj)
    {
        if (Base::streq(wb, "<none>"))
            return;
        auto &manager = Application::Instance->commandManager();
        Command *cmd = manager.getCommandByName(cmdName(wb).c_str());
        if (!cmd)
            manager.addCommand(new StdCmdWorkbenchItem(wb, pyObj));
    }

    static void remove(const char *wb)
    {
        auto &manager = Application::Instance->commandManager();
        Command *cmd = manager.getCommandByName(cmdName(wb).c_str());
        if (cmd)
            manager.removeCommand(cmd);
    }

    std::string _cmdName;
    std::string _pixmap;
    std::string _menuText;
    std::string _toolTips;
};

PyObject* Application::sAddWorkbenchHandler(PyObject * /*self*/, PyObject *args)
{
    PyObject*   pcObject;
    std::string item;
    if (!PyArg_ParseTuple(args, "O", &pcObject))
        return NULL;

    try {
        // get the class object 'Workbench' from the main module that is expected
        // to be base class for all workbench classes
        Py::Module module("__main__");
        Py::Object baseclass(module.getAttr(std::string("Workbench")));

        // check whether it is an instance or class object
        Py::Object object(pcObject);
        Py::String name;

        if (PyObject_IsSubclass(object.ptr(), baseclass.ptr()) == 1) {
            // create an instance of this class
            name = object.getAttr(std::string("__name__"));
            Py::Tuple arg;
            Py::Callable creation(object);
            object = creation.apply(arg);
        }
        else if (PyObject_IsInstance(object.ptr(), baseclass.ptr()) == 1) {
            // extract the class name of the instance
            PyErr_Clear(); // PyObject_IsSubclass set an exception
            Py::Object classobj = object.getAttr(std::string("__class__"));
            name = classobj.getAttr(std::string("__name__"));
        }
        else {
            PyErr_SetString(PyExc_TypeError, "arg must be a subclass or an instance of "
                                             "a subclass of 'Workbench'");
            return NULL;
        }

        // Search for some methods and members without invoking them
        Py::Callable(object.getAttr(std::string("Initialize")));
        Py::Callable(object.getAttr(std::string("GetClassName")));
        item = name.as_std_string("ascii");

        PyObject* wb = PyDict_GetItemString(Instance->_pcWorkbenchDictionary,item.c_str());
        if (wb) {
            PyErr_Format(PyExc_KeyError, "'%s' already exists.", item.c_str());
            return NULL;
        }

        Instance->_workbenchPaths[item] = _getCurrentPythonFile(Instance->_ExecFile);

        PyDict_SetItemString(Instance->_pcWorkbenchDictionary,item.c_str(),object.ptr());

        StdCmdWorkbenchItem::add(item.c_str(), object);

        Instance->signalAddWorkbench(item.c_str());
    }
    catch (const Py::Exception&) {
        return NULL;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sRemoveWorkbenchHandler(PyObject * /*self*/, PyObject *args)
{
    char*       psKey;
    if (!PyArg_ParseTuple(args, "s", &psKey))
        return NULL;

    PyObject* wb = PyDict_GetItemString(Instance->_pcWorkbenchDictionary,psKey);
    if (!wb) {
        PyErr_Format(PyExc_KeyError, "No such workbench '%s'", psKey);
        return NULL;
    }

    Instance->signalRemoveWorkbench(psKey);
    WorkbenchManager::instance()->removeWorkbench(psKey);
    PyDict_DelItemString(Instance->_pcWorkbenchDictionary,psKey);
    Instance->_workbenchPaths.erase(psKey);

    StdCmdWorkbenchItem::remove(psKey);

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sGetWorkbenchHandler(PyObject * /*self*/, PyObject *args)
{
    char* psKey;
    if (!PyArg_ParseTuple(args, "s", &psKey))
        return NULL;

    // get the python workbench object from the dictionary
    PyObject* pcWorkbench = PyDict_GetItemString(Instance->_pcWorkbenchDictionary, psKey);
    if (!pcWorkbench) {
        PyErr_Format(PyExc_KeyError, "No such workbench '%s'", psKey);
        return NULL;
    }

    Py_INCREF(pcWorkbench);
    return pcWorkbench;
}

PyObject* Application::sListWorkbenchHandlers(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Py_INCREF(Instance->_pcWorkbenchDictionary);
    return Instance->_pcWorkbenchDictionary;
}

PyObject* Application::sActiveWorkbenchHandler(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Workbench* actWb = WorkbenchManager::instance()->active();
    if (!actWb) {
        PyErr_SetString(PyExc_AssertionError, "No active workbench\n");
        return NULL;
    }

    // get the python workbench object from the dictionary
    std::string key = actWb->name();
    PyObject* pcWorkbench = PyDict_GetItemString(Instance->_pcWorkbenchDictionary, key.c_str());
    if (!pcWorkbench) {
        PyErr_Format(PyExc_KeyError, "No such workbench '%s'", key.c_str());
        return NULL;
    }

    // object get incremented
    Py_INCREF(pcWorkbench);
    return pcWorkbench;
}

PyObject* Application::sAddResPath(PyObject * /*self*/, PyObject *args)
{
    char* filePath;
    if (!PyArg_ParseTuple(args, "et", "utf-8", &filePath))
        return NULL;
    QString path = QString::fromUtf8(filePath);
    PyMem_Free(filePath);
    if (QDir::isRelativePath(path)) {
        // Home path ends with '/'
        QString home = QString::fromUtf8(App::GetApplication().getHomePath());
        path = home + path;
    }

    BitmapFactory().addPath(path);
    Translator::instance()->addPath(path);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sAddLangPath(PyObject * /*self*/, PyObject *args)
{
    char* filePath;
    if (!PyArg_ParseTuple(args, "et", "utf-8", &filePath))
        return NULL;
    QString path = QString::fromUtf8(filePath);
    PyMem_Free(filePath);
    if (QDir::isRelativePath(path)) {
        // Home path ends with '/'
        QString home = QString::fromUtf8(App::GetApplication().getHomePath());
        path = home + path;
    }

    Translator::instance()->addPath(path);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sAddIconPath(PyObject * /*self*/, PyObject *args)
{
    char* filePath;
    if (!PyArg_ParseTuple(args, "et", "utf-8", &filePath))
        return NULL;
    QString path = QString::fromUtf8(filePath);
    PyMem_Free(filePath);
    if (QDir::isRelativePath(path)) {
        // Home path ends with '/'
        QString home = QString::fromUtf8(App::GetApplication().getHomePath());
        path = home + path;
    }

    BitmapFactory().addPath(path);
    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sAddIcon(PyObject * /*self*/, PyObject *args)
{
    const char *iconName;
    Py_buffer content;
    const char *format = "XPM";
    if (!PyArg_ParseTuple(args, "ss*|s", &iconName, &content, &format))
        return nullptr;

    QPixmap icon;
    if (BitmapFactory().findPixmapInCache(iconName, icon)) {
        PyErr_SetString(PyExc_AssertionError, "Icon with this name already registered");
        PyBuffer_Release(&content);
        return nullptr;
    }

    const char* contentStr = static_cast<const char*>(content.buf);
    QByteArray ary(contentStr, content.len);
    icon.loadFromData(ary, format);

    const char *filepath = nullptr;
    if (icon.isNull()){
        filepath = contentStr;
        QString file = QString::fromUtf8(contentStr, content.len);
        icon.load(file);
    }

    PyBuffer_Release(&content);

    if (icon.isNull()) {
        PyErr_SetString(Base::BaseExceptionFreeCADError, "Invalid icon added to application");
        return NULL;
    }

    BitmapFactory().addPixmapToCache(iconName, icon, filepath);

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sGetIcon(PyObject * /*self*/, PyObject *args)
{
    char *iconName;
    PyObject *original = Py_False;
    if (!PyArg_ParseTuple(args, "s|O", &iconName, &original))
        return NULL;

    PythonWrapper wrap;
    wrap.loadGuiModule();
    wrap.loadWidgetsModule();
    QPixmap pxOriginal;
    auto pixmap = BitmapFactory().pixmap(iconName, false, PyObject_IsTrue(original) ? &pxOriginal : nullptr);
    if(!pixmap.isNull())
        return Py::new_reference_to(wrap.fromQIcon(new QIcon(pxOriginal.isNull()?pixmap:pxOriginal)));
    Py_Return;
}

PyObject* Application::sGetIconContext(PyObject * /*self*/, PyObject *args)
{
    char *iconName;
    if (!PyArg_ParseTuple(args, "s", &iconName))
        return NULL;

    Py::List res;
    for (auto &ctx : BitmapFactory().getContext(iconName))
        res.append(Py::String(ctx));
    return Py::new_reference_to(res);
}

PyObject* Application::sAddIconContext(PyObject * /*self*/, PyObject *args)
{
    char *iconName;
    char *ctx;
    if (!PyArg_ParseTuple(args, "ss", &iconName, &ctx))
        return NULL;

    BitmapFactory().addContext(iconName, ctx);
    Py_Return;
}

PyObject* Application::sIsIconCached(PyObject * /*self*/, PyObject *args)
{
    char *iconName;
    if (!PyArg_ParseTuple(args, "s", &iconName))
        return NULL;

    QPixmap icon;
    return Py::new_reference_to(Py::Boolean(BitmapFactory().findPixmapInCache(iconName, icon)));
}

PyObject* Application::sGetIconNames(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;

    Py::List res;
    for (auto &name : BitmapFactory().pixmapNames())
        res.append(Py::String(name.toUtf8().constData()));
    return Py::new_reference_to(res);
}

PyObject* Application::sAddCommand(PyObject * /*self*/, PyObject *args)
{
    char*       pName;
    char*       pSource=0;
    PyObject*   pcCmdObj;
    if (!PyArg_ParseTuple(args, "sO|s", &pName,&pcCmdObj,&pSource))
        return NULL;

    // get the call stack to find the Python module name
    //
    std::string module, group;
    try {
        Base::PyGILStateLocker lock;

        std::string file = _getCurrentPythonFile(Instance->_ExecFile);

        Base::FileInfo fi(file);
        // convert backslashes to slashes
        group = fi.fileNamePure();

        std::string lastName;
        std::string path = fi.dirPath();

        do {
            Base::FileInfo info(path);
            std::string name = info.fileName();
            if (name == "Mod") {
                group = lastName;
                break;
            } else if (name == "freecad") {
                group = "freecad.";
                group += lastName;
                break;
            }
            lastName = std::move(name);
            if (path.find('/') == std::string::npos)
                break;
            path = info.dirPath();
        } while (path.size());

        FC_TRACE("Add command " << pName << ", " << group << ", " << fi.filePath());
    }
    catch (Py::Exception& e) {
        e.clear();
    }

    try {
        Base::PyGILStateLocker lock;

        Py::Object cmd(pcCmdObj);
        if (cmd.hasAttr("GetCommands")) {
            Command* cmd = new PythonGroupCommand(pName, pcCmdObj);
            if (!module.empty()) {
                cmd->setAppModuleName(module.c_str());
            }
            if (!group.empty()) {
                cmd->setGroupName(group.c_str());
            }
            Application::Instance->commandManager().addCommand(cmd);
        }
        else {
            Command* cmd = new PythonCommand(pName, pcCmdObj, pSource);
            if (!module.empty()) {
                cmd->setAppModuleName(module.c_str());
            }
            if (!group.empty()) {
                cmd->setGroupName(group.c_str());
            }
            Application::Instance->commandManager().addCommand(cmd);
        }
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(Base::BaseExceptionFreeCADError, e.what());
        return 0;
    }
    catch (...) {
        PyErr_SetString(Base::BaseExceptionFreeCADError, "Unknown C++ exception raised in Application::sAddCommand()");
        return 0;
    }

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sRunCommand(PyObject * /*self*/, PyObject *args)
{
    char* pName;
    int item = 0;
    if (!PyArg_ParseTuple(args, "s|i", &pName, &item))
        return NULL;

    Gui::Command::LogDisabler d1;
    Gui::SelectionLogDisabler d2;

    Command* cmd = Application::Instance->commandManager().getCommandByName(pName);
    if (cmd) {
        cmd->invoke(item);
        Py_INCREF(Py_None);
        return Py_None;
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command '%s'", pName);
        return 0;
    }
}

PyObject* Application::sDoCommand(PyObject * /*self*/, PyObject *args)
{
    App::ExpressionBlocker::check();

    char *sCmd=0;
    if (!PyArg_ParseTuple(args, "s", &sCmd))
        return NULL;

    Gui::Command::LogDisabler d1;
    Gui::SelectionLogDisabler d2;

    Gui::Command::printPyCaller();
    Gui::Application::Instance->macroManager()->addLine(MacroManager::App, sCmd);

    PyObject *module, *dict;

    Base::PyGILStateLocker locker;
    module = PyImport_AddModule("__main__");
    if (module == NULL)
        return 0;
    dict = PyModule_GetDict(module);
    if (dict == NULL)
        return 0;

    return PyRun_String(sCmd, Py_file_input, dict, dict);
}

PyObject* Application::sDoCommandGui(PyObject * /*self*/, PyObject *args)
{
    App::ExpressionBlocker::check();

    char *sCmd=0;
    if (!PyArg_ParseTuple(args, "s", &sCmd))
        return NULL;

    Gui::Command::LogDisabler d1;
    Gui::SelectionLogDisabler d2;

    Gui::Command::printPyCaller();
    Gui::Application::Instance->macroManager()->addLine(MacroManager::Gui, sCmd);

    PyObject *module, *dict;

    Base::PyGILStateLocker locker;
    module = PyImport_AddModule("__main__");
    if (module == NULL)
        return 0;
    dict = PyModule_GetDict(module);
    if (dict == NULL)
        return 0;

    return PyRun_String(sCmd, Py_file_input, dict, dict);
}

PyObject* Application::sAddModule(PyObject * /*self*/, PyObject *args)
{
    char *pstr=0;
    if (!PyArg_ParseTuple(args, "s", &pstr))
        return NULL;

    try {
        Command::addModule(Command::Doc,pstr);

        Py_INCREF(Py_None);
        return Py_None;
    }
    catch (const Base::Exception& e) {
        PyErr_SetString(PyExc_ImportError, e.what());
        return 0;
    }
}

PyObject* Application::sShowDownloads(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    Gui::Dialog::DownloadManager::getInstance();

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sShowPreferences(PyObject * /*self*/, PyObject *args)
{
    char *pstr=0;
    int idx=0;
    if (!PyArg_ParseTuple(args, "|si", &pstr, &idx))
        return NULL;
    Gui::Dialog::DlgPreferencesImp cDlg(getMainWindow());
    if (pstr)
        cDlg.activateGroupPage(QString::fromUtf8(pstr),idx);
    WaitCursor wc;
    wc.restoreCursor();
    cDlg.exec();
    wc.setWaitCursor();

    Py_INCREF(Py_None);
    return Py_None;
}

PyObject* Application::sCreateViewer(PyObject * /*self*/, PyObject *args)
{
    int num_of_views = 1;
    char* title = nullptr;
    // if one argument (int) is given
    if (PyArg_ParseTuple(args, "|is", &num_of_views, &title))
    {
        if (num_of_views < 0)
            return NULL;
        else if (num_of_views==1)
        {
            View3DInventor* viewer = new View3DInventor(0, 0);
            if (title)
                viewer->setWindowTitle(QString::fromUtf8(title));
            Gui::getMainWindow()->addWindow(viewer);
            return viewer->getPyObject();
        }
        else
        {
            SplitView3DInventor* viewer = new SplitView3DInventor(num_of_views, 0, 0);
            if (title)
                viewer->setWindowTitle(QString::fromUtf8(title));
            Gui::getMainWindow()->addWindow(viewer);
            return viewer->getPyObject();
        }
    }
    return Py_None;
}

PyObject* Application::sGetMarkerIndex(PyObject * /*self*/, PyObject *args)
{
    char *pstr   = 0;
    int  defSize = 9;
    if (!PyArg_ParseTuple(args, "|si", &pstr, &defSize))
        return NULL;

    PY_TRY {
        ParameterGrp::handle const hGrp = App::GetApplication().GetParameterGroupByPath("User parameter:BaseApp/Preferences/View");

        //find the appropriate marker style string token
        std::string marker_arg = pstr;

        std::list<std::pair<std::string, std::string> > markerList = {
            {"square", "DIAMOND_FILLED"},
            {"cross", "CROSS"},
            {"plus", "PLUS"},
            {"empty", "SQUARE_LINE"},
            {"quad", "SQUARE_FILLED"},
            {"circle", "CIRCLE_LINE"},
            {"default", "CIRCLE_FILLED"}
        };

        std::list<std::pair<std::string, std::string>>::iterator markerStyle;

        for (markerStyle = markerList.begin(); markerStyle != markerList.end(); ++markerStyle)
        {
            if (marker_arg == (*markerStyle).first || marker_arg == (*markerStyle).second)
                break;
        }

        marker_arg = "CIRCLE_FILLED";

        if (markerStyle != markerList.end())
            marker_arg = (*markerStyle).second;

        //get the marker size
        int sizeList[]={5, 7, 9};

        if (std::find(std::begin(sizeList), std::end(sizeList), defSize) == std::end(sizeList))
            defSize = 9;

        return Py_BuildValue("i", Gui::Inventor::MarkerBitmaps::getMarkerIndex(marker_arg, defSize));
    }PY_CATCH;
}

PyObject* Application::sReload(PyObject * /*self*/, PyObject *args)
{
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    PY_TRY {
        auto doc = Application::Instance->reopen(App::GetApplication().getDocument(name));
        if(doc)
            return doc->getPyObject();
    }PY_CATCH;
    Py_Return;
}

PyObject* Application::sLoadFile(PyObject * /*self*/, PyObject *args, PyObject *kwd)
{
    char *path, *mod="";
    PyObject *interactive = Py_False;
    static char *kwlist[] = {"path","module","interactive",0};
    if (!PyArg_ParseTupleAndKeywords(args, kwd, 
                "s|sO", kwlist, &path, &mod, &interactive))
        return 0;                             // NULL triggers exception
    PY_TRY {
        Base::FileInfo fi(path);
        if (!fi.exists()) {
            PyErr_Format(PyExc_IOError, "File %s doesn't exist.", path);
            return 0;
        }

        std::stringstream str;
        std::string module = mod;
        if (module.empty()) {
            if((fi.isDir() && Base::FileInfo(fi.filePath()+"/Document.xml").exists()) 
                    || fi.fileName() == "Document.xml") 
            {
                if(!fi.isDir()) 
                    fi.setFile(fi.dirPath());
            } else if (PyObject_IsTrue(interactive)) {
                QString selectedFilter;
                SelectModule::Dict dict = SelectModule::importHandler(
                        QString::fromUtf8(path), selectedFilter);
                if (dict.size())
                    module = dict.begin().value().toUtf8().constData();
            } else {
                std::string ext = fi.extension();
                std::vector<std::string> modules = App::GetApplication().getImportModules(ext.c_str());
                if (modules.empty()) {
                    PyErr_Format(PyExc_IOError, "Filetype %s is not supported.", ext.c_str());
                    return 0;
                }
                else {
                    module = modules.front();
                }
            }
        }

        App::Document *doc = App::GetApplication().getDocumentByPath(fi.filePath().c_str(), 1);
        if (doc && fi.filePath() != doc->FileName.getValue()) {
            int res = QMessageBox::warning (getMainWindow(), QObject::tr("Duplicate file path"), 
                        QStringLiteral("%1\n\n%2\n  ->\n%3\n\n%4").arg(
                            QObject::tr("You are about to load a file through some symbolic link "
                                        "to an already loaded document. "),
                            QString::fromUtf8(fi.filePath().c_str()),
                            QString::fromUtf8(doc->FileName.getValue()),
                            QObject::tr("Are you sure you want to continue?")),
                        QMessageBox::Yes | QMessageBox::No);
            if (res == QMessageBox::No) {
                App::GetApplication().setActiveDocument(doc);
                Py_Return;
            }
        }

        Application::Instance->open(fi.filePath().c_str(), module.c_str());

        Py_Return;
    } PY_CATCH
}

PyObject* Application::sAddDocObserver(PyObject * /*self*/, PyObject *args)
{
    PyObject* o;
    if (!PyArg_ParseTuple(args, "O",&o))
        return NULL;
    PY_TRY {
        DocumentObserverPython::addObserver(Py::Object(o));
        Py_Return;
    } PY_CATCH;
}

PyObject* Application::sRemoveDocObserver(PyObject * /*self*/, PyObject *args)
{
    PyObject* o;
    if (!PyArg_ParseTuple(args, "O",&o))
        return NULL;
    PY_TRY {
        DocumentObserverPython::removeObserver(Py::Object(o));
        Py_Return;
    } PY_CATCH;
}

PyObject* Application::sCoinRemoveAllChildren(PyObject * /*self*/, PyObject *args)
{
    PyObject *pynode;
    if (!PyArg_ParseTuple(args, "O", &pynode))
        return NULL;

    PY_TRY {
        void* ptr = 0;
        Base::Interpreter().convertSWIGPointerObj("pivy.coin","_p_SoGroup", pynode, &ptr, 0);
        coinRemoveAllChildren(reinterpret_cast<SoGroup*>(ptr));
        Py_Return;
    }PY_CATCH;
}

PyObject* Application::sSetExecFile(PyObject * /*self*/, PyObject *args)
{
    const char *file = 0;
    if (!PyArg_ParseTuple(args, "|s", &file))
        return NULL;

    Base::PyGILStateLocker lock;
    if (!file)
        Instance->_ExecFile.clear();
    else
        Instance->_ExecFile = file;
    Py_Return;
}

PyObject* Application::sListUserEditModes(PyObject * /*self*/, PyObject *args)
{
    Py::List ret;
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    for (auto const &uem : Instance->listUserEditModes()) {
        ret.append(Py::String(uem.second));
    }
    return Py::new_reference_to(ret);
}

PyObject* Application::sGetUserEditMode(PyObject * /*self*/, PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return NULL;
    return Py::new_reference_to(Py::String(Instance->getUserEditModeName()));
}

PyObject* Application::sSetUserEditMode(PyObject * /*self*/, PyObject *args)
{
    char *mode = "";
    if (!PyArg_ParseTuple(args, "s", &mode))
        return NULL;
    bool ok = Instance->setUserEditMode(std::string(mode));
    return Py::new_reference_to(Py::Boolean(ok));
}
