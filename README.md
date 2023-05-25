NexRAN
======

NexRAN is an [O-RAN](https://o-ran.org/) [RIC](https://wiki.o-ran-sc.org/pages/viewpage.action?pageId=1179659) [xApp](https://wiki.o-ran-sc.org/pages/viewpage.action?pageId=1179662) that provides low-level RAN control.

Getting Started
---------------

To get started with NexRAN, you can build a simple Docker image:

    $ cd nexran
    $ docker build -f Dockerfile -t nexran:latest .

Next, you can run the image in a container and send commands to its
northbound RESTful management interface:

    $ docker run --rm -it -p 8000:8000 nexran:latest

    $ curl -i -X GET http://localhost:8000/v1/slices ; echo
    HTTP/1.1 200 OK
    Connection: Close
    Content-Length: 97

    {"slices":[{"name":"default","allocation_policy":{"type":"proportional","share":1024},"ues":[]}]}

The complete northbound API can be found in the source tree in
`etc/northbound-openapi.json`, or online at http://powderrenewpublic.pages.flux.utah.edu/nexran/etc/northbound-openapi.json .

It's probably easiest to consume the API in [rendered form](https://petstore.swagger.io/?url=http://powderrenewpublic.pages.flux.utah.edu/nexran/etc/northbound-openapi.json)
via Swagger's renderer, or with another tool.

You can use the Swagger Inspector to poke at your running container from
within your browser if you prefer, if you are willing to install a
browser extension: https://inspector.swagger.io/builder/?url=http://powderrenewpublic.pages.flux.utah.edu/nexran/etc/northbound-openapi.json

Finally, you can use the [Swagger Editor](http://editor.swagger.io/?url=http://powderrenewpublic.pages.flux.utah.edu/nexran/etc/northbound-openapi.json)
to generate a client in a language of your choice.  Of course, there are
many other tools to accomplish this.

Modifying the Source Code
-------------------------

The NexRAN application logic is contained in [src](src), which includes
headers from [include](include).  It relies on a callback-style E2AP library
([lib/e2ap](lib/e2ap)) and an E2SM library ([lib/e2sm](lib/e2sm)) to handle
the details of E2 messaging.  The E2AP library matches sent messages to
replies, and invokes callbacks into the proper E2SM subclass so that
service-model-specific handlers can process replies according to their own
logic.  Parts of this callback interface are still under development, but
the intent is to push state tracking into the libraries.  New service models
should subclass the `e2sm::Model` class in [lib/e2sm/include/e2sm.h](lib/e2sm/include/e2sm.h),
like the `e2sm::kpm::KpmModel` class does [lib/e2sm/include/e2sm_kpm.h](lib/e2sm/include/e2sm_kpm.h).

The `main` entrypoint is located in [src/main.cc](src/main.cc).  It loads
configuration (from file, argv, and env, in that order -- see
[src/config.cc](src/config.cc), passes that to an instance of `nexran::App`,
and invokes the `start()` method on the App.

The implementation of `nexran::App` is primarily in
[src/nexran.cc](src/nexran.cc).  It contains the necessary handlers to
convert from an RMR message to an E2 message; those handlers pass to the
`e2ap` library for further processing to other callbacks.
`App::send_message` passes an encoded E2AP message to the named RMR
endpoint.  On the receive path, the e2ap handlers are responsible to decode
the message and pass to the relevant service model instance, if relevant.
For instance, the `nexran::App::handle(e2sm::kpm::KpmIndication *kind)`
handler processes KPM indications and runs closed-loop controls to
automatically adjust slice share proportions.

`nexran::App`'s handler callbacks operate over the NodeB, Slice, and Ue
objects created by the northbound interface
([src/restserver.cc](src/restserver.cc)).  The northbound interface is
mostly boilerplate and calls back into the app to handle operation
semantics.  However, it relies on RESTful "object" definitions (subclasses
of the `AbstractResource` class in [include/nexran.h](include/nexran.h); the
implementations of those objects are in [src/nodeb.cc](src/nodeb.cc),
[src/slice.cc](src/slice.cc), [src/ue.cc](src/ue.cc), and [src/policy.cc](src/policy.cc).
These `AbstractResource` subclasses define their JSON schema definition in the code,
and the generic parse logic converts incoming JSON requests to object instances using
them.
