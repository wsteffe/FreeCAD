/***************************************************************************
 *   Copyright (c) 2020 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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
# include <sstream>
#endif

#include "Command.h"
#include "Action.h"
#include "Application.h"
#include "MainWindow.h"
#include "Selection.h"
#include "Window.h"
#include "PythonWrapper.h"

// inclusion of the generated files (generated out of AreaPy.xml)
#include "CommandPy.h"
#include "CommandPy.cpp"
#include "ShortcutManager.h"


// returns a string which represents the object e.g. when printed in python
std::string CommandPy::representation(void) const
{
    return std::string("<Command object>");
}

PyObject* CommandPy::get(PyObject *args)
{
    char* pName;
    if (!PyArg_ParseTuple(args, "s", &pName))
        return nullptr;

    Command* cmd = Application::Instance->commandManager().getCommandByName(pName);
    if (cmd) {
        CommandPy* cmdPy = new CommandPy(cmd);
        return cmdPy;
    }

    Py_Return;
}

PyObject* CommandPy::update(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    getMainWindow()->updateActions();
    Py_Return;
}

PyObject* CommandPy::listAll(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    std::vector <Command*> cmds = Application::Instance->commandManager().getAllCommands();
    PyObject* pyList = PyList_New(cmds.size());
    int i=0;
    for ( std::vector<Command*>::iterator it = cmds.begin(); it != cmds.end(); ++it ) {
        PyObject* str = PyUnicode_FromString((*it)->getName());
        PyList_SetItem(pyList, i++, str);
    }
    return pyList;
}

PyObject* CommandPy::listByShortcut(PyObject *args)
{
    char* shortcut_to_find;
    bool bIsRegularExp = false;
    if (!PyArg_ParseTuple(args, "s|b", &shortcut_to_find, &bIsRegularExp))
        return nullptr;

    std::vector <Command*> cmds = Application::Instance->commandManager().getAllCommands();
    std::vector <std::string> matches;
    for (Command* c : cmds){
        Action* action = c->getAction();
        if (action){
            QString spc = QString::fromLatin1(" ");
            if(bIsRegularExp){
               QRegExp re = QRegExp(QString::fromLatin1(shortcut_to_find));
               re.setCaseSensitivity(Qt::CaseInsensitive);
               if (!re.isValid()){
                   std::stringstream str;
                   str << "Invalid regular expression: " << shortcut_to_find;
                   throw Py::RuntimeError(str.str());
               }

               if (re.indexIn(action->shortcut().toString().remove(spc).toUpper()) != -1){
                   matches.push_back(c->getName());
               }
            }
            else if (action->shortcut().toString().remove(spc).toUpper() ==
                     QString::fromLatin1(shortcut_to_find).remove(spc).toUpper()) {
                matches.push_back(c->getName());
            }
        }
    }

    PyObject* pyList = PyList_New(matches.size());
    int i=0;
    for (std::string match : matches) {
        PyObject* str = PyUnicode_FromString(match.c_str());
        PyList_SetItem(pyList, i++, str);
    }
    return pyList;
}

PyObject* CommandPy::run(PyObject *args)
{
    int item = 0;
    if (!PyArg_ParseTuple(args, "|i", &item))
        return nullptr;

    Gui::Command::LogDisabler d1;
    Gui::SelectionLogDisabler d2;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        cmd->invoke(item);
        Py_Return;
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject* CommandPy::isActive(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        PY_TRY {
            return Py::new_reference_to(Py::Boolean(cmd->isActive()));
        }
        PY_CATCH;
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject* CommandPy::getShortcut(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        PyObject* str = PyUnicode_FromString(cmd->getAction() ? cmd->getAction()->shortcut().toString().toStdString().c_str() : "");
        return str;
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject* CommandPy::setShortcut(PyObject *args)
{
    char* pShortcut;
    if (!PyArg_ParseTuple(args, "s", &pShortcut))
        return nullptr;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        ShortcutManager::instance()->setShortcut(cmd->getName(), pShortcut);
        return Py::new_reference_to(Py::Boolean(true));
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject* CommandPy::resetShortcut(PyObject *args)
{

    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        ShortcutManager::instance()->reset(cmd->getName());
        return Py::new_reference_to(Py::Boolean(true));
    } else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject* CommandPy::getInfo(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        Action* action = cmd->getAction();
        PyObject* pyList = PyList_New(6);
        const char* menuTxt = cmd->getMenuText();
        const char* tooltipTxt = cmd->getToolTipText();
        const char* whatsThisTxt = cmd->getWhatsThis();
        const char* statustipTxt = cmd->getStatusTip();
        const char* pixMapTxt = cmd->getPixmap();
        std::string shortcutTxt = "";
        if (action)
            shortcutTxt = action->shortcut().toString().toStdString();

        PyObject* strMenuTxt = PyUnicode_FromString(menuTxt ? menuTxt : "");
        PyObject* strTooltipTxt = PyUnicode_FromString(tooltipTxt ? tooltipTxt : "");
        PyObject* strWhatsThisTxt = PyUnicode_FromString(whatsThisTxt ? whatsThisTxt : "");
        PyObject* strStatustipTxt = PyUnicode_FromString(statustipTxt ? statustipTxt : "");
        PyObject* strPixMapTxt = PyUnicode_FromString(pixMapTxt ? pixMapTxt : "");
        PyObject* strShortcutTxt = PyUnicode_FromString(!shortcutTxt.empty() ? shortcutTxt.c_str() : "");
        PyList_SetItem(pyList, 0, strMenuTxt);
        PyList_SetItem(pyList, 1, strTooltipTxt);
        PyList_SetItem(pyList, 2, strWhatsThisTxt);
        PyList_SetItem(pyList, 3, strStatustipTxt);
        PyList_SetItem(pyList, 4, strPixMapTxt);
        PyList_SetItem(pyList, 5, strShortcutTxt);
        return pyList;
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject* CommandPy::getAction(PyObject *args)
{
    if (!PyArg_ParseTuple(args, ""))
        return nullptr;

    Command* cmd = this->getCommandPtr();
    if (cmd) {
        Action* action = cmd->getAction();
        ActionGroup* group = qobject_cast<ActionGroup*>(action);

        PythonWrapper wrap;
        wrap.loadWidgetsModule();

        Py::List list;
        if (group) {
            for (auto a : group->actions())
                list.append(wrap.fromQObject(a));
        }
        else if (action) {
            list.append(wrap.fromQObject(action->action()));
        }

        return Py::new_reference_to(list);
    }
    else {
        PyErr_Format(Base::BaseExceptionFreeCADError, "No such command");
        return nullptr;
    }
}

PyObject *CommandPy::registerCallback(PyObject *tuple)
{
    const char *cmd;
    PyObject *pyCallback;
    if (!PyArg_ParseTuple(tuple, "sO", &cmd, &pyCallback))
        return nullptr;
    if (!PyCallable_Check(pyCallback)) {
        PyErr_Format(PyExc_TypeError, "Expect the second argument to be a callable with signature (cmd:string, idx:int) -> int");
        return nullptr;
    }

    Py::Callable cb(pyCallback);
    auto callback = [cb](const char *cmd, int idx) {
        Base::PyGILStateLocker lock;
        try {
            Py::TupleN args(Py::String(cmd?cmd:""), Py::Int(idx));
            auto ret = cb.apply(args);
            if (!ret.isNone() && !ret.isTrue())
                return false;
        } catch (Py::Exception &) {
            Base::PyException e;
            e.ReportException();
        } catch (Base::Exception &e) {
            e.ReportException();
        }
        return true;
    };

    PY_TRY {
        int id = Application::Instance->commandManager().registerCallback(callback, cmd);
        return Py::new_reference_to(Py::Int(id));
    } PY_CATCH;
}

PyObject *CommandPy::unregisterCallback(PyObject *tuple)
{
    int id;
    if (!PyArg_ParseTuple(tuple, "i", &id))
        return nullptr;
    PY_TRY {
        Application::Instance->commandManager().unregisterCallback(id);
        Py_Return;
    } PY_CATCH;
}

PyObject *CommandPy::getCustomAttributes(const char* /*attr*/) const
{
    return 0;
}

int CommandPy::setCustomAttributes(const char* /*attr*/, PyObject* /*obj*/)
{
    return 0;
}
