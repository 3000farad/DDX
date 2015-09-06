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

#ifndef REMDEV_H
#define REMDEV_H

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QTimer>
#include <QTimeZone>
#include <QFlags>
#include <QMutex>
#include <QThread>
#include <QByteArray>
#include <QPointer>
#include "../rapidjson/include/rapidjson/document.h"
#include "../rapidjson/include/rapidjson/stringbuffer.h"
#include "../rapidjson/include/rapidjson/writer.h"
#include "../rapidjson/include/rapidjson/reader.h"
#include "constants.h"

class DevMgr;

/*!
 * \brief DDX-RPC remote device base class
 * 
 * ### %Request & %Response Document Privacy
 * The root documents produced by the incoming object handler are hidden by design so as
 * to prevent any mistyping attacks with double members.  While the JSON specification
 * disallows key duplication in objects, RapidJSON does not signal the phenomenon as a
 * parse error for efficiency reasons.  Furthermore, such JSON data may actually be useful
 * for certain cases (such as the DDX-RPC `Settings` type) if the standard is ever changed.
 * However, when unexpected, an attacker can theoretically use duplicates to bypass the
 * type restrictions made prior to Request and Response delivery.  Always use the pointers
 * provided by these two classes rather than using Value::FindMember on the root document.
 */
class RemDev : public QObject
{
	Q_OBJECT
public:
#ifndef QT_DEBUG
	friend class TestDev;
#endif
	
	typedef int LocalId;
	
	enum DeviceRoles {
		DaemonRole = 0x1,  //!< Can execute paths
		ManagerRole = 0x2,  //!< An interface for a device which executes paths
		VertexRole = 0x4,  //!< A data responder or producer which does not execute paths
		ListenerRole = 0x8,  //!< A destination for loglines and alerts
		GlobalRole = 0x80  //!< A pseudo-role which indicates role-less information
	};
	
	//! Enumerates various disconnection reasons
	enum DisconnectReason {
		UnknownReason,  //!< Unknown disconnection
		ShuttingDown,  //!< The disconnecting member is shutting down by request
		Restarting,  //!< The disconnecting member is restarting and will be back shortly
		FatalError,  //!< The disconnecting member experienced a fatal error and is shutting down
		ConnectionTerminated,  //!< The connection was explicitly terminated
		RegistrationTimeout,  //!< The connection was alive too long without registering
		BufferOverflow,  //!< The connection sent an object too long to be handled
		StreamClosed,  //!< The stream was closed by its low-level handler
		EncryptionRequired  //!< Encryption is required on this connection
	};
	
	/*!
	 * \brief Represents an incoming RPC request or notification
	 * 
	 * Note that the JSON data will also be deleted when the Response is deleted, so if a copy
	 * of the #mainVal is required, that copy must be built with one of RapidJSON's deep copy
	 * operations, such as Value::CopyFrom.  Handlers **must** eventually delete the Request
	 * object they are passed.
	 * 
	 * ## Responding to Requests
	 * Incoming DDX-RPC requests and notifications are passed to their handlers with the
	 * Request class.  This class and RemDev provide a convenient API for using RapidJSON
	 * to produce and send a corresponding response or error response.  The simplest way to
	 * respond to requests is using the overloads of RemDev::sendRequest and  RemDev::sendError
	 * which take in a pointer to a Request.  If you need a reference to a RapidJSON allocator
	 * when building supplemental information (as is often necessary), #alloc will return the
	 * same allocator used to produce the final response object.
	 * 
	 */
	class Request {
	public:
		friend class RemDev;
		friend class TestDev;  // TODO:  Remove
		
		~Request() {
			delete doc;
			if (outDoc) delete outDoc;
		}
		
		/*!
		 * \brief Retrieve the allocator for outgoing RapidJSON 
		 * \return Reference to the allocator
		 */
		rapidjson::MemoryPoolAllocator<> &alloc() {
			if ( ! outDoc) outDoc = new rapidjson::Document;
			return outDoc->GetAllocator();
		}
		
		/*!
		 * \brief Whether this is a notification or request
		 * \return True if notification
		 */
		bool isRequest() const {
			return (bool) id;
		}
		
		//! Method name
		const char *method;
		
		//! The contents of the params element (may be 0, otherwise guaranteed to be an object)
		rapidjson::Value *params;
		
		//! The device which sent the request
		RemDev *device;
		
	private:
		//! Internal constructor for RemDev
		Request(const char *method, rapidjson::Value *params, rapidjson::Document *doc,
				RemDev *device, rapidjson::Value *id) {
			this->method = method;
			this->params = params;
			this->device = device;
			this->id = id;
			this->doc = doc;
			this->outDoc = 0;
		}
		
		//! ID value (no type checking, will be 0 if this is a notification)
		rapidjson::Value *id;
		
		//! Root document pointer
		rapidjson::Document *doc;
		
		//! Response document for use with the direct-response versions of RemDev::sendRequest and RemDev::sendError
		rapidjson::Document *outDoc;
	};
	
	/*!
	 * \brief Represents a response to an RPC request
	 * 
	 * Note that the JSON data will also be deleted when the Response is deleted, so if a copy
	 * of the #mainVal is required, that copy must be built with one of RapidJSON's deep copy
	 * operations, such as Value::CopyFrom.
	 * 
	 * Response handlers must have the prototype "void fn(Response *)" and will be called with
	 * queued connections (and thus will be executed by the receiving event loop). They **must**
	 * eventually delete the Response they are passed.  The also must either be declared as
	 * slots or with the `Q_INVOKABLE` macro.
	 * 
	 * The only guarantee made if #successful is true is that #mainVal is set.  If #successful is
	 * false, #mainVal is guaranteed to be a "verified" error object.  This means that it has a
	 * "code" member for which IsInt() returns true and a "message" member for which IsString()
	 * returns true.  
	 */
	class Response {
	public:
		friend class RemDev;
		
		~Response() {
			delete doc;
		}
		
		//! True if the response was a response, false if there was an error
		bool successful;
		
		//! The integer ID returned by the corresponding sendRequest call
		int id;
		
		//! The contents of the "response" element on success or "error" on error
		rapidjson::Value *mainVal;
		
		//!The method name which was passed to sendRequest
		const char *method;
		
		//! The device which received the response
		RemDev *device;
		
	private:
		//! Internal constructor for RemDev
		Response(bool successful, int id, const char *method, rapidjson::Document *doc,
				 RemDev *device, rapidjson::Value *mainVal = 0) {
			this->successful = successful;
			this->id = id;
			this->method = method;
			this->mainVal = mainVal;
			this->device = device;
			this->doc = doc;
		}
		
		//! Root document pointer (may be equivalent to #mainVal)
		rapidjson::Document *doc;
	};
	
	explicit RemDev(DevMgr *dm, QByteArray *ref = 0);
	
	~RemDev();
	
	/*!
	 * \brief Send a new request
	 * \param self The object on which the handler will be called
	 * \param handler The name of the handler function to call
	 * \param method The method name (UTF-8, must live through handling)
	 * \param params Any parameters (0 to omit, will be **nullified, not deleted**)
	 * \param doc Pointer to RapidJSON Document (0 if none, will be **deleted**)
	 * \param timeout Request timeout in msecs (0 to disable timeout, not recommended)
	 * \return The integer ID which will also be in the corresponding Response object
	 * 
	 * See the Response class for information on handling the resulting response.
	 * 
	 * Note that if this is called immediately before termination, it will return -1
	 * and no request will be sent.
	 */
	int sendRequest(QObject *self, const char *handler, const char *method, rapidjson::Value *params = 0,
					rapidjson::Document *doc = 0, qint64 timeout = DEFAULT_REQUEST_TIMEOUT);
	
	/*!
	 * \brief Send a successful response directly to a Request
	 * \param req The Request object (will be **deleted**)
	 * \param result The result (0 -> true, will be **nullified, not deleted**)
	 * 
	 * If \a req is a notification, it will still be deleted but no response will be sent.
	 * 
	 * This is an overloaded function.
	 */
	void sendResponse(Request *req, rapidjson::Value *result = 0);
	
	/*!
	 * \brief Send a successful response
	 * \param id The remote-generated transaction ID (will be **nullified, not deleted**)
	 * \param doc Pointer to RapidJSON Document (0 if none, will be **deleted**)
	 * \param result The result (0 -> true, will be **nullified, not deleted**)
	 */
	void sendResponse(rapidjson::Value &id, rapidjson::Document *doc = 0, rapidjson::Value *result = 0);
	
	/*!
	 * \brief Send an error response directly to a Request
	 * \param req The Request object (will be **deleted**)
	 * \param code The integer error code
	 * \param msg The error message
	 * \param data Pointer to any data (0 to omit, will be **nullified, not deleted**)
	 * 
	 * If \a req is a notification, it will still be deleted but no response will be sent.
	 * 
	 * This is an overloaded function.
	 */
	void sendError(Request *req, int code, const QString &msg, rapidjson::Value *data = 0) noexcept;
	
	/*!
	 * \brief Send an error response directly to a Request by code alone
	 * \param req The Request object (will be **deleted**)
	 * \param code The integer error code
	 * 
	 * Supports:
	 * - `E_JSON_INTERNAL` ("Internal error")
	 * - `E_ACCESS_DENIED` ("Access denied")
	 * - `E_NOT_SUPPORTED` ("Not supported")
	 * - `E_JSON_PARAMS` ("Invalid params")
	 * - `E_JSON_METHOD` ("Method not found")
	 * 
	 * If \a req is a notification, it will still be deleted but no response will be sent.
	 * 
	 * This is an overloaded function.
	 */
	void sendError(Request *req, int code = E_JSON_INTERNAL) noexcept;
	
	/*!
	 * \brief Send an error response
	 * \param id Pointer to remote-generated transaction ID, (0 -> null, will be **nullified, not deleted**)
	 * \param code The integer error code
	 * \param doc Pointer to RapidJSON Document (0 if none, will be **deleted**)
	 * \param msg The error message
	 * \param data Pointer to any data (0 to omit, will be **nullified, not deleted**)
	 */
	void sendError(rapidjson::Value *id, int code, const QString &msg, rapidjson::Value *data = 0,
				   rapidjson::Document *doc = 0) noexcept;
	
	/*!
	 * \brief Send a notification
	 * \param method The method name (UTF8)
	 * \param doc Pointer to RapidJSON Document (0 if none, will be **deleted**)
	 * \param params Any parameters (0 to omit, will be **nullified, not deleted**)
	 */
	void sendNotification(const char *method, rapidjson::Document *doc = 0,
						  rapidjson::Value *params = 0) noexcept;
	
	static QByteArray serializeValue(const rapidjson::Value &v);
	
	bool valid() const noexcept {return registered;}
	
	virtual bool isEncrypted() const noexcept =0;
	
#ifdef QT_DEBUG
	void printReqs() const;
#endif
	
public slots:
	
	/*!
	 * \brief Close this connection
	 * \param reason The reason for disconnection
	 * \param fromRemote Should be true if closing on behalf of the remote device
	 * 
	 * _Note:_ this will schedule the called object for deletion.
	 */
	void close(DisconnectReason reason = ConnectionTerminated, bool fromRemote = false) noexcept;
	
	/*!
	 * \brief Poll for operations that have timed out
	 * 
	 * Two types of timeouts are possible with DDX-RPC connections, both of which
	 * are checked by this function.
	 * 
	 * The first is individual request timeouts.
	 * These are mainly meant to protect against attacks in which a connecting client
	 * triggers repeated requests without responding to any, causing the #reqs member
	 * to grow indefinitely.  If a request does not receive a response within its
	 * requester-specified timeout time, an error will be sent to its handler and the
	 * corresponding RequestRef entry will be removed.
	 * 
	 * Registration timeouts will occur when a connection is not successfully
	 * registered within a user-set time interval.  Registration timeouts cannot
	 * be disabled and are meant to inhibit non-DDX connections from sitting
	 * unregistered forever.  Registration timeouts cause disconnection.
	 * 
	 * This function is called regularly by the DevMgr::timeoutPoller QTimer.
	 */
	void timeoutPoll() noexcept;
	
signals:
	
	void postToLogArea(const QString &msg) const;
	
	void deviceDisconnected(RemDev *dev, DisconnectReason reason, bool fromRemote) const;
	
	void readyForRegistration() const;
	
private slots:
	
	void init() noexcept;
	
protected:
	
	enum RegistrationState {
		UnregisteredState = 0x0,
		RegSentFlag = 0x1,
		RegAcceptedFlag = 0x2,
		RemoteRegAcceptedFlag = 0x4,
		RegisteredState = RegSentFlag|RegAcceptedFlag|RemoteRegAcceptedFlag
	};
	
	DevMgr *dm;  //!< Convenience pointer to DevMgr instance
	
	QByteArray cid;
	
	QByteArray remote_cid;
	
	QByteArray locale;
	
	qint64 connectTime;
	
	bool inbound;
	
	RegistrationState regState;
	
	volatile bool closed;
	
	/*!
	 * \brief Handle a single, complete incoming item
	 * \param data The raw data to use (will be deleted)
	 */
	void handleItem(char *data) noexcept;
	
	/*!
	 * \brief Mark the connection as ready
	 * 
	 * Must be called by subclasses as soon as the connection is ready for transmission
	 */
	void connectionReady() noexcept;
	
	/*!
	 * \brief Send a log line tagged with the cid
	 * \param msg The message
	 * \param isAlert Whether it is destined for the user
	 * 
	 * This is an overload which converts the QString to QByteArray.
	 */
	void log(const QString &msg, bool isAlert = false) const noexcept;
	
	virtual void sub_init() noexcept {}
	
	virtual void terminate(DisconnectReason reason, bool fromRemote) noexcept =0;
	
	/*!
	 * \brief Write a single RPC item
	 * \param buffer The data to be written (**must** be manually freed)
	 * 
	 * This function should quickly write to a buffer and then return.
	 * 
	 * _Warning:_ This function **must** be made thread-safe!
	 */
	virtual void writeItem(rapidjson::StringBuffer *buffer) noexcept =0;
	
	virtual const char *getType() const noexcept =0;
	
private:
	
	/*!
	 * \brief Maintains handling information about an outgoing RPC request
	 */
	struct RequestRef {
		RequestRef(QObject *handlerObj, const char *handlerFn, const char *method, qint64 timeout) {
			time = QDateTime::currentMSecsSinceEpoch();
			if (timeout) timeout_time = time + timeout;
			else timeout_time = 0;
			this->handlerObj = handlerObj;
			this->handlerFn = handlerFn;
			this->method = method;
		}
		RequestRef() {handlerObj = 0;}  // Mark invalid
		
		/*!
		 * \brief Determine request validity
		 * \param checkTime Current time or 0 to disable timeout check
		 * \return Whether the request is still valid
		 * 
		 * Requests can become invalid if their receiver is destroyed or they timeout.
		 */
		bool valid(qint64 checkTime) const {
			if (checkTime && timeout_time)
				if (timeout_time < checkTime) return false;
			return ! handlerObj.isNull();
		}
		// Note: No id is necessary because it is the key in RequestHash
		QPointer<QObject> handlerObj;
		const char *handlerFn;
		const char *method;
		qint64 time;
		qint64 timeout_time;
	};
	
	enum MainValType {
		ParamsT,
		ResultT,
		ErrorT
	};
	
	//! A hash for maintaing lists of outgoing requests
	typedef QHash<LocalId, RequestRef> RequestHash;
	
	//! Manages all open outgoing requests to direct responses appropriately
	RequestHash reqs;
	
	//! Locks the request hash and lastId variable
	mutable QMutex req_id_lock;
	
	int pollerRefCount;
	
	LocalId lastId;
	
	qint64 registrationTimeoutTime;
	
	bool registered;
	
	QByteArray *ref;
	
	/*!
	 * \brief Master constructor
	 * \param dm
	 * \param inbound
	 */
	RemDev(DevMgr *dm, QByteArray *ref);
	
	//! Start this device's threads
	void startThread();
	
	/*!
	 * \brief Stringify and deliver a complete JSON document
	 * \param doc Root document (will be **deleted)
	 */
	void sendDocument(rapidjson::Document *doc);
	
	/*!
	 * \brief TODO
	 * \param doc Root document (will be **deleted**)
	 */
	void handleRegistration(rapidjson::Document *doc);
	
	/*!
	 * \brief TODO
	 * \param doc Root document (will be **deleted**)
	 */
	void handleDisconnect(rapidjson::Document *doc);
	
	/*!
	 * \brief Build and deliver a simulated error to an outgoing request
	 * \param id The integer ID of the request
	 * \param req The RequestRef object corresponding to the request
	 * \param code The integer error code
	 * \param msg The error message
	 */
	void simulateError(int id, const RequestRef &req, int code, const QString &msg);
	
	/*!
	 * \brief Log an incoming or simulated error
	 * \param errorVal The error object to log (must be verified, see Response)
	 * \param method The method name (will be assumed to be a null error if 0)
	 */
	void logError(const rapidjson::Value *errorVal, const char *method = 0) const;
	
	/*!
	 * \brief Set up an existing RapidJSON document for delivery
	 * \param doc The document to prepare
	 * \param a Convenience reference to \a doc's allocator
	 */
	static inline void prepareDocument(rapidjson::Document *doc, rapidjson::MemoryPoolAllocator<> &a);
};

Q_DECLARE_METATYPE(RemDev::Response*)
Q_DECLARE_METATYPE(RemDev::Request*)

#endif // REMDEV_H
