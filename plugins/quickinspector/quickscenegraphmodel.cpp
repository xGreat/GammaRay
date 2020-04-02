/*
  quickscenegraphmodel.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014-2020 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Anton Kreuzkamp <anton.kreuzkamp@kdab.com>

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

#include "quickscenegraphmodel.h"

#include <private/qquickitem_p.h>
#include "quickitemmodelroles.h"

#include <QQuickWindow>
#include <QThread>
#include <QSGNode>

#include <algorithm>

// Q_DECLARE_METATYPE(ObjectView<QSGNode> )

using namespace GammaRay;

QuickSceneGraphModel::QuickSceneGraphModel(QObject *parent)
    : ObjectModelBase<QAbstractItemModel>(parent)
    , m_rootNode(nullptr)
{
}

QuickSceneGraphModel::~QuickSceneGraphModel() = default;

void QuickSceneGraphModel::setWindow(ObjectHandle<QQuickWindow> window)
{
    beginResetModel();
    clear();
    if (m_window)
        disconnect(m_window.object(), &QQuickWindow::afterRendering, this, nullptr);
    m_window = std::move(window);
    m_rootNode = m_window ? m_window->rootNode() : ObjectHandle<QSGNode> {};
    if (m_window && m_rootNode) {
        updateSGTree(false);
        connect(m_window.object(), &QQuickWindow::afterRendering, this, [this]{ updateSGTree(); });
    }

    endResetModel();
}

void QuickSceneGraphModel::updateSGTree(bool emitSignals)
{
    Q_ASSERT(m_window->rootNode() == m_rootNode);
    if (!m_window)
        return;

    m_window->refresh_rootNode();
    auto root = m_window->rootNode();
    if (root != m_rootNode) { // everything changed, reset
        beginResetModel();
        clear();
        m_rootNode = root;
        if (m_window && m_rootNode)
            updateSGTree(false);
        endResetModel();
    } else {
        m_childParentMap[m_rootNode.objectId()] = ObjectId {};
        m_parentChildMap[ObjectId {}].resize(1);
        m_parentChildMap[ObjectId {}][0] = m_rootNode.objectId();

        populateFromNode(m_rootNode, emitSignals);
        collectItemNodes(m_window->contentItem());
    }
}

QVariant QuickSceneGraphModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid())
        return QVariant();

    auto node = nodeForIndex(index);

    if (role == Qt::DisplayRole) {
        if (index.column() == 0) {
            return Util::addressToString(node.object());
        } else if (index.column() == 1) {
            switch (node->type()) {
            case QSGNode::BasicNodeType:
                return "Node";
            case QSGNode::GeometryNodeType:
                return "Geometry Node";
            case QSGNode::TransformNodeType:
                return "Transform Node";
            case QSGNode::ClipNodeType:
                return "Clip Node";
            case QSGNode::OpacityNodeType:
                return "Opacity Node";
            case QSGNode::RootNodeType:
                return "Root Node";
            case QSGNode::RenderNodeType:
                return "Render Node";
            }
        }
    } else if (role == ObjectModel::ObjectRole) {
        return QVariant::fromValue(node.object());
    }

    return QVariant();
}

int QuickSceneGraphModel::rowCount(const QModelIndex &parent) const
{
    if (parent.column() == 1)
        return 0;

    if (!parent.isValid())
        return 1;
    if (parent.internalPointer() == nullptr)
        return 0;

    auto parentNode = nodeForIndex(parent);
    return m_parentChildMap.value(parentNode.objectId()).size();
}

QModelIndex QuickSceneGraphModel::parent(const QModelIndex &child) const
{
    return indexForNode(m_childParentMap.value(ObjectId { child.internalPointer(), "QSGNode" }));
}

QModelIndex QuickSceneGraphModel::index(int row, int column, const QModelIndex &parent) const
{
    auto parentNode = nodeForIndex(parent);
    const auto children = m_parentChildMap.value(parentNode.objectId());

    if (row < 0 || column < 0 || row >= children.size() || column >= columnCount())
        return {};

    return createIndex(row, column, children.at(row).id());
}

void QuickSceneGraphModel::clear()
{
    m_rootNode.clear();
    m_childParentMap.clear();
    m_parentChildMap.clear();
}

// indexForNode() is expensive, so only use it when really needed
#define GET_INDEX if (emitSignals && !hasMyIndex) { myIndex = indexForNode(node.objectId()); hasMyIndex = true; \
}

void QuickSceneGraphModel::populateFromNode(ObjectView<QSGNode> node, bool emitSignals)
{
    if (!node)
        return;

    QVector<ObjectId> &childList = m_parentChildMap[node.objectId()];
    QVector<ObjectHandle<QSGNode>> oldChildList(childList.size()); // We hold this list purely to defer destruction of the handles
    std::transform(childList.cbegin(), childList.cend(), oldChildList.begin(),
        [](ObjectId id) {
            return ObjectShadowDataRepository::handleForObject(reinterpret_cast<QSGNode*>(id.asVoidStar()));
        }
    );

    node->refresh_children();
    node->refresh_childCount(); // TODO maybe we should remove this property, to prevent the two properties to get out of sync...
    QVector<ObjectHandle<QSGNode> > newChildList = node->children();

    QModelIndex myIndex; // don't call indexForNode(node) here yet, in the common case of few changes we waste a lot of time here
    bool hasMyIndex = false;

    std::sort(newChildList.begin(), newChildList.end(),
              [](const ObjectHandle<QSGNode> &lhs, const ObjectHandle<QSGNode> &rhs) {
                return lhs.objectId() < rhs.objectId();
              });

    auto i = childList.begin();
    auto j = newChildList.constBegin();

    while (i != childList.end() && j != newChildList.constEnd()) {
        if (*i < j->objectId()) { // handle deleted node
            emit nodeDeleted(*i);
            GET_INDEX
            if (emitSignals) {
                const auto idx = std::distance(childList.begin(), i);
                beginRemoveRows(myIndex, idx, idx);
            }
            pruneSubTree(*i);
            i = childList.erase(i);
            if (emitSignals)
                endRemoveRows();
        } else if (*i > j->objectId()) { // handle added node
            GET_INDEX
            const auto idx = std::distance(childList.begin(), i);
            if (m_childParentMap.contains(j->objectId())) { // move from elsewhere in our tree
                const auto sourceIdx = indexForNode(j->objectId());
                Q_ASSERT(sourceIdx.isValid());
#if 0
                if (emitSignals)
                    beginMoveRows(sourceIdx.parent(), sourceIdx.row(), sourceIdx.row(), myIndex,
                                  idx);
                m_parentChildMap[m_childParentMap.value(*j)].remove(sourceIdx.row());
                m_childParentMap.insert(*j, node);
                i = childList.insert(i, *j);
                if (emitSignals)
                    endMoveRows();
#else
                if (emitSignals) {
                    beginRemoveRows(sourceIdx.parent(), sourceIdx.row(), sourceIdx.row());
                }
                m_parentChildMap[m_childParentMap.value(j->objectId())].remove(sourceIdx.row());
                m_childParentMap.remove(j->objectId());
                if (emitSignals) {
                    endRemoveRows();
                    beginInsertRows(myIndex, idx, idx);
                }
                m_childParentMap.insert(j->objectId(), node.objectId());
                i = childList.insert(i, j->objectId());
                if (emitSignals) {
                    endInsertRows();
                }
#endif
                populateFromNode(*j, emitSignals);
            } else { // entirely new
                if (emitSignals)
                    beginInsertRows(myIndex, idx, idx);
                m_childParentMap.insert(j->objectId(), node.objectId());
                i = childList.insert(i, j->objectId());
                populateFromNode(*j, false);
                if (emitSignals)
                    endInsertRows();
            }
            ++i;
            ++j;
        } else { // already known node, no change
            populateFromNode(*j, emitSignals);
            ++i;
            ++j;
        }
    }
    if (i == childList.end() && j != newChildList.constEnd()) {
        // Add remaining new items to list and inform the client
        // process the remaining items in pairs of n entirely new ones and 0-1 moved ones
        GET_INDEX
        while (j != newChildList.constEnd()) {
            const auto newBegin = j;
            while (j != newChildList.constEnd() && !m_childParentMap.contains(j->objectId()))
                ++j;

            // newBegin to j - 1 is new, j is either moved or end
            if (newBegin != j) { // new elements
                if (emitSignals) {
                    const auto idx = childList.size();
                    const auto count = std::distance(newBegin, j);
                    beginInsertRows(myIndex, idx, idx + count - 1);
                }
                for (auto it = newBegin; it != j; ++it) {
                    m_childParentMap.insert(it->objectId(), node.objectId());
                    childList.append(it->objectId());
                }
                for (auto it = newBegin; it != j; ++it)
                    populateFromNode(*it, false);
                if (emitSignals)
                    endInsertRows();
            }

            if (j != newChildList.constEnd() && m_childParentMap.contains(j->objectId())) { // one moved element, important to recheck if this is still a move, in case the above has removed it meanwhile...
                const auto sourceIdx = indexForNode(j->objectId());
                Q_ASSERT(sourceIdx.isValid());
#if 0
                if (emitSignals) {
                    const auto idx = childList.size();
                    beginMoveRows(sourceIdx.parent(), sourceIdx.row(), sourceIdx.row(), myIndex,
                                  idx);
                }
                m_parentChildMap[m_childParentMap.value(j->objectId())].remove(sourceIdx.row());
                m_childParentMap.insert(j->objectId(), node);
                childList.append(j->objectId());
                if (emitSignals)
                    endMoveRows();
#else
                if (emitSignals) {
                    beginRemoveRows(sourceIdx.parent(), sourceIdx.row(), sourceIdx.row());
                }
                m_parentChildMap[m_childParentMap.value(j->objectId())].remove(sourceIdx.row());
                m_childParentMap.remove(j->objectId());
                if (emitSignals) {
                    endRemoveRows();
                    const auto idx = childList.size();
                    beginInsertRows(myIndex, idx, idx);
                }
                m_childParentMap.insert(j->objectId(), node.objectId());
                childList.append(j->objectId());
                if (emitSignals) {
                    endInsertRows();
                }
#endif
                populateFromNode(*j, emitSignals);
                ++j;
            }
        }
    } else if (i != childList.end()) { // Inform the client about the removed rows
        GET_INDEX
        const auto idx = std::distance(childList.begin(), i);
        const auto count = std::distance(i, childList.end());

        for (auto it = i; it != childList.end(); ++it)
            emit nodeDeleted(*it);

        if (emitSignals)
            beginRemoveRows(myIndex, idx, idx + count - 1);
        for (; i != childList.end(); ++i)
            pruneSubTree(*i);
        childList.remove(idx, count);
        if (emitSignals)
            endRemoveRows();
    }

//     Q_ASSERT(childList == newChildList);
}

#undef GET_INDEX

void QuickSceneGraphModel::collectItemNodes(ObjectView<QQuickItem> item)
{
    if (!item)
        return;

//     QQuickItemPrivate *priv = QQuickItemPrivate::get(item);
    if (!item->itemNodeInstance()) // Explicitly avoid calling priv->itemNode() here, which would create a new node outside the scenegraph's behavior.
        return;

    ObjectView<QSGNode> itemNode { item->itemNodeInstance() };
    m_itemNodeItemMap[itemNode.objectId()] = item;

    foreach (ObjectView<QQuickItem> child, item->childItems())
        collectItemNodes(child);
}

QModelIndex QuickSceneGraphModel::indexForNode(ObjectId node) const
{
    if (!node)
        return {};

    ObjectId parent = m_childParentMap.value(node);

    const QVector<ObjectId> &siblings = m_parentChildMap[parent];
    Q_ASSERT(std::is_sorted(siblings.constBegin(), siblings.constEnd()));
    auto it = std::lower_bound(siblings.constBegin(), siblings.constEnd(), node);
    if (it == siblings.constEnd() || *it != node)
        return QModelIndex();

    const int row = std::distance(siblings.constBegin(), it);
    return createIndex(row, 0, node.id());
}

ObjectView<QSGNode> QuickSceneGraphModel::sgNodeForItem(ObjectView<QQuickItem> item) const
{
    return item ? item->itemNodeInstance() : nullptr;
}

ObjectView<QQuickItem> QuickSceneGraphModel::itemForSgNode(ObjectView<QSGNode> node) const
{
    ObjectId nodeId = node.objectId();
    while (nodeId && !m_itemNodeItemMap.contains(nodeId)) {
        // If there's no entry for node, take its parent
        nodeId = m_childParentMap[nodeId];
    }
    return m_itemNodeItemMap[nodeId];
}

bool QuickSceneGraphModel::verifyNodeValidity(ObjectView<QSGNode> node)
{
    if (node == m_rootNode)
        return true;

    ObjectView<QQuickItem> item = itemForSgNode(node);
    if (!item)
        return false;

    ObjectView<QSGNode> itemNode { item->itemNodeInstance() };
    bool valid = itemNode == node || recursivelyFindChild(itemNode, node);
    if (!valid) {
        // The tree got dirty without us noticing. Expecting more to be invalid,
        // so update the whole tree to ensure it's current.
        setWindow(m_window);
    }
    return valid;
}

bool QuickSceneGraphModel::recursivelyFindChild(ObjectView<QSGNode> root, ObjectView<QSGNode> child) const
{
    for (ObjectView<QSGNode> childNode : root->children()) {
        if (childNode == child || recursivelyFindChild(childNode, child))
            return true;
    }
    return false;
}

void QuickSceneGraphModel::pruneSubTree(ObjectId nodeId)
{
    foreach (auto childId, m_parentChildMap.value(nodeId))
        pruneSubTree(childId);
    m_parentChildMap.remove(nodeId);
    m_childParentMap.remove(nodeId);
}

ObjectView<QSGNode> QuickSceneGraphModel::nodeForIndex(const QModelIndex& index) const
{
    return ObjectShadowDataRepository::viewForObject(reinterpret_cast<QSGNode*>(index.internalPointer()));
}

