# -*- coding: utf-8 -*-

'''

message.py: simple logger for Singularity python helper

The error names (prefix) and integer level are assigned as follows:

ABRT -4
ERROR -3
WARNING -2
LOG -1
INFO 1
QUIET 0
VERBOSE 2
VERBOSE1 2
VERBOSE2 3
VERBOSE3 4
DEBUG 5

VERBOSE is equivalent to VERBOSE1 (this is mirroring the C code)
and for each level, calling it corresponds to calling the class'
function for it. E.g., DEBUG --> bot.debug('This is the message!')

The following levels are to stderr:

5,4,3,2,1,-1,-2,-3,-4

The following levels are only to stdout

1

The following levels do nothing (quiet)

0

Copyright (c) 2016-2017, Vanessa Sochat. All rights reserved. 

"Singularity" Copyright (c) 2016, The Regents of the University of California,
through Lawrence Berkeley National Laboratory (subject to receipt of any
required approvals from the U.S. Dept. of Energy).  All rights reserved.
 
This software is licensed under a customized 3-clause BSD license.  Please
consult LICENSE file distributed with the sources of this project regarding
your rights to use or distribute this software.
 
NOTICE.  This Software was developed under funding from the U.S. Department of
Energy and the U.S. Government consequently retains certain rights. As such,
the U.S. Government has been granted for itself and others acting on its
behalf a paid-up, nonexclusive, irrevocable, worldwide license in the Software
to reproduce, distribute copies to the public, prepare derivative works, and
perform publicly and display publicly, and to permit other to do so. 

'''

import os
import sys

ABRT = -4
ERROR = -3
WARNING = -2
LOG = -1
INFO = 1
QUIET = 0
VERBOSE = VERBOSE1 = 2
VERBOSE2 = 3
VERBOSE3 = 4
DEBUG = 5


class SingularityMessage:

    def __init__(self,MESSAGELEVEL=None):
        self.level = get_logging_level()
        self.history = []
        self.errorStream = sys.stderr
        self.outputStream = sys.stdout        

    def emitError(self,level):
        '''determine if a level should print to
        stderr, includes all levels but INFO and QUIET'''
        if level in [ABRT,
                     ERROR,
                     WARNING,
                     LOG,
                     VERBOSE,
                     VERBOSE1,
                     VERBOSE2,
                     VERBOSE3,
                     DEBUG ]:
            return True
        return False


    def emitOutput(self,level):
        '''determine if a level should print to stdout
        only includes INFO'''
        if level in [INFO]:
            return True
        return False


    def isEnabledFor(self,messageLevel):
        '''check if a messageLevel is enabled to emit a level
        '''
        if messageLevel <= self.level:
            return True
        return False


    def emit(self,level,message,prefix=None):
        '''emit is the main function to print the message
        optionally with a prefix
        :param level: the level of the message
        :param message: the message to print
        :param prefix: a prefix for the message
        '''

        if prefix is not None:
            prefix = "%s " %(prefix)
        else:
            prefix = ""

        # Add the prefix 
        message = "%s%s" %(prefix,message)

        if not message.endswith('\n'):
            message = "%s\n" %message

        # If the level is quiet, only print to error
        if self.level == QUIET:
            pass

        # Otherwise if in range print to stdout and stderr
        elif self.isEnabledFor(level):
            if self.emitError(level):
                self.errorStream.write(message)
            else:
                self.outputStream.write(message)

        # Add all log messages to history
        self.history.append(message)


    def get_logs(self,join_newline=True):
        ''''get_logs will return the complete history, joined by newline
        (default) or as is.
        '''
        if join_newline:
            return '\n'.join(self.history)
        return self.history
        


    def show_progress(self,iteration,total,length=100,min_level=1,carriage_return=True):
        '''create a terminal progress bar, default bar shows for verbose+
        :param iteration: current iteration (Int)
        :param total: total iterations (Int)
        :param length: character length of bar (Int)
        '''
        percent = 100 * (iteration / float(total))
        progress = int(length * iteration // total)

        # Download sizes can be imperfect, setting carriage_return to False
        # and writing newline with caller cleans up the UI
        if percent >= 100:
            progress = percent = 100

        bar = '█' * progress + '-' * (length - progress)

        # Only show progress bar for level > min_level
        if self.level > min_level:
            percent = ("{0:.1f}").format(percent)
            sys.stdout.write('\rProgress |%s| %s%s' % (bar, percent, '%')),
            if iteration == total and carriage_return: 
                sys.stdout.write('\n')
            sys.stdout.flush()



    def abort(self,message):
        self.emit(ABRT,message,'ABRT')        

    def error(self,message):
        self.emit(ERROR,message,'ERROR')        

    def warning(self,message):
        self.emit(WARNING,message,'WARNING')        

    def log(self,message):
        self.emit(LOG,message,'LOG')        

    def info(self,message):
        self.emit(INFO,message)        

    def verbose(self,message):
        self.emit(VERBOSE,message,"VERBOSE")        

    def verbose1(self,message):
        self.emit(VERBOSE,message,"VERBOSE1")        

    def verbose2(self,message):
        self.emit(VERBOSE2,message,'VERBOSE2')        

    def verbose3(self,message):
        self.emit(VERBOSE3,message,'VERBOSE3')        

    def debug(self,message):
        self.emit(DEBUG,message,'DEBUG')        

    def is_quiet(self):
        '''is_quiet returns true if the level is under 1
        '''
        if self.level < 1:
            return False
        return True
    

def get_logging_level():
    '''get_logging_level will configure a logging to standard out based on the user's
    selected level, which should be in an environment variable called
    SINGULARITY_MESSAGELEVEL. if SINGULARITY_MESSAGELEVEL is not set, the maximum level
    (5) is assumed (all messages). levels from
    https://github.com/singularityware/singularity/blob/master/src/lib/message.h

    
    #define ABRT -4
    #define ERROR -3
    #define WARNING -2
    #define LOG -1
    #define INFO 1

    implied define: QUIET 0

    #define VERBOSE 2
    #define VERBOSE1 2
    #define VERBOSE2 3
    #define VERBOSE3 4
    #define DEBUG 5
    '''

    return int(os.environ.get("SINGULARITY_MESSAGELEVEL",5))
    

bot = SingularityMessage()
