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

ActionCollection* ActionCollection::instance()
{
    static ActionCollection ac;
    return &ac;
}

QList<QAction*> ActionCollection::actions() const
{
    return m_actions.keys();
}

void ActionCollection::setActions(std::initializer_list<QAction*> actions)
{
    for (auto a : actions) {
        if (!m_actions.contains(a)) {
            ActionInfo& ai = m_actions[a];
            // Initialise default keys with what's set on the action
            ai.defaultKeys = a->shortcuts();
        }
    }
}

QKeySequence ActionCollection::defaultShortcut(QAction* action) const
{
    const auto shortcuts = m_actions.value(action).defaultKeys;
    return shortcuts.isEmpty() ? QKeySequence() : shortcuts.first();
}

void ActionCollection::setDefaultShortcut(QAction* action, const QKeySequence& keys)
{
    setDefaultShortcuts(action, {keys});
}

void ActionCollection::setDefaultShortcut(QAction* action,
                                          QKeySequence::StandardKey standard,
                                          const QKeySequence& fallback)
{
    if (!QKeySequence::keyBindings(standard).isEmpty()) {
        setDefaultShortcuts(action, QKeySequence::keyBindings(standard));
    } else if (fallback != 0) {
        setDefaultShortcut(action, QKeySequence(fallback));
    }
}

void ActionCollection::setDefaultShortcuts(QAction* action, const QList<QKeySequence>& keys)
{
    setShortcuts(action, keys);
    m_actions[action].defaultKeys = keys;
}

const QKeySequence ActionCollection::shortcut(QAction* a) const
{
    const ActionInfo& ai = m_actions.value(a);
    if (!ai.copyShortcuts.isEmpty()) {
        return ai.copyShortcuts.front()->key();
    }
    return a->shortcut();
}

void ActionCollection::setShortcuts(QAction* action, const QList<QKeySequence>& keys)
{
    // For any provided shortcuts that match the system shortcut for copy-to-clipboard,
    // instead of registering them directly with the action, give the
    // m_copyShortcutActionCallback a chance to intercept the event.

    ActionInfo& ai = m_actions[action];
    ai.copyShortcuts.clear();

    QList<QKeySequence> otherShortcuts;
    for (const QKeySequence& k : keys) {
        static const auto copyShortcuts = QKeySequence::keyBindings(QKeySequence::Copy);
        if (copyShortcuts.contains(k)) {
            const auto shortcut = new QShortcut(action->parentWidget());
            shortcut->setKey(k);
            connect(shortcut, &QShortcut::activated, this, [this, action]() {
                if (m_copyShortcutActionCallback && m_copyShortcutActionCallback()) {
                    return;
                }
                action->activate(QAction::Trigger);
            });
            ai.copyShortcuts.append(shortcut);
        } else {
            otherShortcuts.append(k);
        }
    }

    action->setShortcuts(otherShortcuts);
}

void ActionCollection::restoreShortcutsFromDefaults()
{
    for (auto it = m_actions.constBegin(), end = m_actions.constEnd(); it != end; ++it) {
        setShortcuts(it.key(), it.value().defaultKeys);
    }
}

void ActionCollection::restoreShortcutsFromConfig()
{
    const auto configShortcuts = Config::instance()->getShortcuts();
    QHash<QString, QAction*> actionsByName;
    for (auto it = m_actions.keyBegin(), end = m_actions.keyEnd(); it != end; ++it) {
        actionsByName.insert((*it)->objectName(), *it);
    }
    for (const auto& shortcut : configShortcuts) {
        if (actionsByName.contains(shortcut.name)) {
            const auto key = QKeySequence::fromString(shortcut.shortcut);
            setShortcuts(actionsByName.value(shortcut.name), {key});
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
