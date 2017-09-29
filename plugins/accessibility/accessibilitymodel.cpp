/*
  accessibilitymodel.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2017 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

  Licensees holding valid commercial KDAB GammaRay licenses may use this file in
  accordance with GammaRay Commercial License Agreement provided with the Software.

  Contact info@kdab.com if any conditions of this licensing are not clear to you.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "accessibilitymodel.h"

#include <core/enumutil.h>
#include <core/objectdataprovider.h>
#include <core/varianthandler.h>

#include <common/objectid.h>
#include <common/sourcelocation.h>

#include <QCoreApplication>

using namespace GammaRay;

AccessibilityModel::AccessibilityModel(QObject *parent)
    : QAbstractItemModel(parent)
{
    beginResetModel();
    populate(QAccessible::queryAccessibleInterface(QCoreApplication::instance()));
    endResetModel();
}

AccessibilityModel::~AccessibilityModel()
{
}

int AccessibilityModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return 2;
}

int AccessibilityModel::rowCount(const QModelIndex &parent) const
{
    auto parentIface = reinterpret_cast<QAccessibleInterface*>(parent.internalPointer());
    return m_parentChildMap.value(parentIface).size();
}

QVariant AccessibilityModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    auto iface = reinterpret_cast<QAccessibleInterface*>(index.internalPointer());
    if (index.column() == 0) {
        switch (role) {
            case Qt::DisplayRole:
                return VariantHandler::displayString(iface);
            case ObjectModel::ObjectRole:
                return QVariant::fromValue<void*>(iface);
            case ObjectModel::ObjectIdRole:
                return QVariant::fromValue(ObjectId(iface->object()));
            case ObjectModel::CreationLocationRole:
            {
                const auto loc = ObjectDataProvider::creationLocation(iface->object());
                if (loc.isValid())
                    return QVariant::fromValue(loc);
            }
        }
    } else if (index.column() == 1) {
        switch (role) {
            case Qt::DisplayRole:
                return EnumUtil::enumToString(QVariant::fromValue(iface->role()), nullptr, &QAccessible::staticMetaObject);
        }
    }

    return QVariant();
}

QModelIndex AccessibilityModel::index(int row, int column, const QModelIndex &parent) const
{
    auto parentIface = reinterpret_cast<QAccessibleInterface*>(parent.internalPointer());
    const auto children = m_parentChildMap.value(parentIface);
    if (row < 0 || column < 0 || row >= children.size() || column >= columnCount())
        return QModelIndex();
    return createIndex(row, column, children.at(row));
}

QModelIndex AccessibilityModel::parent(const QModelIndex &child) const
{
    auto childIface = reinterpret_cast<QAccessibleInterface*>(child.internalPointer());
    return indexForIface(m_childParentMap.value(childIface));
}

QVariant AccessibilityModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
            case 0: return tr("Name");
            case 1: return tr("Role");
        }
    }

    return QAbstractItemModel::headerData(section, orientation, role);
}

void AccessibilityModel::populate(QAccessibleInterface *iface)
{
    if (!iface)
        return;

    m_childParentMap[iface] = iface->parent();
    m_parentChildMap[iface->parent()].push_back(iface);
    m_objectInterfaceMap[iface->object()] = iface;
    m_interfaceObjectMap[iface] = iface->object();

    for (int i = 0; i < iface->childCount(); ++i)
        populate(iface->child(i));

    auto &children = m_parentChildMap[iface->parent()];
    std::sort(children.begin(), children.end());
}

void AccessibilityModel::objectDestroyed(QObject* obj)
{
    const auto it = m_objectInterfaceMap.constFind(obj);
    if (it != m_objectInterfaceMap.constEnd())
        return;

    remove(it.value());
}

void AccessibilityModel::remove(QAccessibleInterface* iface)
{
    auto parentIface = m_childParentMap.value(iface);
    const auto idx = indexForIface(parentIface);

    auto &children = m_parentChildMap[parentIface];
    auto it = std::lower_bound(children.begin(), children.end(), iface);
    if (it == children.end())
        return;
    const auto row = std::distance(children.begin(), it);

    beginRemoveRows(idx, row, row);
    removeRecursive(iface);
    children.erase(it);
    endRemoveRows();
}

void AccessibilityModel::removeRecursive(QAccessibleInterface* iface)
{
    auto obj = m_interfaceObjectMap.value(iface);
    m_interfaceObjectMap.remove(iface);
    m_objectInterfaceMap.remove(obj);

    const auto children = m_parentChildMap.value(iface);
    foreach (const auto &child, children)
        removeRecursive(child);
    m_parentChildMap.remove(iface);
    m_childParentMap.remove(iface);
}

void AccessibilityModel::handleUpdate(QAccessibleEvent* event)
{
    switch (event->type()) {
        case QAccessible::NameChanged:
        {
            const auto idx = indexForIface(event->accessibleInterface());
            if (idx.isValid())
                emit dataChanged(idx, idx);
            break;
        }
        default:
            break;
    }
}

QModelIndex AccessibilityModel::indexForIface(QAccessibleInterface *iface) const
{
    if (!iface)
        return QModelIndex();

    auto parent = m_childParentMap.value(iface);
    const auto parentIndex = indexForIface(parent);
    if (!parentIndex.isValid() && parent)
        return QModelIndex();

    const auto &siblings = m_parentChildMap[parent];
    auto it = std::lower_bound(siblings.constBegin(), siblings.constEnd(), iface);
    if (it == siblings.constEnd() || *it != iface)
        return QModelIndex();

    const int row = std::distance(siblings.constBegin(), it);
    return index(row, 0, parentIndex);
}
