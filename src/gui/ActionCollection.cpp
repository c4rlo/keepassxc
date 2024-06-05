/*
 *  Copyright (C) 2024 KeePassXC Team <team@keepassxc.org>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "ActionCollection.h"
#include "core/Config.h"
#include <algorithm>

QList<QAction*> ActionCollection::actions() const
{
    return m_actions.keys();
}

void ActionCollection::addAction(QAction* action)
{
    addAction(action, QList<QKeySequence>{});
}

void ActionCollection::addAction(QAction* action, const QKeySequence& defaultShortcut)
{
    addAction(action, QList<QKeySequence>{defaultShortcut});
}

void ActionCollection::addAction(QAction* action,
                                 QKeySequence::StandardKey defaultShortcut,
                                 const QKeySequence& fallback)
{
    if (!QKeySequence::keyBindings(defaultShortcut).isEmpty()) {
        addAction(action, QKeySequence::keyBindings(defaultShortcut));
    } else if (!fallback.isEmpty()) {
        addAction(action, QKeySequence(fallback));
    } else {
        addAction(action);
    }
}

void ActionCollection::addAction(QAction* action, const QList<QKeySequence>& defaultShortcuts)
{
    m_actions[action].defaultShortcuts = defaultShortcuts;
    setShortcuts(action, defaultShortcuts);
}

QKeySequence ActionCollection::defaultShortcut(QAction* action) const
{
    const auto shortcuts = m_actions.value(action).defaultShortcuts;
    return shortcuts.isEmpty() ? QKeySequence() : shortcuts.first();
}

const QKeySequence ActionCollection::shortcut(QAction* a) const
{
    const ActionInfo& ai = m_actions.value(a);
    if (!ai.copyShortcuts.isEmpty()) {
        return ai.copyShortcuts.front()->key();
    }
    return a->shortcut();
}

void ActionCollection::setShortcuts(QAction* action, const QList<QKeySequence>& shortcuts)
{
    // For any provided shortcuts that match the system shortcut for copy-to-clipboard,
    // instead of registering them directly with the action, give the
    // m_copyShortcutActionCallback a chance to intercept the event.

    ActionInfo& ai = m_actions[action];
    for (const auto& qshortcut : ai.copyShortcuts) {
        delete qshortcut.data();
    }
    ai.copyShortcuts.clear();

    QList<QKeySequence> otherShortcuts;
    for (const QKeySequence& shortcut : shortcuts) {
        static const auto standardCopyShortcuts = QKeySequence::keyBindings(QKeySequence::Copy);
        if (standardCopyShortcuts.contains(shortcut)) {
            const auto qshortcut = new QShortcut(action->parentWidget());
            qshortcut->setKey(shortcut);
            connect(qshortcut, &QShortcut::activated, this, [this, action]() {
                if (m_copyShortcutActionCallback && m_copyShortcutActionCallback()) {
                    return;
                }
                action->activate(QAction::Trigger);
            });
            ai.copyShortcuts.append(qshortcut);
        } else {
            otherShortcuts.append(shortcut);
        }
    }

    action->setShortcuts(otherShortcuts);
}

void ActionCollection::restoreShortcutsFromDefaults()
{
    for (auto it = m_actions.constBegin(), end = m_actions.constEnd(); it != end; ++it) {
        setShortcuts(it.key(), it.value().defaultShortcuts);
    }
}

void ActionCollection::restoreShortcutsFromConfig()
{
    const auto configShortcuts = Config::instance()->getShortcuts();
    QHash<QString, QAction*> actionsByName;
    for (auto it = m_actions.keyBegin(), end = m_actions.keyEnd(); it != end; ++it) {
        actionsByName.insert((*it)->objectName(), *it);
    }
    for (const auto& configShortcut : configShortcuts) {
        if (actionsByName.contains(configShortcut.name)) {
            const auto shortcut = QKeySequence::fromString(configShortcut.shortcut);
            setShortcuts(actionsByName.value(configShortcut.name), {shortcut});
        }
    }
}

void ActionCollection::saveShortcutsToConfig()
{
    QList<Config::ShortcutEntry> configShortcuts;
    configShortcuts.reserve(m_actions.size());
    for (auto it = m_actions.keyBegin(), end = m_actions.keyEnd(); it != end; ++it) {
        // Only store non-default shortcut assignments
        const auto s = shortcut(*it);
        if (s != defaultShortcut(*it)) {
            configShortcuts << Config::ShortcutEntry{(*it)->objectName(), s.toString()};
        }
    }
    Config::instance()->setShortcuts(configShortcuts);
}

QAction* ActionCollection::getConflictingShortcut(const QAction* action, const QKeySequence& seq) const
{
    // Empty sequences don't conflict with anything
    if (seq.isEmpty()) {
        return nullptr;
    }

    for (auto it = m_actions.keyBegin(), end = m_actions.keyEnd(); it != end; ++it) {
        if (*it != action && shortcut(*it) == seq) {
            return *it;
        }
    }

    return nullptr;
}

void ActionCollection::setCopyShortcutActionCallback(const std::function<bool()>& callback)
{
    m_copyShortcutActionCallback = callback;
}
