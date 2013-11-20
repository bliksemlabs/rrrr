from bottle 	import run, route, request, response
from router_api import find_connection


@route('/find')
def search_request():
    response.content_type = 'application/json'

    if 'conn_n' in request.query:
        return find_connection(request.query_string, int(request.query['conn_n']))
    else:
        return find_connection(request.query_string, 5) # return default number of connections - 5


run(host='localhost', port=8080, debug=True)
