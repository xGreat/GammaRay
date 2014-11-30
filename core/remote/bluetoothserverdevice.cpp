/*
  bluetoothserverdevice.cpp

  This file is part of GammaRay, the Qt application inspection and
  manipulation tool.

  Copyright (C) 2014 Klarälvdalens Datakonsult AB, a KDAB Group company, info@kdab.com
  Author: Volker Krause <volker.krause@kdab.com>

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

#include "bluetoothserverdevice.h"

#include <QBluetoothServer>
#include <QBluetoothServiceInfo>
#include <QBluetoothSocket>

using namespace GammaRay;

BluetoothServerDevice::BluetoothServerDevice(QObject* parent):
    ServerDevice(parent),
    m_server(new QBluetoothServer(QBluetoothServiceInfo::RfcommProtocol, this))
{
    connect(m_server, &QBluetoothServer::newConnection, this, &ServerDevice::newConnection);
}

BluetoothServerDevice::~BluetoothServerDevice()
{
}

bool BluetoothServerDevice::listen()
{
    if (!m_server->listen())
        return false;

    QBluetoothServiceInfo serviceInfo;

    static const QLatin1String serviceUuid("c78660df-2208-4e12-a5f0-70291ec09948");
    serviceInfo.setServiceUuid(QBluetoothUuid(serviceUuid));

    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceName, tr("GammaRay Probe"));
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceDescription, tr("KDAB GammaRay Qt introspection probe"));
    serviceInfo.setAttribute(QBluetoothServiceInfo::ServiceProvider, tr("KDAB"));

    serviceInfo.setAttribute(QBluetoothServiceInfo::BrowseGroupList, QBluetoothUuid(QBluetoothUuid::PublicBrowseGroup));

    QBluetoothServiceInfo::Sequence protocolDescriptorList;
    QBluetoothServiceInfo::Sequence protocol;
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::L2cap));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    protocol.clear();
    protocol << QVariant::fromValue(QBluetoothUuid(QBluetoothUuid::Rfcomm)) << QVariant::fromValue(quint8(m_server->serverPort()));
    protocolDescriptorList.append(QVariant::fromValue(protocol));
    serviceInfo.setAttribute(QBluetoothServiceInfo::ProtocolDescriptorList, protocolDescriptorList);

    return serviceInfo.registerService();
}

QUrl BluetoothServerDevice::externalAddress() const
{
    QUrl url;
    url.setScheme("bluetooth");
    url.setHost('[' + m_server->serverAddress().toString() + ']');
    url.setPort(m_server->serverPort());
    return url;
}

QString BluetoothServerDevice::errorString() const
{
    // TODO
    return QString::number(m_server->error());
}

QIODevice* BluetoothServerDevice::nextPendingConnection()
{
    Q_ASSERT(m_server->hasPendingConnections());
    return m_server->nextPendingConnection();
}
