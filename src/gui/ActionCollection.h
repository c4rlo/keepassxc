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

#ifndef KEEPASSXC_ACTION_COLLECTION_H
#define KEEPASSXC_ACTION_COLLECTION_H

#include <QAction>
#include <QHash>
#include <QKeySequence>
#include <QObject>
#include <QPointer>
#include <QShortcut>

#include <functional>

/**
 * This class manages all actions that are shortcut configurable.
 * It also allows you to access the actions inside it from anywhere
 * in the gui code.
 */
class ActionCollection : public QObject
{
    Q_OBJECT

public:
    ActionCollection() = default;

    QList<QAction*> actions() const;
    void addAction(QAction* a);
    void addAction(QAction* a, const QKeySequence& defaultShortcut);
    void addAction(QAction* a, QKeySequence::StandardKey defaultShortcut, const QKeySequence& fallback);
    void addAction(QAction* a, const QList<QKeySequence>& defaultShortcuts);

    QKeySequence defaultShortcut(QAction* a) const;
    const QKeySequence shortcut(QAction* a) const;
    void setShortcuts(QAction* a, const QList<QKeySequence>& keys);

    // Check if any action conflicts with @p seq and return the conflicting action or nullptr
    QAction* getConflictingShortcut(const QAction* action, const QKeySequence& seq) const;

    // Register a callback function that gets invoked when a shortcut gets triggered
    // that has been registered with an action, but that is also a system shortcut
    // for the copy-to-clipboard action. If the callback function return true, the
    // action is not triggered.
    void setCopyShortcutActionCallback(const std::function<bool()>& callback);

    void restoreShortcutsFromDefaults();
    void restoreShortcutsFromConfig();
    void saveShortcutsToConfig();

private:
    struct ActionInfo
    {
        // Default shortcut for action
        QList<QKeySequence> defaultShortcuts;

        // Shortcuts for action that conflict with system-wide copy-to-clipboard action;
        // these get special handling. (Normally, shortcuts are set directly on the action.)
        QList<QPointer<QShortcut>> copyShortcuts;
    };

    QHash<QAction*, ActionInfo> m_actions;
    std::function<bool()> m_copyShortcutActionCallback;

    Q_DISABLE_COPY(ActionCollection)
};

#endif
