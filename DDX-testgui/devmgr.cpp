/******************************************************************************
 *                         DATA DISPLAY APPLICATION X                         *
 *                            2B TECHNOLOGIES, INC.                           *
 *                                                                            *
 * The DDX is free software: you can redistribute it and/or modify it under   *
 * the terms of the GNU General Public License as published by the Free       *
 * Software Foundation, either version 3 of the License, or (at your option)  *
 * any later version.  The DDX is distributed in the hope that it will be     *
 * useful, but WITHOUT ANY WARRANTY; without even the implied warranty of     *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General  *
 * Public License for more details.  You should have received a copy of the   *
 * GNU General Public License along with the DDX.  If not, see                *
 * <http://www.gnu.org/licenses/>.                                            *
 *                                                                            *
 *  For more information about the DDX, check out the 2B website or GitHub:   *
 *       <http://twobtech.com/DDX>       <https://github.com/2BTech/DDX>      *
 ******************************************************************************/

#include "devmgr.h"
#include "mainwindow.h"

DevMgr::DevMgr(MainWindow *parent) : QObject(parent)
{
	mw = parent;
	unregCt = 0;
	closing = false;
	qRegisterMetaType<Response*>("Response*");
	mw->getLogArea()->appendPlainText(tr("Testing: %1").arg(QMetaType::isRegistered(QMetaType::type("Response*"))));
}

DevMgr::~DevMgr()
{
	
}

void DevMgr::closeAll(RemDev::DisconnectReason reason) {
	closing = true;
	dLock.lock();
	for (int i = 0; i < devices.size(); i++) {
		devices.at(i)->close(reason);
	}
	devices.clear();
	dLock.unlock();
	closing = false;
}

QByteArray DevMgr::addDevice(RemDev *dev) {
	connect(dev, &RemDev::postToLogArea, mw->getLogArea(), &QPlainTextEdit::appendPlainText);
	dLock.lock();
	devices.append(dev);
	dLock.unlock();
	QString name = tr("Unknown%1").arg(++unregCt);
	return name.toUtf8();
}

void DevMgr::removeDevice(RemDev *dev) {
	if (closing) return;
	dLock.lock();
	devices.removeOne(dev);
	dLock.unlock();
}
