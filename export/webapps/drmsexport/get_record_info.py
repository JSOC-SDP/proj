#!/usr/bin/env python3

import argparse
import copy
import json
import os
import sys

from drms_export import Connection, ExpServerBaseError, Error as ExportError, ErrorCode as ExportErrorCode, ErrorResponse, Response, get_arguments as ss_get_arguments, get_message, send_message
from drms_parameters import DRMSParams, DPMissingParameterError
from drms_utils import Arguments as Args, ArgumentsError as ArgsError, Choices, CmdlParser, Formatter as DrmsLogFormatter, ListAction, Log as DrmsLog, LogLevel as DrmsLogLevel, LogLevelAction as DrmsLogLevelAction, MakeObject, StatusCode as SC
from utils import extract_program_and_module_args, get_db_host

DEFAULT_LOG_FILE = 'gri_log.txt'

class StatusCode(SC):
    # info success status codes
    SUCCESS = (0, 'success')

class ErrorCode(ExportErrorCode):
    # info failure error codes
    FAILURE = (1, 'info failure')

    PARAMETERS = (101, 'failure locating DRMS parameters')
    ARGUMENTS = (102, 'bad arguments')
    LOGGING = (103, 'failure logging messages')
    EXPORT_ACTION = (104, 'failure calling export action')
    RESPONSE = (105, 'unable to generate valid response')
    EXPORT_SERVER = (106, 'export-server communication error')
    UNHANDLED_EXCEPTION = (107, 'unhandled exception')

class GriBaseError(ExportError):
    text_response = False

    def __init__(self, *, exc_info=None, error_message=None):
        if exc_info is not None:
            import traceback

            # for use with some exception handlers
            self.exc_info = exc_info
            e_type, e_obj, e_tb = exc_info
            file_info = traceback.extract_tb(e_tb)[0]
            file_name = file_info.filename if hasattr(file_info, 'filename') else ''
            line_number = str(file_info.lineno) if hasattr(file_info, 'lineno') else ''

            if error_message is None:
                error_message = f'{file_name}:{line_number}: {e_type.__name__}: {str(e_obj)}'
            else:
                error_message = f'{error_message} [ {file_name}:{line_number}: {e_type.__name__}: {str(e_obj)} ]'

        super().__init__(error_message=error_message)

    @property
    def response(self):
        if self.text_response:
            # need to generate a text response, not regular json; return a response that has a generate_text() method
            return GetRecordTableAction.generate_error_response(self)
        else:
            return super().response

class ParametersError(GriBaseError):
    _error_code = ErrorCode.PARAMETERS

class ArgumentsError(GriBaseError):
    _error_code = ErrorCode.ARGUMENTS

class LoggingError(GriBaseError):
    _error_code = ErrorCode.LOGGING

class ExportActionError(GriBaseError):
    _error_code = ErrorCode.EXPORT_ACTION

class ResponseError(GriBaseError):
    _error_code = ErrorCode(ErrorCode.RESPONSE)

class ExportServerError(GriBaseError):
    _error_code = ErrorCode.EXPORT_SERVER

class UnhandledExceptionError(GriBaseError):
    _error_code = ErrorCode.UNHANDLED_EXCEPTION

class TableFlagsAction(argparse.Action):
    def __call__(self, parser, namespace, value, option_string=None):
        # convert json arguments to dict
        table_flags_dict = self.json_to_dict(value)
        setattr(namespace, self.dest, table_flags_dict)

    @classmethod
    def json_to_dict(cls, json_text):
        return json.loads(json_text)

def name_to_ws_obj(name, drms_params):
    webserver_dict = {}
    webserver_dict['host'] = name
    if name is None or len(name) == 0 or name.strip().lower() == 'none':
        # assume public
        webserver_dict['public'] = True
    else:
        webserver_dict['public'] = True if name.lower() != drms_params.get_required('WEB_DOMAIN_PRIVATE') else False

    return MakeObject(name='webserver', data=webserver_dict)()

def webserver_action_constructor(self, drms_params):
    self._drms_params = drms_params

def webserver_action(self, parser, namespace, value, option_string=None):
    webserver_obj = name_to_ws_obj(value, self._drms_params)
    setattr(namespace, self.dest, webserver_obj)

def create_webserver_action(drms_params):
    cls = type('WebserverAction', (argparse.Action,),
    {
        '_drms_params' : drms_params,
        '__call__' : webserver_action,
    })

    return cls

class Arguments(Args):
    _arguments = None

    @classmethod
    def get_arguments(cls, *, is_program, program_name=None, program_args=None, module_args=None, drms_params, refresh=True):
        if cls._arguments is None or refresh:
            try:
                private_db_host = drms_params.get_required('SERVER')
                db_port = int(drms_params.get_required('DRMSPGPORT'))
                db_name = drms_params.get_required('DBNAME')
                db_user = drms_params.get_required('WEB_DBUSER')
            except DPMissingParameterError as exc:
                raise ParametersError(exc_info=sys.exc_info(), error_message=str(exc))

            if is_program:
                try:
                    log_file = os.path.join(drms_params.get_required('EXPORT_LOG_DIR'), DEFAULT_LOG_FILE)
                except DPMissingParameterError as exc:
                    raise ParametersError(exc_info=sys.exc_info(), error_message=str(exc))

                args = None

                if program_args is not None and len(program_args) > 0:
                    args = program_args

                parser_args = { 'usage' : '%(prog)s specification=<DRMS record-set specification> dbhost=<db host> [ -k/--keywords=<keywords> ] [ -K/--links=<links> ] [ -l/--log-file=<log file path> [ -L/--logging-level=<critical/error/warning/info/debug> ] [ -N/--dbname=<db name> ] [ -P/--dbport=<db port> ] [ -s/--segments=<segments> ] [ -U/--dbuser=<db user>] [ -w/--webserver=<host> ] ' }
                if program_name is not None and len(program_name) > 0:
                    parser_args['prog'] = program_name

                parser = CmdlParser(**parser_args)

                # required
                parser.add_argument('specification', help='the DRMS record-set specification identifying the records for which information is to be obtained', metavar='<DRMS record-set specification>', dest='specification', required=True)
                parser.add_argument('dbhost', help='the machine hosting the database that contains export requests from this site', metavar='<db host>', dest='db_host', required=True)

                # optional
                parser.add_argument('-k', '--keywords', help='list of keywords for which information is returned', action=ListAction, dest='keywords', default=None)
                parser.add_argument('-K', '--links', help='list of links for which information is returned', action=ListAction, dest='links', default=None)
                parser.add_argument('-l', '--log-file', help='the path to the log file', metavar='<log file>', dest='log_file', default=log_file)
                parser.add_argument('-L', '--logging-level', help='the amount of logging to perform; in order of increasing verbosity: critical, error, warning, info, debug', metavar='<logging level>', dest='logging_level', action=DrmsLogLevelAction, default=DrmsLogLevel.ERROR)
                parser.add_argument('-N', '--dbname', help='the name of the database that contains export requests', metavar='<db name>', dest='db_name', default=db_name)
                parser.add_argument('-n', '--number-records', help='the maximum number of records for which information is returned', metavar='<maximum number of records>', dest='number_records', type=int, default=None)
                parser.add_argument('-P', '--dbport', help='the port on the host machine that is accepting connections for the database', metavar='<db host port>', dest='db_port', type=int, default=db_port)
                parser.add_argument('-s', '--segments', help='list of segments for which information is returned', action=ListAction, dest='segments', default=None)
                parser.add_argument('-t', '--table-flags', help='json string containing boolean flags used to filter in table columns', action=TableFlagsAction, dest='table_flags', default=None) # converted to dict
                parser.add_argument('-U', '--dbuser', help='the name of the database user account', metavar='<db user>', dest='db_user', default=db_user)
                parser.add_argument('-w', '--webserver', help='the webserver invoking this script', metavar='<webserver>', action=create_webserver_action(drms_params), dest='webserver', default=name_to_ws_obj(None, drms_params))

                arguments = Arguments(parser=parser, args=args)
            else:
                # `program_args` has all `arguments` values, in final form; validate them
                def extract_module_args(*, specification, table_flags=None, db_host, keywords=None, links=None, db_name=db_name, number_records=None, db_port=db_port, segments=None, db_user=db_user, webserver=None, log=None):
                    arguments = {}

                    arguments['specification'] = specification
                    arguments['db_host'] = db_host
                    arguments['keywords'] = keywords # list
                    arguments['links'] = links # list
                    arguments['db_name'] = db_name
                    arguments['number_records'] = number_records
                    arguments['db_port'] = db_port
                    arguments['segments'] = segments # list
                    arguments['table_flags'] = table_flags # dict, if set
                    arguments['db_user'] = db_user
                    arguments['webserver'] = name_to_ws_obj(webserver, drms_params) # sets webserver.public = True if webserver is None

                    GetRecordInfoAction.set_log(log)

                    return arguments

                # dict
                module_args_dict = extract_module_args(**module_args)
                arguments = Arguments(parser=None, args=module_args_dict)

            # if the caller has specified a public webserver, make sure that the db specified is not the private one
            if arguments.webserver.public and arguments.db_host == private_db_host:
                raise ArgumentsError(error_message=f'cannot specify private db server to handle public webserver requests')

            arguments.private_db_host = private_db_host

            cls._arguments = arguments

        return cls._arguments

def get_request_status(response_dict):
    error_code = None
    status_code = None

    try:
        error_code = ErrorCode(int(response_dict['status']))
    except KeyError:
        pass

    if error_code is None:
        try:
            status_code = StatusCode(int(response_dict['status']))
        except KeyError:
            raise ExportServerError(exc_info=sys.exc_info(), error_message=f'unexpected show_series status returned {str(response_dict["status"])}')

    return (error_code, status_code)

def get_request_status_code(response_dict):
    # socket server returns status 0 or 1
    error_code, status_code = get_request_status(response_dict)
    code = error_code if error_code is not None else status_code

    return code

def get_response(client_response_dict, requesting_table, log):
    log.write_debug([ f'[ get_response ]' ])

    response_dict = copy.deepcopy(client_response_dict)
    response_dict['status_code'] = get_request_status_code(response_dict)

    if requesting_table:
        # special response that has generate_text() method (which returns a table of text)
        response = GetRecordTableAction.generate_response(**response_dict)
    else:
        response = Response.generate_response(**response_dict)

    return response

def send_request(request, connection, log):
    json_message = json.dumps(request)
    send_message(connection, json_message)
    message = get_message(connection)

    return message

def perform_action(*, action_obj, is_program, program_name=None, **kwargs):
    # catch all expections so we can always generate a response
    response = None
    log = None

    try:
        # removes each options with a value of None
        program_args, module_args = extract_program_and_module_args(is_program=is_program, **kwargs)

        try:
            drms_params = DRMSParams()

            if drms_params is None:
                raise ParametersError(error_message='unable to locate DRMS parameters package')

            arguments = Arguments.get_arguments(is_program=is_program, program_name=program_name, program_args=program_args, module_args=module_args, drms_params=drms_params)
        except ArgsError as exc:
            raise ArgumentsError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
        except Exception as exc:
            raise ArgumentsError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')

        if is_program:
            try:
                formatter = DrmsLogFormatter('%(asctime)s - %(levelname)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
                log = DrmsLog(arguments.log_file, arguments.logging_level, formatter)
                GetRecordInfoAction._log = log
            except Exception as exc:
                raise LoggingError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')

            requesting_table = arguments.table_flags is not None
        else:
            log = action_obj.log
            requesting_table = isinstance(action_obj, GetRecordTableAction)

        if is_program:
            log.write_debug([ f'[ perform_action ] program invocation' ])
        else:
            log.write_debug([ f'[ perform_action ] module invocation' ])

        log.write_debug([ f'[ perform_action ] action arguments: {str(arguments)}' ])

        if not GetRecordInfoAction.is_valid_specification(arguments.specification, arguments.db_host, arguments.webserver.host, log):
            raise ArgumentsError(error_message=f'invalid record-set specification')

        parsed_specification = GetRecordInfoAction.get_parsed_specification(arguments.specification, arguments.db_host, arguments.webserver.host)

        if not parsed_specification.attributes.hasfilts and arguments.number_records is None:
            raise ArgumentsError(error_message=f'must specify either a record-set filter, or a maximum number of records')

        series = []
        for subset in parsed_specification.attributes.subsets:
            series.append(subset.seriesname)

        resolved_db_host = get_db_host(webserver=arguments.webserver, series=series, private_db_host=arguments.private_db_host, db_host=arguments.db_host, db_port=arguments.db_port, db_name=arguments.db_name, db_user=arguments.db_user, exc=ExportActionError, log=log)

        if resolved_db_host is None:
            raise ArgumentsError(error_message=f'unable to determine db host suitable for series {", ".join(series)}')

        # make request to socket server
        try:
            # use socket server to call drms_parserecset
            nested_arguments = ss_get_arguments(is_program=False, module_args={})

            with Connection(server=nested_arguments.server, listen_port=nested_arguments.listen_port, timeout=nested_arguments.message_timeout, log=log) as connection:
                if requesting_table:
                    # `table_flags` is a dict
                    message = { 'request_type' : 'record_info_table', 'specification' : arguments.specification, 'keywords' : arguments.keywords, 'segments' : arguments.segments, 'links' : arguments.links, 'number_records' : arguments.number_records, 'table_flags' : arguments.table_flags, 'db_host' : resolved_db_host }
                else:
                    message = { 'request_type' : 'record_info', 'specification' : arguments.specification, 'keywords' : arguments.keywords, 'segments' : arguments.segments, 'links' : arguments.links, 'number_records' : arguments.number_records, 'db_host' : resolved_db_host }

                response = send_request(message, connection, log)

                # message is raw JSON from jsoc_info
                record_set_info_dict = json.loads(response)

                if record_set_info_dict.get('export_server_status') == 'export_server_error':
                    raise ExportServerError(error_message=f'{record_set_info_dict["error_message"]}')

                message = { 'request_type' : 'quit' }
                send_request(message, connection, log)

            response = get_response(record_set_info_dict, requesting_table, log)
        except ExpServerBaseError as exc:
            raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
        except Exception as exc:
            raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
    except GriBaseError as exc:
        response = exc.response
        error_message = exc.message

        if log:
            log.write_error([ error_message ])
        elif is_program:
            print(error_message)
    except Exception as exc:
        response = UnhandledExceptionError(exc_info=sys.exc_info(), error_message=f'{str(exc)}').response
        error_message = str(exc)

        if log:
            log.write_error([ error_message ])
        elif is_program:
            print(error_message)

    if log is not None:
        log.write_info([ f'[ perform_action ] request complete; status {response.attributes.drms_export_status_description}' ])

    return response

# for use in export web app
from action import Action
from parse_specification import ParseSpecificationAction
class GetRecordInfoAction(Action):
    actions = [ 'get_record_set_info' ]

    _log = None

    def __init__(self, *, method, specification, db_host, keywords=None, links=None, log=None, db_name=None, number_records=None, db_port=None, segments=None, db_user=None, webserver=None):
        self._method = getattr(self, method)
        self._specification = specification
        self._db_host = db_host # the host `webserver` uses (private webserver uses private db host)
        self._options = {}
        self._options['keywords'] = keywords # list
        self._options['links'] = links # list
        self._options['log'] = log
        self._options['db_name'] = db_name
        self._options['number_records'] = number_records # int
        self._options['db_port'] = db_port # int
        self._options['segments'] = segments # list
        self._options['db_user'] = db_user
        self._options['webserver'] = webserver # host name - gets converted to object in `get_arguments()`

    def get_record_set_info(self):
        response = perform_action(action_obj=self, is_program=False, specification=self._specification, table_flags=None, db_host=self._db_host, options=self._options)
        return response

    @property
    def log(self):
        return self.__class__._log

    @log.setter
    def log(self, log):
        self.__class__._log = log

    @classmethod
    def set_log(cls, log=None):
        if cls._log is None:
            cls._log = DrmsLog(None, None, None) if log is None else log

    @classmethod
    def get_log(cls):
        return cls._log

    @classmethod
    def get_parsed_specification(cls, specification, db_host, webserver):
        action = Action.action(action_type='parse_specification', args={ 'log' : cls._log, 'specification' : specification, 'db_host' : db_host, 'webserver' : webserver })
        parsed_specification = action()

        if isinstance(parsed_specification, ErrorResponse) or parsed_specification is None:
            cls._log.write_error([ f'[ get_parsed_specification ] {parsed_specification.attributes.error_message}'])
            raise ArgumentsError(error_message=f'unable to parse specification `{specification}`')

        return parsed_specification

    @classmethod
    def is_valid_specification(cls, specification, db_host, webserver, log):
        cls.set_log(log)
        is_valid = None

        try:
            if db_host is None:
                # if this method is called before URL arguments are parsed, then `db_host` is not known;
                # use default (public) export DB (specification parser does not use DB so it does not matter)
                try:
                    drms_params = DRMSParams()

                    if drms_params is None:
                        raise ParametersError(error_message='unable to locate DRMS parameters package')

                    db_host_resolved = drms_params.get_required('EXPORT_DB_HOST_DEFAULT')
                except DPMissingParameterError as exc:
                    raise ParametersError(exc_info=sys.exc_info(), error_message=str(exc))
            else:
                db_host_resolved = db_host

            # parse specification
            response = cls.get_parsed_specification(specification, db_host_resolved, webserver)

            is_valid = False if isinstance(response, ErrorResponse) else True
        except:
            is_valid = False

        return is_valid

class GetRecordTableActionResponse(Response):
    def __init__(self, *, status_code, **kwargs):
        super().__init__(status_code=status_code, **kwargs)
        self._text_response = None

    def generate_text(self):
        if self._text_response is None:
            # the `table` attribute has the table text; the `attributes` property will make all kwargs available as properties
            self._text_response = self.attributes.table

        return self._text_response

class GetRecordTableActionErrorResponse(GetRecordTableActionResponse):
    def __init__(self, *, error_code, error_message=None, **kwargs):
        super().__init__(status_code=error_code, error_message=error_message, **kwargs)

    def generate_text(self):
        if self._text_response is None:
            # the `attributes` property will make all kwargs available as properties; use the `error_message` property
            self._text_response = self.attributes.error_message

        return self._text_response

class GetRecordTableAction(GetRecordInfoAction):
    actions = [ 'get_record_set_table' ]

    def __init__(self, *, method, specification, table_flags, db_host, keywords=None, links=None, log=None, db_name=None, number_records=None, db_port=None, segments=None, db_user=None, webserver=None):
        super().__init__(method=method, specification=specification, db_host=db_host, keywords=keywords, links=links, log=log, db_name=db_name, number_records=number_records, db_port=db_port, segments=segments, db_user=db_user, webserver=webserver)

        self._table_flags = table_flags # dict

    def get_record_set_table(self):
        response = perform_action(action_obj=self, is_program=False, specification=self._specification, table_flags=self._table_flags, db_host=self._db_host, options=self._options)
        return response

    @classmethod
    def generate_response(cls, *, status_code=None, **kwargs):
        return GetRecordTableActionResponse.generate_response(status_code=status_code, **kwargs)

    @classmethod
    def generate_error_response(cls, *, error_code=None, gri_error, **kwargs):
        # gri_error._error_message has appropriate error message
        return GetRecordTableActionErrorResponse.generate_response(status_code=error_code, error_message=gri_error._error_message, **kwargs)

class GetRecordInfoLegacyResponse(Response):
    def remove_extraneous(self, response_dict, keys):
        for key in keys:
            if key in response_dict:
                del response_dict[key]

        return response_dict

    # override parent to remove attributes not present in legacy API
    def generate_serializable_dict(self):
        serializable_dict = copy.deepcopy(super().generate_serializable_dict())
        sanitized_serializable_dict = self.remove_extraneous(serializable_dict, [ 'drms_export_status', 'drms_export_status_code', 'drms_export_status_description' ] )

        return sanitized_serializable_dict

class GetRecordInfoLegacyAction(Action):
    actions = [] # abstract

    def convert_data_types(self, args_dict):
        converted = {}
        for key, val in args_dict.items():
            if type(val) == bool:
                converted[key] = int(val)
            elif type(val) == list:
                converted[key] = ','.join(val)
            else:
                converted[key] = val

        return converted

    def perform_action(self):
        try:
            log = self._options['log']
            nested_arguments = self.get_nested_arguments()

            try:
                response = self.send_request(nested_arguments=nested_arguments)
            except ExpServerBaseError as exc:
                raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
            except Exception as exc:
                raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
        except GriBaseError as exc:
            response = exc.response
            error_message = exc.message

            if log:
                log.write_error([ error_message ])
        except Exception as exc:
            response = UnhandledExceptionError(exc_info=sys.exc_info(), error_message=f'{str(exc)}').response
            error_message = str(exc)

            if log:
                log.write_error([ error_message ])

        return response

    def get_nested_arguments(self):
        log = self._options['log']

        if self._options['db_host'] is None:
            drms_params = DRMSParams()

            if drms_params is None:
                raise ParametersError(error_message='unable to locate DRMS parameters package')

            self._options['db_host'] = drms_params.get_required('SERVER')

        # use socket server to call jsoc_fetch
        return ss_get_arguments(is_program=False, module_args={})

    def get_request_status_code(self, response_dict):
        error_code, status_code = self.get_request_status(response_dict)
        code = error_code if error_code is not None else status_code

        return code

    def get_response(self, client_response_dict, log):
        log.write_debug([ f'[ {self.__class__.__name__}.get_response ]' ])

        response_dict = copy.deepcopy(client_response_dict)
        response_dict['status_code'] = self.get_request_status_code(response_dict)
        response = GetRecordInfoLegacyResponse.generate_response(**response_dict)

        return response

    def get_request_status(self, response_dict):
        error_code = None
        status_code = None

        try:
            error_code = ErrorCode(int(response_dict['status']))
        except KeyError:
            pass

        if error_code is None:
            try:
                status_code = StatusCode(int(response_dict['status']))
            except KeyError:
                raise ExportServerError(exc_info=sys.exc_info(), error_message=f'unexpected {str(self)} status returned {str(response_dict["status"])}')

        return (error_code, status_code)

    def __str__(self):
        return self.__class__.__name__

class JsocInfoLegacyAction(GetRecordInfoLegacyAction):
    actions = [ 'legacy_get_record_info_json' ]

    _log = None

    def __init__(self, *, method, operation, specification, db_host=None, log=None, **kwargs):
        self._method = getattr(self, method)
        self._operation = operation
        self._specification = specification
        self._options = {}
        self._options['db_host'] = db_host
        self._options['log'] = log if log is not None else DrmsLog(None, None, None)
        self._legacy_arguments = self.convert_data_types(kwargs)

    def legacy_get_record_info_json(self):
        return self.perform_action()

    def send_request(self, *, nested_arguments):
        log = self._options['log']

        with Connection(server=nested_arguments.server, listen_port=nested_arguments.listen_port, timeout=nested_arguments.message_timeout, log=log) as connection:
            message = { 'request_type' : 'legacy_jsoc_info', 'operation' : self._operation, 'specification' : self._specification, 'db_host' : self._options['db_host'] }
            message.update(self._legacy_arguments) # error if the user has provided non-expected arguments
            response = send_request(message, connection, log)

            # message is raw JSON from checkAddress.py
            legacy_jsoc_info_dict = json.loads(response)

            if legacy_jsoc_info_dict.get('export_server_status') == 'export_server_error':
                raise ExportServerError(error_message=f'{legacy_jsoc_info_dict["error_message"]}')

            message = { 'request_type' : 'quit' }
            send_request(message, connection, log)

        response = self.get_response(legacy_jsoc_info_dict, log)
        return response

    @classmethod
    def is_valid_processing(cls, arguments_json, log):
        cls.set_log(log)
        is_valid = None
        try:
            json.loads(arguments_json)
            is_valid = True
        except:
            is_valid = False

        return is_valid

class ShowInfoLegacyAction(GetRecordInfoLegacyAction):
    actions = [ 'legacy_get_record_info_text' ]

    _log = None

    def __init__(self, *, method, specification, db_host=None, log=None, **kwargs):
        self._method = getattr(self, method)
        self._specification = specification
        self._options = {}
        self._options['db_host'] = db_host
        self._options['log'] = log if log is not None else DrmsLog(None, None, None)
        self._legacy_arguments = self.convert_data_types(kwargs)

    def legacy_get_record_info_text(self):
        return self.perform_action()

    def send_request(self, *, nested_arguments):
        log = self._options['log']

        with Connection(server=nested_arguments.server, listen_port=nested_arguments.listen_port, timeout=nested_arguments.message_timeout, log=log) as connection:
            message = { 'request_type' : 'legacy_show_info', 'specification' : self._specification, 'db_host' : self._options['db_host'] }
            message.update(self._legacy_arguments) # error if the user has provided non-expected arguments
            response = send_request(message, connection, log)

            # message is raw JSON from checkAddress.py
            legacy_show_info_dict = json.loads(response)

            if legacy_show_info_dict.get('export_server_status') == 'export_server_error':
                raise ExportServerError(error_message=f'{legacy_show_info_dict["error_message"]}')

            message = { 'request_type' : 'quit' }
            send_request(message, connection, log)

        response = self.get_response(legacy_show_info_dict, log)
        return response

    def get_request_status(self, response_dict):
        # will be either 0 or 1 (socket server calls show_info, which prints text and sets exit code, but does not
        # print a status code); socket server returns text in JSON obj with a status of 0 or 1)
        return super().get_request_status(response_dict)

def run_tests():
    formatter = DrmsLogFormatter('%(asctime)s - %(levelname)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    log = DrmsLog(sys.stdout, DrmsLogLevelAction.string_to_level('debug'), formatter)

    log.write_debug([ 'testing `legacy_get_record_info_json`'])
    jila = JsocInfoLegacyAction(method='legacy_get_record_info_json', operation='series_struct', specification='hmi.v_720s', db_host='hmidb2', log=log)
    response = jila()
    response_dict = response.generate_serializable_dict()
    log.write_debug([ str(response_dict) ])

    jila = JsocInfoLegacyAction(method='legacy_get_record_info_json', operation='rs_summary', specification='hmi.m_720s[2018.8.15/144m]', db_host='hmidb2', log=log)
    response = jila()
    response_dict = response.generate_serializable_dict()
    log.write_debug([ str(response_dict) ])

    jila = JsocInfoLegacyAction(method='legacy_get_record_info_json', operation='rs_list', key='t_rec,obs_vr', specification='hmi.m_720s[2018.8.15/144m]', db_host='hmidb2', log=log)
    response = jila()
    response_dict = response.generate_serializable_dict()
    log.write_debug([ str(response_dict), '' ])

    log.write_debug([ 'testing `legacy_get_record_info_text`'])
    sila = ShowInfoLegacyAction(method='legacy_get_record_info_text', specification='hmi.m_720s[2018.8.8]', key='t_rec,obs_vr', db_host='hmidb2', log=log)
    response = sila()
    response_dict = response.generate_serializable_dict()
    log.write_debug([ str(response_dict) ])

if __name__ == "__main__":
    response = perform_action(action_obj=None, is_program=True)
    print(response.generate_json())

    # Always return 0. If there was an error, an error code (the 'status' property) and message (the 'statusMsg' property) goes in the returned HTML.
    sys.exit(0)
else:
    pass
