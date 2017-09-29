/*
  accessibilitymodel.h

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

#ifndef GAMMARAY_ACCESSIBILITYMODEL_H
#define GAMMARAY_ACCESSIBILITYMODEL_H

#include <common/objectmodel.h>

#include <QAbstractItemModel>
#include <QAccessible>
#include <QHash>

Q_DECLARE_METATYPE(QAccessibleInterface*)

namespace GammaRay {

class AccessibilityModel : public QAbstractItemModel
{
    Q_OBJECT
public:
    explicit AccessibilityModel(QObject *parent = nullptr);
    ~AccessibilityModel();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void handleUpdate(QAccessibleEvent *event);

public slots:
    void objectDestroyed(QObject *obj);

private:
    void populate(QAccessibleInterface *iface);
    void remove(QAccessibleInterface *iface);
    void removeRecursive(QAccessibleInterface *iface);
    QModelIndex indexForIface(QAccessibleInterface *iface) const;

    QHash<QAccessibleInterface*, QAccessibleInterface*> m_childParentMap;
    QHash<QAccessibleInterface*, QVector<QAccessibleInterface*> > m_parentChildMap;
    QHash<QObject*, QAccessibleInterface*> m_objectInterfaceMap;
    QHash<QAccessibleInterface*, QObject*> m_interfaceObjectMap;
};
}

#endif // GAMMARAY_ACCESSIBILITYMODEL_H
