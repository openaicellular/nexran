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

Finally, you can use the [Swagger Editor](https://editor.swagger.io/?url=http://powderrenewpublic.pages.flux.utah.edu/nexran/etc/northbound-openapi.json)
to generate a client in a language of your choice.  Of course, there are
many other tools to accomplish this.
