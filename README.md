# telling
#### C++ microservice patterns for real-time applications.

(This library is a work in progress and the API is not stable.)

Telling builds upon NNG and the Scalability Protocols to provide a client-service system for building applications based on HTTP-like messages and the following patterns:

* Request-Reply (clients make requests which services fulfill)
* Pipeline (clients send one-way messages to services)
* Pub-Sub (services publish messages on topics to which clients subscribe)

The library is designed to facilitate numerous small clients and services within a single process, but also works with interprocess and internet connections.