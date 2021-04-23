#!/usr/bin/env python3

from json import dumps

__all__ = [ 'Response' ]

class Response(object):
    def __init__(self, *, status_code, msg=None, **kwargs):
        self._status = status_code
        self._msg = msg
        self._kwargs = kwargs
        self._json_response = None
        self._dict_response = None

    def generate_json(self):
        if self._json_response is None:
            self._json_response = dumps(self.generate_dict())
        return self._json_response

    def generate_dict(self):
        if self._dict_response is None:
            self._dict_response = { "status" : int(self._status), "msg" : self._msg }
            self._dict_response.update(self._kwargs)
        return self._dict_response

    @classmethod
    def generate_response(cls, *, status_code, msg=None, **kwargs):
        if msg is None:
            msg = status_code.fullname

        return cls(status_code=status_code, msg=msg, kwargs)        

class ErrorResponse(Response):
    def __init__(self, *, error_code, msg=None):
        super().__init__(status_code=error_code, msg=msg)
