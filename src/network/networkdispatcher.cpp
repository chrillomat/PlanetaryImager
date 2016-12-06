/*
 * GuLinux Planetary Imager - https://github.com/GuLinux/PlanetaryImager
 * Copyright (C) 2016  Marco Gulino <marco@gulinux.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "networkdispatcher.h"
#include <QHash>
#include <QHash>
#include <QCoreApplication>
#include "commons/utils.h"

using namespace std;

DPTR_IMPL(NetworkDispatcher) {
  QSet<NetworkReceiver *> receivers;
  QTcpSocket *socket = nullptr;
  void readyRead();
};


DPTR_IMPL(NetworkReceiver) {
  const NetworkDispatcher::ptr dispatcher;
  QHash<NetworkPacket::Type, bool> packets_processed;
  QHash<NetworkPacket::Type, NetworkReceiver::HandlePacket> handlers;
};

NetworkReceiver::NetworkReceiver(const NetworkDispatcher::ptr &dispatcher) : dptr(dispatcher)
{
  dispatcher->attach(this);
}

NetworkReceiver::~NetworkReceiver()
{
  d->dispatcher->detach(this);
}

NetworkDispatcher::ptr NetworkReceiver::dispatcher() const
{
  return d->dispatcher;
}


void NetworkReceiver::wait_for_processed(const QString& name) const
{
  d->packets_processed[name] = false;
  while(! d->packets_processed[name])
    qApp->processEvents();
}



void NetworkReceiver::register_handler(const NetworkPacket::Type& name, const HandlePacket handler)
{
  d->handlers[name] = handler;
}

void NetworkReceiver::handle(const NetworkPacket::ptr& packet)
{
  LOG_F_SCOPE
  qDebug() << "Handling packet " << packet->name();
  auto handler = d->handlers[packet->name()];
  if(handler)
    handler(packet);
  d->packets_processed[packet->name()] = true;
}



NetworkDispatcher::NetworkDispatcher(QObject* parent) : QObject{parent}, dptr()
{
  static bool metatypes_registered = false;
  if(!metatypes_registered) {
    metatypes_registered = true;
    qRegisterMetaType<NetworkPacket::ptr>("NetworkPacket::ptr");
  }
}

NetworkDispatcher::~NetworkDispatcher()
{
}

void NetworkDispatcher::attach(NetworkReceiver* receiver)
{
  d->receivers.insert(receiver);
}

void NetworkDispatcher::detach(NetworkReceiver* receiver)
{
  d->receivers.remove(receiver);
}

void NetworkDispatcher::setSocket(QTcpSocket* socket)
{
  delete d->socket;
  d->socket = socket;
  connect(socket, &QTcpSocket::readyRead, bind(&Private::readyRead, d.get()));
}

void NetworkDispatcher::send(const NetworkPacket::ptr &packet) {
  qDebug() << "Sending tcp packet: " << packet->name();
  packet->sendTo(d->socket);
}

void NetworkDispatcher::queue_send(const NetworkPacket::ptr& packet)
{
  QMetaObject::invokeMethod(this, "send", Q_ARG(NetworkPacket::ptr, packet) );
}


void NetworkDispatcher::Private::readyRead()
{
  auto packet = make_shared<NetworkPacket>();
  packet->receiveFrom(socket);

  for(auto receiver: receivers)
    receiver->handle(packet);
}
