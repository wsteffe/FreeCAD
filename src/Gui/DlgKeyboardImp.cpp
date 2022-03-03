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
# include <QAction>
# include <QHeaderView>
# include <QMessageBox>
#endif

#include <Base/Parameter.h>
#include <Base/Tools.h>
#include <Base/Console.h>

#include "DlgKeyboardImp.h"
#include "ui_DlgKeyboard.h"
#include "Action.h"
#include "Application.h"
#include "BitmapFactory.h"
#include "Command.h"
#include "Widgets.h"
#include "Window.h"
#include "PrefWidgets.h"
#include "ShortcutManager.h"

FC_LOG_LEVEL_INIT("Gui", true, true)

using namespace Gui::Dialog;

namespace Gui { namespace Dialog {
typedef std::vector< std::pair<QLatin1String, QString> > GroupMap;

struct GroupMap_find {
    const QLatin1String& item;
    GroupMap_find(const QLatin1String& item) : item(item) {}
    bool operator () (const std::pair<QLatin1String, QString>& elem) const
    {
        return elem.first == item;
    }
};
}
}

/* TRANSLATOR Gui::Dialog::DlgCustomKeyboardImp */

/**
 *  Constructs a DlgCustomKeyboardImp which is a child of 'parent', with the
 *  name 'name' and widget flags set to 'f'
 *
 *  The dialog will by default be modeless, unless you set 'modal' to
 *  true to construct a modal dialog.
 */
DlgCustomKeyboardImp::DlgCustomKeyboardImp( QWidget* parent  )
  : CustomizeActionPage(parent)
  , ui(new Ui_DlgCustomKeyboard)
  , widgetStates(new Gui::PrefWidgetStates(this, false))
  , firstShow(true)
{
    ui->setupUi(this);

    widgetStates->addSplitter(ui->splitter);

    ui->editCommand->setPlaceholderText(tr("Type to search..."));
    auto completer = new CommandCompleter(ui->editCommand, this);
    connect(completer, SIGNAL(commandActivated(QByteArray)), this, SLOT(onCommandActivated(QByteArray)));

    QStringList labels;
    labels << tr("Icon") << tr("Command") << tr("Shortcut");
    ui->commandTreeWidget->setHeaderLabels(labels);
    ui->commandTreeWidget->setIconSize(QSize(32, 32));
    ui->commandTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);

    labels.clear();
    labels << tr("Name") << tr("Title");
    ui->assignedTreeWidget->setHeaderLabels(labels);
    ui->assignedTreeWidget->header()->hide();
    ui->assignedTreeWidget->header()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    ui->assignedTreeWidget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);

    ui->shortcutTimeout->initAutoSave();

    populateCategories();

    timer.setSingleShot(true);
    QObject::connect(&timer, SIGNAL(timeout()), this, SLOT(onTimer()));
}

/** Destroys the object and frees any allocated resources */
DlgCustomKeyboardImp::~DlgCustomKeyboardImp()
{
}

void DlgCustomKeyboardImp::populateCategories()
{
    CommandManager & cCmdMgr = Application::Instance->commandManager();
    std::map<std::string,Command*> sCommands = cCmdMgr.getCommands();

    GroupMap groupMap;
    groupMap.push_back(std::make_pair(QLatin1String("File"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("Edit"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("View"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("Standard-View"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("Tools"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("Window"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("Help"), QString()));
    groupMap.push_back(std::make_pair(QLatin1String("Macros"), qApp->translate("Gui::MacroCommand", "Macros")));

    for (std::map<std::string,Command*>::iterator it = sCommands.begin(); it != sCommands.end(); ++it) {
        QLatin1String group(it->second->getGroupName());
        QString text = it->second->translatedGroupName();
        GroupMap::iterator jt;
        jt = std::find_if(groupMap.begin(), groupMap.end(), GroupMap_find(group));
        if (jt != groupMap.end()) {
            if (jt->second.isEmpty())
                jt->second = text;
        }
        else {
            groupMap.push_back(std::make_pair(group, text));
        }
    }
    groupMap.push_back(std::make_pair(QLatin1String("All"), tr("All")));

    for (GroupMap::iterator it = groupMap.begin(); it != groupMap.end(); ++it) {
        if (ui->categoryBox->findData(it->first) < 0) {
            ui->categoryBox->addItem(it->second);
            ui->categoryBox->setItemData(ui->categoryBox->count()-1, QVariant(it->first), Qt::UserRole);
        }
    }
}

void DlgCustomKeyboardImp::showEvent(QShowEvent* e)
{
    Q_UNUSED(e);
    // If we did this already in the constructor we wouldn't get the vertical scrollbar if needed.
    // The problem was noticed with Qt 4.1.4 but may arise with any later version.
    if (firstShow) {
        on_categoryBox_activated(ui->categoryBox->currentIndex());
        firstShow = false;
    }
}

void DlgCustomKeyboardImp::onCommandActivated(const QByteArray &name)
{
    CommandManager & cCmdMgr = Application::Instance->commandManager();
    Command *cmd = cCmdMgr.getCommandByName(name.constData());
    if (!cmd)
        return;

    QString group = QString::fromLatin1(cmd->getGroupName());
    int index = ui->categoryBox->findData(group);
    if (index < 0) {
        populateCategories();
        index = ui->categoryBox->findData(group);
        if (index < 0)
            return;
    }
    int retry = 0;
    if (index != ui->categoryBox->currentIndex()) {
        ui->categoryBox->setCurrentIndex(index);
        on_categoryBox_activated(index);
        retry = 1;
    }
    for (;retry < 2; ++retry) {
        for (int i=0 ; i<ui->commandTreeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem *item = ui->commandTreeWidget->topLevelItem(i);
            if (item->data(1, Qt::UserRole).toByteArray() == name) {
                ui->commandTreeWidget->setCurrentItem(item);
                return;
            }
        }
        // Since the 'Customize...' dialog is now modaless, the user may
        // activate some new workbench, thus adding new commands. Try to
        // refresh the command list under the current category if haven't done
        // so.
        on_categoryBox_activated(index);
    }
}

/** Shows the description for the corresponding command */
void DlgCustomKeyboardImp::on_commandTreeWidget_currentItemChanged(QTreeWidgetItem* item)
{
    if (!item)
        return;

    QVariant data = item->data(1, Qt::UserRole);
    QByteArray name = data.toByteArray(); // command name

    CommandManager & cCmdMgr = Application::Instance->commandManager();
    Command* cmd = cCmdMgr.getCommandByName(name.constData());
    if (cmd) {
        QKeySequence ks = ShortcutManager::instance()->getShortcut(
                cmd->getName(), cmd->getAccel());
        QKeySequence ks2 = QString::fromLatin1(cmd->getAccel());
        QKeySequence ks3 = ui->editShortcut->text();
        if (ks.isEmpty())
            ui->accelLineEditShortcut->setText( tr("none") );
        else
            ui->accelLineEditShortcut->setText(ks.toString(QKeySequence::NativeText));

        ui->buttonAssign->setEnabled(!ui->editShortcut->text().isEmpty() && (ks != ks3));
        ui->buttonReset->setEnabled((ks != ks2));
    }

    ui->textLabelDescription->setText(item->toolTip(1));
    populatePriorityList();
}

/** Shows all commands of this category */
void DlgCustomKeyboardImp::on_categoryBox_activated(int index)
{
    QVariant data = ui->categoryBox->itemData(index, Qt::UserRole);
    QString group = data.toString();
    ui->commandTreeWidget->clear();
    ui->buttonAssign->setEnabled(false);
    ui->buttonReset->setEnabled(false);
    ui->accelLineEditShortcut->clear();
    ui->editShortcut->clear();

    CommandManager & cCmdMgr = Application::Instance->commandManager();
    std::vector<Command*> aCmds = cCmdMgr.getGroupCommands( group.toLatin1() );

    if (group == QLatin1String("Macros")) {
        for (std::vector<Command*>::iterator it = aCmds.begin(); it != aCmds.end(); ++it) {
            QTreeWidgetItem* item = new QTreeWidgetItem(ui->commandTreeWidget);
            item->setText(1, QString::fromUtf8((*it)->getMenuText()));
            item->setToolTip(1, QString::fromUtf8((*it)->getToolTipText()));
            item->setData(1, Qt::UserRole, QByteArray((*it)->getName()));
            item->setSizeHint(0, QSize(32, 32));
            if ((*it)->getPixmap())
                item->setIcon(0, BitmapFactory().iconFromTheme((*it)->getPixmap()));
            item->setText(2, (*it)->getShortcut());
        }
    }
    else if (group == QLatin1String("All")) {
        for (const Command *cmd : Application::Instance->commandManager().getAllCommands()) {
            QTreeWidgetItem* item = new QTreeWidgetItem(ui->commandTreeWidget);
            if (dynamic_cast<const MacroCommand*>(cmd)) {
                item->setText(1, QString::fromUtf8(cmd->getMenuText()));
                item->setToolTip(1, QString::fromUtf8(cmd->getToolTipText()));
            } else {
                item->setText(1, qApp->translate(cmd->className(), cmd->getMenuText()));
                item->setToolTip(1, qApp->translate(cmd->className(), cmd->getToolTipText()));
            }
            item->setData(1, Qt::UserRole, QByteArray(cmd->getName()));
            item->setSizeHint(0, QSize(32, 32));
            if (cmd->getPixmap())
                item->setIcon(0, BitmapFactory().iconFromTheme(cmd->getPixmap()));
            item->setText(2, cmd->getShortcut());
        }
    }
    else {
        for (std::vector<Command*>::iterator it = aCmds.begin(); it != aCmds.end(); ++it) {
            QTreeWidgetItem* item = new QTreeWidgetItem(ui->commandTreeWidget);
            item->setText(1, qApp->translate((*it)->className(), (*it)->getMenuText()));
            item->setToolTip(1, qApp->translate((*it)->className(), (*it)->getToolTipText()));
            item->setData(1, Qt::UserRole, QByteArray((*it)->getName()));
            item->setSizeHint(0, QSize(32, 32));
            if ((*it)->getPixmap())
                item->setIcon(0, BitmapFactory().iconFromTheme((*it)->getPixmap()));
            item->setText(2, (*it)->getShortcut());
        }
    }
}

void DlgCustomKeyboardImp::setShortcutOfCurrentAction(const QString& accelText)
{
    QTreeWidgetItem* item = ui->commandTreeWidget->currentItem();
    if (!item)
        return;

    QVariant data = item->data(1, Qt::UserRole);
    QByteArray name = data.toByteArray(); // command name

    QString nativeText;
    if (!accelText.isEmpty()) {
        QKeySequence shortcut = accelText;
        nativeText = shortcut.toString(QKeySequence::NativeText);
        ui->accelLineEditShortcut->setText(accelText);
        ui->editShortcut->clear();
    }
    else {
        ui->accelLineEditShortcut->clear();
        ui->editShortcut->clear();
    }
    ShortcutManager::instance()->setShortcut(name, nativeText.toLatin1());

    ui->buttonAssign->setEnabled(false);
    ui->buttonReset->setEnabled(true);
}

/** Assigns a new accelerator to the selected command. */
void DlgCustomKeyboardImp::on_buttonAssign_clicked()
{
    setShortcutOfCurrentAction(ui->editShortcut->text());
}

/** Clears the accelerator of the selected command. */
void DlgCustomKeyboardImp::on_buttonClear_clicked()
{
    setShortcutOfCurrentAction(QString());
}

/** Resets the accelerator of the selected command to the default. */
void DlgCustomKeyboardImp::on_buttonReset_clicked()
{
    QTreeWidgetItem* item = ui->commandTreeWidget->currentItem();
    if (!item)
        return;

    QVariant data = item->data(1, Qt::UserRole);
    QByteArray name = data.toByteArray(); // command name
    ShortcutManager::instance()->reset(name);

    QString txt = ShortcutManager::instance()->getShortcut(name);
    ui->accelLineEditShortcut->setText((txt.isEmpty() ? tr("none") : txt));
    ui->buttonReset->setEnabled( false );

    populatePriorityList();
}

/** Resets the accelerator of all commands to the default. */
void DlgCustomKeyboardImp::on_buttonResetAll_clicked()
{
    ShortcutManager::instance()->resetAll();
    ui->buttonReset->setEnabled(false);
}

/** Checks for an already occupied shortcut. */
void DlgCustomKeyboardImp::on_editShortcut_textChanged(const QString& )
{
    timer.start(200);
}

void DlgCustomKeyboardImp::onTimer()
{
    QTreeWidgetItem* item = ui->commandTreeWidget->currentItem();
    if (item) {
        QVariant data = item->data(1, Qt::UserRole);
        QByteArray name = data.toByteArray(); // command name

        CommandManager & cCmdMgr = Application::Instance->commandManager();
        Command* cmd = cCmdMgr.getCommandByName(name.constData());

        if (!ui->editShortcut->isNone())
            ui->buttonAssign->setEnabled(true);
        else {
            if (cmd && cmd->getAction() && cmd->getAction()->shortcut().isEmpty())
                ui->buttonAssign->setEnabled(false); // both key sequences are empty
        }
    }
    populatePriorityList();
}

void DlgCustomKeyboardImp::populatePriorityList()
{
    ui->assignedTreeWidget->clear();
    QString sc = ui->editShortcut->isNone() ? ui->accelLineEditShortcut->text() : ui->editShortcut->text();
    auto actionList = ShortcutManager::instance()->getActionsByShortcut(sc);
    std::reverse(actionList.begin(), actionList.end());
    for (size_t i=0; i<actionList.size(); ++i) {
        const auto &info = actionList[i];
        if (!info.second)
            continue;
        QTreeWidgetItem* item = new QTreeWidgetItem(ui->assignedTreeWidget);
        item->setText(0, QString::fromUtf8(info.first));
        item->setText(1, info.second->text());
        item->setToolTip(0, info.second->toolTip());
        item->setIcon(0, info.second->icon());
        item->setData(0, Qt::UserRole, info.first);
    }
    ui->assignedTreeWidget->resizeColumnToContents(0);
    ui->assignedTreeWidget->resizeColumnToContents(1);
}

void DlgCustomKeyboardImp::on_buttonUp_clicked()
{
    onUpdatePriorityList(true);
}

void DlgCustomKeyboardImp::on_buttonDown_clicked()
{
    onUpdatePriorityList(false);
}

void DlgCustomKeyboardImp::on_assignedTreeWidget_currentItemChanged(QTreeWidgetItem *item)
{
    ui->buttonUp->setEnabled(item!=nullptr);
    ui->buttonDown->setEnabled(item!=nullptr);
}

void DlgCustomKeyboardImp::onUpdatePriorityList(bool up)
{
    auto item = ui->assignedTreeWidget->currentItem();
    if (!item)
        return;

    int index = ui->assignedTreeWidget->indexOfTopLevelItem(item);
    if (index < 0)
        return;
    if ((index == 0 && up)
            || (index == ui->assignedTreeWidget->topLevelItemCount()-1 && !up))
        return;

    std::vector<QByteArray> actions;
    for (int i=0; i<ui->assignedTreeWidget->topLevelItemCount(); ++i) {
        auto item = ui->assignedTreeWidget->topLevelItem(i);
        actions.push_back(item->data(0, Qt::UserRole).toByteArray());
    }

    auto it = actions.begin() + index;
    auto itNext = up ? it - 1 : it + 1;
    std::swap(*it, *itNext);
    std::reverse(actions.begin(), actions.end());
    ShortcutManager::instance()->setPriority(actions);

    ui->assignedTreeWidget->takeTopLevelItem(index);
    if (up)
        ui->assignedTreeWidget->insertTopLevelItem(index-1, item);
    else
        ui->assignedTreeWidget->insertTopLevelItem(index+1, item);
    ui->assignedTreeWidget->setCurrentItem(item);
}

void DlgCustomKeyboardImp::onAddMacroAction(const QByteArray& macro)
{
    QVariant data = ui->categoryBox->itemData(ui->categoryBox->currentIndex(), Qt::UserRole);
    QString group = data.toString();
    if (group == QLatin1String("Macros"))
    {
        CommandManager & cCmdMgr = Application::Instance->commandManager();
        Command* pCmd = cCmdMgr.getCommandByName(macro);
        QTreeWidgetItem* item = new QTreeWidgetItem(ui->commandTreeWidget);
        item->setToolTip(1, QString::fromUtf8(pCmd->getToolTipText()));
        item->setData(1, Qt::UserRole, macro);
        item->setSizeHint(0, QSize(32, 32));
        if (pCmd->getPixmap())
            item->setIcon(0, BitmapFactory().iconFromTheme(pCmd->getPixmap()));
    }
}

void DlgCustomKeyboardImp::onRemoveMacroAction(const QByteArray& macro)
{
    QVariant data = ui->categoryBox->itemData(ui->categoryBox->currentIndex(), Qt::UserRole);
    QString group = data.toString();
    if (group == QLatin1String("Macros"))
    {
        for (int i=0; i<ui->commandTreeWidget->topLevelItemCount(); i++) {
            QTreeWidgetItem* item = ui->commandTreeWidget->topLevelItem(i);
            QByteArray command = item->data(1, Qt::UserRole).toByteArray();
            if (command == macro) {
                ui->commandTreeWidget->takeTopLevelItem(i);
                delete item;
                break;
            }
        }
    }
}

void DlgCustomKeyboardImp::onModifyMacroAction(const QByteArray& macro)
{
    QVariant data = ui->categoryBox->itemData(ui->categoryBox->currentIndex(), Qt::UserRole);
    QString group = data.toString();
    if (group == QLatin1String("Macros"))
    {
        CommandManager & cCmdMgr = Application::Instance->commandManager();
        Command* pCmd = cCmdMgr.getCommandByName(macro);
        for (int i=0; i<ui->commandTreeWidget->topLevelItemCount(); i++) {
            QTreeWidgetItem* item = ui->commandTreeWidget->topLevelItem(i);
            QByteArray command = item->data(1, Qt::UserRole).toByteArray();
            if (command == macro) {
                item->setText(1, QString::fromUtf8(pCmd->getMenuText()));
                item->setToolTip(1, QString::fromUtf8(pCmd->getToolTipText()));
                item->setData(1, Qt::UserRole, macro);
                item->setSizeHint(0, QSize(32, 32));
                if (pCmd->getPixmap())
                    item->setIcon(0, BitmapFactory().iconFromTheme(pCmd->getPixmap()));
                if (item->isSelected())
                    ui->textLabelDescription->setText(item->toolTip(1));
                break;
            }
        }
    }
}

void DlgCustomKeyboardImp::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        ui->retranslateUi(this);
        int count = ui->categoryBox->count();

        CommandManager & cCmdMgr = Application::Instance->commandManager();
        for (int i=0; i<count; i++) {
            QVariant data = ui->categoryBox->itemData(i, Qt::UserRole);
            std::vector<Command*> aCmds = cCmdMgr.getGroupCommands(data.toByteArray());
            if (!aCmds.empty()) {
                QString text = aCmds[0]->translatedGroupName();
                ui->categoryBox->setItemText(i, text);
            }
        }
        on_categoryBox_activated(ui->categoryBox->currentIndex());
    }
    else if (e->type() == QEvent::StyleChange)
        on_categoryBox_activated(ui->categoryBox->currentIndex());
    QWidget::changeEvent(e);
}

#include "moc_DlgKeyboardImp.cpp"
