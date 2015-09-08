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

#include "netdev.h"
#include "devmgr.h"
#include "network.h"

NetDev::NetDev(Network *n, DevMgr *dm, qintptr socketDescriptor) : RemDev(dm, 0) {
	s = 0;
	this->n = n;
	descriptor = socketDescriptor;
}

NetDev::NetDev(Network *n, DevMgr *dm, int ref, const QString &hostName, quint16 port,
			   QAbstractSocket::NetworkLayerProtocol protocol) : RemDev(dm, ref) {
	s = 0;
	this->n = n;
	descriptor = 0;
	this->hostName = hostName;
	this->port = port;
	this->protocol = protocol;
}

NetDev::~NetDev() {
	if (s) delete s;
}

void NetDev::sub_init() noexcept {
	// Build & configure socket
	s = new QSslSocket(this);
	s->setSslConfiguration(n->getSslConfig());
	// Handle incoming connection
	if (descriptor) {
		if ( ! s->setSocketDescriptor(descriptor)) {
			log(tr("Could not "))
			deleteLater();
			return;
		}
		if (s->state() != QAbstractSocket::ConnectedState) {
			s->deleteLater();
			return;
		}
		pending.append(PendingConnection(s, 0));
		// The error signals are overloaded, so we need to use these nasty things
		connect(s, static_cast<void(QSslSocket::*)(QAbstractSocket::SocketError)>(&QSslSocket::error),
				this, &Network::handleNetworkError);
		connect(s, static_cast<void(QSslSocket::*)(const QList<QSslError> &)>(&QSslSocket::sslErrors),
				this, &Network::handleEncryptionErrors);
		connect(s, &QSslSocket::encrypted, this, &Network::handleSocketNowEncrypted);
		s->startServerEncryption();
		log(tr("Received new connection from %1:%2").arg(s->peerAddress().toString(), QString::number(s->peerPort())));
	}
	// Create outgoing connection
	else {
		
	}
	
	
	
	
	
	
	
	
	
	// Determine basic information about the connection itself
	bool usingIPv6 = s->peerAddress().protocol() == QAbstractSocket::IPv6Protocol;
	QHostAddress localhost = usingIPv6 ? QHostAddress::LocalHostIPv6 : QHostAddress::LocalHost;
	bool isLocal = s->peerAddress() == localhost;
	
	s->setSocketOption(QAbstractSocket::LowDelayOption, 1);  // Disable Nagel's algorithm
	if ( ! isLocal) s->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
	
	// QTcpServer::error is overloaded, so we need to use this nasty thing
	// The error signals are overloaded, so we need to use these nasty things
	connect(s, static_cast<void(QSslSocket::*)(QAbstractSocket::SocketError)>(&QSslSocket::error),
			this, &NetDev::handleNetworkError);
	connect(s, static_cast<void(QSslSocket::*)(const QList<QSslError> &)>(&QSslSocket::sslErrors),
			this, &NetDev::handleEncryptionErrors);
	connect(s, &QTcpSocket::disconnected, this, &NetDev::handleDisconnection);
	connect(s, &QTcpSocket::readyRead, this, &NetDev::handleData);
	connectionReady();
}

void NetDev::terminate(DisconnectReason reason, bool fromRemote) noexcept {
	(void) reason;
	(void) fromRemote;
	s->disconnectFromHost();
}

void NetDev::writeItem(rapidjson::StringBuffer *buffer) noexcept {
	s->write(buffer->GetString(), buffer->GetSize());
	delete buffer;
}

void NetDev::handleData() {
	if ( ! s->isEncrypted()) return;
	while (s->canReadLine()) {
		qint64 size = s->bytesAvailable() + 1;
		char *buffer = new char[size];
		qint64 amount = s->readLine(buffer, size);
		if (amount > 0) handleItem(buffer);
		else delete buffer;
	}
}

void NetDev::handleDisconnection() {
	log("Remote disconnected");
	// TODO
	close(UnknownReason, true);
}

void NetDev::handleNetworkError(QAbstractSocket::SocketError error) {
	// TODO
	
	// RemoteClosedError is emitted even on normal disconnections
	if (error == QAbstractSocket::RemoteHostClosedError) return;
	
	log(QString("DDX bug: Unhandled network error (QAbstractSocket): '%1'").arg(error));
}

void NetDev::handleEncryptionErrors(const QList<QSslError> & errors) {
	// TODO
}
