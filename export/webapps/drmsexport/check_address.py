#!/usr/bin/env python3

# This is a version of checkAddress.py modified to run inside a Flask context.

# The arguments to this script are parsed by cgi.FieldStorage(), which knows how to parse
# both HTTP GET and POST requests. A nice feature is that we can test the script as it runs in a CGI context
# by simply running on the command line with a single argument that is equivalent to an HTTP GET parameter string
# (e.g., address=gimli@mithril.com&addresstab=jsoc.export_addresses&domaintab=jsoc.export_addressdomains).

# Parameters:
#   address (required) - The email address to check or register.
#   addresstab (required) - The database table containing all registered (or registration-pending) email addresses.
#   domaintab (required) - The database table containing all email domains.
#   dbuser (optional) - The database account to be used when connecting to the database. The default is the value of the WEB_DBUSER parameter in DRMSParams.
#   checkonly (optional) - If set to 1, then no attept is made to register an unregistered email. In this case, if no error occurs then the possible return status codes are StatusCode.REGISTERED_ADDRESS, StatusCode.REGISTRATION_PENDING, or StatusCode.UNREGISTERED_ADDRESS. The default is False (unknown addresses are registered).

import argparse
import copy
import json
import os.path
import psycopg2
import re
import uuid
import smtplib
import sys

from drms_export import Connection, Error as ExportError, ErrorCode as ExportErrorCode, ExpServerBaseError, get_arguments as ss_get_arguments, get_message, Response, send_message
from drms_parameters import DRMSParams, DPMissingParameterError
from drms_utils import Arguments as Args, ArgumentsError as ArgsError, CmdlParser, Formatter as DrmsLogFormatter, Log as DrmsLog, LogLevel as DrmsLogLevel, LogLevelAction as DrmsLogLevelAction, StatusCode as ExportStatusCode
from utils import extract_program_and_module_args

DEFAULT_LOG_FILE = 'ca_log.txt'

class StatusCode(ExportStatusCode):
    REGISTRATION_INITIATED = (1, 'registration of {address} initiated')
    REGISTRATION_PENDING = (2, 'registration of {address} is pending')
    REGISTERED_ADDRESS = (3, '{address} is registered')
    UNREGISTERED_ADDRESS = (4, '{address} is not registered')

class ErrorCode(ExportErrorCode):
    PARAMETERS = (1, 'failure locating DRMS parameters')
    ARGUMENTS = (2, 'bad arguments')
    LOGGING = (3, 'failure logging messages')
    MAIL = (4, 'failure sending mail')
    DB = (5, 'failure executing database command')
    DB_CONNECTION = (6, 'failure connecting to database')
    DUPLICATION = (7, 'address is already registered')
    UNHANDLED_EXCEPTION = (8, 'unhandled exception')
    EXPORT_SERVER = (9, 'export-server communication error')

class CaBaseError(ExportError):
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

class ParametersError(CaBaseError):
    _error_code = ErrorCode.PARAMETERS

class ArgumentsError(CaBaseError):
    _error_code = ErrorCode.ARGUMENTS

class LoggingError(CaBaseError):
    _error_code = ErrorCode.LOGGING

class MailError(CaBaseError):
    _error_code = ErrorCode.MAIL

class DBError(CaBaseError):
    _error_code = ErrorCode.DB

class DBConnectionError(CaBaseError):
    _error_code = ErrorCode.DB_CONNECTION

class DuplicationError(CaBaseError):
    _error_code = ErrorCode.DUPLICATION

class UnhandledExceptionError(CaBaseError):
    _error_code = ErrorCode.UNHANDLED_EXCEPTION

class ExportServerError(CaBaseError):
    _error_code = ErrorCode.EXPORT_SERVER

class CheckAddressResponse(Response):
    _status_code = None

    def __init__(self, *, address, **kwargs):
        super().__init__(address=address, **kwargs)

class RegistrationInitiatedResponse(CheckAddressResponse):
    _status_code = StatusCode(StatusCode.REGISTRATION_INITIATED)

class RegistrationPendingResponse(CheckAddressResponse):
    _status_code = StatusCode(StatusCode.REGISTRATION_PENDING)

class RegisteredResponse(CheckAddressResponse):
    _status_code = StatusCode(StatusCode.REGISTERED_ADDRESS)

class UnregisteredResponse(CheckAddressResponse):
    _status_code = StatusCode(StatusCode.UNREGISTERED_ADDRESS)

class UnquoteAction(argparse.Action):
    def __call__(self, parser, namespace, value, option_string=None):
        unquoted = urllib.parse.unquote(value)
        setattr(namespace, self.dest, unquoted)

class Arguments(Args):
    _arguments = None

    @classmethod
    def get_arguments(cls, *, is_program, program_name=None, program_args=None, module_args=None, drms_params, refresh=True):
        if cls._arguments is None or refresh:
            try:
                db_host = drms_params.get_required('SERVER') # must be internal db
                db_port = int(drms_params.get_required('DRMSPGPORT'))
                db_name = drms_params.get_required('DBNAME')
                db_user = drms_params.get_required('WEB_DBUSER')
                address_info_fn = drms_params.get_required('EXPORT_ADDRESS_INFO_FN')
                address_info_insert_fn = drms_params.get_required('EXPORT_ADDRESS_INFO_INSERT_FN')
                user_info_fn = drms_params.get_required('EXPORT_USER_INFO_FN')
                user_info_insert_fn = drms_params.get_required('EXPORT_USER_INFO_INSERT_FN')
                regemail_timeout = drms_params.get_required('REGEMAIL_TIMEOUT')
            except DPMissingParameterError as exc:
                raise ParametersError(error_message=f'{str(exc)}')

            if is_program:
                try:
                    log_file = os.path.join(drms_params.get_required('EXPORT_LOG_DIR'), DEFAULT_LOG_FILE)
                except DPMissingParameterError as exc:
                    raise ParametersError(exc_info=sys.exc_info(), error_message=str(exc))

                args = None

                if program_args is not None and len(program_args) > 0:
                    args = program_args

                parser_args = { 'usage' : '%(prog)s address=<email address to register/check> operation=<register/check> [ --log-file=<path to log file> ] [ --logging-level=<critical/error/warning/info/debug> ] [ --name=<user\'s name> ] [ --snail=<user snail mail address> ] [ --dbhost=<db host> ] [ --dbport=<db port> ] [ --dbname=<db name> ] [ --dbuser=<db user>] ' }
                if program_name is not None and len(program_name) > 0:
                    parser_args['prog'] = program_name

                parser = CmdlParser(**parser_args)

                # all arguments are considered optional in argparse (see `prefix_chars`); we can therefore do this:
                # parser.add_argument('-H', 'H', '--dbhost', ...)
                parser.add_argument('a', 'address', help='the email address to register or check', metavar='<email address>', dest='address', action=UnquoteAction, required=True)
                parser.add_argument('o', 'operation', help='the operation: register or check', metavar='<operation>', choices=[ 'check', 'register' ], dest='operation', required=True)

                # optional
                parser.add_argument('-l', '--log-file', help='the path to the log file', metavar='<log file>', dest='log_file', default=log_file)
                parser.add_argument('-L', '--logging-level', help='the amount of logging to perform; in order of increasing verbosity: critical, error, warning, info, debug', metavar='<logging level>', dest='logging_level', action=DrmsLogLevelAction, default=DrmsLogLevel.ERROR)
                parser.add_argument('-n', '--name', help='the user name to register', metavar='<export user\'s name>', dest='user_name', action=UnquoteAction, default='NULL')
                parser.add_argument('-s', '--snail', help='the user snail-mail address to register', metavar='<export user\'s snail mail>', dest='user_snail', action=UnquoteAction, default='NULL')
                parser.add_argument('-H', '--dbhost', help='the host machine of the internal database that is used to manage pending export requests', metavar='<db host>', dest='db_host', default=db_host)
                parser.add_argument('-P', '--dbport', help='The port on the host machine that is accepting connections for the database', metavar='<db host port>', dest='db_port', type=int, default=db_port)
                parser.add_argument('-N', '--dbname', help='the name of the database used to manage pending export requests', metavar='<db name>', dest='db_name', default=db_name)
                parser.add_argument('-U', '--dbuser', help='the name of the database user account', metavar='<db user>', dest='db_user', default=db_user)

                arguments = Arguments(parser=parser, args=args)
            else:
                def extract_module_args(*, address, operation, log=None, db_host=db_host, db_port=db_port, db_name=db_name, db_user=db_user, user_name=None, user_snail=None):
                    arguments = {}

                    arguments['address'] = address
                    arguments['operation'] = operation
                    arguments['db_host'] = db_host
                    arguments['db_port'] = db_port
                    arguments['db_name'] = db_name
                    arguments['db_user'] = db_user
                    arguments['user_name'] = user_name
                    arguments['user_snail'] = user_snail

                    CheckAddressAction.set_log(log)

                    return arguments

                module_args_dict = extract_module_args(**module_args)
                arguments = Arguments(parser=None, args=module_args_dict)

            arguments.address_info_fn = address_info_fn
            arguments.address_info_insert_fn = address_info_insert_fn
            arguments.user_info_fn = user_info_fn
            arguments.user_info_insert_fn = user_info_insert_fn
            arguments.regemail_timeout = regemail_timeout

            # Do a quick validation on the email address.
            reg_exp = re.compile(r'\s*[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,6}')
            match_obj = reg_exp.match(arguments.address)
            if match_obj is None:
                raise ArgumentsError(error_message=f'{arguments.address} is not a valid email address')

            cls._arguments = arguments

        return cls._arguments

def SendMail(address, timeout, confirmation):
    subject = 'CONFIRM EXPORT ADDRESS'
    fromAddr = 'jsoc@solarpost.stanford.edu'
    toAddrs = [ address ]
    bccAddrs = [ 'art.amezcua@stanford.edu' ]
    msg = 'From: ' + fromAddr + '\nTo: ' + ','.join(toAddrs) + '\nSubject: ' + subject + '\nThis message was automatically generated by the JSOC export system at Stanford.\n\nYou have requested that data be exported from the JSOC. To do so, you must register your email address with the export system. To complete the registration process, please reply to this message within ' + timeout + ' minutes. Please do not modify the body of this message when replying. The server will extract the embedded confirmation code to verify that the provided email address is valid. You will receive another email message notifying you of the disposition of your registration.'
    msg += '\n[' + str(confirmation) + ']'

    try:
        server = smtplib.SMTP('solarpost.stanford.edu')
        server.sendmail(fromAddr, toAddrs + bccAddrs, msg)
        server.quit()
    except Exception as exc:
        # If any exception happened, then the email message was not received.
        raise MailError(error_message=f'unable to send email message to {",".join(toAddrs)} to confirm address')

def generate_registered_or_pending_response(cursor, arguments, confirmation):
    response = None

    # get requestor ID also
    cmd = f'SELECT id FROM {arguments.user_info_fn}(\'{arguments.address}\')'
    try:
        cursor.execute(cmd)
        rows = cursor.fetchall()
    except psycopg2.Error as exc:
        # handle database-command errors
        raise DBError(error_message=f'{str(exc)}')

    user_id = rows[0][0]

    if confirmation is None or len(confirmation) == 0:
        # if confirmation == None ==> registered
        response = RegisteredResponse.generate_response(address=arguments.address, user_id=user_id)
    else:
        # if confirmation != None ==> pending registration
        response = RegistrationPendingResponse.generate_response(address=arguments.address, user_id=user_id)

    return response

def send_request(request, connection, log):
    json_message = json.dumps(request)
    send_message(connection, json_message)
    message = get_message(connection)

    return message

# for use in export web app
from action import Action
class CheckAddressAction(Action):
    actions = [ 'check_address', 'register_address' ]

    _log = None

    def __init__(self, *, method, address, log=None, db_host=None, db_port=None, db_name=None, db_user=None, user_name=None, user_snail=None):
        self._method = getattr(self, method)
        self._address = address
        self._options = {}
        self._options['log'] = log
        self._options['db_host'] = db_host
        self._options['db_port'] = db_port
        self._options['db_name'] = db_name
        self._options['db_user'] = db_user
        self._options['user_name'] = user_name
        self._options['user_snail'] = user_snail

    def check_address(self):
        # returns dict
        response = perform_action(action_obj=self, is_program=False, operation='check', address=self._address, options=self._options)
        return response

    def register_address(self):
        # returns dict
        response = perform_action(action_obj=self, is_program=False, operation='register', address=self._address, options=self._options)
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

class CheckAddressLegacyResponse(Response):
    def remove_extraneous(self, response_dict, keys):
        for key in keys:
            if key in response_dict:
                del response_dict[key]

        return response_dict

    # override parent to remove attributes not present in legacy API
    def generate_serializable_dict(self):
        serializable_dict = copy.deepcopy(super().generate_serializable_dict())
        sanitized_serializable_dict = self.remove_extraneous(serializable_dict, [ 'drms_export_status', 'drms_export_status_code', 'drms_export_status_description', 'address'] )

        return sanitized_serializable_dict

class CheckAddressLegacyAction(Action):
    actions = [ 'legacy_check_address' ]

    _log = None

    def __init__(self, *, method, address, db_host=None, log=None, **kwargs):
        self._method = getattr(self, method)
        self._address = address
        self._options = {}
        self._options['db_host'] = db_host
        self._options['log'] = log if log is not None else DrmsLog(None, None, None)
        self._legacy_arguments = self.convert_booleans(kwargs)

    def convert_booleans(self, args_dict):
        converted = {}
        for key, val in args_dict.items():
            if type(val) == bool:
                converted[key] = int(val)
            else:
                converted[key] = val

        return converted

    def legacy_check_address(self):
        # pass through to check-address
        log = self._options['log']

        try:
            if self._options['db_host'] is None:
                drms_params = DRMSParams()

                if drms_params is None:
                    raise ParametersError(error_message='unable to locate DRMS parameters package')

                self._options['db_host'] = drms_params.get_required('SERVER')

            # use socket server to call jsoc_fetch
            nested_arguments = ss_get_arguments(is_program=False, module_args={})

            try:
                with Connection(server=nested_arguments.server, listen_port=nested_arguments.listen_port, timeout=nested_arguments.message_timeout, log=log) as connection:
                    message = { 'request_type' : 'legacy_check_address', 'address' : self._address, 'db_host' : self._options['db_host'] }
                    message.update(self._legacy_arguments) # error if the user has provided non-expected arguments
                    response = send_request(message, connection, log)

                    # message is raw JSON from checkAddress.py
                    legacy_check_address_dict = json.loads(response)

                    if legacy_check_address_dict.get('export_server_status') == 'export_server_error':
                        raise ExportServerError(error_message=f'{legacy_check_address_dict["error_message"]}')

                    message = { 'request_type' : 'quit' }
                    send_request(message, connection, log)

                response = self.get_response(legacy_check_address_dict, log)
            except ExpServerBaseError as exc:
                raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
            except Exception as exc:
                raise ExportServerError(exc_info=sys.exc_info(), error_message=f'{str(exc)}')
        except CaBaseError as exc:
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

    def get_response(self, client_response_dict, log):
        log.write_debug([ f'[ get_response ]' ])

        response_dict = copy.deepcopy(client_response_dict)
        check_address_status = response_dict['status']

        if check_address_status == 1:
            response_dict['status_code'] = StatusCode.REGISTRATION_INITIATED
        elif check_address_status == 2:
            response_dict['status_code'] = StatusCode.REGISTERED_ADDRESS
        elif check_address_status == 3:
            response_dict['status_code'] = StatusCode.REGISTERATION_PENDING
        elif check_address_status == 4:
            response_dict['status_code'] = StatusCode.UNREGISTERED_ADDRESS
        else:
            # there should be no other status possible
            response_dict['status_code'] = StatusCode.FAILURE

        response = CheckAddressLegacyResponse.generate_response(address=self._address, **response_dict)

        return response

def initiate_registration(cursor, arguments, log):
    # the address is not in the db, and the user did request registration ==> registration initiated
    confirmation = uuid.uuid4()

    # ensure confirmation does not already exist in addresses_tab
    cmd = f'SELECT confirmation FROM {arguments.address_info_fn}() WHERE confirmation = \'{str(confirmation)}\''

    try:
        log.write_debug([ f'[ initiate_registration ] executing SQL `{cmd}`' ])
        cursor.execute(cmd)
        rows = cursor.fetchall()
        if len(rows) > 0:
            raise DuplicationError(error_message=f'cannot insert row into address table; confirmation {str(confirmation)} already exists')
    except psycopg2.Error as exc:
        # Handle database-command errors.
        raise DBError(error_message=f'{str(exc)}')

    # insert into the addresses table (and domains table if need be)
    log.write_debug([ f'[ initiate_registration ] inserting address `{arguments.address}`, confirmation `{str(confirmation)}` into database' ])
    cmd = f"SELECT * FROM {arguments.address_info_insert_fn}('{arguments.address}', '{str(confirmation)}')"
    try:
        log.write_debug([ f'[ initiate_registration ] executing SQL `{cmd}`' ])
        cursor.execute(cmd)
    except psycopg2.Error as exc:
        # Handle database-command errors.
        raise DBError(error_message=f'{str(exc)}')

    # we have to also insert into the export user table since we have that information now, not after the user has replied to the registration email
    # (which is processed by registerAddress.py); if a failure happens anywhere along the way, we need to delete the entry from the export user table
    log.write_debug([ f'[ initiate_registration ] inserting user information `{arguments.address}`, name `{arguments.user_name}`, snail `{arguments.user_snail}` into database' ])
    cmd = f"SELECT id FROM {arguments.user_info_insert_fn}('{arguments.address}', '{arguments.user_name}', '{arguments.user_snail}')"

    try:
        log.write_debug([ f'[ initiate_registration ] executing SQL `{cmd}`' ])
        cursor.execute(cmd)
        rows = cursor.fetchall()
    except psycopg2.Error as exc:
        # Handle database-command errors.
        raise DBError(error_message=f'{str(exc)}')

    user_id = rows[0][0]

    # send an email message out with a new confirmation code
    log.write_debug([ f'[ initiate_registration ] sending confirmation `{str(confirmation)}` to `{arguments.address}`' ])
    SendMail(arguments.address, arguments.regemail_timeout, confirmation)

    msg = f'Your email address is being registered for use with the export system. You will receive an email message from user jsoc. Please reply to this email message within {arguments.regemail_timeout} minutes without modifying the body.'

    response = RegistrationInitiatedResponse.generate_response(address=arguments.address, msg=msg, user_id=user_id)

    return response

def perform_action(*, action_obj, is_program, program_name=None, **kwargs):
    response = None
    log = None

    try:
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
                CheckAddressAction._log = log
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
            log.write_debug([ f'[ perform_action ] connecting to database: host={arguments.db_host}, port={str(arguments.db_port)}, user={arguments.db_user}, db={arguments.db_name}' ])
            with psycopg2.connect(database=arguments.db_name, user=arguments.db_user, host=arguments.db_host, port=str(arguments.db_port)) as conn:
                with conn.cursor() as cursor:
                    cmd = f'SELECT confirmation FROM {arguments.address_info_fn}(\'{arguments.address}\')'

                    try:
                        log.write_debug([ f'executing SQL `{cmd}`' ])
                        cursor.execute(cmd)
                        rows = cursor.fetchall()

                        if len(rows) == 0:
                            # action depends on operation
                            log.write_debug([ f'address `{arguments.address}` not in database' ])
                            if arguments.operation == 'check':
                                # the address is not in the db, and the user did not request registration ==> not registered
                                response = UnregisteredResponse.generate_response(address=arguments.address, user_id=-1)
                            else:
                                log.write_debug([ f'initating registration of `{arguments.address}`' ])
                                response = initiate_registration(cursor, arguments, log)

                        elif len(rows) == 1:
                            # the address is in the db
                            confirmation = rows[0][0]
                            response = generate_registered_or_pending_response(cursor, arguments, confirmation)
                        else:
                            raise DBError(error_message=f'unexpected number of rows returned: {cmd}')
                    except psycopg2.Error as exc:
                        # handle database-command errors
                        raise DBError(error_message=f'{str(exc)}')
        except psycopg2.OperationalError as exc:
            # closes the cursor and connection
            raise DBConnectionError(error_message=f'unable to connect to the database: {str(exc)}')
    except CaBaseError as exc:
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

def run_tests():
    formatter = DrmsLogFormatter('%(asctime)s - %(levelname)s - %(message)s', datefmt='%Y-%m-%d %H:%M:%S')
    log = DrmsLog(sys.stdout, DrmsLogLevelAction.string_to_level('debug'), formatter)

    log.write_debug([ 'testing `legacy_check_address`'])
    cala = CheckAddressLegacyAction(method='legacy_check_address', address='arta@sun.stanford.edu', checkonly=True, db_host='hmidb2', log=log)
    response = cala()
    response_dict = response.generate_serializable_dict()
    log.write_debug([ str(response_dict) ])

if __name__ == "__main__":
    response = perform_action(action_obj=None, is_program=True)
    print(response.generate_json())

    # Always return 0. If there was an error, an error code (the 'status' property) and message (the 'statusMsg' property) goes in the returned HTML.
    sys.exit(0)
else:
    pass
