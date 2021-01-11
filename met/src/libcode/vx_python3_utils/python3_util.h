

////////////////////////////////////////////////////////////////////////


#ifndef  __MET_PYTHON3_UTIL_H__
#define  __MET_PYTHON3_UTIL_H__


////////////////////////////////////////////////////////////////////////


#include <iostream>


////////////////////////////////////////////////////////////////////////


extern "C" {

#include "Python.h"

}


////////////////////////////////////////////////////////////////////////


static const char user_python_path_env [] = "MET_PYTHON_EXE";

static const char wrappers_dir         [] = "MET_BASE/wrappers";

////////////////////////////////////////////////////////////////////////

static const char stdout_redirect_code [] =
   "import sys\n"
   "class StdoutStream:\n"
   "    def __init__(self):\n"
   "        self.buffer = ''\n"
   "    def write(self, text):\n"
   "        self.buffer = self.buffer + text\n"
   "stdout_stream = StdoutStream()\n"
   "sys.stdout = stdout_stream";

static const char stderr_redirect_code [] =
   "import sys\n"
   "class StderrStream:\n"
   "    def __init__(self):\n"
   "        self.buffer = ''\n"
   "    def write(self, text):\n"
   "        self.buffer = self.buffer + text\n"
   "stderr_stream = StderrStream()\n"
   "sys.stderr = stderr_stream";

////////////////////////////////////////////////////////////////////////


static const int max_tuple_data_dims = 10;


////////////////////////////////////////////////////////////////////////


extern std::ostream & operator<<(std::ostream &, PyObject *);


////////////////////////////////////////////////////////////////////////

   //
   //  returns null if the given object has no attribute of that name
   //

extern PyObject * get_attribute(PyObject *, const char * attribute_name);


////////////////////////////////////////////////////////////////////////


extern int          pyobject_as_int           (PyObject *);
extern double       pyobject_as_double        (PyObject *);
extern std::string  pyobject_as_string        (PyObject *);
extern ConcatString pyobject_as_concat_string (PyObject *);


////////////////////////////////////////////////////////////////////////


extern void run_python_string(const char *);


////////////////////////////////////////////////////////////////////////


#endif   /*  __MET_PYTHON3_UTIL_H__  */


////////////////////////////////////////////////////////////////////////

