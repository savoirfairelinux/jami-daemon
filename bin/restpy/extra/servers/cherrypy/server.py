import cherrypy

from ring_api.server.api import root, ring, user

class CherryServer:
    """ In construction
        @TODO: Use a MethodDispatcher to make it aware of
            the HTTP methods (POST, etc.)
    """
    config = {
        '/': {
            'request.dispatch': cherrypy.dispatch.MethodDispatcher(),
            'tools.sessions.on': True,
            'tools.response_headers.on': True,
            'tools.response_headers.headers': [('Content-Type', 'text/plain')]
        }
    }

    def __init__(self, host, port, dring, thread_pool=10):
        self.dring = dring

        cherrypy.config.update(self.config)
        cherrypy.tree.mount(root.Root(dring))
        cherrypy.server.unsubscribe()

        self.server = cherrypy._cpserver.Server()
        self.server._socket_host = host
        self.server.socket_port = port
        self.server.thread_pool = thread_pool
        self.server.subscribe()

    def start(self):
        cherrypy.engine.start()
        cherrypy.engine.block()

    def stop(self):
        cherrypy.engine.stop()
