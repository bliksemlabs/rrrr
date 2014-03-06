import uwsgi
import zmq
import struct

COMMON_HEADERS = [('Content-Type', 'application/json'), ('Access-Control-Allow-Origin', '*'), ('Access-Control-Allow-Headers', 'Requested-With,Content-Type')]

context = zmq.Context()

def light(environ, start_response):
    if environ['PATH_INFO'] in ['/favicon.ico']:
        start_response('404 NOK', COMMON_HEADERS)
        return ''

    qstring = environ['QUERY_STRING']
    if qstring == '':
        start_response('406 NOK', COMMON_HEADERS)
        return ''

    request_bliksem = context.socket(zmq.REQ)
    request_bliksem.connect("tcp://127.0.0.1:9292")
    poller = zmq.Poller()
    poller.register(request_bliksem, zmq.POLLIN)

    request_bliksem.send(qstring)

    socks = dict(poller.poll(1000))
    if socks.get(request_bliksem) == zmq.POLLIN:
        reply = request_bliksem.recv()
        start_response('200 OK', COMMON_HEADERS + [('Content-length', str(len(reply)))])
        return reply
    else:
        start_response('500 NOK', COMMON_HEADERS)
        return ''

uwsgi.applications = {'': light}
