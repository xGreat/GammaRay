/*
  accessibility.cpp

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

#include "accessibility.h"
#include "accessibilitymodel.h"

#include <core/enumutil.h>
#include <core/metaproperty.h>
#include <core/metaobject.h>
#include <core/metaobjectrepository.h>
#include <core/probeinterface.h>
#include <core/propertycontroller.h>
#include <core/remote/serverproxymodel.h>
#include <core/util.h>
#include <core/varianthandler.h>

#include <common/objectbroker.h>

#include <kde/krecursivefilterproxymodel.h>

#include <QDebug>
#include <QItemSelectionModel>
#include <QWindow>

Q_DECLARE_METATYPE(QAccessible::State)
Q_DECLARE_METATYPE(QAccessibleActionInterface*)
Q_DECLARE_METATYPE(QAccessibleEditableTextInterface*)
Q_DECLARE_METATYPE(QAccessibleImageInterface*)
Q_DECLARE_METATYPE(QAccessibleTableCellInterface*)
Q_DECLARE_METATYPE(QAccessibleTableInterface*)
Q_DECLARE_METATYPE(QAccessibleTextInterface*)
Q_DECLARE_METATYPE(QAccessibleValueInterface*)

using namespace GammaRay;

static void gammaray_a11y_event_handler(QAccessibleEvent *event)
{
    Accessibility::instance()->handleUpdate(event);
}

Accessibility* Accessibility::s_instance = nullptr;

Accessibility::Accessibility(ProbeInterface *probe, QObject *parent)
    : QObject(parent)
    , m_propertyController(new PropertyController(QStringLiteral("com.kdab.GammaRay.Accessibility"), this))
{
    s_instance = this;
    registerMetaTypes();

    m_model = new AccessibilityModel(this);
    auto proxy = new ServerProxyModel<KRecursiveFilterProxyModel>(this);
    proxy->setSourceModel(m_model);
    proxy->addRole(ObjectModel::ObjectIdRole);
    proxy->addRole(ObjectModel::CreationLocationRole);
    probe->registerModel(QStringLiteral("com.kdab.GammaRay.AccessibilityModel"), proxy);

    m_selectionModel = ObjectBroker::selectionModel(proxy);
    connect(m_selectionModel, &QItemSelectionModel::selectionChanged, this, &Accessibility::itemSelected);

    connect(probe->probe(), SIGNAL(objectDestroyed(QObject*)), m_model, SLOT(objectDestroyed(QObject*)));

    m_prevUpdateHandler = QAccessible::installUpdateHandler(gammaray_a11y_event_handler);
}

Accessibility::~Accessibility()
{
    QAccessible::installUpdateHandler(m_prevUpdateHandler);
    s_instance = nullptr;
}

Accessibility* Accessibility::instance()
{
    return s_instance;
}

void Accessibility::handleUpdate(QAccessibleEvent* event)
{
    if (m_prevUpdateHandler)
        (*m_prevUpdateHandler)(event);
    qDebug() << event->type();
    m_model->handleUpdate(event);
}

void Accessibility::itemSelected(const QItemSelection& sel)
{
    if (sel.isEmpty()) {
        m_propertyController->setObject(nullptr);
        return;
    }

    const auto index = sel.at(0).topLeft();
    const auto iface = index.data(ObjectModel::ObjectRole).value<void*>();
    m_propertyController->setObject(iface, "QAccessibleInterface");
}

// support for non-const getters...
template <typename Class, typename GetterReturnType>
static MetaProperty* makeNonConstProperty(const char *name, GetterReturnType(Class::*getter)())
{
    return new MetaPropertyImpl<Class, GetterReturnType>(name, reinterpret_cast<GetterReturnType(Class::*)() const>(getter), nullptr);
}
#define MO_ADD_PROPERTY_NC(Class, Getter) \
    mo->addProperty(makeNonConstProperty(#Getter, &Class::Getter))

void Accessibility::registerMetaTypes()
{
    MetaObject *mo = nullptr;
    MO_ADD_METAOBJECT0(QAccessibleInterface);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, actionInterface);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, backgroundColor);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, childCount);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, editableTextInterface);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, focusChild);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, foregroundColor);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, imageInterface);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, isValid);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, object);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, parent);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, rect);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, role);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, state);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, tableCellInterface);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, tableInterface);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, textInterface);
    MO_ADD_PROPERTY_NC(QAccessibleInterface, valueInterface);
    MO_ADD_PROPERTY_RO(QAccessibleInterface, window);

    MO_ADD_METAOBJECT0(QAccessibleActionInterface);
    MO_ADD_PROPERTY_RO(QAccessibleActionInterface, actionNames);

    MO_ADD_METAOBJECT0(QAccessibleImageInterface);
    MO_ADD_PROPERTY_RO(QAccessibleImageInterface, imageDescription);
    MO_ADD_PROPERTY_RO(QAccessibleImageInterface, imageSize);
    MO_ADD_PROPERTY_RO(QAccessibleImageInterface, imagePosition);

    MO_ADD_METAOBJECT0(QAccessibleTableCellInterface);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, columnExtent);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, columnHeaderCells);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, columnIndex);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, isSelected);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, rowExtent);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, rowHeaderCells);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, rowIndex);
    MO_ADD_PROPERTY_RO(QAccessibleTableCellInterface, table);

    MO_ADD_METAOBJECT0(QAccessibleTableInterface);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, caption);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, columnCount);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, rowCount);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, selectedCellCount);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, selectedCells);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, selectedColumnCount);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, selectedColumns);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, selectedRows);
    MO_ADD_PROPERTY_RO(QAccessibleTableInterface, summary);

    MO_ADD_METAOBJECT0(QAccessibleTextInterface);
    MO_ADD_PROPERTY_RO(QAccessibleTextInterface, characterCount);
    MO_ADD_PROPERTY_RO(QAccessibleTextInterface, cursorPosition);
    MO_ADD_PROPERTY_RO(QAccessibleTextInterface, selectionCount);

    MO_ADD_METAOBJECT0(QAccessibleValueInterface);
    MO_ADD_PROPERTY   (QAccessibleValueInterface, currentValue, setCurrentValue);
    MO_ADD_PROPERTY_RO(QAccessibleValueInterface, maximumValue);
    MO_ADD_PROPERTY_RO(QAccessibleValueInterface, minimumStepSize);
    MO_ADD_PROPERTY_RO(QAccessibleValueInterface, minimumValue);

    VariantHandler::registerStringConverter<QAccessibleInterface*>([](QAccessibleInterface *iface) -> QString {
        if (!iface)
            return QStringLiteral("<null>");
        const auto name = iface->text(QAccessible::Name);
        if (!name.isEmpty())
            return name;
        return QLatin1Char('[') + EnumUtil::enumToString(QVariant::fromValue(iface->role()), nullptr, &QAccessible::staticMetaObject) + QLatin1Char(']');
    });
    VariantHandler::registerStringConverter<QAccessibleActionInterface*>(Util::addressToString);
    VariantHandler::registerStringConverter<QAccessibleEditableTextInterface*>(Util::addressToString);
    VariantHandler::registerStringConverter<QAccessibleImageInterface*>(Util::addressToString);
    VariantHandler::registerStringConverter<QAccessibleTableCellInterface*>(Util::addressToString);
    VariantHandler::registerStringConverter<QAccessibleTableInterface*>(Util::addressToString);
    VariantHandler::registerStringConverter<QAccessibleTextInterface*>(Util::addressToString);
    VariantHandler::registerStringConverter<QAccessibleValueInterface*>(Util::addressToString);
}

#undef MO_ADD_PROPERTY_NC
