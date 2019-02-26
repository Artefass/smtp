import subprocess
import os
import socket
import sys
from time import sleep
import signal
import shutil
import filecmp


def start_server(port, logfile, maildir):
    return subprocess.Popen(['../smtpserver', '-d', '127.0.0.1', '-p', str(port), '-l', logfile, '-m', maildir, '-r',
                             '-n', 'mysmtp.pvs.bmstu'])


def start_server_valgrind(port, logfile, maildir):
    return subprocess.Popen(['valgrind', '--leak-check=full', '--child-silent-after-fork=yes', '--error-exitcode=77', '../smtpserver', '-d', '127.0.0.1', '-p', str(port), '-l', logfile, '-m', maildir, '-r',
                             '-n', 'mysmtp.pvs.bmstu'])


def stop_server(server):
    os.kill(server.pid, signal.SIGINT)
    try:
        server.wait(timeout=2)
    except subprocess.TimeoutExpired:
        print('ERROR: SERVER HUNG ON EXIT, KILLING')
        server.kill()
        return False
    return True

def stop_server_valgrind(server):
    os.kill(server.pid, signal.SIGINT)
    try:
        server.wait(timeout=5)
    except subprocess.TimeoutExpired:
        print('ERROR: SERVER HUNG ON EXIT, KILLING')
        server.kill()
        return False
    return_code = server.returncode
    return return_code != 77

def norm_str(s):
    stripped = s.strip(' \r\n')
    if stripped == '':
        return None
    elif stripped[0] == '#':
        return None
    else:
        return stripped + '\r\n'


def server_reader(s):
    buf = ''
    data = True
    while data:
        data = s.recv(256).decode('ASCII')
        buf += data
        index = buf.find('\r\n')
        while index != -1:
            yield buf[:index]
            buf = buf[index+2:]
            index = buf.find('\r\n')
    return buf


def exec_test(filename, logfile, maildir, ipv6=False, valgrind=False):
    port = 7548
    if valgrind:
        server = start_server_valgrind(port, logfile, maildir)
        sleep(1)
    else:
        server = start_server(port, logfile, maildir)
        sleep(0.1)


    try:
        if ipv6:
            serversocket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
            serversocket.settimeout(10)
            serversocket.connect(('::1', port))
        else:
            serversocket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            serversocket.settimeout(10)
            serversocket.connect(('127.0.0.1', port))
    except socket.error:
        print('ERROR: COULD NOT CONNECT')
        stop_server(server)
        return False

    with open(filename, 'r') as f:
        lines = [x for x in map(norm_str, f.readlines()) if x is not None]

    reader = server_reader(serversocket)
    try:
        for line in lines:
            if line[0:2] == 'C:':
                line = line[2:]
                serversocket.send(line.encode('ASCII'))
            elif line[0:2] == 'S:':
                line = line[2:]
                server_line = next(reader)
                expected_code = line[:3]
                got_code = server_line[:3]
                if expected_code != got_code:
                    print('ERROR: EXPECTED {} GOT {}'.format(expected_code, got_code))
                    return False
            else:
                print('WARNING: UNKNOWN LINE FORMAT: {}'.format(line))
    except socket.timeout:
        print('ERROR: SERVER TIMEOUT')
        return False
    finally:
        serversocket.close()
        if valgrind:
            exit_success = stop_server_valgrind(server)
        else:
            exit_success = stop_server(server)

    return exit_success


def are_dir_trees_equal(dir1, dir2):
    """
    Compare two directories recursively. Files in each directory are
    assumed to be equal if their names and contents are equal.

    @param dir1: First directory path
    @param dir2: Second directory path

    @return: True if the directory trees are the same and
        there were no errors while accessing the directories or files,
        False otherwise.
   """

    dirs_cmp = filecmp.dircmp(dir1, dir2, ignore=['gitstub'])
    if len(dirs_cmp.left_only)>0 or len(dirs_cmp.right_only)>0 or \
        len(dirs_cmp.funny_files)>0:
        return False
    (_, mismatch, errors) =  filecmp.cmpfiles(
        dir1, dir2, dirs_cmp.common_files, shallow=False)
    if len(mismatch)>0 or len(errors)>0:
        return False
    for common_dir in dirs_cmp.common_dirs:
        new_dir1 = os.path.join(dir1, common_dir)
        new_dir2 = os.path.join(dir2, common_dir)
        if not are_dir_trees_equal(new_dir1, new_dir2):
            return False
    return True


class SystemTest(object):
    def __init__(self, scenario_name, reference_dir=None):
        self.scenario_name = scenario_name
        self.reference_dir = reference_dir
        self.result = False

    def run(self, ipv6=False, valgrind=False):
        print('SCENARIO {}: START'.format(self.scenario_name))

        logfile = "./{}-log".format(self.scenario_name)
        maildir = "./{}-mail/".format(self.scenario_name)

        if os.path.exists(logfile):
            os.remove(logfile)
        if os.path.exists(maildir):
            shutil.rmtree(maildir)

        self.result = exec_test(self.scenario_name, logfile, maildir, ipv6=ipv6, valgrind=valgrind)
        if self.result:
            pass
            #print('SCENARIO {}: COMMAND SUCCESS'.format(self.scenario_name))
        else:
            print('SCENARIO {}: COMMAND FAIL'.format(self.scenario_name))

        if self.result and self.reference_dir is not None:
            self.result = are_dir_trees_equal(maildir, self.reference_dir)
            if self.result:
                pass
                #print('SCENARIO {}: DIRECTORY COMPARISON SUCCESS'.format(self.scenario_name))
            else:
                print('SCENARIO {}: DIRECTORY COMPARISON FAIL'.format(self.scenario_name))

        if self.result:
            print('SCENARIO {}: SUCCESS'.format(self.scenario_name))
            if os.path.exists(logfile):
                os.remove(logfile)
            shutil.rmtree(maildir)
        else:
            print('SCENARIO {}: FAIL'.format(self.scenario_name))


def test_suite():
    return [
        SystemTest('starts'),
        SystemTest('can_quit'),
        SystemTest('correct_enters'),
        #SystemTest('incorrect_exits'),
        #SystemTest('unknown_exits'),
        SystemTest('letter_to_this_server', 'letter_to_this_server-expected'),
        SystemTest('letter_to_other_server', 'letter_to_other_server-expected'),
        SystemTest('two_mails', 'two_mails-expected'),
        SystemTest('unrecognized_command'),
        SystemTest('rcpt_before_mailto', 'letter_to_this_server-expected'),
        SystemTest('rset_resets', 'letter_to_this_server-expected'),
        SystemTest('vrfy_fails'),
    ]


def run_tests_and_report_results(suite, runfunc):
    for case in suite:
        runfunc(case)
    results = [x.result for x in suite]
    success = results.count(True)
    failure = results.count(False)
    print("-" * 24)
    print('PASSED {}/{} TESTS'.format(success, len(results)))
    if failure != 0:
        print('{} TESTS FAILED'.format(failure))
        return False
    else:
        print('OK')
        return True


def ipv4_tests():
    print('IPV4')
    print('-'*24)
    return run_tests_and_report_results(test_suite(), lambda x: x.run(ipv6=False))


def ipv6_tests():
    print('IPV6')
    print('-'*24)
    return run_tests_and_report_results(test_suite(), lambda x: x.run(ipv6=True))

def exec_valgrind():
    print('Valgrind')
    print('-'*24)
    return run_tests_and_report_results(test_suite(), lambda x: x.run(ipv6=False, valgrind=True))


if __name__ == '__main__':
    result = False
    if len(sys.argv) == 1:
        sys.argv += ['ipv4', 'ipv6', 'valgrind']
    if 'ipv4' in sys.argv:
        result = ipv4_tests() and result
    if 'ipv6' in sys.argv:
        result = ipv6_tests() and result
    if 'valgrind' in sys.argv:
        result = exec_valgrind() and result

    if result:
        sys

