# telling
#### C++ microservice patterns for real-time applications.

(This library is a work in progress and the API is not stable.)

Telling builds upon NNG and the Scalability Protocols to provide a client-service system for building applications.  The system uses HTTP-like messages to implement Request-Reply, Publish-Subscribe and Pipeline patterns.

The library is designed to accommodate single programs that contain many small clients and services, but offers equivalent support for networking and inter-process communication.

## Clients & Services

Telling's architecture is designed around **named services** and **anonymous clients**.  Services register themselves with a process-wide **Server** under unique URIs.  **Requests** sent by clients are routed to the service with the **longest matching URI**.

Because clients are anonymous, services can only communicate with them by publishing **Reports** to topics the client has subscribed to, or issuing **Replies** to individual Requests.

It is possible, but rarely useful to have multiple Servers within a single process.  In the future, it may be interesting to enable Servers to act as gateways to other Servers across a network.

## Networking Patterns

| Pattern              | Client Sends... | Service Sends... |
| -------------------- | --------------- | ---------------- |
| Request-Reply        | Request         | Reply            |
| Pipeline (Push-Pull) | Request         | —                |
| Publish-Subscribe    | —               | Report           |

#### Request-Reply

This pattern is the most powerful and useful in Telling, but also the most resource-intensive.

Clients send a Request message to a server, which uses a message format identical to an HTTP request.  The Server routes the request to the Service with the longest matching URI prefix, which may then issue a Reply to the original Client.  Replies use a message format identical to HTTP replies.

Lost Requests will always result in a detectable error event, such as a timeout, at the client.  Lost replies may or may not be detectable at the Service but will result in a timeout on the Client side.

#### Push-Pull

Essentially, a Request with no Reply — this is more efficient than Request-Reply but otherwise uses the same message format and routing rules.

Lost Requests will always result in a detectable error event at the client and the service.

####Publish-Subscribe

Services may distribute Reports, messages which begin with a URI called the "topic".  Clients may subscribe to topics as prefixes — receiving any message whose URI begins with the subscribed text.

Delivery of reports is on a best-effort basis; in some cases, clients may not receive a published message and neither the service nor the client will be able to detect this without application-specific measures.

## Message Formats

Telling uses an optional message format based on the structure of HTTP requests and replies.  Each message begins with a start-line, followed by a series of headers (key-value fields) and a blank line.  Following the blank line is the body of the message.  This format is compatible with HTTP/1.1.

The start-line varies based on the type of message, and consists of the checked fields, in order and separated by spaces:

| Format       | Method | URI/Topic | Protocol | Status | Reason Phrase |
| ------------ | ------ | --------- | -------- | ------ | ------------- |
| HTTP Request | ✔      | ✔         | ✔        |        |               |
| Request      | ✔      | ✔         | ✔        |        |               |
| Report       |        | ✔         | ✔†       | ✔†     | ✔†            |
| Reply        |        |           | ✔        | ✔      | ✔†            |
| HTTP Reply   |        |           | ✔        | ✔      | ✔             |

† — optional element.  Note that the Reason Phrase may contain spaces.)

Telling's Request and Reply messages correspond to their HTTP equivalents, with some additional flexibility to save computational resources:

* The Reason-Phrase is optional.
* Headers usually omit carriage return (`\r`) characters.

Reports have no equivalent in HTTP.  They are used for publish-subscribe communications in Telling, combining elements of Request and Reply.
