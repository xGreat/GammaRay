/*
  accessibilitywidget.cpp

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

#include "accessibilitywidget.h"
#include "ui_accessibilitywidget.h"

#include <ui/contextmenuextension.h>
#include <ui/searchlinecontroller.h>

#include <common/objectbroker.h>
#include <common/objectmodel.h>

#include <QMenu>

using namespace GammaRay;

AccessibilityWidget::AccessibilityWidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::AccessibilityWidget)
{
    ui->setupUi(this);

    auto model = ObjectBroker::model(QStringLiteral("com.kdab.GammaRay.AccessibilityModel"));
    ui->a11yTree->setModel(model);
    ui->a11yTree->setSelectionModel(ObjectBroker::selectionModel(model));

    new SearchLineController(ui->a11yTreeSearchLine, model);
//     ui->a11yTree->header()->setResizeMode(0, QHeaderView::ResizeToContents);
    connect(ui->a11yTree, &QTreeView::customContextMenuRequested, this, &AccessibilityWidget::contextMenu);

    ui->a11yPropertyWidget->setObjectBaseName(QStringLiteral("com.kdab.GammaRay.Accessibility"));
}

AccessibilityWidget::~AccessibilityWidget()
{
}

void AccessibilityWidget::contextMenu(QPoint pos)
{
    auto index = ui->a11yTree->indexAt(pos);
    if (!index.isValid())
        return;
    index = index.sibling(0, index.row());

    const auto objectId = index.data(ObjectModel::ObjectIdRole).value<ObjectId>();
    QMenu menu;
    ContextMenuExtension ext(objectId);
    ext.setLocation(ContextMenuExtension::Creation, index.data(ObjectModel::CreationLocationRole).value<SourceLocation>());
    ext.populateMenu(&menu);

    menu.exec(ui->a11yTree->viewport()->mapToGlobal(pos));
}
