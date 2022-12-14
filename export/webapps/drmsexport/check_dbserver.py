#!/usr/bin/env python3

# This is a version of checkExpDbServer.py modified to run inside a Flask context.

# check_dbserver.py determines if series are accessible publicly;

# returns a JSON object where the `server` attribute identifies the DB host that can serve data-series information for all
# series provided in the `series` program argument - if the server identified by the `public_db_host` program argument
# can serve all series, then the value of `server` is the value of `public_db_host`; otherwise the value of `server` is
# the internal DB host; in the returned JSON object, the `series` attribute contains a list of objects, one for each series
# provided in the `series` program argument; each object contains a single attribute - the name of the series; the value of
# this attribute is another object which contains a single attribute, `server`, whose value is the value of the
# `public_db_host` program argument if the series is DIRECTLY accessible from the public server, or whose value is the
# private server if the series is accessible from the private server only; `server` is None if the series does
# not exist, or if the series is accessible from the private server only BUT not on the whitelist; return examples:
#   { "server" : "hmidb", "series" : [{ "hmi.M_45s" : { "server" : "hmidb2" } }, { "hmi.on_white_list" : { "server" : "hmidb" }}], "status" : 0 }
#   { "server" : "hmidb2", "series" : [{ "hmi.M_45s" : { "server" : "hmidb2" } }, { "hmi.not_on_white_list" : { "server" : None }}], "status" : 0 }
#   { "server" : "hmidb2", "series" : [{ "hmi.M_45s" : { "server" : "hmidb2" } }, { "hmi.does_not_exist" : { "server" : None }}], "status" : 0 }

import argparse
import functools
import json
import os
import sys

from drms_parameters import DRMSParams, DPMissingParameterError
from drms_utils import Arguments as Args, ArgumentsError as ArgsError, CmdlParser, Formatter as DrmsLogFormatter, ListAction, Log as DrmsLog, LogLevel as DrmsLogLevel, LogLevelAction as DrmsLogLevelAction, MakeObject, StatusCode as ExportStatusCode
from drms_export import Connection, ExpServerBaseError, Error as ExportError, ErrorCode as ExportErrorCode, Response, get_arguments as ss_get_arguments, get_message, send_message
from utils import extract_program_and_module_args

DEFAULT_LOG_FILE = 'cd_log.txt'

class StatusCode(ExportStatusCode):
    SUCCESS = (0, 'success')

class ErrorCode(ExportErrorCode):
    PARAMETERS = (1, 'failure locating DRMS parameters')
    ARGUMENTS = (2, 'bad arguments')
    LOGGING = (4, 'failure logging messages')
    WHITELIST = (5, 'whitelists are unsupported')
    SERIES_INFO = (6, 'unable to obtain series information')
    EXPORT_SERVER = (7, 'export-server communication error')
    UNHANDLED_EXCEPTION = (8, 'unhandled exception')

class CdbBaseError(ExportError):
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

class ParametersError(CdbBaseError):
    _error_code = ErrorCode.PARAMETERS

class ArgumentsError(CdbBaseError):
    _error_code = ErrorCode.ARGUMENTS

class LoggingError(CdbBaseError):
    _error_code = ErrorCode.LOGGING

class WhitelistError(CdbBaseError):
    _error_code = ErrorCode.WHITELIST

class SeriesInfoError(CdbBaseError):
    _error_code = ErrorCode.SERIES_INFO

class ExportServerError(CdbBaseError):
    _error_code = ErrorCode.EXPORT_SERVER

class UnhandledExceptionError(CdbBaseError):
    _error_code = ErrorCode.UNHANDLED_EXCEPTION

class ValidateArgumentAction(argparse.Action):
    def __call__(self, parser, namespace, value, option_string=None):
        # the server specified must not be the internal server
        if self.dest == 'wl_file' and not parser.drms_params.WL_HASWL:
            raise ArgumentsError(error_message=f'{option_string} specified a white-list file, but this DRMS does not support series whitelists')

        if value.lower() == parser.drms_params.SERVER.lower():
            raise ArgumentsError(error_message=f'{option_string} specified the internal server, but you must specify an external server')

        setattr(namespace, self.dest, value)

class Arguments(Args):
    _arguments = None

    @classmethod
    def get_arguments(cls, *, is_program, program_name=None, program_args=None, module_args=None, drms_params, refresh=True):
        if cls._arguments is None or refresh:
            try:
                db_port = int(drms_params.get_required('DRMSPGPORT'))
                db_name = drms_params.get_required('DBNAME')
                db_user = drms_params.get_required('WEB_DBUSER')
                wl_file = drms_params.get_required('WL_FILE')
                private_db_host = drms_params.get_required('SERVER')
                has_wl = drms_params.get_required('WL_HASWL')
            except DPMissingParameterError as exc:
                raise ParametersError(error_message=str(exc))

            if is_program:
                try:
                    log_file = os.path.join(drms_params.get_required('EXPORT_LOG_DIR'), DEFAULT_LOG_FILE)
                except DPMissingParameterError as exc:
                    raise ParametersError(error_message=str(exc))

                args = None

                if program_args is not None and len(program_args) > 0:
                    args = program_args

                parser_args = { 'usage' : '%(prog)s public-db-host=<db host> series=<DRMS series list> [ -l/--log-file=<log file path> [ -L/--logging-level=<critical/error/warning/info/debug> ] [ -/P--dbport=<db port> ] [ -N/--dbname=<db name> ] [ -U/--dbuser=<db user> ] [ -w/--wlfile=<white-list text file> ]' }

                if program_name is not None and len(program_name) > 0:
                    parser_args['prog'] = program_name

                parser = CmdlParser(**parser_args)

                # to give the parser access to a few drms parameters inside ValidateArgumentAction
                parser.drms_params = drms_params

                # Required
                parser.add_argument('public-db-host', help='the machine hosting the EXTERNAL database that serves DRMS data series names.', metavar='<db host>', action=ValidateArgumentAction, dest='public_db_host', required=True)
                parser.add_argument('s', 'series', help='a comma-separated list of series to be checked', metavar='<series>', action=ListAction, dest='series', required=True) # ListAction makes a list out of comma-separated-list string

                # Optional
                parser.add_argument('-l', '--log-file', help='the path to the log file', metavar='<log file>', dest='log_file', default=log_file)
                parser.add_argument('-L', '--logging-level', help='the amount of logging to perform; in order of increasing verbosity: critical, error, warning, info, debug', metavar='<logging level>', dest='logging_level', action=DrmsLogLevelAction, default=DrmsLogLevel.ERROR)
                parser.add_argument('-P', '--dbport', help='the port on the machine hosting DRMS data series names', metavar='<db host port>', dest='db_port', default=db_port)
                parser.add_argument('-N', '--dbname', help='the name of the database serving DRMS series names', metavar='<db name>', dest='db_name', default=db_name)
                parser.add_argument('-U', '--dbuser', help='the user to log-in to the serving database as', metavar='<db user>', dest='db_user', default=db_user)
                parser.add_argument('-r', '--use-regex', help='if set, then `series` contains a regular expression that identifies a set of series', dest='use_regex', action='store_true')
                parser.add_argument('-w', '--wlfile', help='the text file containing the definitive list of internal series accessible via the external web site', metavar='<white-list file>', dest='wl_file', action=ValidateArgumentAction, default=wl_file)

                arguments = Arguments(parser=parser, args=args)
            else:
                # `program_args` has all `arguments` values, in final form; validate them
                # `series` is py list of DRMS data series
                def extract_module_args(*, public_db_host, series, log=None, db_port=db_port, db_name=db_name, db_user=db_user, use_regex=False, wl_file=wl_file):
                    arguments = {}

                    arguments['public_db_host'] = public_db_host
                    arguments['series'] = series # list
                    arguments['db_port'] = db_port
                    arguments['db_name'] = db_name
                    arguments['db_user'] = db_user
                    arguments['use_regex'] = use_regex
                    arguments['wl_file'] = wl_file

                    DetermineDbServerAction.set_log(log)

                    return arguments

                # dict
                module_args_dict = extract_module_args(**module_args)
                arguments = Arguments(parser=None, args=module_args_dict)

            arguments.private_db_host = private_db_host
            arguments.has_wl = has_wl

            cls._arguments = arguments

        return cls._arguments


# for use in export web app
from action import Action
from get_series_info import GetSeriesInfoAction
class DetermineDbServerAction(Action):
    actions = [ 'determine_db_server' ]

    _log = None

    def __init__(self, *, method, public_db_host, series, log=None, db_port=None, db_name=None, db_user=None, use_regex=False):
        self._method = getattr(self, method)
        self._public_db_host = public_db_host
        self._series = series # py list
        self._options = {}
        self._options['log'] = log
        self._options['db_port'] = db_port
        self._options['db_name'] = db_name
        self._options['db_user'] = db_user
        self._options['use_regex'] = use_regex

    def determine_db_server(self):
        # returns dict
        response = perform_action(action_obj=self, is_program=False, public_db_host=self._public_db_host, series=self._series, options=self._options)
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

    # `series` is a py list of series
    @classmethod
    def is_valid_series_set(cls, series, db_host, webserver, log):
        cls.set_log(log)

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

            return GetSeriesInfoAction.is_valid_series_set(series, db_host_resolved, webserver, cls._log)
        except:
            return False

def get_whitelist(wl_file):
    white_list = set()

    with open(wl_file, 'r') as f_white_list:
        # NOTE: This script does not attempt to validate the series in the whitelist - there could be invalid entries in that
        # file. Series from the whitelist that match a series in internal-DB series are returned to the caller.
        for series in f_white_list:
            white_list.add(series.strip().lower())

    return white_list

def send_request(request, connection, log):
    json_message = json.dumps(request)
    send_message(connection, json_message)
    message = get_message(connection)

    return message

# the user has provided a regex
# returns:
#   # { "hmi.m_720s" : [ "hmi.M_720s", "magnetograms with a cadence of 720 seconds." ],
    #   "hmi.m_45s" : [ "hmi.M_45s", "magnetograms with a cadence of 45 seconds." ]
    # }
#   #
def determine_series(regex, public_db_host, private_db_host, has_wl, wl_file):
    nested_arguments = ss_get_arguments(is_program=False, module_args={})
    log = DetermineDbServerAction.get_log()

    series_obj = {} # { "hmi.m_720s" : [ "hmi.M_720s", "magnetograms with a cadence of 720 seconds." ] }

    with Connection(server=nested_arguments.server, listen_port=nested_arguments.listen_port, timeout=nested_arguments.message_timeout, log=log) as connection:
        log.write_info([ f'[ determine_series] connection to socket server successful' ])

        sanitized_regex = regex.strip().lower().replace(".", "[.]")

        # check both public and private server, and merge
        message = { 'request_type' : 'series_list', 'series_regex' : sanitized_regex, 'db_host' : public_db_host }
        response = json.loads(send_request(message, connection, log))

        if response.get('export_server_status') == 'export_server_error':
            raise ExportServerError(error_message=f'{response["error_message"]}')

        if len(response['names']) != 0:
            for series_info in response['names']:
                series_lower = series_info['name'].strip().lower()
                series_obj[series_lower] = [ series_info['name'], series_info['note'] ]

        if has_wl:
            message = { 'request_type' : 'series_list', 'series_regex' : sanitized_regex, 'db_host' : private_db_host }
            response = json.loads(send_request(message, connection, log))

            if len(response['names']) != 0:
                white_list = get_whitelist(wl_file) # if no whitelist exists, then this is the empty set

                for series_info in response['names']:
                    series_lower = series_info['name'].strip().lower()
                    if series_lower in white_list and series_lower not in series_obj:
                        # this series is visible to the public, and not yet already in the series_obj
                        series_obj[series_lower] = [ series_info['name'], series_info['note'] ]

        message = { 'request_type' : 'quit' }
        send_request(message, connection, log)

        return series_obj

# the user has provided a list of series
@functools.lru_cache
def determine_server(series, public_db_host, private_db_host, has_wl, wl_file):
    nested_arguments = ss_get_arguments(is_program=False, module_args={})
    log = DetermineDbServerAction.get_log()

    with Connection(server=nested_arguments.server, listen_port=nested_arguments.listen_port, timeout=nested_arguments.message_timeout, log=log) as connection:
        log.write_info([ f'[ determine_server] connection to socket server successful' ])
        series_obj = None
        supporting_server = False
        use_public_server = True

        series_regex = f'^{series.strip().lower().replace(".", "[.]")}$'

        message = { 'request_type' : 'series_list', 'series_regex' : series_regex, 'db_host' : public_db_host }
        response = json.loads(send_request(message, connection, log))

        if response.get('export_server_status') == 'export_server_error':
            raise ExportServerError(error_message=f'{response["error_message"]}')

        if len(response['names']) == 0:
            if has_wl:
                # try private server
                message = { 'request_type' : 'series_list', 'series_regex' : series_regex, 'db_host' : private_db_host }
                response = json.loads(send_request(message, connection, log))

                white_list = get_whitelist(wl_file) # if no whitelist exists, then this is the empty set

                if series.lower() in white_list and len(response['names']) != 0:
                    series_obj = { series : { 'server' : private_db_host } }
                    supporting_server = True
                    use_public_server = False
                else:
                    series_obj = { series : { 'server' : None } }
            else:
                series_obj = { series : { 'server' : None } }
        else:
            series_obj = { series : { 'server' : public_db_host } }
            supporting_server = True

        message = { 'request_type' : 'quit' }
        send_request(message, connection, log)

    return (supporting_server, use_public_server, series_obj)

def perform_action(*, action_obj, is_program, program_name=None, **kwargs):
    response = None
    log = None

    try:
        program_args, module_args = extract_program_and_module_args(is_program=is_program, **kwargs)

        try:
            drms_params = DRMSParams()

            if drms_params is None:
                raise ParametersError(error_message='unable to locate DRMS parameters file (drmsparams.py)')

            arguments = Arguments.get_arguments(is_program=is_program, program_name=program_name, program_args=program_args, module_args=module_args, drms_params=drms_params)
        except ArgsError as exc:
            raise ArgumentsError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
        except Exception as exc:
            raise ArgumentsError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')

        if is_program:
            try:
                formatter = DrmsLogFormatter('%(asctime)s - %(levelname)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
                log = DrmsLog(arguments.log_file, arguments.logging_level, formatter)
                DetermineDbServerAction._log = log
            except Exception as exc:
                raise LoggingError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
        else:
            log = action_obj.log

        if is_program:
            log.write_debug([ f'[ perform_action ] program invocation' ])
        else:
            log.write_debug([ f'[ perform_action ] module invocation' ])

        log.write_debug([ f'[ perform_action ] action arguments: {str(arguments)}' ])

        try:
            response_dict = {}

            if len(arguments.series) > 0:
                if (arguments.use_regex):
                    series_obj = determine_series(arguments.series[0], arguments.public_db_host, arguments.private_db_host, arguments.has_wl, arguments.wl_file)
                    response_dict['series'] = series_obj
                else:
                    response_dict['series'] = []
                    use_public_server = True
                    white_list = None
                    supporting_server = True

                    for series in arguments.series:
                        is_supported, is_public, series_obj = determine_server(series, arguments.public_db_host, arguments.private_db_host, arguments.has_wl, arguments.wl_file)
                        response_dict['series'].append(series_obj)
                        use_public_server &= is_public
                        supporting_server &= is_supported

                    if supporting_server:
                        response_dict['server'] = arguments.public_db_host if use_public_server else arguments.private_db_host
                    else:
                        response_dict['server'] = None
            else:
                response_dict['server'] = None
                response_dict['series'] = []

            # response examples:
            #   { "server" : "hmidb", "series" : [{ "hmi.M_45s" : { "server" : "hmidb2" } }, { "hmi.on_white_list" : { "server" : "hmidb" }}], "status" : 0 }
            #   { "server" : "hmidb2", "series" : [{ "hmi.M_45s" : { "server" : "hmidb2" } }, { "hmi.not_on_white_list" : { "server" : None }}], "status" : 0 }
            #   { "server" : "hmidb2", "series" : [{ "hmi.M_45s" : { "server" : "hmidb2" } }, { "hmi.does_not_exist" : { "server" : None }}], "status" : 0 }
            response = Response.generate_response(status_code=StatusCode.SUCCESS, **response_dict)
        except ExpServerBaseError as exc:
            raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
    except CdbBaseError as exc:
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

    return response

# Parse arguments
if __name__ == "__main__":
    response = perform_action(action_obj=None, is_program=True)
    print(response.generate_json())

    # Always return 0. If there was an error, an error code (the 'status' property) and message (the 'statusMsg' property) goes in the returned HTML.
    sys.exit(0)
else:
    pass
