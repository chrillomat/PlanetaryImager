/*
 * GuLinux Planetary Imager - https://github.com/GuLinux/PlanetaryImager
 * Copyright (C) 2017  Marco Gulino <marco@gulinux.net>
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

#include "scriptingengine.h"
#include <QtQml/QJSEngine>
#include <QDebug>
#include "protocol/scriptingprotocol.h"

using namespace std;


class ScriptingConsole : public QObject {
  Q_OBJECT
public:
  ScriptingConsole(const NetworkDispatcher::ptr &dispatcher, QObject *parent = nullptr) : QObject{parent}, dispatcher{dispatcher} {}
public slots:
  void log(const QJSValue &v);
private:
  NetworkDispatcher::ptr dispatcher;
};

void ScriptingConsole::log(const QJSValue& v)
{
  dispatcher->send(ScriptingProtocol::packetScriptReply() << v.toString());
}


DPTR_IMPL(ScriptingEngine) {
  PlanetaryImager::ptr planetaryImager;
  ScriptingEngine *q;
  QJSEngine engine;
};




ScriptingEngine::ScriptingEngine(const PlanetaryImager::ptr &planetaryImager, const NetworkDispatcher::ptr &dispatcher, QObject *parent) 
: QObject(parent),  NetworkReceiver{dispatcher}, dptr(planetaryImager, this)
{
  QJSValue console = d->engine.newQObject(new ScriptingConsole(dispatcher, this));
  QJSValue planetaryImagerJs = d->engine.newQObject(planetaryImager.get());
  d->engine.globalObject().setProperty("planetaryImager", planetaryImagerJs);
  d->engine.globalObject().setProperty("console", console);
  register_handler(ScriptingProtocol::Script, [this](const NetworkPacket::ptr &packet) {
    run(packet->payloadVariant().toString());
  });
  connect(this, &ScriptingEngine::reply, this, [dispatcher](const QString &message) {
    dispatcher->send(ScriptingProtocol::packetScriptReply() << message);
  });
}

ScriptingEngine::~ScriptingEngine()
{
}

void ScriptingEngine::run(const QString& script)
{
  qDebug() << "Running script: " << script;
  QJSValue v = d->engine.evaluate(script);
  if(v.isError())
    emit reply(v.toString());
}

#include "scriptingengine.moc"
